#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

// ==============================================================================
// MLX推論エンジン (C++ API - Python経由なし)
// ==============================================================================
// Apple Silicon GPU + Unified Memory (64GB) を最大限活用
// ==============================================================================

namespace mlx {

/**
 * @brief MLXモデル情報
 */
struct ModelInfo {
    std::string name;           ///< モデル名
    std::string path;           ///< モデルパス
    size_t parameterCount;      ///< パラメータ数
    size_t memoryUsageMB;       ///< メモリ使用量（MB）
    bool isLoaded;              ///< ロード済みフラグ
};

/**
 * @brief 推論リクエスト
 */
struct InferenceRequest {
    std::string prompt;                         ///< プロンプト
    size_t maxTokens = 512;                     ///< 最大トークン数
    float temperature = 0.7f;                   ///< サンプリング温度
    std::function<void(const std::string&)> onToken;  ///< トークンごとのコールバック
    std::function<void(const std::string&)> onComplete; ///< 完了時のコールバック
    std::function<void(const std::string&)> onError;    ///< エラー時のコールバック
};

/**
 * @brief MLX推論エンジン
 *
 * Apple Silicon専用の高速推論エンジン
 * - Unified Memory完全活用
 * - Metal GPU加速
 * - 非同期推論
 */
class MLXEngine {
public:
    MLXEngine();
    ~MLXEngine();

    // コピー・ムーブ禁止
    MLXEngine(const MLXEngine&) = delete;
    MLXEngine& operator=(const MLXEngine&) = delete;

    /**
     * @brief モデルディレクトリのスキャン
     *
     * @param modelsDir モデルディレクトリのパス
     * @return 利用可能なモデルのリスト
     */
    std::vector<ModelInfo> scanModels(const std::string& modelsDir);

    /**
     * @brief モデルのロード
     *
     * @param modelPath モデルのパス
     * @return 成功ならtrue
     *
     * @note
     * CP5 必須成功要件:
     * - 7Bモデルで最低50 tokens/sec
     * - メモリ不足時の安全停止（Circuit Breaker）
     */
    bool loadModel(const std::string& modelPath);

    /**
     * @brief モデルのアンロード
     */
    void unloadModel();

    /**
     * @brief 推論実行（非同期）
     *
     * @param request 推論リクエスト
     *
     * @note
     * GUIスレッドをブロックせず、専用スレッドで実行
     */
    void inferAsync(const InferenceRequest& request);

    /**
     * @brief 現在ロード中のモデル情報を取得
     */
    const ModelInfo* getCurrentModel() const;

    /**
     * @brief 推論中かどうか
     */
    bool isInferring() const;

    /**
     * @brief GPU使用率を取得（0.0〜1.0）
     */
    float getGPUUsage() const;

    /**
     * @brief メモリ使用率を取得（0.0〜1.0）
     */
    float getMemoryUsage() const;

    /**
     * @brief トークン生成速度を取得（tokens/sec）
     */
    float getTokensPerSecond() const;

private:
    /**
     * @brief 推論ワーカースレッドのメインループ
     */
    void inferenceWorkerLoop();

    /**
     * @brief 実際の推論処理（スタブ実装）
     */
    std::string runInference(const std::string& prompt, size_t maxTokens, float temperature);

private:
    std::unique_ptr<ModelInfo> m_currentModel;    ///< 現在ロード中のモデル
    std::atomic<bool> m_isInferring{false};       ///< 推論中フラグ
    std::atomic<float> m_tokensPerSecond{0.0f};   ///< トークン生成速度

    mutable std::mutex m_mutex;                   ///< スレッド保護用mutex

    // TODO: 実際のMLX C++ API構造体をここに追加
    // void* m_mlxModel = nullptr;
};

} // namespace mlx
