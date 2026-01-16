/**
 * Module: core/PythonManager.hpp
 * 説明: Pythonランタイムへの非同期アクセスを管理するクラス。
 *       GUIスレッドをブロックしないための非同期キューとGIL制御を提供する。
 * 外部依存:
 *   - pybind11
 *   - std::thread
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

namespace aaaaa {

class PythonManager {
public:
  // コンストラクタ：ワーカースレッドを開始する
  PythonManager();
  // デストラクタ：スレッドを安全に停止する
  ~PythonManager();

  // Pythonタスクをキューに追加する（非同期実行）
  // GUIスレッドから呼ぶことを想定
  void Enqueue(std::function<void()> task);

  // 現在のキューサイズを取得（UI表示用など）
  size_t GetQueueSize() const;

private:
  // ワーカースレッドのメインループ
  void WorkerLoop();

  std::queue<std::function<void()>> task_queue_;
  std::mutex queue_mutex_;
  std::condition_variable condition_var_;

  std::thread worker_thread_;
  std::atomic<bool> running_;
};

} // namespace aaaaa
