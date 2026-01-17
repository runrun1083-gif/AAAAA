#include "FileScanner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace filesystem {

FileScanner::FileScanner() {
    std::cout << "[FileScanner] 初期化完了" << std::endl;
}

FileScanner::~FileScanner() {
    std::cout << "[FileScanner] シャットダウン完了" << std::endl;
}

size_t FileScanner::scanDirectory(const std::string& rootPath) {
    std::cout << "[FileScanner] ディレクトリスキャン開始: " << rootPath << std::endl;

    // CP2 必須成功要件: ナノ秒単位で計測
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

    // CP2 必須成功要件: 完全ロード証明（ナノ秒ログ）
    std::cout << "[FileScanner] スキャン完了:" << std::endl;
    std::cout << "  ファイル数: " << m_files.size() << std::endl;
    std::cout << "  スキャン時間: " << getScanTimeMs() << " ms" << std::endl;
    std::cout << "  スキャン時間（ナノ秒）: " << m_scanTimeNanos << " ns" << std::endl;

    // string interning統計
    auto stats = getInterningStats();
    std::cout << "  String Interning:" << std::endl;
    std::cout << "    総文字列: " << stats.totalStrings << std::endl;
    std::cout << "    ユニーク文字列: " << stats.uniqueStrings << std::endl;
    std::cout << "    節約メモリ: " << (stats.memorySavedBytes / 1024.0) << " KB" << std::endl;

    // CP2 必須成功要件: 1万ファイル50ms以下
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
                // 特定のディレクトリをスキップ
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

                    // タイムスタンプを記録
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

    // CP2 必須成功要件: string interning（省メモリ）
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

    // 簡易的な依存関係解析
    try {
        std::ifstream file(path);
        if (file.is_open()) {
            std::string line;
            std::regex includeRegex(R"(#include\s+[<"]([^>"]+)[>"])");
            std::regex importRegex(R"((?:from|import)\s+([a-zA-Z0-9_.]+))");

            while (std::getline(file, line)) {
                std::smatch match;

                // C++ #include
                if (std::regex_search(line, match, includeRegex)) {
                    info.dependencies.push_back(internString(match[1].str()));
                }

                // Python import
                if (std::regex_search(line, match, importRegex)) {
                    info.dependencies.push_back(internString(match[1].str()));
                }

                // 最初の100行のみ解析（高速化）
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
        totalStrings += 3;  // path, name, extension
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
    // CP2 必須成功要件: FSEvents差分更新
    // 簡易実装: タイムスタンプ比較

    size_t changedCount = 0;

    for (auto& file : m_files) {
        try {
            fs::path path(file.path);
            if (!fs::exists(path)) {
                // ファイルが削除された
                changedCount++;
                continue;
            }

            auto lastWriteTime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                lastWriteTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            auto currentTime = std::chrono::system_clock::to_time_t(sctp);

            if (m_fileTimestamps[file.path] != currentTime) {
                // ファイルが変更された
                std::cout << "[FileScanner] 変更検出: " << file.name << std::endl;

                // 再スキャン
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

// ==============================================================================
// DependencyGraph 実装
// ==============================================================================

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
                // 循環検出
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
        // パスから名前のみ抽出
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
