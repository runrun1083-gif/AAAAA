// ==============================================================================
// Resource Manager 実装
// ==============================================================================

#include "ResourceManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <fstream>
#include <sstream>
#endif

namespace resource {

// ==============================================================================
// グローバルインスタンス
// ==============================================================================

static ResourceManager* g_resourceManager = nullptr;

ResourceManager* GetResourceManager() {
    if (!g_resourceManager) {
        g_resourceManager = new ResourceManager();
    }
    return g_resourceManager;
}

// ==============================================================================
// ResourceManager 実装
// ==============================================================================

ResourceManager::ResourceManager() {
    std::cout << "[ResourceManager] 初期化開始" << std::endl;

    // デフォルトバジェット: 48GB（安全マージン確保）
    m_budget.astCacheLimit = 12ULL * 1024 * 1024 * 1024;
    m_budget.mlxModelLimit = 28ULL * 1024 * 1024 * 1024;
    m_budget.guiBufferLimit = 4ULL * 1024 * 1024 * 1024;
    m_budget.parserBufferLimit = 4ULL * 1024 * 1024 * 1024;

    std::cout << "[ResourceManager] メモリバジェット設定:" << std::endl;
    std::cout << "  AST Cache:     " << (m_budget.astCacheLimit / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  MLX Model:     " << (m_budget.mlxModelLimit / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  GUI Buffer:    " << (m_budget.guiBufferLimit / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  Parser Buffer: " << (m_budget.parserBufferLimit / 1024 / 1024 / 1024) << " GB" << std::endl;

    size_t total = (m_budget.astCacheLimit + m_budget.mlxModelLimit +
                   m_budget.guiBufferLimit + m_budget.parserBufferLimit) / 1024 / 1024 / 1024;
    std::cout << "  合計:          " << total << " GB (75% of 64GB)" << std::endl;

    std::cout << "[ResourceManager] 初期化完了" << std::endl;
}

ResourceManager::~ResourceManager() {
    stopMonitoring();
    std::cout << "[ResourceManager] シャットダウン完了" << std::endl;
}

void ResourceManager::setMemoryBudget(const MemoryBudget& budget) {
    m_budget = budget;

    std::cout << "[ResourceManager] バジェット更新: "
              << ((m_budget.astCacheLimit + m_budget.mlxModelLimit +
                  m_budget.guiBufferLimit + m_budget.parserBufferLimit) / 1024 / 1024 / 1024)
              << " GB" << std::endl;
}

bool ResourceManager::requestMemory(size_t bytes, const char* purpose) {
    std::cout << "[ResourceManager] メモリリクエスト: "
              << (bytes / 1024 / 1024) << " MB (" << purpose << ")" << std::endl;

    // 用途別の上限チェック
    size_t* targetUsage = nullptr;
    size_t limit = 0;

    if (strcmp(purpose, "AST") == 0) {
        targetUsage = &m_usage.astCacheUsed;
        limit = m_budget.astCacheLimit;
    } else if (strcmp(purpose, "MLX") == 0) {
        targetUsage = &m_usage.mlxModelUsed;
        limit = m_budget.mlxModelLimit;
    } else if (strcmp(purpose, "GUI") == 0) {
        targetUsage = &m_usage.guiBufferUsed;
        limit = m_budget.guiBufferLimit;
    } else if (strcmp(purpose, "Parser") == 0) {
        targetUsage = &m_usage.parserBufferUsed;
        limit = m_budget.parserBufferLimit;
    } else {
        std::cerr << "[ResourceManager] 警告: 不明な用途 \"" << purpose << "\"" << std::endl;
        return false;
    }

    // 上限チェック
    if (*targetUsage + bytes > limit) {
        std::cout << "[ResourceManager] バジェット超過検知 ("
                  << purpose << ": "
                  << ((*targetUsage + bytes) / 1024 / 1024) << " MB > "
                  << (limit / 1024 / 1024) << " MB)" << std::endl;

        // 自動LRU退避を試みる
        size_t targetReduction = (*targetUsage + bytes) - (limit * 0.8);
        size_t evicted = evictToTarget(m_usage.getTotalUsed() - targetReduction);

        if (evicted < targetReduction) {
            std::cerr << "[ResourceManager] エラー: メモリ確保失敗" << std::endl;
            return false;
        }
    }

    // 確保成功
    *targetUsage += bytes;

    std::cout << "[ResourceManager] メモリ確保成功: "
              << purpose << " 使用中 "
              << (*targetUsage / 1024 / 1024) << " MB / "
              << (limit / 1024 / 1024) << " MB" << std::endl;

    return true;
}

void ResourceManager::releaseMemory(size_t bytes, const char* purpose) {
    std::cout << "[ResourceManager] メモリ解放: "
              << (bytes / 1024 / 1024) << " MB (" << purpose << ")" << std::endl;

    if (strcmp(purpose, "AST") == 0) {
        m_usage.astCacheUsed -= bytes;
    } else if (strcmp(purpose, "MLX") == 0) {
        m_usage.mlxModelUsed -= bytes;
    } else if (strcmp(purpose, "GUI") == 0) {
        m_usage.guiBufferUsed -= bytes;
    } else if (strcmp(purpose, "Parser") == 0) {
        m_usage.parserBufferUsed -= bytes;
    }
}

size_t ResourceManager::getSystemAvailableMemory() const {
#ifdef __APPLE__
    // macOS: vm_statistics64
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmStats;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
        // Free pages + Inactive pages
        size_t freeMemory = (vmStats.free_count + vmStats.inactive_count) * vm_page_size;
        return freeMemory;
    }
#endif

#ifdef __linux__
    // Linux: /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    size_t memAvailable = 0;

    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> memAvailable;
            return memAvailable * 1024; // KB → bytes
        }
    }
#endif

    // フォールバック
    return 0;
}

int ResourceManager::getMemoryPressureLevel() const {
    size_t available = getSystemAvailableMemory();
    size_t totalUsed = m_usage.getTotalUsed();

    // 簡易的なメモリ圧迫判定
    // 実際の macOS では memorystatus API を使用
    if (available < 2ULL * 1024 * 1024 * 1024) {  // < 2GB
        return 2; // Critical (赤)
    } else if (available < 8ULL * 1024 * 1024 * 1024) {  // < 8GB
        return 1; // Warning (黄)
    }

    return 0; // Normal (緑)
}

size_t ResourceManager::evictToTarget(size_t targetBytes) {
    std::cout << "[ResourceManager] LRU退避開始: 目標 "
              << (targetBytes / 1024 / 1024) << " MB" << std::endl;

    size_t totalEvicted = 0;

    // 登録された退避コールバックを順次実行
    for (const auto& [name, callback] : m_evictionCallbacks) {
        if (m_usage.getTotalUsed() <= targetBytes) {
            break; // 目標達成
        }

        size_t toEvict = m_usage.getTotalUsed() - targetBytes;
        size_t evicted = callback(toEvict);

        std::cout << "[ResourceManager] " << name << " から "
                  << (evicted / 1024 / 1024) << " MB 退避" << std::endl;

        totalEvicted += evicted;
    }

    std::cout << "[ResourceManager] LRU退避完了: "
              << (totalEvicted / 1024 / 1024) << " MB 解放" << std::endl;

    return totalEvicted;
}

void ResourceManager::registerEvictionCallback(const std::string& name, EvictionCallback callback) {
    std::cout << "[ResourceManager] 退避コールバック登録: " << name << std::endl;
    m_evictionCallbacks[name] = callback;
}

void ResourceManager::startMonitoring() {
    if (m_monitoringActive) {
        return;
    }

    m_monitoringActive = true;

    std::cout << "[ResourceManager] 定期監視スレッド開始" << std::endl;

    // 監視スレッド起動
    std::thread([this]() {
        while (m_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            size_t totalUsed = m_usage.getTotalUsed();
            size_t totalLimit = m_budget.astCacheLimit + m_budget.mlxModelLimit +
                               m_budget.guiBufferLimit + m_budget.parserBufferLimit;

            float usageRatio = static_cast<float>(totalUsed) / totalLimit;

            // 危険閾値超過時に自動退避
            if (usageRatio > m_budget.dangerThreshold) {
                std::cout << "[ResourceManager] 警告: メモリ使用率 "
                          << (usageRatio * 100) << "% (危険閾値 "
                          << (m_budget.dangerThreshold * 100) << "%)" << std::endl;

                size_t targetUsage = totalLimit * m_budget.warningThreshold;
                evictToTarget(targetUsage);
            }

            // Memory Pressure 赤ゾーン検知
            int pressureLevel = getMemoryPressureLevel();
            if (pressureLevel == 2) {
                std::cerr << "[ResourceManager] 緊急: Memory Pressure Critical!" << std::endl;

                // 緊急退避: 50%まで削減
                evictToTarget(totalLimit * 0.5);
            }
        }
    }).detach();
}

void ResourceManager::stopMonitoring() {
    if (m_monitoringActive) {
        m_monitoringActive = false;
        std::cout << "[ResourceManager] 定期監視スレッド停止" << std::endl;
    }
}

} // namespace resource
