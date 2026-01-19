// imgui_amalgamated.mm
// Amalgamated from: core/Application.h, core/Application.cpp,
//                   core/CoreEngine.h, core/CoreEngine.cpp,
//                   core/main.cpp, core/main.mm
// Ref: 4648e847292311c2ca5b18e593b69ac2c1160bf1
//
// Generated according to SANDBOX-TEMPLATE and 6-Layer Architecture.
// NOTE: Manual verification & compilation required (especially Objective-C/Metal paths).
//
// -----------------------------------------------------------------------------
// 1) @SDK_INCLUDES
// -----------------------------------------------------------------------------
// Apple frameworks (for macOS builds)
#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#endif

// Cross-platform / C++ standard headers
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <filesystem>

// Third-party headers used by project
#include <entt/entt.hpp>
#include <imgui.h>
#include <imnodes.h>
#include <backends/imgui_impl_glfw.h>
#ifdef __APPLE__
#include <backends/imgui_impl_metal.h>
#else
#include <backends/imgui_impl_opengl3.h>
#endif

#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// 2) @AMALGAMATED_MACROS
// -----------------------------------------------------------------------------
#define IMGUI_ENABLE_METAL 1

// Helper macro guard for inserted sandbox blocks
#ifndef AMALGAMATED_SANDBOX_GUARD
#define AMALGAMATED_SANDBOX_GUARD 1
#endif

// -----------------------------------------------------------------------------
// 3) @FORWARD_DECLARATIONS
// -----------------------------------------------------------------------------
// All forward declarations (struct/class/@class/@protocol)
namespace resource { struct MemoryBudget; struct MemoryUsage; class ResourceManager; ResourceManager* GetResourceManager(); }
namespace layout { struct Vector2; struct GraphNode; struct GraphEdge; class ForceDirectedLayout; }
namespace filesystem { struct FileInfo; class FileScanner; class DependencyGraph; }
namespace node { enum class NodeType; struct PositionComponent; struct NameComponent; struct TypeComponent; struct DataComponent; struct ConnectionComponent; struct ExecutionStateComponent; class NodeSystem; const char* nodeTypeToString(NodeType); }
namespace mlx { struct ModelInfo; struct InferenceRequest; class MLXEngine; }
namespace ast { struct Symbol; class TreeSitterParser; class ASTCache; }

// Objective-C forward declarations (if any Objective-C classes are used)
#ifdef __OBJC__
@class NSString;
#endif

// -----------------------------------------------------------------------------
// 4) @TYPE_DEFINITIONS
// -----------------------------------------------------------------------------
// Types collected and ordered (from CoreEngine.h and Application.h)
// (Minor edits applied to keep order and remove circularity)

// -------------------- resource --------------------
namespace resource {

struct MemoryBudget {
    size_t astCacheLimit = 12ULL * 1024 * 1024 * 1024;
    size_t mlxModelLimit = 28ULL * 1024 * 1024 * 1024;
    size_t guiBufferLimit = 4ULL * 1024 * 1024 * 1024;
    size_t parserBufferLimit = 4ULL * 1024 * 1024 * 1024;
    float dangerThreshold = 0.85f;
    float warningThreshold = 0.75f;
    bool expertModeEnabled = false;
    size_t expertMaxLimit = 60ULL * 1024 * 1024 * 1024;
};

struct MemoryUsage {
    size_t astCacheUsed = 0;
    size_t mlxModelUsed = 0;
    size_t guiBufferUsed = 0;
    size_t parserBufferUsed = 0;

    size_t getTotalUsed() const {
        return astCacheUsed + mlxModelUsed + guiBufferUsed + parserBufferUsed;
    }
};

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

// Global accessor (prototype)
ResourceManager* GetResourceManager();

} // namespace resource

// -------------------- layout --------------------
namespace layout {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
    Vector2() = default;
    Vector2(float x_, float y_) : x(x_), y(y_) {}
    Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    Vector2 operator*(float scalar) const { return Vector2(x * scalar, y * scalar); }
    float length() const { return std::sqrt(x * x + y * y); }
    Vector2 normalized() const { float len = length(); if (len > 0.0001f) return Vector2(x / len, y / len); return Vector2(0,0); }
};

struct GraphNode {
    std::string id;
    Vector2 position;
    Vector2 velocity;
    Vector2 force;
    float mass = 1.0f;
    bool fixed = false;
    std::string cluster;
};

struct GraphEdge {
    std::string from;
    std::string to;
    float weight = 1.0f;
};

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

    void addNode(const std::string& id, const Vector2& initialPos = Vector2(0,0), const std::string& cluster = "");
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
    Vector2 computeAttractionForce(const GraphNode& node1, const GraphNode& node2, const Parameters& params) const;
    Vector2 computeRepulsionForce(const GraphNode& node1, const GraphNode& node2, const Parameters& params) const;
    bool hasConverged(float threshold) const;
    bool edgesIntersect(const Vector2& a1, const Vector2& a2, const Vector2& b1, const Vector2& b2) const;

    std::unordered_map<std::string, GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
};

} // namespace layout

// -------------------- filesystem --------------------
namespace filesystem {

struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    size_t size;
    std::time_t lastModified;
    std::vector<std::string> dependencies;
    std::vector<std::string> symbols;
};

class FileScanner {
public:
    FileScanner();
    ~FileScanner();

    size_t scanDirectory(const std::string& rootPath);

    const std::vector<FileInfo>& getFiles() const { return m_files; }
    size_t getFileCount() const { return m_files.size(); }
    int64_t getScanTimeNanos() const { return m_scanTimeNanos; }
    double getScanTimeMs() const { return m_scanTimeNanos / 1000000.0; }

    struct InterningStats { size_t totalStrings; size_t uniqueStrings; size_t memorySavedBytes; };
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

class DependencyGraph {
public:
    void buildFromFiles(const std::vector<FileInfo>& files);
    std::vector<std::string> getDependencies(const std::string& filePath) const;
    std::vector<std::string> getDependents(const std::string& filePath) const;
    std::vector<std::vector<std::string>> detectCycles() const;
    std::string toDot() const;
private:
    void detectCyclesRecursive(const std::string& node,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& recursionStack,
        std::vector<std::string>& currentPath,
        std::vector<std::vector<std::string>>& cycles) const;

    std::unordered_map<std::string, std::vector<std::string>> m_dependencies;
    std::unordered_map<std::string, std::vector<std::string>> m_dependents;
};

} // namespace filesystem

// -------------------- node --------------------
namespace node {

enum class NodeType { LLM, Prompt, Memory, Output, Input, Function };

struct PositionComponent { float x = 0.0f; float y = 0.0f; };
struct NameComponent { std::string name; };
struct TypeComponent { NodeType type; };
struct DataComponent { std::string data; };
struct ConnectionComponent { std::vector<entt::entity> inputs; std::vector<entt::entity> outputs; };
struct ExecutionStateComponent {
    enum class State { Idle, Running, Completed, Error };
    State state = State::Idle;
    std::string message;
};

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

// -------------------- mlx --------------------
namespace mlx {

struct ModelInfo { std::string name; std::string path; size_t parameterCount; size_t memoryUsageMB; bool isLoaded; };

struct InferenceRequest {
    std::string prompt;
    size_t maxTokens = 512;
    float temperature = 0.7f;
    std::function<void(const std::string&)> onToken;
    std::function<void(const std::string&)> onComplete;
    std::function<void(const std::string&)> onError;
};

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
};

} // namespace mlx

// -------------------- ast --------------------
namespace ast {

struct Symbol { std::string name; std::string type; std::string filePath; uint32_t startLine; uint32_t endLine; std::vector<std::string> dependencies; };

class TreeSitterParser {
public:
    TreeSitterParser();
    ~TreeSitterParser();
    std::vector<Symbol> parseFile(const std::string& filePath, const std::string& sourceCode);
    std::vector<Symbol> parseIncremental(const std::string& filePath, const std::string& oldContent, const std::string& newContent);
    void setLanguage(const std::string& extension);
    struct ParseStats { size_t totalParsed = 0; size_t incrementalUpdates = 0; double avgParseTimeMs = 0.0; size_t symbolCount = 0; };
    ParseStats getStats() const { return m_stats; }
private:
    typedef struct TSParser TSParser;
    typedef struct TSTree TSTree;
    typedef struct TSNode TSNode;
    typedef struct TSLanguage TSLanguage;
    TSParser* m_parser = nullptr;
    const TSLanguage* m_language = nullptr;
    std::unordered_map<std::string, TSTree*> m_treeCache;
    ParseStats m_stats;
    void extractSymbols(const TSNode* node, const std::string& filePath, const std::string& sourceCode, std::vector<Symbol>& symbols);
    Symbol processNode(const TSNode* node, const std::string& filePath, const std::string& sourceCode);
};

class ASTCache {
public:
    void addSymbols(const std::string& filePath, const std::vector<Symbol>& symbols);
    std::vector<Symbol> search(const std::string& query) const;
    std::vector<Symbol> getFileSymbols(const std::string& filePath) const;
    struct DependencyGraph { std::unordered_map<std::string, std::vector<std::string>> edges; };
    DependencyGraph buildDependencyGraph() const;
    size_t getMemoryUsageBytes() const;
    void evictToTarget(size_t targetBytes);
private:
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByFile;
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByName;
    std::vector<std::string> m_lruQueue;
};

} // namespace ast

// -------------------- Application (class forward) --------------------
namesp