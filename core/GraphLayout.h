#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>

// ==============================================================================
// グラフ自動配置アルゴリズム
// ==============================================================================
// CP2 必須成功要件:
// - 配置の安定性（メンタルモデル維持）
// - エッジ交差最小化
// - 自動クラスタリング（フォルダ階層ベース）
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
    std::string id;         ///< ノードID
    Vector2 position;       ///< 現在位置
    Vector2 velocity;       ///< 速度
    Vector2 force;          ///< 累積力
    float mass = 1.0f;      ///< 質量
    bool fixed = false;     ///< 固定フラグ

    std::string cluster;    ///< クラスタID（フォルダパス等）
};

/**
 * @brief グラフエッジ
 */
struct GraphEdge {
    std::string from;       ///< 始点ノードID
    std::string to;         ///< 終点ノードID
    float weight = 1.0f;    ///< エッジの重み
};

/**
 * @brief Force-Directed Layout アルゴリズム
 *
 * CP2 必須成功要件:
 * - 配置の安定性: ノード追加時に既存配置が大きく変わらない
 * - エッジ交差最小化: Fruchterman-Reingoldアルゴリズム
 * - クラスタリング: フォルダ階層で自動グループ化
 */
class ForceDirectedLayout {
public:
    /**
     * @brief レイアウトパラメータ
     */
    struct Parameters {
        float attractionStrength = 0.5f;    ///< 引力の強さ
        float repulsionStrength = 5000.0f;  ///< 斥力の強さ
        float springLength = 200.0f;        ///< バネの自然長
        float damping = 0.8f;               ///< 減衰係数
        float timeStep = 0.1f;              ///< 時間ステップ
        int maxIterations = 100;            ///< 最大イテレーション数
        float convergenceThreshold = 0.1f;  ///< 収束判定閾値

        // クラスタリング
        bool enableClustering = true;       ///< クラスタリング有効化
        float clusterAttractionBonus = 2.0f;  ///< 同クラスタ内の引力ボーナス
        float clusterRepulsionPenalty = 0.5f; ///< 異クラスタ間の斥力ペナルティ
    };

    ForceDirectedLayout();
    ~ForceDirectedLayout();

    /**
     * @brief ノードを追加
     */
    void addNode(const std::string& id, const Vector2& initialPos = Vector2(0, 0),
                 const std::string& cluster = "");

    /**
     * @brief エッジを追加
     */
    void addEdge(const std::string& from, const std::string& to, float weight = 1.0f);

    /**
     * @brief ノードを固定
     */
    void fixNode(const std::string& id, bool fixed = true);

    /**
     * @brief レイアウト計算を実行
     *
     * @param params パラメータ
     * @return 実際のイテレーション数
     *
     * @note
     * CP2 必須成功要件:
     * - 配置の安定性: 増分レイアウトで既存ノードの位置を維持
     */
    int computeLayout(const Parameters& params);

    /**
     * @brief ノードの位置を取得
     */
    Vector2 getNodePosition(const std::string& id) const;

    /**
     * @brief すべてのノード位置を取得
     */
    std::unordered_map<std::string, Vector2> getAllPositions() const;

    /**
     * @brief エッジの交差数を取得
     *
     * @return 交差数
     *
     * @note CP2 必須成功要件: エッジ交差最小化
     */
    int getEdgeCrossings() const;

    /**
     * @brief クラスタの境界を取得
     */
    struct ClusterBounds {
        std::string clusterId;
        Vector2 min;
        Vector2 max;
        Vector2 center;
    };
    std::vector<ClusterBounds> getClusterBounds() const;

    /**
     * @brief レイアウトをリセット
     */
    void reset();

private:
    /**
     * @brief 1ステップ更新
     */
    void step(const Parameters& params);

    /**
     * @brief 引力を計算
     */
    Vector2 computeAttractionForce(const GraphNode& node1, const GraphNode& node2,
                                    const Parameters& params) const;

    /**
     * @brief 斥力を計算
     */
    Vector2 computeRepulsionForce(const GraphNode& node1, const GraphNode& node2,
                                   const Parameters& params) const;

    /**
     * @brief 収束判定
     */
    bool hasConverged(float threshold) const;

    /**
     * @brief エッジ交差判定
     */
    bool edgesIntersect(const Vector2& a1, const Vector2& a2,
                        const Vector2& b1, const Vector2& b2) const;

private:
    std::unordered_map<std::string, GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
};

} // namespace layout
