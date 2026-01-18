// ==============================================================================
// Tree-sitter AST パーサー実装
// ==============================================================================

#include "TreeSitterParser.h"
#include <iostream>
#include <chrono>

// TODO: Tree-sitter本体のリンク
// #include <tree_sitter/api.h>
//
// 外部言語パーサー（実装時に追加）
// extern "C" {
//     const TSLanguage *tree_sitter_cpp();
//     const TSLanguage *tree_sitter_python();
//     const TSLanguage *tree_sitter_typescript();
// }

namespace ast {

// ==============================================================================
// TreeSitterParser 実装
// ==============================================================================

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

void TreeSitterParser::extractSymbols(TSNode node,
                                      const std::string& filePath,
                                      const std::string& sourceCode,
                                      std::vector<Symbol>& symbols) {
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

Symbol TreeSitterParser::processNode(TSNode node,
                                     const std::string& filePath,
                                     const std::string& sourceCode) {
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

// ==============================================================================
// ASTCache 実装
// ==============================================================================

void ASTCache::addSymbols(const std::string& filePath,
                         const std::vector<Symbol>& symbols) {
    std::cout << "[ASTCache] シンボル追加: " << filePath
              << " (" << symbols.size() << " symbols)" << std::endl;

    m_symbolsByFile[filePath] = symbols;

    // 名前インデックスも更新
    for (const auto& sym : symbols) {
        m_symbolsByName[sym.name].push_back(sym);
    }

    // LRUキューを更新
    m_lruQueue.erase(
        std::remove(m_lruQueue.begin(), m_lruQueue.end(), filePath),
        m_lruQueue.end()
    );
    m_lruQueue.push_back(filePath);
}

std::vector<Symbol> ASTCache::search(const std::string& query) const {
    std::cout << "[ASTCache] 検索: \"" << query << "\"" << std::endl;

    std::vector<Symbol> results;

    // TODO: Trie木によるプレフィックス検索
    // 現在はシンプルな前方一致
    for (const auto& [name, symList] : m_symbolsByName) {
        if (name.find(query) == 0) {  // 前方一致
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

    // LRUキューの先頭（最も古い）から削除
    while (currentUsage > targetBytes && !m_lruQueue.empty()) {
        std::string oldestFile = m_lruQueue.front();
        m_lruQueue.erase(m_lruQueue.begin());

        // シンボル削除
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
