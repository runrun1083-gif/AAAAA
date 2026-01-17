#include "Application.h"
#include <iostream>
#include <exception>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

// ==============================================================================
// AI開発ステーション - メインエントリーポイント (macOS版)
// ==============================================================================

int main(int argc, char* argv[]) {
    (void)argc;  // 未使用警告回避
    (void)argv;

    @autoreleasepool {
        // Metalデバイスの検出と情報表示
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            NSString* deviceName = [device name];
            std::cout << "[Metal] GPU検出: " << [deviceName UTF8String] << std::endl;

            // Unified Memory サイズ
            uint64_t maxWorkingSetSize = [device recommendedMaxWorkingSetSize];
            double memoryGB = maxWorkingSetSize / (1024.0 * 1024.0 * 1024.0);
            std::cout << "[Metal] Unified Memory: " << memoryGB << " GB" << std::endl;

            // Apple Siliconかどうか
            if ([device supportsFamily:MTLGPUFamilyApple7]) {
                std::cout << "[Metal] Apple Silicon (M1/M2/M3) 検出" << std::endl;
            }

            std::cout << std::endl;
        } else {
            std::cerr << "[Metal] 警告: Metalデバイスが検出できませんでした" << std::endl;
        }

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
}

#else
#error "This file should only be compiled on macOS"
#endif
