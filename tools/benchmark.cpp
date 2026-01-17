// ==============================================================================
// AI開発ステーション - ベンチマーク計測ツール
// ==============================================================================
// QA必須項目の自動計測とエビデンス出力
// ==============================================================================

#include "../core/FileScanner.h"
#include "../core/GraphLayout.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sys/resource.h>

using namespace std::chrono;

// メモリ使用量取得（Linux）
size_t getCurrentMemoryUsageKB() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss; // KB単位
}

// ベンチマーク結果構造体
struct BenchmarkResult {
    std::string testName;
    double timeMs;
    size_t memoryKB;
    size_t itemCount;

    void print() const {
        std::cout << std::setw(50) << std::left << testName
                  << std::setw(12) << std::right << std::fixed << std::setprecision(2) << timeMs << " ms"
                  << std::setw(12) << memoryKB << " KB"
                  << std::setw(12) << itemCount << " items"
                  << std::endl;
    }
};

// FileScanner ベンチマーク
BenchmarkResult benchmarkFileScanner(const std::string& path, int iterations = 1) {
    BenchmarkResult result;
    result.testName = "FileScanner: " + path;

    size_t totalFiles = 0;
    double totalTime = 0.0;

    filesystem::FileScanner scanner;

    for (int i = 0; i < iterations; ++i) {
        auto start = high_resolution_clock::now();

        size_t fileCount = scanner.scanDirectory(path);

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        totalFiles = fileCount;
        totalTime += duration.count() / 1000.0; // ms
    }

    result.timeMs = totalTime / iterations;
    result.itemCount = totalFiles;
    result.memoryKB = getCurrentMemoryUsageKB();

    // String interning 効果
    auto stats = scanner.getInterningStats();
    std::cout << "  └─ String interning: "
              << stats.uniqueStrings << " unique / "
              << stats.totalStrings << " total, "
              << "メモリ削減: " << (stats.memorySavedBytes / 1024.0) << " KB"
              << std::endl;

    return result;
}

// GraphLayout ベンチマーク
BenchmarkResult benchmarkGraphLayout(int nodeCount) {
    BenchmarkResult result;
    result.testName = "GraphLayout: " + std::to_string(nodeCount) + " nodes";

    layout::ForceDirectedLayout layout;

    // ノードを追加
    for (int i = 0; i < nodeCount; ++i) {
        std::string id = "node_" + std::to_string(i);
        layout::Vector2 pos(
            static_cast<float>(rand() % 800 + 100),
            static_cast<float>(rand() % 600 + 100)
        );
        layout.addNode(id, pos, "cluster_" + std::to_string(i / 10));
    }

    // エッジを追加（各ノードから次の3ノードへ）
    for (int i = 0; i < nodeCount - 3; ++i) {
        std::string from = "node_" + std::to_string(i);
        for (int j = 1; j <= 3; ++j) {
            std::string to = "node_" + std::to_string(i + j);
            layout.addEdge(from, to);
        }
    }

    // レイアウト計算
    layout::ForceDirectedLayout::Parameters params;
    params.maxIterations = 200;
    params.enableClustering = true;

    auto start = high_resolution_clock::now();

    int iterations = layout.computeLayout(params);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    result.timeMs = duration.count() / 1000.0;
    result.itemCount = iterations;
    result.memoryKB = getCurrentMemoryUsageKB();

    int crossings = layout.getEdgeCrossings();
    std::cout << "  └─ イテレーション: " << iterations
              << ", エッジ交差数: " << crossings
              << std::endl;

    return result;
}

// メモリリークテスト
BenchmarkResult benchmarkMemoryLeak(int iterations) {
    BenchmarkResult result;
    result.testName = "Memory Leak Test: " + std::to_string(iterations) + " iterations";

    size_t initialMemory = getCurrentMemoryUsageKB();

    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        filesystem::FileScanner scanner;
        scanner.scanDirectory("/home/user/AAAAA/core");

        layout::ForceDirectedLayout layout;
        for (int j = 0; j < 10; ++j) {
            layout.addNode("node_" + std::to_string(j),
                         layout::Vector2(100.0f, 100.0f), "cluster");
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    size_t finalMemory = getCurrentMemoryUsageKB();

    result.timeMs = duration.count();
    result.itemCount = iterations;
    result.memoryKB = finalMemory - initialMemory;

    std::cout << "  └─ 初期メモリ: " << initialMemory << " KB, "
              << "最終メモリ: " << finalMemory << " KB, "
              << "増加: " << result.memoryKB << " KB"
              << std::endl;

    return result;
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "AI開発ステーション - ベンチマーク計測" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::vector<BenchmarkResult> results;

    // ヘッダー
    std::cout << std::setw(50) << std::left << "テスト項目"
              << std::setw(12) << std::right << "時間"
              << std::setw(12) << "メモリ"
              << std::setw(12) << "項目数"
              << std::endl;
    std::cout << std::string(86, '-') << std::endl;

    // 1. FileScanner ベンチマーク
    std::cout << "\n[1] FileScanner 性能計測" << std::endl;

    auto r1 = benchmarkFileScanner("/home/user/AAAAA/core", 10);
    r1.print();
    results.push_back(r1);

    auto r2 = benchmarkFileScanner("/home/user/AAAAA", 5);
    r2.print();
    results.push_back(r2);

    // 2. GraphLayout ベンチマーク
    std::cout << "\n[2] GraphLayout 性能計測" << std::endl;

    auto r3 = benchmarkGraphLayout(100);
    r3.print();
    results.push_back(r3);

    auto r4 = benchmarkGraphLayout(500);
    r4.print();
    results.push_back(r4);

    auto r5 = benchmarkGraphLayout(1000);
    r5.print();
    results.push_back(r5);

    // 3. メモリリークテスト
    std::cout << "\n[3] メモリリーク検証" << std::endl;

    auto r6 = benchmarkMemoryLeak(100);
    r6.print();
    results.push_back(r6);

    // 結果をファイルに出力
    std::cout << "\n========================================" << std::endl;
    std::cout << "結果を benchmark_results.txt に出力中..." << std::endl;

    std::ofstream outFile("benchmark_results.txt");
    outFile << "AI開発ステーション - ベンチマーク結果\n";
    outFile << "実行日時: " << __DATE__ << " " << __TIME__ << "\n\n";

    for (const auto& result : results) {
        outFile << result.testName << "\n";
        outFile << "  時間: " << result.timeMs << " ms\n";
        outFile << "  メモリ: " << result.memoryKB << " KB\n";
        outFile << "  項目数: " << result.itemCount << "\n\n";
    }

    outFile.close();

    std::cout << "完了" << std::endl;

    return 0;
}
