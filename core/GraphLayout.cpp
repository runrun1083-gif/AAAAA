#include "GraphLayout.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

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

    // CP2 必須成功要件: 配置の安定性
    // 既存ノードがある場合は、その近くに配置
    if (m_nodes.empty()) {
        node.position = initialPos;
    } else {
        // ランダムな小さなオフセットで配置（大きく変わらないように）
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

    // CP2 必須成功要件: エッジ交差最小化
    int crossings = getEdgeCrossings();
    std::cout << "[GraphLayout] レイアウト完了:" << std::endl;
    std::cout << "  イテレーション: " << iteration << std::endl;
    std::cout << "  エッジ交差数: " << crossings << std::endl;

    return iteration;
}

void ForceDirectedLayout::step(const Parameters& params) {
    // すべてのノードの力をリセット
    for (auto& pair : m_nodes) {
        pair.second.force = Vector2(0, 0);
    }

    // 斥力の計算（すべてのノードペア）
    for (auto& pair1 : m_nodes) {
        for (auto& pair2 : m_nodes) {
            if (pair1.first == pair2.first) continue;

            Vector2 repulsion = computeRepulsionForce(pair1.second, pair2.second, params);
            pair1.second.force = pair1.second.force + repulsion;
        }
    }

    // 引力の計算（エッジで接続されたノードペア）
    for (const auto& edge : m_edges) {
        auto& node1 = m_nodes[edge.from];
        auto& node2 = m_nodes[edge.to];

        Vector2 attraction = computeAttractionForce(node1, node2, params);

        node1.force = node1.force + attraction;
        node2.force = node2.force + (attraction * -1.0f);
    }

    // 位置の更新
    for (auto& pair : m_nodes) {
        if (pair.second.fixed) continue;

        // 速度の更新
        pair.second.velocity = pair.second.velocity + (pair.second.force * params.timeStep);

        // 減衰
        pair.second.velocity = pair.second.velocity * params.damping;

        // 位置の更新
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

    // Hookeの法則: F = k * (d - d0)
    float displacement = distance - params.springLength;
    float force = params.attractionStrength * displacement;

    // CP2 必須成功要件: 自動クラスタリング
    // 同じクラスタのノードには強い引力
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
        // 同じ位置にある場合は小さなランダムな力
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return Vector2(dist(gen), dist(gen)) * 10.0f;
    }

    // Coulombの法則: F = k / d^2
    float force = params.repulsionStrength / (distance * distance);

    // CP2 必須成功要件: 自動クラスタリング
    // 異なるクラスタのノードには弱い斥力
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
    // CP2 必須成功要件: エッジ交差最小化

    int crossings = 0;

    for (size_t i = 0; i < m_edges.size(); ++i) {
        for (size_t j = i + 1; j < m_edges.size(); ++j) {
            const auto& edge1 = m_edges[i];
            const auto& edge2 = m_edges[j];

            // 共有ノードがある場合はスキップ
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
    // 線分交差判定（CCWアルゴリズム）
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

    // クラスタごとにノードを集計
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

    // 中心を計算
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
