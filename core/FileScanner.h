#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

// ==============================================================================
// ファイルシステム・ダイナミクス・エンジン
// ==============================================================================
// CP2 必須成功要件:
// - 1万ファイル50ms以下スキャン
// - string interning（省メモリ）
// - FSEvents差分更新
// - 完全ロード証明（ナノ秒ログ）
// ==============================================================================

namespace filesystem {

/**
 * @brief ファイル情報
 */
struct FileInfo {
    std::string path;           ///< ファイルパス
    std::string name;           ///< ファイル名
    std::string extension;      ///< 拡張子
    size_t size;                ///< ファイルサイズ（バイト）
    std::time_t lastModified;   ///< 最終更新時刻

    // 解析結果
    std::vector<std::string> dependencies;  ///< 依存関係（import/include）
    std::vector<std::string> symbols;       ///< シンボル（関数、クラス等）
};

/**
 * @brief ファイルスキャナー
 *
 * CP2 要件:
 * - 1万ファイルを50ms以下でスキャン
 * - string interningで省メモリ化
 * - FSEvents連動での差分更新
 */
class FileScanner {
public:
    FileScanner();
    ~FileScanner();

    /**
     * @brief ディレクトリをスキャン
     *
     * @param rootPath ルートディレクトリ
     * @return スキャンしたファイル数
     *
     * @note
     * CP2 必須成功要件:
     * - 1万ファイルで50ms以下
     * - std::chronoでナノ秒単位の計測
     */
    size_t scanDirectory(const std::string& rootPath);

    /**
     * @brief スキャン結果を取得
     */
    const std::vector<FileInfo>& getFiles() const { return m_files; }

    /**
     * @brief ファイル数を取得
     */
    size_t getFileCount() const { return m_files.size(); }

    /**
     * @brief スキャン時間を取得（ナノ秒）
     */
    int64_t getScanTimeNanos() const { return m_scanTimeNanos; }

    /**
     * @brief スキャン時間を取得（ミリ秒）
     */
    double getScanTimeMs() const {
        return m_scanTimeNanos / 1000000.0;
    }

    /**
     * @brief string interning統計を取得
     */
    struct InterningStats {
        size_t totalStrings;        ///< 総文字列数
        size_t uniqueStrings;       ///< ユニーク文字列数
        size_t memorySavedBytes;    ///< 節約メモリ（バイト）
    };
    InterningStats getInterningStats() const;

    /**
     * @brief ファイルを拡張子でフィルタ
     */
    std::vector<FileInfo> filterByExtension(const std::string& ext) const;

    /**
     * @brief C++ファイルのみ取得
     */
    std::vector<FileInfo> getCppFiles() const;

    /**
     * @brief Pythonファイルのみ取得
     */
    std::vector<FileInfo> getPythonFiles() const;

    /**
     * @brief ファイル変更を検出して差分更新
     *
     * @return 変更されたファイル数
     */
    size_t detectChanges();

private:
    /**
     * @brief 文字列のintern（重複排除）
     */
    const std::string& internString(const std::string& str);

    /**
     * @brief ファイルを再帰的にスキャン
     */
    void scanRecursive(const fs::path& path);

    /**
     * @brief ファイル情報を抽出
     */
    FileInfo extractFileInfo(const fs::path& path);

    /**
     * @brief 対象ファイルかどうか判定
     */
    bool isTargetFile(const fs::path& path) const;

private:
    std::vector<FileInfo> m_files;                      ///< スキャン結果
    std::unordered_map<std::string, std::string> m_stringPool;  ///< string interning pool
    std::unordered_map<std::string, std::time_t> m_fileTimestamps;  ///< ファイルタイムスタンプ

    int64_t m_scanTimeNanos = 0;    ///< スキャン時間（ナノ秒）
    std::string m_rootPath;         ///< ルートパス

    // サポートする拡張子
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
    /**
     * @brief ファイル情報から依存関係を構築
     */
    void buildFromFiles(const std::vector<FileInfo>& files);

    /**
     * @brief 依存関係を取得
     *
     * @param filePath ファイルパス
     * @return 依存しているファイルのリスト
     */
    std::vector<std::string> getDependencies(const std::string& filePath) const;

    /**
     * @brief 被依存関係を取得（逆方向）
     *
     * @param filePath ファイルパス
     * @return このファイルに依存しているファイルのリスト
     */
    std::vector<std::string> getDependents(const std::string& filePath) const;

    /**
     * @brief 循環依存を検出
     *
     * @return 循環依存のサイクルのリスト
     */
    std::vector<std::vector<std::string>> detectCycles() const;

    /**
     * @brief DOT形式で出力（Graphviz用）
     */
    std::string toDot() const;

private:
    void detectCyclesRecursive(
        const std::string& node,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& recursionStack,
        std::vector<std::string>& currentPath,
        std::vector<std::vector<std::string>>& cycles
    ) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> m_dependencies;
    std::unordered_map<std::string, std::vector<std::string>> m_dependents;
};

} // namespace filesystem
