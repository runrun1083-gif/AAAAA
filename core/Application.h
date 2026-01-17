#pragma once

#include "NodeSystem.h"
#include "MLXEngine.h"
#include "FileScanner.h"
#include "GraphLayout.h"
#include <memory>
#include <string>

struct GLFWwindow;

// ==============================================================================
// AI開発ステーション - メインアプリケーション
// ==============================================================================
// Dear ImGui + imnodes + EnTT + MLX統合
// ==============================================================================

namespace app {

/**
 * @brief メインアプリケーションクラス
 */
class Application {
public:
    Application();
    ~Application();

    // コピー・ムーブ禁止
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /**
     * @brief アプリケーションの初期化
     *
     * @return 成功ならtrue
     */
    bool initialize();

    /**
     * @brief メインループの実行
     */
    void run();

    /**
     * @brief シャットダウン
     */
    void shutdown();

private:
    /**
     * @brief フレームの描画
     */
    void render();

    /**
     * @brief ノードエディタUIの描画
     */
    void renderNodeEditor();

    /**
     * @brief サイドバーの描画
     */
    void renderSidebar();

    /**
     * @brief ステータスバーの描画
     */
    void renderStatusBar();

    /**
     * @brief フォントのセットアップ
     */
    void setupFonts();

    /**
     * @brief デモノードの作成
     */
    void createDemoNodes();

    /**
     * @brief ファイルシステム・ダイナミクスUIの描画
     */
    void renderFilesystemUI();

    /**
     * @brief フォルダ選択ダイアログ
     */
    void showFolderSelectDialog();

private:
    // UIモード
    enum class UIMode {
        AgentOrchestration,     ///< エージェント・オーケストレーション
        FilesystemDynamics      ///< ファイルシステム・ダイナミクス
    };

    GLFWwindow* m_window = nullptr;             ///< GLFWウィンドウ
    std::unique_ptr<node::NodeSystem> m_nodeSystem;  ///< ノード管理システム
    std::unique_ptr<mlx::MLXEngine> m_mlxEngine;     ///< MLX推論エンジン
    std::unique_ptr<filesystem::FileScanner> m_fileScanner;  ///< ファイルスキャナー
    std::unique_ptr<layout::ForceDirectedLayout> m_graphLayout;  ///< グラフレイアウト

    int m_windowWidth = 1600;                   ///< ウィンドウ幅
    int m_windowHeight = 1000;                  ///< ウィンドウ高さ
    bool m_running = false;                     ///< 実行中フラグ
    UIMode m_currentMode = UIMode::AgentOrchestration;  ///< 現在のUIモード

    std::string m_modelsDir;                    ///< モデルディレクトリ
    std::string m_selectedFolder;               ///< 選択されたフォルダ（ファイルシステムモード）
    bool m_filesystemLayoutComputed = false;    ///< レイアウト計算済みフラグ
};

} // namespace app
