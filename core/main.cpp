#include "Application.h"
#include <iostream>
#include <exception>

// ==============================================================================
// AI開発ステーション - メインエントリーポイント (Linux版)
// ==============================================================================

int main(int argc, char* argv[]) {
    (void)argc;  // 未使用警告回避
    (void)argv;

    try {
        // アプリケーションのインスタンス作成
        app::Application app;

        // 初期化
        if (!app.initialize()) {
            std::cerr << "[Main] アプリケーションの初期化に失敗しました" << std::endl;
            return 1;
        }

        // メインループ実行
        app.run();

        // シャットダウン
        app.shutdown();

        std::cout << "[Main] 正常終了" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Main] 致命的エラー: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Main] 不明な致命的エラー" << std::endl;
        return 1;
    }
}
