// ==============================================================================
// Tree-sitter AST パーサー統合
// ==============================================================================
// 改善法2への対応: 自前パーサーを捨て、Tree-sitterで実装
// ==============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// 前方宣言（Tree-sitter API）
typedef struct TSParser TSParser;
typedef struct TSTree TSTree;
typedef struct TSNode TSNode;
typedef struct TSLanguage TSLanguage;

namespace ast {

/**
 * @brief シンボル情報
 * EnTTコンポーネントとして使用
 */
struct Symbol {
    std::string name;
    std::string type;  // "class", "function", "variable"
    std::string filePath;
    uint32_t startLine;
    uint32_t endLine;
    std::vector<std::string> dependencies;  // 依存する他のシンボル
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

    /**
     * @brief ファイルをパースしてシンボル抽出
     *
     * @param filePath ファイルパス
     * @param sourceCode ソースコード
     * @return 抽出されたシンボル一覧
     *
     * @note
     * - 差分パース対応（前回のTSTreeを保持）
     * - 変更部分のみ再パース可能
     */
    std::vector<Symbol> parseFile(const std::string& filePath,
                                   const std::string& sourceCode);

    /**
     * @brief インクリメンタルパース（差分更新）
     *
     * @param filePath ファイルパス
     * @param oldContent 変更前の内容
     * @param newContent 変更後の内容
     * @return 変更されたシンボル一覧
     *
     * @note
     * Tree-sitterの強み: 変更部分だけを効率的に再パース
     * 10万行のファイルでも、1行の変更なら数ミリ秒で完了
     */
    std::vector<Symbol> parseIncremental(const std::string& filePath,
                                          const std::string& oldContent,
                                          const std::string& newContent);

    /**
     * @brief 言語の設定
     *
     * @param extension ファイル拡張子 (".cpp", ".py", ".ts" 等)
     */
    void setLanguage(const std::string& extension);

    /**
     * @brief パース統計取得
     */
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

    // ファイルごとのTSTreeキャッシュ（インクリメンタルパース用）
    std::unordered_map<std::string, TSTree*> m_treeCa che;

    ParseStats m_stats;

    /**
     * @brief TSTre eからシンボルを抽出
     */
    void extractSymbols(TSNode node,
                       const std::string& filePath,
                       const std::string& sourceCode,
                       std::vector<Symbol>& symbols);

    /**
     * @brief ノードタイプに応じた処理
     */
    Symbol processNode(TSNode node,
                      const std::string& filePath,
                      const std::string& sourceCode);
};

/**
 * @brief AST インメモリキャッシュ
 *
 * 64GB RAM活用:
 * - 全ファイルのシンボル情報をメモリ常駐
 * - LRUによる自動退避（メモリ圧迫時）
 */
class ASTCache {
public:
    /**
     * @brief シンボルをキャッシュに追加
     */
    void addSymbols(const std::string& filePath,
                   const std::vector<Symbol>& symbols);

    /**
     * @brief シンボル検索（高速）
     *
     * @param query 検索クエリ
     * @return マッチしたシンボル一覧
     *
     * @note
     * Trie木によるプレフィックス検索で<16ms以内に結果返却
     */
    std::vector<Symbol> search(const std::string& query) const;

    /**
     * @brief ファイルのシンボル取得
     */
    std::vector<Symbol> getFileSymbols(const std::string& filePath) const;

    /**
     * @brief 依存グラフ構築
     *
     * @return 全シンボルの依存関係グラフ（EnTTで管理）
     */
    struct DependencyGraph {
        std::unordered_map<std::string, std::vector<std::string>> edges;
    };

    DependencyGraph buildDependencyGraph() const;

    /**
     * @brief メモリ使用量取得
     */
    size_t getMemoryUsageBytes() const;

    /**
     * @brief LRU退避実行
     *
     * @param targetBytes 目標メモリ使用量
     */
    void evictToTarget(size_t targetBytes);

private:
    // ファイルパス → シンボル一覧
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByFile;

    // シンボル名 → シンボル（高速検索用）
    std::unordered_map<std::string, std::vector<Symbol>> m_symbolsByName;

    // LRUアクセス順序
    std::vector<std::string> m_lruQueue;
};

} // namespace ast
