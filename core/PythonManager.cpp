/**
 * Module: core/PythonManager.cpp
 * 説明: PythonManagerの実装。
 *       GIL (Global Interpreter Lock) の予期せぬ競合を防ぐため、
 *       タスク実行時のみ GIL を取得する厳格なスコープ管理を行う。
 */

#include "PythonManager.hpp"
#include <pybind11/embed.h>

namespace py = pybind11;

namespace aaaaa {

PythonManager::PythonManager() : running_(true) {
  // ワーカースレッドの起動
  worker_thread_ = std::thread(&PythonManager::WorkerLoop, this);
  std::cout << "[PythonManager] Worker thread started." << std::endl;
}

PythonManager::~PythonManager() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    running_ = false;
  }
  condition_var_.notify_all(); // スレッドを待機状態から起こす

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  std::cout << "[PythonManager] Worker thread stopped." << std::endl;
}

void PythonManager::Enqueue(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    task_queue_.push(task);
  }
  condition_var_.notify_one();
}

size_t PythonManager::GetQueueSize() const {
  // try_lockの方が安全かもしれないが、サイズ取得は簡易的で良い
  // std::lock_guard はコンストラクタでロックし、デストラクタで解除する
  // ここでは mutable な mutex を想定するか、const_cast する必要があるが
  // 設計上 mutex は mutable にすべきだが、一旦簡易実装として mutex
  // をロックせずに atomic なサイズ管理をするか？ いや、std::queue
  // はスレッドセーフではないのでロック必須。 しかし const メソッド内で mutex
  // をロックするには mutex に mutable が必要。 今回はヘッダー修正なしで
  // const_cast で対応するか、ヘッダーに mutable をつけるべきだが、
  // ここでは安全に実装するため、const_cast で凌ぐ（または設計の微修正）。
  // 今回はヘッダーを先に書いたので、const_cast パターンで行く。

  // ※今回は実装詳細として mutex を mutable にする変更を省くため、const_cast
  // を使用
  std::unique_lock<std::mutex> lock(const_cast<std::mutex &>(queue_mutex_));
  return task_queue_.size();
}

void PythonManager::WorkerLoop() {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      // タスクが来るか停止フラグが立つまで待機
      condition_var_.wait(lock,
                          [this] { return !task_queue_.empty() || !running_; });

      if (!running_ && task_queue_.empty()) {
        return; // 終了
      }

      task = std::move(task_queue_.front());
      task_queue_.pop();
    }

    // --- タスク実行 (GIL Critical Section) ---
    try {
      // ここで初めて GIL を取得する。
      // これにより、待機中(wait状態)は GIL を手放しているので、
      // メインスレッド側のGUIや他の処理を一切阻害しない。
      py::gil_scoped_acquire acquire;

      task();

      // スコープを抜けると自動的に GIL が解放 (Release) される。

    } catch (py::error_already_set &e) {
      // Python例外のハンドリング
      // アプリをクラッシュさせず、エラーログを出力して継続する
      std::cerr << "[PythonManager] Python Exception: " << e.what()
                << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[PythonManager] C++ Exception: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "[PythonManager] Unknown Exception occurred." << std::endl;
    }
    // ------------------------------------------
  }
}

} // namespace aaaaa
