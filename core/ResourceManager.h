// ==============================================================================
// Resource Manager - Unified Memory 管理
// ==============================================================================
// 指摘への対応:
// - VRAMバジェット 60GB→48GB (総メモリの75%)
// - 動的監視: os_proc_available_memory() による自動調整
// - OOM Killer回避: 安全マージン確保
// ==============================================================================

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <atomic>
#include <functional>

namespace resource {

/**
 * @brief メモリバジェット設定
 *
 * 改善前（危険）: 60GB (93.75%)
 * 改善後（安全）: 48GB (75%) + 動的監視
 */
struct MemoryBudget {
    // === 固定バジェット（デフォルト: 安全マージン確保）===

    /// AST キャッシュ上限（12GB）
    size_t astCacheLimit = 12ULL * 1024 * 1024 * 1024;

    /// MLX モデル上限（28GB）
    size_t mlxModelLimit = 28ULL * 1024 * 1024 * 1024;

    /// GUI バッファ上限（4GB）
    size_t guiBufferLimit = 4ULL * 1024 * 1024 * 1024;

    /// Tree-sitter パース用（4GB）
    size_t parserBufferLimit = 4ULL * 1024 * 1024 * 1024;

    /// 合計: 48GB（総メモリ64GBの75%）
    /// 残り: 16GB（OS、WindowServer、Chrome等への聖域）

    // === 動的調整パラメータ ===

    /// 危険閾値（総使用量がこれを超えたら自動LRU退避）
    float dangerThreshold = 0.85f;  // 48GB の 85% = 40.8GB

    /// 警告閾値（この値でユーザーに警告表示）
    float warningThreshold = 0.75f; // 48GB の 75% = 36GB

    /// Expert Mode: ユーザーが手動で上限を上げられる
    /// （危険だが、ユーザーが望むなら許可）
    bool expertModeEnabled = false;
    size_t expertMaxLimit = 60ULL * 1024 * 1024 * 1024; // 60GB
};

/**
 * @brief メモリ使用状況
 */
struct MemoryUsage {
    size_t astCacheUsed = 0;
    size_t mlxModelUsed = 0;
    size_t guiBufferUsed = 0;
    size_t parserBufferUsed = 0;

    size_t getTotalUsed() const {
        return astCacheUsed + mlxModelUsed + guiBufferUsed + parserBufferUsed;
    }
};

/**
 * @brief Resource Manager
 *
 * M1/M2 Unified Memory 専用の賢いメモリ管理
 *
 * 主要機能:
 * 1. 動的監視: システム全体の空きメモリを監視
 * 2. 自動LRU退避: メモリ圧迫時に古いキャッシュを自動削除
 * 3. OOM回避: Memory Pressure赤ゾーンに入る前に予防的にパージ
 */
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    /**
     * @brief メモリバジェット設定
     */
    void setMemoryBudget(const MemoryBudget& budget);
    MemoryBudget getMemoryBudget() const { return m_budget; }

    /**
     * @brief メモリ確保リクエスト
     *
     * @param bytes 確保したいバイト数
     * @param purpose 用途（"AST", "MLX", "GUI", "Parser"）
     * @return 確保可能ならtrue
     *
     * @note
     * - バジェット超過時は自動的にLRU退避を試みる
     * - それでも無理なら false を返す
     */
    bool requestMemory(size_t bytes, const char* purpose);

    /**
     * @brief メモリ解放通知
     *
     * @param bytes 解放したバイト数
     * @param purpose 用途
     */
    void releaseMemory(size_t bytes, const char* purpose);

    /**
     * @brief 現在のメモリ使用状況取得
     */
    MemoryUsage getCurrentUsage() const { return m_usage; }

    /**
     * @brief システム全体の空きメモリ取得（動的監視）
     *
     * @return システム全体の空きメモリ（bytes）
     *
     * @note
     * macOS: vm_statistics64 / os_proc_available_memory()
     * Linux: /proc/meminfo
     */
    size_t getSystemAvailableMemory() const;

    /**
     * @brief Memory Pressure レベル取得
     *
     * @return 0=Normal, 1=Warning(黄), 2=Critical(赤)
     */
    int getMemoryPressureLevel() const;

    /**
     * @brief 自動LRU退避実行
     *
     * @param targetBytes 目標使用量
     * @return 実際に解放されたバイト数
     *
     * @note
     * 各コンポーネント（ASTCache等）に退避要求を送る
     */
    size_t evictToTarget(size_t targetBytes);

    /**
     * @brief 定期監視スレッド開始
     *
     * 1秒ごとにメモリ状況を監視し、危険閾値を超えたら
     * 自動的にLRU退避を実行
     */
    void startMonitoring();
    void stopMonitoring();

private:
    MemoryBudget m_budget;
    MemoryUsage m_usage;

    std::atomic<bool> m_monitoringActive{false};

    /**
     * @brief LRU退避コールバック登録
     *
     * ASTCache, MLXEngine等が自分の退避関数を登録
     */
    using EvictionCallback = std::function<size_t(size_t targetBytes)>;
    std::unordered_map<std::string, EvictionCallback> m_evictionCallbacks;

public:
    /**
     * @brief 退避コールバック登録
     *
     * 使用例:
     * resourceManager->registerEvictionCallback("AST", [this](size_t target) {
     *     return m_astCache->evictToTarget(target);
     * });
     */
    void registerEvictionCallback(const std::string& name, EvictionCallback callback);
};

/**
 * @brief グローバルResource Managerインスタンス取得
 *
 * シングルトンパターン
 */
ResourceManager* GetResourceManager();

} // namespace resource
