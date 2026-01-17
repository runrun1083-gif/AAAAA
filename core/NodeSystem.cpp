#include "NodeSystem.h"
#include <iostream>
#include <algorithm>

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

    // コンポーネントを追加
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

    // 接続を解除
    auto& conn = m_registry.get<ConnectionComponent>(entity);

    // このノードへの入力をすべて解除
    for (auto inputNode : conn.inputs) {
        if (m_registry.valid(inputNode)) {
            auto& inputConn = m_registry.get<ConnectionComponent>(inputNode);
            inputConn.outputs.erase(
                std::remove(inputConn.outputs.begin(), inputConn.outputs.end(), entity),
                inputConn.outputs.end()
            );
        }
    }

    // このノードからの出力をすべて解除
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

    // 既に接続されているかチェック
    if (std::find(fromConn.outputs.begin(), fromConn.outputs.end(), to) != fromConn.outputs.end()) {
        std::cerr << "[NodeSystem] 警告: 既に接続されています" << std::endl;
        return;
    }

    // 接続を追加
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

    // 接続を削除
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

// ==============================================================================
// Private メソッド
// ==============================================================================

void NodeSystem::executeNodeRecursive(entt::entity entity, std::string& result) {
    if (!m_registry.valid(entity)) {
        return;
    }

    const auto& name = m_registry.get<NameComponent>(entity);
    const auto& type = m_registry.get<TypeComponent>(entity);
    const auto& data = m_registry.get<DataComponent>(entity);
    const auto& conn = m_registry.get<ConnectionComponent>(entity);

    // 実行状態を更新
    updateNodeState(entity, ExecutionStateComponent::State::Running, "実行中");

    std::cout << "[NodeSystem] ノード実行: " << name.name << " (タイプ: " << nodeTypeToString(type.type) << ")" << std::endl;

    // ノードタイプに応じた処理
    switch (type.type) {
        case NodeType::Input:
            result += "[入力] " + data.data + "\n";
            break;

        case NodeType::Prompt:
            result += "[プロンプト] " + data.data + "\n";
            break;

        case NodeType::LLM:
            // TODO: MLXEngineを呼び出して実際の推論を実行
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

    // 実行状態を更新
    updateNodeState(entity, ExecutionStateComponent::State::Completed, "完了");

    // 接続されている次のノードを実行
    for (auto outputNode : conn.outputs) {
        executeNodeRecursive(outputNode, result);
    }
}

} // namespace node
