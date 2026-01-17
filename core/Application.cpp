#include "Application.h"
#include <imgui.h>
#include <imnodes.h>
#include <backends/imgui_impl_glfw.h>

#ifdef __APPLE__
#include <backends/imgui_impl_metal.h>
#else
#include <backends/imgui_impl_opengl3.h>
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace app {

// ==============================================================================
// グローバル変数とコールバック
// ==============================================================================

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "[GLFW] エラー " << error << ": " << description << std::endl;
}

// ==============================================================================
// Application 実装
// ==============================================================================

Application::Application() {
    // モデルディレクトリのパス設定
    // 環境に応じて適切なパスを設定
    std::vector<std::string> possiblePaths = {
        "/Users/yamaguchinaoyuki/Desktop/JJJ/AAA/models",
        "../models",
        "models"
    };

    for (const auto& path : possiblePaths) {
        if (fs::exists(path)) {
            m_modelsDir = path;
            break;
        }
    }

    if (m_modelsDir.empty()) {
        m_modelsDir = "models";  // デフォルト
    }

    std::cout << "[Application] モデルディレクトリ: " << m_modelsDir << std::endl;
}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    std::cout << "==========================================\n";
    std::cout << "  AI開発ステーション v0.1.0\n";
    std::cout << "  本格実装版\n";
    std::cout << "==========================================\n\n";

    // GLFWの初期化
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        std::cerr << "[Application] GLFWの初期化に失敗しました" << std::endl;
        return false;
    }

    std::cout << "[Application] GLFW初期化完了" << std::endl;

#ifdef __APPLE__
    // macOS: Metal設定
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    std::cout << "[Application] Metalバックエンドを使用" << std::endl;
#else
    // Linux: OpenGL設定
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    std::cout << "[Application] OpenGL 3.3バックエンドを使用" << std::endl;
#endif

    // ウィンドウの作成
    m_window = glfwCreateWindow(
        m_windowWidth,
        m_windowHeight,
        "AI開発ステーション - Flowise再定義 C++/EnTT/MLX",
        nullptr,
        nullptr
    );

    if (!m_window) {
        std::cerr << "[Application] ウィンドウの作成に失敗しました" << std::endl;
        glfwTerminate();
        return false;
    }

    std::cout << "[Application] ウィンドウ作成完了: " << m_windowWidth << "x" << m_windowHeight << std::endl;

#ifndef __APPLE__
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  // VSync有効
#endif

    // Dear ImGuiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    std::cout << "[Application] Dear ImGui初期化完了" << std::endl;

    // スタイル設定
    ImGui::StyleColorsDark();

    // バックエンドの初期化
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);

#ifdef __APPLE__
    // TODO: Metal初期化
    // ImGui_ImplMetal_Init(device);
    std::cout << "[Application] 警告: Metal初期化は未実装（OpenGLフォールバック）" << std::endl;
    ImGui_ImplOpenGL3_Init("#version 150");
#else
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    std::cout << "[Application] ImGuiバックエンド初期化完了" << std::endl;

    // imnodesの初期化
    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();

    std::cout << "[Application] imnodes初期化完了" << std::endl;

    // フォントのセットアップ
    setupFonts();

    // ノードシステムの初期化
    m_nodeSystem = std::make_unique<node::NodeSystem>();
    std::cout << "[Application] ノードシステム初期化完了" << std::endl;

    // MLXエンジンの初期化
    m_mlxEngine = std::make_unique<mlx::MLXEngine>();
    std::cout << "[Application] MLXエンジン初期化完了" << std::endl;

    // モデルのスキャン
    auto models = m_mlxEngine->scanModels(m_modelsDir);
    std::cout << "[Application] 検出されたモデル: " << models.size() << "個" << std::endl;

    // デモノードの作成
    createDemoNodes();

    m_running = true;

    std::cout << "\n[Application] 初期化完了 - メインループ開始\n" << std::endl;

    return true;
}

void Application::run() {
    while (m_running && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // フレーム開始
#ifdef __APPLE__
        // TODO: Metal
        ImGui_ImplOpenGL3_NewFrame();
#else
        ImGui_ImplOpenGL3_NewFrame();
#endif
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UIの描画
        render();

        // レンダリング
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);

#ifdef __APPLE__
        // TODO: Metal
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

        glfwSwapBuffers(m_window);
    }

    std::cout << "[Application] メインループ終了" << std::endl;
}

void Application::shutdown() {
    if (m_running) {
        std::cout << "[Application] シャットダウン開始..." << std::endl;

        m_running = false;

        // リソースのクリーンアップ
        m_nodeSystem.reset();
        m_mlxEngine.reset();

        // imnodesのクリーンアップ
        ImNodes::DestroyContext();

        // ImGuiのクリーンアップ
#ifdef __APPLE__
        ImGui_ImplOpenGL3_Shutdown();
#else
        ImGui_ImplOpenGL3_Shutdown();
#endif
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // GLFWのクリーンアップ
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();

        std::cout << "[Application] シャットダウン完了" << std::endl;
    }
}

// ==============================================================================
// Private メソッド
// ==============================================================================

void Application::render() {
    // メインドッキングスペース
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // メニューバー
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("新規プロジェクト", "Ctrl+N")) {
                std::cout << "[UI] 新規プロジェクト" << std::endl;
            }
            if (ImGui::MenuItem("開く", "Ctrl+O")) {
                std::cout << "[UI] プロジェクトを開く" << std::endl;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("終了", "Alt+F4")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("表示")) {
            if (ImGui::MenuItem("エージェント・オーケストレーション")) {
                std::cout << "[UI] エージェント・オーケストレーションモード" << std::endl;
            }
            if (ImGui::MenuItem("ファイルシステム解析")) {
                std::cout << "[UI] ファイルシステム解析モード" << std::endl;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ヘルプ")) {
            if (ImGui::MenuItem("バージョン情報")) {
                std::cout << "[UI] AI開発ステーション v0.1.0" << std::endl;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // ドッキングスペース
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();

    // サイドバー
    renderSidebar();

    // ノードエディタ
    renderNodeEditor();

    // ステータスバー
    renderStatusBar();
}

void Application::renderNodeEditor() {
    ImGui::Begin("ノードエディタ");

    ImNodes::BeginNodeEditor();

    // 既存のノードを描画
    auto nodes = m_nodeSystem->getAllNodes();
    auto& registry = m_nodeSystem->getRegistry();

    for (auto entity : nodes) {
        const auto& name = registry.get<node::NameComponent>(entity);
        const auto& type = registry.get<node::TypeComponent>(entity);
        const auto& pos = registry.get<node::PositionComponent>(entity);
        const auto& conn = registry.get<node::ConnectionComponent>(entity);
        const auto& state = registry.get<node::ExecutionStateComponent>(entity);

        int nodeId = static_cast<int>(entity);

        ImNodes::BeginNode(nodeId);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.name.c_str());
        ImNodes::EndNodeTitleBar();

        // 入力ピン
        int inputPinId = nodeId * 1000 + 1;
        ImNodes::BeginInputAttribute(inputPinId);
        ImGui::Text("入力");
        ImNodes::EndInputAttribute();

        // ノードの内容
        ImGui::Spacing();
        ImGui::Text("タイプ: %s", node::nodeTypeToString(type.type));

        // 状態表示
        const char* stateStr = "アイドル";
        ImVec4 stateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        switch (state.state) {
            case node::ExecutionStateComponent::State::Running:
                stateStr = "実行中";
                stateColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                break;
            case node::ExecutionStateComponent::State::Completed:
                stateStr = "完了";
                stateColor = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
                break;
            case node::ExecutionStateComponent::State::Error:
                stateStr = "エラー";
                stateColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                break;
            default:
                break;
        }

        ImGui::TextColored(stateColor, "状態: %s", stateStr);

        // 出力ピン
        int outputPinId = nodeId * 1000 + 2;
        ImNodes::BeginOutputAttribute(outputPinId);
        ImGui::Indent(80);
        ImGui::Text("出力");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();

        // ノードの位置を設定（初回のみ）
        static bool positionsSet = false;
        if (!positionsSet) {
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pos.x, pos.y));
        }
    }

    // リンクの描画
    int linkId = 0;
    for (auto entity : nodes) {
        const auto& conn = registry.get<node::ConnectionComponent>(entity);
        int fromNodeId = static_cast<int>(entity);
        int fromPinId = fromNodeId * 1000 + 2;  // 出力ピン

        for (auto toEntity : conn.outputs) {
            int toNodeId = static_cast<int>(toEntity);
            int toPinId = toNodeId * 1000 + 1;  // 入力ピン

            ImNodes::Link(linkId++, fromPinId, toPinId);
        }
    }

    ImNodes::EndNodeEditor();

    // 新しいリンクの作成
    int startPin, endPin;
    if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
        int startNode = startPin / 1000;
        int endNode = endPin / 1000;

        m_nodeSystem->connectNodes(
            static_cast<entt::entity>(startNode),
            static_cast<entt::entity>(endNode)
        );

        std::cout << "[UI] ノード接続: " << startNode << " -> " << endNode << std::endl;
    }

    ImGui::End();
}

void Application::renderSidebar() {
    ImGui::Begin("サイドバー");

    if (ImGui::CollapsingHeader("モデル", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto models = m_mlxEngine->scanModels(m_modelsDir);

        for (const auto& model : models) {
            if (ImGui::Selectable(model.name.c_str())) {
                std::cout << "[UI] モデル選択: " << model.name << std::endl;
                m_mlxEngine->loadModel(model.path);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("ノード追加", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("LLMノード", ImVec2(-1, 0))) {
            auto entity = m_nodeSystem->createNode(
                node::NodeType::LLM,
                "LLM",
                100.0f,
                100.0f
            );
            std::cout << "[UI] LLMノード追加: " << static_cast<uint32_t>(entity) << std::endl;
        }

        if (ImGui::Button("プロンプトノード", ImVec2(-1, 0))) {
            auto entity = m_nodeSystem->createNode(
                node::NodeType::Prompt,
                "プロンプト",
                100.0f,
                200.0f
            );
            std::cout << "[UI] プロンプトノード追加" << std::endl;
        }

        if (ImGui::Button("出力ノード", ImVec2(-1, 0))) {
            auto entity = m_nodeSystem->createNode(
                node::NodeType::Output,
                "出力",
                100.0f,
                300.0f
            );
            std::cout << "[UI] 出力ノード追加" << std::endl;
        }
    }

    ImGui::End();
}

void Application::renderStatusBar() {
    ImGui::Begin("ステータスバー", nullptr, ImGuiWindowFlags_NoScrollbar);

    // FPS表示
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::SameLine(200);

    // GPU使用率
    float gpuUsage = m_mlxEngine->getGPUUsage();
    ImGui::Text("GPU: %.1f%%", gpuUsage * 100.0f);

    ImGui::SameLine(350);

    // メモリ使用率
    float memUsage = m_mlxEngine->getMemoryUsage();
    ImGui::Text("メモリ: %.1f%%", memUsage * 100.0f);

    ImGui::SameLine(500);

    // トークン生成速度
    float tokensPerSec = m_mlxEngine->getTokensPerSecond();
    ImGui::Text("速度: %.1f tokens/sec", tokensPerSec);

    ImGui::End();
}

void Application::setupFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // デフォルトフォントを読み込み
    // TODO: 日本語フォント（Noto Sans JP）の統合
    io.Fonts->AddFontDefault();

    std::cout << "[Application] フォント読み込み完了" << std::endl;
}

void Application::createDemoNodes() {
    std::cout << "[Application] デモノード作成中..." << std::endl;

    // 入力ノード
    auto inputNode = m_nodeSystem->createNode(
        node::NodeType::Input,
        "ユーザー入力",
        50.0f,
        100.0f
    );

    // プロンプトノード
    auto promptNode = m_nodeSystem->createNode(
        node::NodeType::Prompt,
        "プロンプトテンプレート",
        300.0f,
        100.0f
    );

    // LLMノード
    auto llmNode = m_nodeSystem->createNode(
        node::NodeType::LLM,
        "Qwen2.5-7B",
        550.0f,
        100.0f
    );

    // 出力ノード
    auto outputNode = m_nodeSystem->createNode(
        node::NodeType::Output,
        "結果出力",
        800.0f,
        100.0f
    );

    // ノードを接続
    m_nodeSystem->connectNodes(inputNode, promptNode);
    m_nodeSystem->connectNodes(promptNode, llmNode);
    m_nodeSystem->connectNodes(llmNode, outputNode);

    std::cout << "[Application] デモノード作成完了（4ノード、3接続）" << std::endl;
}

} // namespace app
