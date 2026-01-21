// ==============================================================================
// CoreEngine.h - AI開発ステーション統合ヘッダー（AI最適化版）
// ==============================================================================
// ファイル統合方針:
// - 全コアエンジンを1ファイルに集約
// - AI開発効率化: Read1回で全体把握、依存関係明確化
// - セクション区切りで関数検索容易
// ==============================================================================
// [目次] - AI向けナビゲーション
// ==============================================================================
// §1. External Dependencies & Constants     (L30-80)
// §2. ResourceManager - メモリ管理         (L81-280)
// §3. GraphLayout - Force-directed配置     (L281-490)
// §4. FileScanner - ファイルシステム       (L491-710)
// §5. NodeSystem - EnTTノード管理          (L711-900)
// §6. MLXEngine - AI推論エンジン           (L901-1050)
// ==============================================================================

#pragma once

// ==============================================================================
// §1. External Dependencies & Constants
// ==============================================================================

// 標準ライブラリ
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <cmath>

// 外部ライブラリ
#include <entt/entt.hpp>

namespace fs = std::filesystem;

// グローバル定数
constexpr size_t DEFAULT_MEMORY_BUDGET = 48ULL * 1024 * 1024 * 1024;  // 48GB
constexpr float DANGER_THRESHOLD = 0.85f;
constexpr float WARNING_THRESHOLD = 0.75f;

// ==============================================================================
// §2. ResourceManager - Unified Memory管理（VRAMバジェット48GB）
// ==============================================================================

namespace resource {

/**
 * @brief メモリバジェット設定
 *
 * 改善前（危険）: 60GB (93.75%)
 * 改善後（安全）: 48GB (75%) + 動的監視
 */
struct MemoryBudget {
    size_t astCacheLimit = 12ULL * 1024 * 1024 * 1024;       ///< AST 12GB
    size_t mlxModelLimit = 28ULL * 1024 * 1024 * 1024;       ///< MLX 28GB
    size_t guiBufferLimit = 4ULL * 1024 * 1024 * 1024;       ///< GUI 4GB
    size_t parserBufferLimit = 4ULL * 1024 * 1024 * 1024;    ///< Parser 4GB

    float dangerThreshold = 0.85f;   ///< 危険閾値（自動LRU退避）
    float warningThreshold = 0.75f;  ///< 警告閾値（ユーザー通知）

    bool expertModeEnabled = false;  ///< Expert Mode
    size_t expertMaxLimit = 60ULL * 1024 * 1024 * 1024;  ///< 60GB
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
 */
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    void setMemoryBudget(const MemoryBudget& budget);
    MemoryBudget getMemoryBudget() const { return m_budget; }

    bool requestMemory(size_t bytes, const char* purpose);
    void releaseMemory(size_t bytes, const char* purpose);

    MemoryUsage getCurrentUsage() const { return m_usage; }

    size_t getSystemAvailableMemory() const;
    int getMemoryPressureLevel() const;

    size_t evictToTarget(size_t targetBytes);

    void startMonitoring();
    void stopMonitoring();

    using EvictionCallback = std::function<size_t(size_t targetBytes)>;
    void registerEvictionCallback(const std::string& name, EvictionCallback callback);

private:
    MemoryBudget m_budget;
    MemoryUsage m_usage;
    std::atomic<bool> m_monitoringActive{false};
    std::unordered_map<std::string, EvictionCallback> m_evictionCallbacks;
};

/**
 * @brief グローバルResource Managerインスタンス取得
 */
ResourceManager* GetResourceManager();

} // namespace resource

// ==============================================================================
// §3. GraphLayout - Force-directed自動配置
// ==============================================================================

namespace layout {

/**
 * @brief 2D座標
 */
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    Vector2() = default;
    Vector2(float x_, float y_) : x(x_), y(y_) {}

    Vector2 operator+(const Vector2& other) const {
        return Vector2(x + other.x, y + other.y);
    }

    Vector2 operator-(const Vector2& other) const {
        return Vector2(x - other.x, y - other.y);
    }

    Vector2 operator*(float scalar) const {
        return Vector2(x * scalar, y * scalar);
    }

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2 normalized() const {
        float len = length();
        if (len > 0.0001f) {
            return Vector2(x / len, y / len);
        }
        return Vector2(0, 0);
    }
};

/**
 * @brief グラフノード
 */
struct GraphNode {
    std::string id;
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float mass = 1.0f;
    bool fixed = false;
    std::string cluster;
};

/**
 * @brief グラフエッジ
 */
struct GraphEdge {
    std::string from;
    std::string to;
    float weight = 1.0f;
};

/**
 * @brief Force-Directed Layout アルゴリズム
 */
class ForceDirectedLayout {
public:
    struct Parameters {
        float attractionStrength = 0.5f;
        float repulsionStrength = 5000.0f;
        float springLength = 200.0f;
        float damping = 0.8f;
        float timeStep = 0.1f;
        int maxIterations = 100;
        float convergenceThreshold = 0.1f;
        bool enableClustering = true;
        float clusterAttractionBonus = 2.0f;
        float clusterRepulsionPenalty = 0.5f;
    };

    ForceDirectedLayout();
    ~ForceDirectedLayout();

    void addNode(const std::string& id, const Vector2& initialPos = Vector2(0, 0),
                 const std::string& cluster = "");
    void addEdge(const std::string& from, const std::string& to, float weight = 1.0f);
    void fixNode(const std::string& id, bool fixed = true);

    int computeLayout(const Parameters& params);

    Vector2 getNodePosition(const std::string& id) const;
    std::unordered_map<std::string, Vector2> getAllPositions() const;

    int getEdgeCrossings() const;

    struct ClusterBounds {
        std::string clusterId;
        Vector2 min;
        Vector2 max;
        Vector2 center;
    };
    std::vector<ClusterBounds> getClusterBounds() const;

    void reset();

private:
    void step(const Parameters& params);
    Vector2 computeAttractionForce(const GraphNode& node1, const GraphNode& node2,
                                    const Parameters& params) const;
    Vector2 computeRepulsionForce(const GraphNode& node1, const GraphNode& node2,
                                   const Parameters& params) const;
    bool hasConverged(float threshold) const;
    bool edgesIntersect(const Vector2& a1, const Vector2& a2,
                        const Vector2& b1, const Vector2& b2) const;

    std::unordered_map<std::string, GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
};

} // namespace layout

// ==============================================================================
// §4. FileScanner - ファイルシステム・ダイナミクス
// ==============================================================================

namespace filesystem {

/**
 * @brief ファイル情報
 */
struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    size_t size;
    std::time_t lastModified;
    std::vector<std::string> dependencies;
    std::vector<std::string> symbols;
};

/**
 * @brief ファイルスキャナー（CP2: 1万ファイル50ms以下）
 */
class FileScanner {
public:
    FileScanner();
    ~FileScanner();

    size_t scanDirectory(const std::string& rootPath);

    const std::vector<FileInfo>& getFiles() const { return m_files; }
    size_t getFileCount() const { return m_files.size(); }
    int64_t getScanTimeNanos() const { return m_scanTimeNanos; }
    double getScanTimeMs() const { return m_scanTimeNanos / 1000000.0; }

    struct InterningStats {
        size_t totalStrings;
        size_t uniqueStrings;
        size_t memorySavedBytes;
    };
    InterningStats getInterningStats() const;

    std::vector<FileInfo> filterByExtension(const std::string& ext) const;
    std::vector<FileInfo> getCppFiles() const;
    std::vector<FileInfo> getPythonFiles() const;

    size_t detectChanges();

private:
    const std::string& internString(const std::string& str);
    void scanRecursive(const fs::path& path);
    FileInfo extractFileInfo(const fs::path& path);
    bool isTargetFile(const fs::path& path) const;

    std::vector<FileInfo> m_files;
    std::unordered_map<std::string, std::string> m_stringPool;
    std::unordered_map<std::string, std::time_t> m_fileTimestamps;
    int64_t m_scanTimeNanos = 0;
    std::string m_rootPath;
    std::unordered_set<std::string> m_targetExtensions = {
        ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx",
        ".py", ".js", ".ts", ".java", ".go", ".rs"
    };
};

/**
 * @brief 依存関係グラフ
 */
class DependencyGraph {
public:
    void buildFromFiles(const std::vector<FileInfo>& files);
    std::vector<std::string> getDependencies(const std::string& filePath) const;
    std::vector<std::string> getDependents(const std::string& filePath) const;
    std::vector<std::vector<std::string>> detectCycles() const;
    std::string toDot() const;

private:
    void detectCyclesRecursive(
        const std::string& node,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& recursionStack,
        std::vector<std::string>& currentPath,
        std::vector<std::vector<std::string>>& cycles
    ) const;

    std::unordered_map<std::string, std::vector<std::string>> m_dependencies;
    std::unordered_map<std::string, std::vector<std::string>> m_dependents;
};

} // namespace filesystem

// ==============================================================================
// §5. NodeSystem - EnTTベースノード管理
// ==============================================================================

namespace node {

/**
 * @brief ノードの種類
 */
enum class NodeType {
    LLM,
    Prompt,
    Memory,
    Output,
    Input,
    Function,
};

/**
 * @brief ノードコンポーネント群
 */
struct PositionComponent {
    float x = 0.0f;
    float y = 0.0f;
};

struct NameComponent {
    std::string name;
};

struct TypeComponent {
    NodeType type;
};

struct DataComponent {
    std::string data;
};

struct ConnectionComponent {
    std::vector<entt::entity> inputs;
    std::vector<entt::entity> outputs;
};

struct ExecutionStateComponent {
    enum class State {
        Idle,
        Running,
        Completed,
        Error
    };
    State state = State::Idle;
    std::string message;
};

/**
 * @brief ノード管理システム
 */
class NodeSystem {
public:
    NodeSystem();
    ~NodeSystem();

    entt::entity createNode(NodeType type, const std::string& name, float x, float y);
    void destroyNode(entt::entity entity);

    void connectNodes(entt::entity from, entt::entity to);
    void disconnectNodes(entt::entity from, entt::entity to);

    std::vector<entt::entity> getAllNodes() const;

    void executeGraph(entt::entity startNode, std::function<void(const std::string&)> onComplete);

    entt::registry& getRegistry() { return m_registry; }
    const entt::registry& getRegistry() const { return m_registry; }

    void updateNodePosition(entt::entity entity, float x, float y);
    void updateNodeData(entt::entity entity, const std::string& data);
    void updateNodeState(entt::entity entity, ExecutionStateComponent::State state, const std::string& message = "");

private:
    void executeNodeRecursive(entt::entity entity, std::string& result);

    entt::registry m_registry;
    size_t m_nodeCount = 0;
};

/**
 * @brief ノードタイプを文字列に変換
 */
inline const char* nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::LLM: return "LLM";
        case NodeType::Prompt: return "プロンプト";
        case NodeType::Memory: return "メモリ";
        case NodeType::Output: return "出力";
        case NodeType::Input: return "入力";
        case NodeType::Function: return "関数";
        default: return "不明";
    }
}

} // namespace node

// ==============================================================================
// §6. MLXEngine - AI推論エンジン（Apple Silicon最適化）
// ==============================================================================

namespace mlx {

/**
 * @brief MLXモデル情報
 */
struct ModelInfo {
    std::string name;
    std::string path;
    size_t parameterCount;
    size_t memoryUsageMB;
    bool isLoaded;
};

/**
 * @brief 推論リクエスト
 */
struct InferenceRequest {
    std::string prompt;
    size_t maxTokens = 512;
    float temperature = 0.7f;
    std::function<void(const std::string&)> onToken;
    std::function<void(const std::string&)> onComplete;
    std::function<void(const std::string&)> onError;
};

/**
 * @brief MLX推論エンジン
 */
class MLXEngine {
public:
    MLXEngine();
    ~MLXEngine();

    MLXEngine(const MLXEngine&) = delete;
    MLXEngine& operator=(const MLXEngine&) = delete;

    std::vector<ModelInfo> scanModels(const std::string& modelsDir);
    bool loadModel(const std::string& modelPath);
    void unloadModel();

    void inferAsync(const InferenceRequest& request);

    const ModelInfo* getCurrentModel() const;
    bool isInferring() const;
    float getGPUUsage() const;
    float getMemoryUsage() const;
    float getTokensPerSecond() const;

private:
    void inferenceWorkerLoop();
    std::string runInference(const std::string& prompt, size_t maxTokens, float temperature);

    std::unique_ptr<ModelInfo> m_currentModel;
    std::atomic<bool> m_isInferring{false};
    std::atomic<float> m_tokensPerSecond{0.0f};
    mutable std::mutex m_mutex;

    // Python objects for loaded model (must hold GIL when accessing)
    PyObject* m_model = nullptr;
    PyObject* m_tokenizer = nullptr;
};

} // namespace mlx

// ==============================================================================
// §7. TreeSitterParser - AST解析エンジン（Tree-sitter統合）
// ==============================================================================

// 前方宣言（Tree-sitter API）
typedef struct TSParser TSParser;
typedef struct TSTree TSTree;
typedef struct TSNode TSNode;
typedef struct TSLanguage TSLanguage;

namespace ast {

/**
 * @brief シンボル情報
 */
struct Symbol {
    std::string name;
    std::string type;  ///< "class", "function", "variable"
    std::string filePath;
    uint32_t startLine;
    uint32_t endLine;
    std::vector<std::string> dependencies;
};

/**
 * @brief Tree-sitter AST パーサー
 *
 * 改善法への対応:
 * - Clang LibTooling: 正確だが重い ✗
 * - 自前パーサー: 軽いが不正確 ✗
 * - Tree-sitter: 正確 + 軽量 + インクリメンタル ✓
 */
class TreeSitterParser {
public:
    TreeSitterParser();
    ~TreeSitterParser();

    std::vector<Symbol> parseFile(const std::string& filePath,
                                   const std::string& sourceCode);

    std::vector<Symbol> parseIncremental(const std::string& filePath,
                                          const std::string& oldContent,
                                          const std::string& newContent);

    void setLanguage(const std::string& extension);

    struct ParseStats {
        size_t totalParsed = 0;
        size_t incrementalUpdates = 0;
        double avgParseTimeMs = 0.0;
        size_t symbolCount = 0;
    };

    ParseStats getStats() const { return m_stats; }

private:
    TSParser* m_parser = nullptr;
    const TSLanguage* m_language = nullptr;
    std::unordered_map<std::string, TSTree*> m_treeCache;
    ParseStats m_stats;

    void extractSymbols(const TSNode* node,
                       const std::string& filePath,
                       const std::string& sourceCode,
                       std::vector<Symbol>& symbols);

    Symbol processNode(const TSNode* node,
                      const std::string& filePath,
                      const std::string& sourceCode);
};

/**
 * @brief AST インメモリキャッシュ
 *
 * 64GB RAM活用: LRU退避機能付き
 */
class ASTCache {
public:
    void addSymbols(const std::string& filePath,
                   const std::vector<Symbol>& symbols);

    std::vector<Symbol> search(const std::string& query) const;
    std::vector<Symbol> getFileSymbols(const std::string& filePath) const;

    struct DependencyGraph {
        std::unordered_map<std::string, std::vector<std::string>> edges;
    };

    DependencyGraph buildDependencyGraph() const;

    size_t getMemoryUsageBytes() const;
    void evictToTarget(size_t targetBytes);

private:
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByFile;
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByName;
    std::vector<std::string> m_lruQueue;
};

} // namespace ast

// ==============================================================================
// End of CoreEngine.h
// ==============================================================================
