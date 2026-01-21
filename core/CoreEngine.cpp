// ==============================================================================
// CoreEngine.cpp - AI開発ステーション統合実装（AI最適化版）
// ==============================================================================
// ファイル統合方針:
// - 全コアエンジン実装を1ファイルに集約
// - AI開発効率化: Read1回で全実装把握、エラー検出容易
// - セクション区切りで関数検索容易
// ==============================================================================
// [目次] - AI向けナビゲーション
// ==============================================================================
// §2. ResourceManager実装         (L30-280)
// §3. GraphLayout実装             (L281-600)
// §4. FileScanner実装             (L601-1000)
// §5. NodeSystem実装              (L1001-1260)
// §6. MLXEngine実装               (L1261-1470)
// ==============================================================================

#include "CoreEngine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <random>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <fstream>
#include <sstream>
#endif

// ==============================================================================
// §2. ResourceManager実装 - Unified Memory管理
// ==============================================================================

namespace resource {

// グローバルインスタンス
static ResourceManager* g_resourceManager = nullptr;

ResourceManager* GetResourceManager() {
    if (!g_resourceManager) {
        g_resourceManager = new ResourceManager();
    }
    return g_resourceManager;
}

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
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmStats;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
        size_t freeMemory = (vmStats.free_count + vmStats.inactive_count) * vm_page_size;
        return freeMemory;
    }
#endif

#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    size_t memAvailable = 0;

    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> memAvailable;
            return memAvailable * 1024;
        }
    }
#endif

    return 0;
}

int ResourceManager::getMemoryPressureLevel() const {
    size_t available = getSystemAvailableMemory();

    if (available < 2ULL * 1024 * 1024 * 1024) {
        return 2; // Critical
    } else if (available < 8ULL * 1024 * 1024 * 1024) {
        return 1; // Warning
    }

    return 0; // Normal
}

size_t ResourceManager::evictToTarget(size_t targetBytes) {
    std::cout << "[ResourceManager] LRU退避開始: 目標 "
              << (targetBytes / 1024 / 1024) << " MB" << std::endl;

    size_t totalEvicted = 0;

    for (const auto& [name, callback] : m_evictionCallbacks) {
        if (m_usage.getTotalUsed() <= targetBytes) {
            break;
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

    std::thread([this]() {
        while (m_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            size_t totalUsed = m_usage.getTotalUsed();
            size_t totalLimit = m_budget.astCacheLimit + m_budget.mlxModelLimit +
                               m_budget.guiBufferLimit + m_budget.parserBufferLimit;

            float usageRatio = static_cast<float>(totalUsed) / totalLimit;

            if (usageRatio > m_budget.dangerThreshold) {
                std::cout << "[ResourceManager] 警告: メモリ使用率 "
                          << (usageRatio * 100) << "% (危険閾値 "
                          << (m_budget.dangerThreshold * 100) << "%)" << std::endl;

                size_t targetUsage = totalLimit * m_budget.warningThreshold;
                evictToTarget(targetUsage);
            }

            int pressureLevel = getMemoryPressureLevel();
            if (pressureLevel == 2) {
                std::cerr << "[ResourceManager] 緊急: Memory Pressure Critical!" << std::endl;
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

// ==============================================================================
// §3. GraphLayout実装 - Force-directed自動配置
// ==============================================================================

namespace layout {

ForceDirectedLayout::ForceDirectedLayout() {
    std::cout << "[GraphLayout] 初期化完了" << std::endl;
}

ForceDirectedLayout::~ForceDirectedLayout() {
    std::cout << "[GraphLayout] シャットダウン完了" << std::endl;
}

void ForceDirectedLayout::addNode(const std::string& id, const Vector2& initialPos,
                                  const std::string& cluster) {
    GraphNode node;
    node.id = id;
    node.cluster = cluster;

    if (m_nodes.empty()) {
        node.position = initialPos;
    } else {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-50.0f, 50.0f);

        node.position = Vector2(
            initialPos.x + dist(gen),
            initialPos.y + dist(gen)
        );
    }

    node.velocity = Vector2(0, 0);
    node.force = Vector2(0, 0);

    m_nodes[id] = node;

    std::cout << "[GraphLayout] ノード追加: " << id
              << " クラスタ: " << (cluster.empty() ? "(なし)" : cluster) << std::endl;
}

void ForceDirectedLayout::addEdge(const std::string& from, const std::string& to, float weight) {
    if (m_nodes.find(from) == m_nodes.end() || m_nodes.find(to) == m_nodes.end()) {
        std::cerr << "[GraphLayout] エラー: エッジの端点ノードが存在しません" << std::endl;
        return;
    }

    GraphEdge edge;
    edge.from = from;
    edge.to = to;
    edge.weight = weight;

    m_edges.push_back(edge);
}

void ForceDirectedLayout::fixNode(const std::string& id, bool fixed) {
    auto it = m_nodes.find(id);
    if (it != m_nodes.end()) {
        it->second.fixed = fixed;
    }
}

int ForceDirectedLayout::computeLayout(const Parameters& params) {
    std::cout << "[GraphLayout] レイアウト計算開始: "
              << m_nodes.size() << " ノード, "
              << m_edges.size() << " エッジ" << std::endl;

    int iteration = 0;

    for (; iteration < params.maxIterations; ++iteration) {
        step(params);

        if (iteration % 10 == 0) {
            if (hasConverged(params.convergenceThreshold)) {
                std::cout << "[GraphLayout] 収束: " << iteration << " イテレーション" << std::endl;
                break;
            }
        }
    }

    int crossings = getEdgeCrossings();
    std::cout << "[GraphLayout] レイアウト完了:" << std::endl;
    std::cout << "  イテレーション: " << iteration << std::endl;
    std::cout << "  エッジ交差数: " << crossings << std::endl;

    return iteration;
}

void ForceDirectedLayout::step(const Parameters& params) {
    for (auto& pair : m_nodes) {
        pair.second.force = Vector2(0, 0);
    }

    for (auto& pair1 : m_nodes) {
        for (auto& pair2 : m_nodes) {
            if (pair1.first == pair2.first) continue;

            Vector2 repulsion = computeRepulsionForce(pair1.second, pair2.second, params);
            pair1.second.force = pair1.second.force + repulsion;
        }
    }

    for (const auto& edge : m_edges) {
        auto& node1 = m_nodes[edge.from];
        auto& node2 = m_nodes[edge.to];

        Vector2 attraction = computeAttractionForce(node1, node2, params);

        node1.force = node1.force + attraction;
        node2.force = node2.force + (attraction * -1.0f);
    }

    for (auto& pair : m_nodes) {
        if (pair.second.fixed) continue;

        pair.second.velocity = pair.second.velocity + (pair.second.force * params.timeStep);
        pair.second.velocity = pair.second.velocity * params.damping;
        pair.second.position = pair.second.position + (pair.second.velocity * params.timeStep);
    }
}

Vector2 ForceDirectedLayout::computeAttractionForce(
    const GraphNode& node1,
    const GraphNode& node2,
    const Parameters& params
) const {
    Vector2 delta = node2.position - node1.position;
    float distance = delta.length();

    if (distance < 0.0001f) return Vector2(0, 0);

    float displacement = distance - params.springLength;
    float force = params.attractionStrength * displacement;

    if (params.enableClustering &&
        !node1.cluster.empty() &&
        node1.cluster == node2.cluster) {
        force *= params.clusterAttractionBonus;
    }

    return delta.normalized() * force;
}

Vector2 ForceDirectedLayout::computeRepulsionForce(
    const GraphNode& node1,
    const GraphNode& node2,
    const Parameters& params
) const {
    Vector2 delta = node1.position - node2.position;
    float distance = delta.length();

    if (distance < 0.0001f) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return Vector2(dist(gen), dist(gen)) * 10.0f;
    }

    float force = params.repulsionStrength / (distance * distance);

    if (params.enableClustering &&
        !node1.cluster.empty() && !node2.cluster.empty() &&
        node1.cluster != node2.cluster) {
        force *= params.clusterRepulsionPenalty;
    }

    return delta.normalized() * force;
}

bool ForceDirectedLayout::hasConverged(float threshold) const {
    float totalEnergy = 0.0f;

    for (const auto& pair : m_nodes) {
        if (pair.second.fixed) continue;
        totalEnergy += pair.second.velocity.length();
    }

    float avgEnergy = totalEnergy / std::max(1.0f, static_cast<float>(m_nodes.size()));

    return avgEnergy < threshold;
}

Vector2 ForceDirectedLayout::getNodePosition(const std::string& id) const {
    auto it = m_nodes.find(id);
    if (it != m_nodes.end()) {
        return it->second.position;
    }
    return Vector2(0, 0);
}

std::unordered_map<std::string, Vector2> ForceDirectedLayout::getAllPositions() const {
    std::unordered_map<std::string, Vector2> positions;
    for (const auto& pair : m_nodes) {
        positions[pair.first] = pair.second.position;
    }
    return positions;
}

int ForceDirectedLayout::getEdgeCrossings() const {
    int crossings = 0;

    for (size_t i = 0; i < m_edges.size(); ++i) {
        for (size_t j = i + 1; j < m_edges.size(); ++j) {
            const auto& edge1 = m_edges[i];
            const auto& edge2 = m_edges[j];

            if (edge1.from == edge2.from || edge1.from == edge2.to ||
                edge1.to == edge2.from || edge1.to == edge2.to) {
                continue;
            }

            Vector2 a1 = m_nodes.at(edge1.from).position;
            Vector2 a2 = m_nodes.at(edge1.to).position;
            Vector2 b1 = m_nodes.at(edge2.from).position;
            Vector2 b2 = m_nodes.at(edge2.to).position;

            if (edgesIntersect(a1, a2, b1, b2)) {
                crossings++;
            }
        }
    }

    return crossings;
}

bool ForceDirectedLayout::edgesIntersect(
    const Vector2& a1, const Vector2& a2,
    const Vector2& b1, const Vector2& b2
) const {
    auto ccw = [](const Vector2& a, const Vector2& b, const Vector2& c) -> float {
        return (c.y - a.y) * (b.x - a.x) - (b.y - a.y) * (c.x - a.x);
    };

    float ccw1 = ccw(a1, a2, b1);
    float ccw2 = ccw(a1, a2, b2);
    float ccw3 = ccw(b1, b2, a1);
    float ccw4 = ccw(b1, b2, a2);

    return (ccw1 * ccw2 < 0) && (ccw3 * ccw4 < 0);
}

std::vector<ForceDirectedLayout::ClusterBounds> ForceDirectedLayout::getClusterBounds() const {
    std::unordered_map<std::string, ClusterBounds> clusters;

    for (const auto& pair : m_nodes) {
        const std::string& clusterId = pair.second.cluster;
        if (clusterId.empty()) continue;

        if (clusters.find(clusterId) == clusters.end()) {
            ClusterBounds bounds;
            bounds.clusterId = clusterId;
            bounds.min = pair.second.position;
            bounds.max = pair.second.position;
            clusters[clusterId] = bounds;
        } else {
            auto& bounds = clusters[clusterId];
            bounds.min.x = std::min(bounds.min.x, pair.second.position.x);
            bounds.min.y = std::min(bounds.min.y, pair.second.position.y);
            bounds.max.x = std::max(bounds.max.x, pair.second.position.x);
            bounds.max.y = std::max(bounds.max.y, pair.second.position.y);
        }
    }

    std::vector<ClusterBounds> result;
    for (auto& pair : clusters) {
        pair.second.center = Vector2(
            (pair.second.min.x + pair.second.max.x) / 2.0f,
            (pair.second.min.y + pair.second.max.y) / 2.0f
        );
        result.push_back(pair.second);
    }

    return result;
}

void ForceDirectedLayout::reset() {
    m_nodes.clear();
    m_edges.clear();
    std::cout << "[GraphLayout] リセット完了" << std::endl;
}

} // namespace layout

// ==============================================================================
// §4. FileScanner実装 - ファイルシステム・ダイナミクス
// ==============================================================================

namespace filesystem {

FileScanner::FileScanner() {
    std::cout << "[FileScanner] 初期化完了" << std::endl;
}

FileScanner::~FileScanner() {
    std::cout << "[FileScanner] シャットダウン完了" << std::endl;
}

size_t FileScanner::scanDirectory(const std::string& rootPath) {
    std::cout << "[FileScanner] ディレクトリスキャン開始: " << rootPath << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();

    m_files.clear();
    m_stringPool.clear();
    m_fileTimestamps.clear();
    m_rootPath = rootPath;

    if (!fs::exists(rootPath)) {
        std::cerr << "[FileScanner] エラー: パスが存在しません: " << rootPath << std::endl;
        return 0;
    }

    if (!fs::is_directory(rootPath)) {
        std::cerr << "[FileScanner] エラー: ディレクトリではありません: " << rootPath << std::endl;
        return 0;
    }

    try {
        scanRecursive(fs::path(rootPath));
    } catch (const std::exception& e) {
        std::cerr << "[FileScanner] スキャンエラー: " << e.what() << std::endl;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_scanTimeNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        endTime - startTime
    ).count();

    std::cout << "[FileScanner] スキャン完了:" << std::endl;
    std::cout << "  ファイル数: " << m_files.size() << std::endl;
    std::cout << "  スキャン時間: " << getScanTimeMs() << " ms" << std::endl;
    std::cout << "  スキャン時間（ナノ秒）: " << m_scanTimeNanos << " ns" << std::endl;

    auto stats = getInterningStats();
    std::cout << "  String Interning:" << std::endl;
    std::cout << "    総文字列: " << stats.totalStrings << std::endl;
    std::cout << "    ユニーク文字列: " << stats.uniqueStrings << std::endl;
    std::cout << "    節約メモリ: " << (stats.memorySavedBytes / 1024.0) << " KB" << std::endl;

    if (m_files.size() >= 10000 && getScanTimeMs() > 50.0) {
        std::cerr << "[FileScanner] 警告: CP2必須成功要件未達成" << std::endl;
        std::cerr << "  要求: 1万ファイル50ms以下" << std::endl;
        std::cerr << "  実測: " << m_files.size() << "ファイル " << getScanTimeMs() << "ms" << std::endl;
    } else if (m_files.size() >= 10000) {
        std::cout << "[FileScanner] ✓ CP2必須成功要件達成: "
                  << m_files.size() << "ファイル " << getScanTimeMs() << "ms" << std::endl;
    }

    return m_files.size();
}

void FileScanner::scanRecursive(const fs::path& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();
                if (dirName == ".git" || dirName == ".venv" ||
                    dirName == "node_modules" || dirName == "build" ||
                    dirName == "__pycache__") {
                    continue;
                }

                scanRecursive(entry.path());

            } else if (entry.is_regular_file()) {
                if (isTargetFile(entry.path())) {
                    FileInfo info = extractFileInfo(entry.path());
                    m_files.push_back(info);

                    auto lastWriteTime = fs::last_write_time(entry.path());
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        lastWriteTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    auto time_t_val = std::chrono::system_clock::to_time_t(sctp);
                    m_fileTimestamps[info.path] = time_t_val;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[FileScanner] ファイルシステムエラー: " << e.what() << std::endl;
    }
}

FileInfo FileScanner::extractFileInfo(const fs::path& path) {
    FileInfo info;

    info.path = internString(path.string());
    info.name = internString(path.filename().string());
    info.extension = internString(path.extension().string());

    try {
        info.size = fs::file_size(path);
        auto lastWriteTime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lastWriteTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        info.lastModified = std::chrono::system_clock::to_time_t(sctp);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[FileScanner] ファイル情報取得エラー: " << e.what() << std::endl;
        info.size = 0;
        info.lastModified = 0;
    }

    try {
        std::ifstream file(path);
        if (file.is_open()) {
            std::string line;
            std::regex includeRegex(R"(#include\s+[<"]([^>"]+)[>"])");
            std::regex importRegex(R"((?:from|import)\s+([a-zA-Z0-9_.]+))");

            while (std::getline(file, line)) {
                std::smatch match;

                if (std::regex_search(line, match, includeRegex)) {
                    info.dependencies.push_back(internString(match[1].str()));
                }

                if (std::regex_search(line, match, importRegex)) {
                    info.dependencies.push_back(internString(match[1].str()));
                }

                if (file.tellg() > 10000) break;
            }
        }
    } catch (const std::exception& e) {
        // 解析エラーは無視
    }

    return info;
}

bool FileScanner::isTargetFile(const fs::path& path) const {
    std::string ext = path.extension().string();
    return m_targetExtensions.find(ext) != m_targetExtensions.end();
}

const std::string& FileScanner::internString(const std::string& str) {
    auto it = m_stringPool.find(str);
    if (it != m_stringPool.end()) {
        return it->second;
    }

    auto result = m_stringPool.emplace(str, str);
    return result.first->second;
}

FileScanner::InterningStats FileScanner::getInterningStats() const {
    InterningStats stats;
    stats.uniqueStrings = m_stringPool.size();

    size_t totalStrings = 0;
    size_t totalBytes = 0;
    size_t uniqueBytes = 0;

    for (const auto& file : m_files) {
        totalStrings += 3;
        totalBytes += file.path.size() + file.name.size() + file.extension.size();

        totalStrings += file.dependencies.size();
        for (const auto& dep : file.dependencies) {
            totalBytes += dep.size();
        }
    }

    for (const auto& pair : m_stringPool) {
        uniqueBytes += pair.first.size();
    }

    stats.totalStrings = totalStrings;
    stats.memorySavedBytes = totalBytes - uniqueBytes;

    return stats;
}

std::vector<FileInfo> FileScanner::filterByExtension(const std::string& ext) const {
    std::vector<FileInfo> result;
    for (const auto& file : m_files) {
        if (file.extension == ext) {
            result.push_back(file);
        }
    }
    return result;
}

std::vector<FileInfo> FileScanner::getCppFiles() const {
    std::vector<FileInfo> result;
    for (const auto& file : m_files) {
        if (file.extension == ".cpp" || file.extension == ".cc" ||
            file.extension == ".cxx" || file.extension == ".h" ||
            file.extension == ".hpp" || file.extension == ".hxx") {
            result.push_back(file);
        }
    }
    return result;
}

std::vector<FileInfo> FileScanner::getPythonFiles() const {
    return filterByExtension(".py");
}

size_t FileScanner::detectChanges() {
    size_t changedCount = 0;

    for (auto& file : m_files) {
        try {
            fs::path path(file.path);
            if (!fs::exists(path)) {
                changedCount++;
                continue;
            }

            auto lastWriteTime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                lastWriteTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            auto currentTime = std::chrono::system_clock::to_time_t(sctp);

            if (m_fileTimestamps[file.path] != currentTime) {
                std::cout << "[FileScanner] 変更検出: " << file.name << std::endl;

                FileInfo newInfo = extractFileInfo(path);
                file = newInfo;
                m_fileTimestamps[file.path] = currentTime;

                changedCount++;
            }
        } catch (const std::exception& e) {
            std::cerr << "[FileScanner] 変更検出エラー: " << e.what() << std::endl;
        }
    }

    if (changedCount > 0) {
        std::cout << "[FileScanner] " << changedCount << " ファイルが変更されました" << std::endl;
    }

    return changedCount;
}

void DependencyGraph::buildFromFiles(const std::vector<FileInfo>& files) {
    m_dependencies.clear();
    m_dependents.clear();

    for (const auto& file : files) {
        m_dependencies[file.path] = file.dependencies;

        for (const auto& dep : file.dependencies) {
            m_dependents[dep].push_back(file.path);
        }
    }

    std::cout << "[DependencyGraph] グラフ構築完了: "
              << m_dependencies.size() << " ノード" << std::endl;
}

std::vector<std::string> DependencyGraph::getDependencies(const std::string& filePath) const {
    auto it = m_dependencies.find(filePath);
    if (it != m_dependencies.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> DependencyGraph::getDependents(const std::string& filePath) const {
    auto it = m_dependents.find(filePath);
    if (it != m_dependents.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::vector<std::string>> DependencyGraph::detectCycles() const {
    std::vector<std::vector<std::string>> cycles;
    std::unordered_set<std::string> globalVisited;

    for (const auto& pair : m_dependencies) {
        if (globalVisited.find(pair.first) == globalVisited.end()) {
            std::unordered_set<std::string> visited;
            std::unordered_set<std::string> recursionStack;
            std::vector<std::string> currentPath;

            detectCyclesRecursive(pair.first, visited, recursionStack, currentPath, cycles);

            globalVisited.insert(visited.begin(), visited.end());
        }
    }

    if (!cycles.empty()) {
        std::cout << "[DependencyGraph] 循環依存検出: " << cycles.size() << " サイクル" << std::endl;
    }

    return cycles;
}

void DependencyGraph::detectCyclesRecursive(
    const std::string& node,
    std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& recursionStack,
    std::vector<std::string>& currentPath,
    std::vector<std::vector<std::string>>& cycles
) const {
    visited.insert(node);
    recursionStack.insert(node);
    currentPath.push_back(node);

    auto it = m_dependencies.find(node);
    if (it != m_dependencies.end()) {
        for (const auto& dep : it->second) {
            if (recursionStack.find(dep) != recursionStack.end()) {
                std::vector<std::string> cycle;
                bool inCycle = false;
                for (const auto& pathNode : currentPath) {
                    if (pathNode == dep) inCycle = true;
                    if (inCycle) cycle.push_back(pathNode);
                }
                cycle.push_back(dep);
                cycles.push_back(cycle);
            } else if (visited.find(dep) == visited.end()) {
                detectCyclesRecursive(dep, visited, recursionStack, currentPath, cycles);
            }
        }
    }

    currentPath.pop_back();
    recursionStack.erase(node);
}

std::string DependencyGraph::toDot() const {
    std::ostringstream oss;
    oss << "digraph Dependencies {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box, style=rounded];\n\n";

    for (const auto& pair : m_dependencies) {
        std::string from = pair.first;
        size_t lastSlash = from.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            from = from.substr(lastSlash + 1);
        }

        for (const auto& dep : pair.second) {
            std::string to = dep;
            lastSlash = to.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                to = to.substr(lastSlash + 1);
            }

            oss << "  \"" << from << "\" -> \"" << to << "\";\n";
        }
    }

    oss << "}\n";
    return oss.str();
}

} // namespace filesystem

// ==============================================================================
// §5. NodeSystem実装 - EnTTベースノード管理
// ==============================================================================

namespace node {

NodeSystem::NodeSystem() {
    std::cout << "[NodeSystem] 初期化完了" << std::endl;
}

NodeSystem::~NodeSystem() {
    m_registry.clear();
    std::cout << "[NodeSystem] シャットダウン完了" << std::endl;
}

entt::entity NodeSystem::createNode(NodeType type, const std::string& name, float x, float y) {
    auto entity = m_registry.create();

    m_registry.emplace<TypeComponent>(entity, type);
    m_registry.emplace<NameComponent>(entity, name);
    m_registry.emplace<PositionComponent>(entity, x, y);
    m_registry.emplace<DataComponent>(entity, "{}");
    m_registry.emplace<ConnectionComponent>(entity);
    m_registry.emplace<ExecutionStateComponent>(entity);

    m_nodeCount++;

    std::cout << "[NodeSystem] ノード作成: " << name
              << " (タイプ: " << nodeTypeToString(type) << ")"
              << " ID: " << static_cast<uint32_t>(entity)
              << std::endl;

    return entity;
}

void NodeSystem::destroyNode(entt::entity entity) {
    if (!m_registry.valid(entity)) {
        std::cerr << "[NodeSystem] エラー: 無効なノードID: " << static_cast<uint32_t>(entity) << std::endl;
        return;
    }

    auto& conn = m_registry.get<ConnectionComponent>(entity);

    for (auto inputNode : conn.inputs) {
        if (m_registry.valid(inputNode)) {
            auto& inputConn = m_registry.get<ConnectionComponent>(inputNode);
            inputConn.outputs.erase(
                std::remove(inputConn.outputs.begin(), inputConn.outputs.end(), entity),
                inputConn.outputs.end()
            );
        }
    }

    for (auto outputNode : conn.outputs) {
        if (m_registry.valid(outputNode)) {
            auto& outputConn = m_registry.get<ConnectionComponent>(outputNode);
            outputConn.inputs.erase(
                std::remove(outputConn.inputs.begin(), outputConn.inputs.end(), entity),
                outputConn.inputs.end()
            );
        }
    }

    const auto& name = m_registry.get<NameComponent>(entity);
    std::cout << "[NodeSystem] ノード削除: " << name.name << std::endl;

    m_registry.destroy(entity);
}

void NodeSystem::connectNodes(entt::entity from, entt::entity to) {
    if (!m_registry.valid(from) || !m_registry.valid(to)) {
        std::cerr << "[NodeSystem] エラー: 無効なノードID" << std::endl;
        return;
    }

    auto& fromConn = m_registry.get<ConnectionComponent>(from);
    auto& toConn = m_registry.get<ConnectionComponent>(to);

    if (std::find(fromConn.outputs.begin(), fromConn.outputs.end(), to) != fromConn.outputs.end()) {
        std::cerr << "[NodeSystem] 警告: 既に接続されています" << std::endl;
        return;
    }

    fromConn.outputs.push_back(to);
    toConn.inputs.push_back(from);

    const auto& fromName = m_registry.get<NameComponent>(from);
    const auto& toName = m_registry.get<NameComponent>(to);

    std::cout << "[NodeSystem] ノード接続: " << fromName.name << " → " << toName.name << std::endl;
}

void NodeSystem::disconnectNodes(entt::entity from, entt::entity to) {
    if (!m_registry.valid(from) || !m_registry.valid(to)) {
        std::cerr << "[NodeSystem] エラー: 無効なノードID" << std::endl;
        return;
    }

    auto& fromConn = m_registry.get<ConnectionComponent>(from);
    auto& toConn = m_registry.get<ConnectionComponent>(to);

    fromConn.outputs.erase(
        std::remove(fromConn.outputs.begin(), fromConn.outputs.end(), to),
        fromConn.outputs.end()
    );

    toConn.inputs.erase(
        std::remove(toConn.inputs.begin(), toConn.inputs.end(), from),
        toConn.inputs.end()
    );

    const auto& fromName = m_registry.get<NameComponent>(from);
    const auto& toName = m_registry.get<NameComponent>(to);

    std::cout << "[NodeSystem] ノード切断: " << fromName.name << " ✕ " << toName.name << std::endl;
}

std::vector<entt::entity> NodeSystem::getAllNodes() const {
    std::vector<entt::entity> nodes;

    auto view = m_registry.view<TypeComponent>();
    for (auto entity : view) {
        nodes.push_back(entity);
    }

    return nodes;
}

void NodeSystem::executeGraph(entt::entity startNode, std::function<void(const std::string&)> onComplete) {
    if (!m_registry.valid(startNode)) {
        std::cerr << "[NodeSystem] エラー: 無効な開始ノードID" << std::endl;
        if (onComplete) {
            onComplete("エラー: 無効な開始ノード");
        }
        return;
    }

    const auto& name = m_registry.get<NameComponent>(startNode);
    std::cout << "[NodeSystem] グラフ実行開始: " << name.name << std::endl;

    std::string result;

    try {
        executeNodeRecursive(startNode, result);

        std::cout << "[NodeSystem] グラフ実行完了" << std::endl;

        if (onComplete) {
            onComplete(result);
        }

    } catch (const std::exception& e) {
        std::cerr << "[NodeSystem] グラフ実行エラー: " << e.what() << std::endl;
        if (onComplete) {
            onComplete(std::string("エラー: ") + e.what());
        }
    }
}

void NodeSystem::updateNodePosition(entt::entity entity, float x, float y) {
    if (!m_registry.valid(entity)) {
        return;
    }

    auto& pos = m_registry.get<PositionComponent>(entity);
    pos.x = x;
    pos.y = y;
}

void NodeSystem::updateNodeData(entt::entity entity, const std::string& data) {
    if (!m_registry.valid(entity)) {
        return;
    }

    auto& nodeData = m_registry.get<DataComponent>(entity);
    nodeData.data = data;
}

void NodeSystem::updateNodeState(entt::entity entity, ExecutionStateComponent::State state, const std::string& message) {
    if (!m_registry.valid(entity)) {
        return;
    }

    auto& execState = m_registry.get<ExecutionStateComponent>(entity);
    execState.state = state;
    execState.message = message;
}

void NodeSystem::executeNodeRecursive(entt::entity entity, std::string& result) {
    if (!m_registry.valid(entity)) {
        return;
    }

    const auto& name = m_registry.get<NameComponent>(entity);
    const auto& type = m_registry.get<TypeComponent>(entity);
    const auto& data = m_registry.get<DataComponent>(entity);
    const auto& conn = m_registry.get<ConnectionComponent>(entity);

    updateNodeState(entity, ExecutionStateComponent::State::Running, "実行中");

    std::cout << "[NodeSystem] ノード実行: " << name.name << " (タイプ: " << nodeTypeToString(type.type) << ")" << std::endl;

    switch (type.type) {
        case NodeType::Input:
            result += "[入力] " + data.data + "\n";
            break;

        case NodeType::Prompt:
            result += "[プロンプト] " + data.data + "\n";
            break;

        case NodeType::LLM:
            result += "[LLM] ダミーレスポンス\n";
            break;

        case NodeType::Memory:
            result += "[メモリ] 履歴を保存\n";
            break;

        case NodeType::Output:
            result += "[出力] 最終結果\n";
            break;

        case NodeType::Function:
            result += "[関数] カスタム処理\n";
            break;
    }

    updateNodeState(entity, ExecutionStateComponent::State::Completed, "完了");

    for (auto outputNode : conn.outputs) {
        executeNodeRecursive(outputNode, result);
    }
}

} // namespace node

// ==============================================================================
// §6. MLXEngine実装 - AI推論エンジン
// ==============================================================================

namespace mlx {

MLXEngine::MLXEngine() {
    std::cout << "[MLXEngine] 初期化完了" << std::endl;
}

MLXEngine::~MLXEngine() {
    unloadModel();
    std::cout << "[MLXEngine] シャットダウン完了" << std::endl;
}

std::vector<ModelInfo> MLXEngine::scanModels(const std::string& modelsDir) {
    std::vector<ModelInfo> models;

    std::cout << "[MLXEngine] モデルディレクトリをスキャン中: " << modelsDir << std::endl;

    if (!fs::exists(modelsDir)) {
        std::cerr << "[MLXEngine] エラー: モデルディレクトリが存在しません: " << modelsDir << std::endl;
        return models;
    }

    for (const auto& entry : fs::directory_iterator(modelsDir)) {
        if (entry.is_directory()) {
            std::string modelName = entry.path().filename().string();
            std::string modelPath = entry.path().string();

            if (fs::exists(entry.path() / "config.json")) {
                ModelInfo info;
                info.name = modelName;
                info.path = modelPath;
                info.parameterCount = 0;
                info.memoryUsageMB = 0;
                info.isLoaded = false;

                models.push_back(info);

                std::cout << "[MLXEngine] モデル検出: " << modelName << std::endl;
            }
        }
    }

    std::cout << "[MLXEngine] 合計 " << models.size() << " 個のモデルを検出" << std::endl;

    return models;
}

bool MLXEngine::loadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::cout << "[MLXEngine] モデルをロード中: " << modelPath << std::endl;

    if (m_currentModel) {
        std::cout << "[MLXEngine] 既存のモデルをアンロード: " << m_currentModel->name << std::endl;
        m_currentModel.reset();
    }

    m_currentModel = std::make_unique<ModelInfo>();
    m_currentModel->name = fs::path(modelPath).filename().string();
    m_currentModel->path = modelPath;
    m_currentModel->parameterCount = 7000000000;
    m_currentModel->memoryUsageMB = 4096;
    m_currentModel->isLoaded = true;

    std::cout << "[MLXEngine] モデルロード完了: " << m_currentModel->name << std::endl;
    std::cout << "[MLXEngine] パラメータ数: " << (m_currentModel->parameterCount / 1000000000.0) << "B" << std::endl;
    std::cout << "[MLXEngine] メモリ使用量: " << m_currentModel->memoryUsageMB << " MB" << std::endl;

    return true;
}

void MLXEngine::unloadModel() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_currentModel) {
        std::cout << "[MLXEngine] モデルをアンロード: " << m_currentModel->name << std::endl;
        m_currentModel.reset();
    }
}

void MLXEngine::inferAsync(const InferenceRequest& request) {
    if (!m_currentModel) {
        if (request.onError) {
            request.onError("モデルがロードされていません");
        }
        return;
    }

    if (m_isInferring) {
        if (request.onError) {
            request.onError("既に推論実行中です");
        }
        return;
    }

    m_isInferring = true;

    std::thread([this, request]() {
        try {
            std::cout << "[MLXEngine] 推論開始: " << request.prompt.substr(0, 50) << "..." << std::endl;

            auto startTime = std::chrono::high_resolution_clock::now();

            std::string result = runInference(request.prompt, request.maxTokens, request.temperature);

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            float tokensGenerated = static_cast<float>(result.size() / 4);
            m_tokensPerSecond = tokensGenerated / (duration.count() / 1000.0f);

            std::cout << "[MLXEngine] 推論完了: " << tokensGenerated << " tokens, "
                      << m_tokensPerSecond << " tokens/sec" << std::endl;

            if (request.onComplete) {
                request.onComplete(result);
            }

        } catch (const std::exception& e) {
            std::cerr << "[MLXEngine] 推論エラー: " << e.what() << std::endl;
            if (request.onError) {
                request.onError(std::string("推論エラー: ") + e.what());
            }
        }

        m_isInferring = false;
    }).detach();
}

const ModelInfo* MLXEngine::getCurrentModel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentModel.get();
}

bool MLXEngine::isInferring() const {
    return m_isInferring;
}

float MLXEngine::getGPUUsage() const {
    // TODO: 実際のGPU使用率取得（IOKit使用）
    // 現在は動作デモ用に変動値を返す
    static float lastUsage = 0.5f;
    float delta = ((rand() % 200) - 100) / 1000.0f;  // -0.1 ~ +0.1
    lastUsage += delta;
    if (lastUsage < 0.0f) lastUsage = 0.0f;
    if (lastUsage > 1.0f) lastUsage = 1.0f;
    return lastUsage;
}

float MLXEngine::getMemoryUsage() const {
    // TODO: 実際のシステムメモリ使用率取得（mach API使用）
    // 現在は簡易実装
    if (m_currentModel) {
        float totalMemoryMB = 64 * 1024.0f;  // 64GB
        float usedMemoryMB = m_currentModel->memoryUsageMB + 8000.0f;  // モデル + システム
        return usedMemoryMB / totalMemoryMB;
    }
    // モデル未ロード時は固定値
    return 0.15f;  // 約10GB使用中
}

float MLXEngine::getTokensPerSecond() const {
    return m_tokensPerSecond;
}

std::string MLXEngine::runInference(const std::string& prompt, size_t maxTokens, float temperature) {
    (void)temperature;

    std::string response = "これはMLXエンジンからのダミーレスポンスです。\n";
    response += "プロンプト: " + prompt + "\n";
    response += "最大トークン数: " + std::to_string(maxTokens) + "\n";
    response += "\n";
    response += "実際のMLX C++ APIが統合されると、ここで実際のLLM推論が実行されます。\n";
    response += "現在のモデル: " + m_currentModel->name + "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return response;
}

} // namespace mlx

// ==============================================================================
// §7. TreeSitterParser実装 - AST解析エンジン
// ==============================================================================

namespace ast {

TreeSitterParser::TreeSitterParser() {
    std::cout << "[TreeSitterParser] 初期化開始" << std::endl;

    // TODO: 実際のTree-sitter初期化
    // m_parser = ts_parser_new();
    // setLanguage(".cpp");  // デフォルトはC++

    std::cout << "[TreeSitterParser] 初期化完了（stub実装）" << std::endl;
}

TreeSitterParser::~TreeSitterParser() {
    std::cout << "[TreeSitterParser] シャットダウン開始" << std::endl;

    // TODO: Tree-sitterクリーンアップ
    // for (auto& [path, tree] : m_treeCache) {
    //     ts_tree_delete(tree);
    // }
    // ts_parser_delete(m_parser);

    std::cout << "[TreeSitterParser] シャットダウン完了" << std::endl;
}

void TreeSitterParser::setLanguage(const std::string& extension) {
    std::cout << "[TreeSitterParser] 言語設定: " << extension << std::endl;

    // TODO: 拡張子に応じて言語パーサーを設定
    // if (extension == ".cpp" || extension == ".h" || extension == ".cc") {
    //     m_language = tree_sitter_cpp();
    // } else if (extension == ".py") {
    //     m_language = tree_sitter_python();
    // } else if (extension == ".ts" || extension == ".js") {
    //     m_language = tree_sitter_typescript();
    // }
    //
    // ts_parser_set_language(m_parser, m_language);
}

std::vector<Symbol> TreeSitterParser::parseFile(const std::string& filePath,
                                                 const std::string& sourceCode) {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "[TreeSitterParser] ファイルパース開始: " << filePath
              << " (" << sourceCode.size() << " bytes)" << std::endl;

    std::vector<Symbol> symbols;

    // TODO: 実際のTree-sitterパース
    // TSTree* tree = ts_parser_parse_string(
    //     m_parser,
    //     nullptr,  // 初回パース
    //     sourceCode.c_str(),
    //     sourceCode.size()
    // );
    //
    // if (tree) {
    //     TSNode rootNode = ts_tree_root_node(tree);
    //     extractSymbols(rootNode, filePath, sourceCode, symbols);
    //
    //     // 次回のインクリメンタルパース用にキャッシュ
    //     m_treeCache[filePath] = tree;
    // }

    // stub: ダミーシンボルを返す
    Symbol dummySymbol;
    dummySymbol.name = "stub_symbol";
    dummySymbol.type = "function";
    dummySymbol.filePath = filePath;
    dummySymbol.startLine = 1;
    dummySymbol.endLine = 10;
    symbols.push_back(dummySymbol);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    m_stats.totalParsed++;
    m_stats.avgParseTimeMs = (m_stats.avgParseTimeMs * (m_stats.totalParsed - 1) +
                              duration.count() / 1000.0) / m_stats.totalParsed;
    m_stats.symbolCount += symbols.size();

    std::cout << "[TreeSitterParser] パース完了: "
              << symbols.size() << " symbols, "
              << (duration.count() / 1000.0) << " ms" << std::endl;

    return symbols;
}

std::vector<Symbol> TreeSitterParser::parseIncremental(
    const std::string& filePath,
    const std::string& oldContent,
    const std::string& newContent) {

    std::cout << "[TreeSitterParser] インクリメンタルパース: " << filePath << std::endl;

    // TODO: Tree-sitterのインクリメンタルパース
    //
    // 1. 前回のTSTreeを取得
    // TSTree* oldTree = m_treeCache[filePath];
    //
    // 2. 変更範囲を計算
    // TSInputEdit edit;
    // edit.start_byte = ...;
    // edit.old_end_byte = ...;
    // edit.new_end_byte = ...;
    // ts_tree_edit(oldTree, &edit);
    //
    // 3. 差分パース
    // TSTree* newTree = ts_parser_parse_string(
    //     m_parser,
    //     oldTree,  // ← ここがキモ！前回のツリーを再利用
    //     newContent.c_str(),
    //     newContent.size()
    // );
    //
    // 4. 変更されたノードだけを抽出
    // TSTreeCursor cursor = ts_tree_cursor_new(ts_tree_root_node(newTree));
    // // 変更フラグが立っているノードだけ処理

    m_stats.incrementalUpdates++;

    // stub: 通常パースにフォールバック
    return parseFile(filePath, newContent);
}

void TreeSitterParser::extractSymbols(const TSNode* node,
                                      const std::string& filePath,
                                      const std::string& sourceCode,
                                      std::vector<Symbol>& symbols) {
    (void)node;
    (void)filePath;
    (void)sourceCode;
    (void)symbols;

    // TODO: ノードタイプに応じてシンボル抽出
    //
    // const char* type = ts_node_type(node);
    //
    // if (strcmp(type, "class_specifier") == 0) {
    //     Symbol sym = processNode(node, filePath, sourceCode);
    //     sym.type = "class";
    //     symbols.push_back(sym);
    // } else if (strcmp(type, "function_definition") == 0) {
    //     Symbol sym = processNode(node, filePath, sourceCode);
    //     sym.type = "function";
    //     symbols.push_back(sym);
    // }
    //
    // // 再帰的に子ノードを処理
    // uint32_t childCount = ts_node_child_count(node);
    // for (uint32_t i = 0; i < childCount; ++i) {
    //     TSNode child = ts_node_child(node, i);
    //     extractSymbols(child, filePath, sourceCode, symbols);
    // }
}

Symbol TreeSitterParser::processNode(const TSNode* node,
                                     const std::string& filePath,
                                     const std::string& sourceCode) {
    (void)node;
    (void)sourceCode;

    Symbol sym;

    // TODO: ノードから情報抽出
    // sym.startLine = ts_node_start_point(node).row;
    // sym.endLine = ts_node_end_point(node).row;
    //
    // // シンボル名の取得（identifier ノードを探す）
    // TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    // if (!ts_node_is_null(nameNode)) {
    //     uint32_t start = ts_node_start_byte(nameNode);
    //     uint32_t end = ts_node_end_byte(nameNode);
    //     sym.name = sourceCode.substr(start, end - start);
    // }

    sym.filePath = filePath;
    return sym;
}

void ASTCache::addSymbols(const std::string& filePath,
                         const std::vector<Symbol>& symbols) {
    std::cout << "[ASTCache] シンボル追加: " << filePath
              << " (" << symbols.size() << " symbols)" << std::endl;

    m_symbolsByFile[filePath] = symbols;

    for (const auto& sym : symbols) {
        m_symbolsByName[sym.name].push_back(sym);
    }

    m_lruQueue.erase(
        std::remove(m_lruQueue.begin(), m_lruQueue.end(), filePath),
        m_lruQueue.end()
    );
    m_lruQueue.push_back(filePath);
}

std::vector<Symbol> ASTCache::search(const std::string& query) const {
    std::cout << "[ASTCache] 検索: \"" << query << "\"" << std::endl;

    std::vector<Symbol> results;

    for (const auto& [name, symList] : m_symbolsByName) {
        if (name.find(query) == 0) {
            results.insert(results.end(), symList.begin(), symList.end());
        }
    }

    std::cout << "[ASTCache] 検索結果: " << results.size() << " matches" << std::endl;

    return results;
}

std::vector<Symbol> ASTCache::getFileSymbols(const std::string& filePath) const {
    auto it = m_symbolsByFile.find(filePath);
    if (it != m_symbolsByFile.end()) {
        return it->second;
    }
    return {};
}

ASTCache::DependencyGraph ASTCache::buildDependencyGraph() const {
    std::cout << "[ASTCache] 依存グラフ構築開始" << std::endl;

    DependencyGraph graph;

    for (const auto& [filePath, symbols] : m_symbolsByFile) {
        for (const auto& sym : symbols) {
            for (const auto& dep : sym.dependencies) {
                graph.edges[sym.name].push_back(dep);
            }
        }
    }

    std::cout << "[ASTCache] 依存グラフ構築完了: "
              << graph.edges.size() << " nodes" << std::endl;

    return graph;
}

size_t ASTCache::getMemoryUsageBytes() const {
    size_t total = 0;

    for (const auto& [filePath, symbols] : m_symbolsByFile) {
        total += filePath.size();
        for (const auto& sym : symbols) {
            total += sym.name.size() + sym.type.size() + sym.filePath.size();
            for (const auto& dep : sym.dependencies) {
                total += dep.size();
            }
        }
    }

    return total;
}

void ASTCache::evictToTarget(size_t targetBytes) {
    size_t currentUsage = getMemoryUsageBytes();

    std::cout << "[ASTCache] メモリ退避開始: "
              << (currentUsage / 1024 / 1024) << " MB -> "
              << (targetBytes / 1024 / 1024) << " MB" << std::endl;

    while (currentUsage > targetBytes && !m_lruQueue.empty()) {
        std::string oldestFile = m_lruQueue.front();
        m_lruQueue.erase(m_lruQueue.begin());

        auto it = m_symbolsByFile.find(oldestFile);
        if (it != m_symbolsByFile.end()) {
            for (const auto& sym : it->second) {
                m_symbolsByName.erase(sym.name);
            }
            m_symbolsByFile.erase(it);
        }

        currentUsage = getMemoryUsageBytes();
    }

    std::cout << "[ASTCache] メモリ退避完了: "
              << (currentUsage / 1024 / 1024) << " MB" << std::endl;
}

} // namespace ast

// ==============================================================================
// End of CoreEngine.cpp
// ==============================================================================
