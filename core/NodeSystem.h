#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <functional>

// ==============================================================================
// EnTTベースのノード管理システム
// ==============================================================================
// FlowiseのようなノードグラフをECSで実装
// ==============================================================================

namespace node {

/**
 * @brief ノードの種類
 */
enum class NodeType {
    LLM,            ///< LLMモデルノード
    Prompt,         ///< プロンプトテンプレートノード
    Memory,         ///< メモリ/履歴管理ノード
    Output,         ///< 出力ノード
    Input,          ///< 入力ノード
    Function,       ///< カスタム関数ノード
};

/**
 * @brief ノードの位置情報（Component）
 */
struct PositionComponent {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief ノードの表示名（Component）
 */
struct NameComponent {
    std::string name;
};

/**
 * @brief ノードのタイプ（Component）
 */
struct TypeComponent {
    NodeType type;
};

/**
 * @brief ノードのデータ（Component）
 */
struct DataComponent {
    std::string data;  ///< JSON形式のデータ
};

/**
 * @brief ノードの接続情報（Component）
 */
struct ConnectionComponent {
    std::vector<entt::entity> inputs;   ///< 入力元のノード
    std::vector<entt::entity> outputs;  ///< 出力先のノード
};

/**
 * @brief ノード実行状態（Component）
 */
struct ExecutionStateComponent {
    enum class State {
        Idle,       ///< アイドル
        Running,    ///< 実行中
        Completed,  ///< 完了
        Error       ///< エラー
    };

    State state = State::Idle;
    std::string message;  ///< 状態メッセージ
};

/**
 * @brief ノード管理システム
 *
 * EnTTレジストリを使ってノードを管理
 */
class NodeSystem {
public:
    NodeSystem();
    ~NodeSystem();

    /**
     * @brief ノードを作成
     *
     * @param type ノードの種類
     * @param name ノードの名前
     * @param x X座標
     * @param y Y座標
     * @return 作成されたノードのEntity ID
     */
    entt::entity createNode(NodeType type, const std::string& name, float x, float y);

    /**
     * @brief ノードを削除
     *
     * @param entity 削除するノードのEntity ID
     */
    void destroyNode(entt::entity entity);

    /**
     * @brief ノード間を接続
     *
     * @param from 出力元ノード
     * @param to 入力先ノード
     */
    void connectNodes(entt::entity from, entt::entity to);

    /**
     * @brief ノード間の接続を解除
     *
     * @param from 出力元ノード
     * @param to 入力先ノード
     */
    void disconnectNodes(entt::entity from, entt::entity to);

    /**
     * @brief すべてのノードを取得
     */
    std::vector<entt::entity> getAllNodes() const;

    /**
     * @brief ノードグラフを実行
     *
     * @param startNode 開始ノード
     * @param onComplete 完了時のコールバック
     */
    void executeGraph(entt::entity startNode, std::function<void(const std::string&)> onComplete);

    /**
     * @brief EnTTレジストリへのアクセス
     */
    entt::registry& getRegistry() { return m_registry; }
    const entt::registry& getRegistry() const { return m_registry; }

    /**
     * @brief ノードの座標を更新
     */
    void updateNodePosition(entt::entity entity, float x, float y);

    /**
     * @brief ノードのデータを更新
     */
    void updateNodeData(entt::entity entity, const std::string& data);

    /**
     * @brief ノードの実行状態を更新
     */
    void updateNodeState(entt::entity entity, ExecutionStateComponent::State state, const std::string& message = "");

private:
    /**
     * @brief ノードグラフを再帰的に実行
     */
    void executeNodeRecursive(entt::entity entity, std::string& result);

private:
    entt::registry m_registry;  ///< EnTT レジストリ
    size_t m_nodeCount = 0;     ///< ノード生成カウンタ
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
