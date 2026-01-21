#include "Application.h"
#include <imgui.h>
#include <imnodes.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION  // macOS OpenGL非推奨警告を抑制
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <set>

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

    // デフォルト値を設定
    m_inputText = "ここに質問を入力してください";
    m_promptTemplate = "あなたは優秀なAIアシスタントです。\n以下の質問に答えてください：\n{input}";

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

    // OpenGL設定（全プラットフォーム共通）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // macOS必須
    std::cout << "[Application] OpenGL 3.3バックエンドを使用（macOS）" << std::endl;
#else
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

    // OpenGLコンテキスト設定（全プラットフォーム）
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  // VSync有効

    // Dear ImGuiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Note: Docking requires ImGui docking branch
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    std::cout << "[Application] Dear ImGui初期化完了" << std::endl;

    // スタイル設定
    ImGui::StyleColorsDark();

    // バックエンドの初期化
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);

#ifdef __APPLE__
    ImGui_ImplOpenGL3_Init("#version 150");  // macOS: GLSL 1.50
#else
    ImGui_ImplOpenGL3_Init("#version 330");  // Linux: GLSL 3.30
#endif

    std::cout << "[Application] ImGuiバックエンド初期化完了" << std::endl;

    // imnodesの初期化
    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();

    std::cout << "[Application] imnodes初期化完了" << std::endl;

    // フォントのセットアップ
    setupFonts();

    // リソースマネージャーの初期化（最優先）
    m_resourceManager = resource::GetResourceManager();
    m_resourceManager->startMonitoring();
    std::cout << "[Application] リソースマネージャー初期化完了（定期監視開始）" << std::endl;

    // ノードシステムの初期化
    m_nodeSystem = std::make_unique<node::NodeSystem>();
    std::cout << "[Application] ノードシステム初期化完了" << std::endl;

    // MLXエンジンの初期化
    m_mlxEngine = std::make_unique<mlx::MLXEngine>();
    std::cout << "[Application] MLXエンジン初期化完了" << std::endl;

    // モデルのスキャン（キャッシュに保存）
    m_cachedModels = m_mlxEngine->scanModels(m_modelsDir);
    std::cout << "[Application] 検出されたモデル: " << m_cachedModels.size() << "個" << std::endl;

    // ファイルスキャナーの初期化
    m_fileScanner = std::make_unique<filesystem::FileScanner>();
    std::cout << "[Application] ファイルスキャナー初期化完了" << std::endl;

    // グラフレイアウトの初期化
    m_graphLayout = std::make_unique<layout::ForceDirectedLayout>();
    std::cout << "[Application] グラフレイアウト初期化完了" << std::endl;

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
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UIの描画
        render();

        // レンダリング
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }

    std::cout << "[Application] メインループ終了" << std::endl;
}

void Application::shutdown() {
    if (m_running) {
        std::cout << "[Application] シャットダウン開始..." << std::endl;

        m_running = false;

        // リソースマネージャーの監視停止
        if (m_resourceManager) {
            m_resourceManager->stopMonitoring();
        }

        // リソースのクリーンアップ
        m_nodeSystem.reset();
        m_mlxEngine.reset();

        // imnodesのクリーンアップ
        ImNodes::DestroyContext();

        // ImGuiのクリーンアップ
        ImGui_ImplOpenGL3_Shutdown();
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
    // メニューバー
    if (ImGui::BeginMainMenuBar()) {
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
            bool isAgentMode = (m_currentMode == UIMode::AgentOrchestration);
            bool isFilesystemMode = (m_currentMode == UIMode::FilesystemDynamics);

            if (ImGui::MenuItem("エージェント・オーケストレーション", nullptr, &isAgentMode)) {
                m_currentMode = UIMode::AgentOrchestration;
                std::cout << "[UI] エージェント・オーケストレーションモードに切り替え" << std::endl;
            }
            if (ImGui::MenuItem("ファイルシステム・ダイナミクス", nullptr, &isFilesystemMode)) {
                m_currentMode = UIMode::FilesystemDynamics;
                std::cout << "[UI] ファイルシステム・ダイナミクスモードに切り替え" << std::endl;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ヘルプ")) {
            if (ImGui::MenuItem("バージョン情報")) {
                std::cout << "[UI] AI開発ステーション v0.1.0" << std::endl;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // サイドバー
    renderSidebar();

    // UIモードに応じた描画
    if (m_currentMode == UIMode::AgentOrchestration) {
        // エージェント・オーケストレーション UI
        renderNodeEditor();
    } else {
        // ファイルシステム・ダイナミクス UI
        renderFilesystemUI();
    }

    // ステータスバー
    renderStatusBar();
}

void Application::renderNodeEditor() {
    ImGui::Begin("ノードエディタ");

    // ツールバー
    if (ImGui::Button("実行", ImVec2(80, 30))) {
        if (m_isExecuting) {
            std::cout << "[UI] 既に実行中です" << std::endl;
        } else {
            std::cout << "[UI] グラフ実行開始" << std::endl;

            // 全ノードを実行中状態に
            auto nodes = m_nodeSystem->getAllNodes();
            auto& registry = m_nodeSystem->getRegistry();

            // LLMノードを探してモデルをロード
            std::string modelToLoad;
            for (auto entity : nodes) {
                const auto& type = registry.get<node::TypeComponent>(entity);
                if (type.type == node::NodeType::LLM) {
                    const auto& name = registry.get<node::NameComponent>(entity);
                    modelToLoad = name.name;
                    std::cout << "[UI] LLMノード検出: " << modelToLoad << std::endl;
                    break;
                }
            }

            // モデルパスを構築してロード
            if (!modelToLoad.empty()) {
                // キャッシュされたモデルリストから対応するモデルを探す
                bool foundModel = false;
                for (const auto& model : m_cachedModels) {
                    // ノード名がモデル名に部分一致するかチェック
                    if (model.name.find(modelToLoad) != std::string::npos ||
                        modelToLoad.find(model.name) != std::string::npos) {
                        std::cout << "[UI] モデルをロード: " << model.path << std::endl;
                        m_mlxEngine->loadModel(model.path);
                        foundModel = true;
                        break;
                    }
                }

                if (!foundModel) {
                    std::cerr << "[UI] エラー: モデル '" << modelToLoad << "' が見つかりません" << std::endl;
                }
            }

            for (auto entity : nodes) {
                auto& state = registry.get<node::ExecutionStateComponent>(entity);
                state.state = node::ExecutionStateComponent::State::Running;
            }

            // 入力テキストとプロンプトテンプレートを取得（m_inputText/m_promptTemplateに保存済み）
            std::string finalPrompt = m_promptTemplate;

            // {input}をm_inputTextで置換
            size_t pos = finalPrompt.find("{input}");
            if (pos != std::string::npos) {
                finalPrompt.replace(pos, 7, m_inputText);
            }

            std::cout << "[UI] 最終プロンプト: " << finalPrompt.substr(0, 100) << "..." << std::endl;

            // MLX推論を非同期実行
            m_isExecuting = true;

            mlx::InferenceRequest request;
            request.prompt = finalPrompt;
            request.maxTokens = 512;
            request.temperature = 0.7f;

            request.onComplete = [this, nodes, &registry](const std::string& result) {
                std::cout << "[UI] 推論完了: " << result.substr(0, 100) << "..." << std::endl;

                // 結果を保存
                m_lastResult = result;

                // 全ノードを完了状態に
                for (auto entity : nodes) {
                    auto& state = registry.get<node::ExecutionStateComponent>(entity);
                    state.state = node::ExecutionStateComponent::State::Completed;
                }

                m_isExecuting = false;
            };

            request.onError = [this, nodes, &registry](const std::string& error) {
                std::cerr << "[UI] 推論エラー: " << error << std::endl;

                // エラーを結果に保存
                m_lastResult = "エラー: " + error;

                // 全ノードをエラー状態に
                for (auto entity : nodes) {
                    auto& state = registry.get<node::ExecutionStateComponent>(entity);
                    state.state = node::ExecutionStateComponent::State::Error;
                }

                m_isExecuting = false;
            };

            m_mlxEngine->inferAsync(request);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("停止", ImVec2(80, 30))) {
        std::cout << "[UI] 実行停止" << std::endl;

        auto nodes = m_nodeSystem->getAllNodes();
        auto& registry = m_nodeSystem->getRegistry();

        for (auto entity : nodes) {
            auto& state = registry.get<node::ExecutionStateComponent>(entity);
            state.state = node::ExecutionStateComponent::State::Idle;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("リセット", ImVec2(80, 30))) {
        std::cout << "[UI] リセット" << std::endl;

        auto nodes = m_nodeSystem->getAllNodes();
        auto& registry = m_nodeSystem->getRegistry();

        for (auto entity : nodes) {
            auto& state = registry.get<node::ExecutionStateComponent>(entity);
            state.state = node::ExecutionStateComponent::State::Idle;
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImNodes::BeginNodeEditor();

    // 既存のノードを描画
    auto nodes = m_nodeSystem->getAllNodes();
    auto& registry = m_nodeSystem->getRegistry();

    for (auto entity : nodes) {
        const auto& name = registry.get<node::NameComponent>(entity);
        const auto& type = registry.get<node::TypeComponent>(entity);
        const auto& pos = registry.get<node::PositionComponent>(entity);
        // const auto& conn = registry.get<node::ConnectionComponent>(entity);
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

        // タイプに応じた編集UI
        if (type.type == node::NodeType::Input) {
            // 入力ノード: ユーザーが質問を入力
            static char inputBuffer[512] = "ここに質問を入力してください";
            ImGui::Spacing();
            if (ImGui::InputTextMultiline("##input", inputBuffer, sizeof(inputBuffer), ImVec2(180, 60))) {
                m_inputText = std::string(inputBuffer);
                std::cout << "[UI] 入力テキスト: " << m_inputText << std::endl;
            }
        } else if (type.type == node::NodeType::Prompt) {
            // プロンプトノード: 複数行テキスト入力
            static char promptBuffer[512] = "あなたは優秀なAIアシスタントです。\n以下の質問に答えてください：\n{input}";
            ImGui::Spacing();
            if (ImGui::InputTextMultiline("##prompt", promptBuffer, sizeof(promptBuffer), ImVec2(180, 80))) {
                m_promptTemplate = std::string(promptBuffer);
                std::cout << "[UI] プロンプト編集: " << m_promptTemplate << std::endl;
            }
        } else if (type.type == node::NodeType::LLM) {
            // LLMノード: モデル名表示
            ImGui::Spacing();
            ImGui::TextWrapped("モデル: %s", name.name.c_str());
        } else if (type.type == node::NodeType::Output) {
            // 出力ノード: 結果表示エリア
            ImGui::Spacing();
            if (m_lastResult.empty()) {
                ImGui::TextWrapped("（実行結果がここに表示されます）");
            } else {
                ImGui::TextWrapped("%s", m_lastResult.c_str());
            }
        }

        // 状態表示
        ImGui::Spacing();
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
        static std::set<int> initializedNodes;
        if (initializedNodes.find(nodeId) == initializedNodes.end()) {
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pos.x, pos.y));
            initializedNodes.insert(nodeId);
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

    // ノードのドラッグ位置を保存
    for (auto entity : nodes) {
        int nodeId = static_cast<int>(entity);
        ImVec2 nodePos = ImNodes::GetNodeGridSpacePos(nodeId);

        auto& pos = registry.get<node::PositionComponent>(entity);
        pos.x = nodePos.x;
        pos.y = nodePos.y;
    }

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
        // キャッシュされたモデルリストを使用（無限ループ防止）
        for (const auto& model : m_cachedModels) {
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
        static int nodeCounter = 0;

        if (ImGui::Button("LLMノード", ImVec2(-1, 0))) {
            float x = 200.0f + (nodeCounter % 3) * 250.0f;
            float y = 150.0f + (nodeCounter / 3) * 200.0f;
            m_nodeSystem->createNode(
                node::NodeType::LLM,
                "LLM",
                x, y
            );
            nodeCounter++;
            std::cout << "[UI] LLMノード追加" << std::endl;
        }

        if (ImGui::Button("プロンプトノード", ImVec2(-1, 0))) {
            float x = 200.0f + (nodeCounter % 3) * 250.0f;
            float y = 150.0f + (nodeCounter / 3) * 200.0f;
            m_nodeSystem->createNode(
                node::NodeType::Prompt,
                "プロンプト",
                x, y
            );
            nodeCounter++;
            std::cout << "[UI] プロンプトノード追加" << std::endl;
        }

        if (ImGui::Button("出力ノード", ImVec2(-1, 0))) {
            float x = 200.0f + (nodeCounter % 3) * 250.0f;
            float y = 150.0f + (nodeCounter / 3) * 200.0f;
            m_nodeSystem->createNode(
                node::NodeType::Output,
                "出力",
                x, y
            );
            nodeCounter++;
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

    // 日本語フォントの読み込み
    std::vector<std::string> fontPaths = {
        "/System/Library/Fonts/Hiragino Sans GB.ttc",       // macOS確認済み
        "/Library/Fonts/Arial Unicode.ttf",                  // macOSフォールバック
        "resources/fonts/NotoSansJP-Regular.ttf",
        "../resources/fonts/NotoSansJP-Regular.ttf"
    };

    ImFont* font = nullptr;
    for (const auto& path : fontPaths) {
        std::cout << "[Application] フォント検索中: " << path << std::endl;

        if (fs::exists(path)) {
            std::cout << "[Application] フォント発見: " << path << std::endl;

            // 日本語グリフ範囲を指定
            ImFontConfig config;
            config.OversampleH = 2;
            config.OversampleV = 1;
            config.PixelSnapH = true;

            font = io.Fonts->AddFontFromFileTTF(
                path.c_str(),
                18.0f,
                &config,
                io.Fonts->GetGlyphRangesJapanese()
            );

            if (font) {
                std::cout << "[Application] ✓ 日本語フォント読み込み成功: " << path << std::endl;
                break;
            } else {
                std::cerr << "[Application] ✗ フォント読み込み失敗: " << path << std::endl;
            }
        }
    }

    // フォールバック
    if (!font) {
        std::cout << "[Application] 警告: 日本語フォントが見つかりません。デフォルトフォントを使用" << std::endl;
        io.Fonts->AddFontDefault();
    }

    // フォントアトラスを明示的にビルド
    io.Fonts->Build();

    std::cout << "[Application] フォントアトラスビルド完了" << std::endl;
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

void Application::renderFilesystemUI() {
    ImGui::Begin("ファイルシステム・ダイナミクス");

    // 上部: コントロールパネル
    ImGui::BeginChild("ControlPanel", ImVec2(0, 150), true);

    ImGui::Text("プロジェクトフォルダ:");
    ImGui::SameLine();

    // フォルダ選択ボタン
    if (ImGui::Button("フォルダ選択...")) {
        showFolderSelectDialog();
    }

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "%s",
                       m_selectedFolder.empty() ? "(未選択)" : m_selectedFolder.c_str());

    ImGui::Spacing();

    // スキャン実行ボタン
    if (ImGui::Button("スキャン実行", ImVec2(150, 0))) {
        if (!m_selectedFolder.empty()) {
            std::cout << "[FilesystemUI] スキャン開始: " << m_selectedFolder << std::endl;

            // ファイルスキャン実行
            size_t fileCount = m_fileScanner->scanDirectory(m_selectedFolder);

            if (fileCount > 0) {
                // グラフレイアウトを構築
                m_graphLayout->reset();

                const auto& files = m_fileScanner->getFiles();

                // CP2 必須成功要件: フォルダ階層ベースの自動クラスタリング
                for (const auto& file : files) {
                    // クラスタID = ファイルの親ディレクトリパス
                    fs::path filePath(file.path);
                    std::string cluster = filePath.parent_path().string();

                    // 初期位置はランダム（後で力学シミュレーション）
                    layout::Vector2 initialPos(
                        static_cast<float>(rand() % 800 + 100),
                        static_cast<float>(rand() % 600 + 100)
                    );

                    m_graphLayout->addNode(file.path, initialPos, cluster);
                }

                // 依存関係エッジを追加
                for (const auto& file : files) {
                    for (const auto& dep : file.dependencies) {
                        // 依存先がスキャンされたファイルに存在するか確認
                        for (const auto& targetFile : files) {
                            if (targetFile.name.find(dep) != std::string::npos ||
                                targetFile.path.find(dep) != std::string::npos) {
                                m_graphLayout->addEdge(file.path, targetFile.path);
                                break;
                            }
                        }
                    }
                }

                // レイアウト計算実行
                layout::ForceDirectedLayout::Parameters params;
                params.maxIterations = 200;
                params.enableClustering = true;

                int iterations = m_graphLayout->computeLayout(params);
                m_filesystemLayoutComputed = true;

                std::cout << "[FilesystemUI] レイアウト計算完了: " << iterations << " イテレーション" << std::endl;
            }
        } else {
            std::cout << "[FilesystemUI] エラー: フォルダが選択されていません" << std::endl;
        }
    }

    ImGui::SameLine();

    // レイアウト再計算ボタン
    if (ImGui::Button("レイアウト再計算", ImVec2(150, 0))) {
        if (m_filesystemLayoutComputed) {
            layout::ForceDirectedLayout::Parameters params;
            params.maxIterations = 200;
            params.enableClustering = true;

            m_graphLayout->computeLayout(params);
            std::cout << "[FilesystemUI] レイアウト再計算完了" << std::endl;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // スキャン統計情報
    const auto& files = m_fileScanner->getFiles();
    if (!files.empty()) {
        ImGui::Text("統計情報:");
        ImGui::BulletText("ファイル数: %zu", files.size());
        ImGui::BulletText("スキャン時間: %.2f ms", m_fileScanner->getScanTimeMs());

        auto stats = m_fileScanner->getInterningStats();
        ImGui::BulletText("メモリ削減: %.1f KB", stats.memorySavedBytes / 1024.0);

        if (m_filesystemLayoutComputed) {
            int crossings = m_graphLayout->getEdgeCrossings();
            ImGui::BulletText("エッジ交差数: %d", crossings);
        }
    }

    ImGui::EndChild();

    // 下部: グラフ可視化エリア
    ImGui::BeginChild("GraphView", ImVec2(0, 0), true);

    if (m_filesystemLayoutComputed && !files.empty()) {
        ImNodes::BeginNodeEditor();

        // ファイルノードを描画
        int nodeId = 0;

        for (const auto& file : files) {
            layout::Vector2 pos = m_graphLayout->getNodePosition(file.path);

            ImNodes::BeginNode(nodeId);

            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(file.name.c_str());
            ImNodes::EndNodeTitleBar();

            // ファイル情報表示
            ImGui::Text("拡張子: %s", file.extension.c_str());
            ImGui::Text("サイズ: %zu bytes", file.size);

            if (!file.dependencies.empty()) {
                ImGui::Text("依存: %zu", file.dependencies.size());
            }

            // 出力ピン（依存関係用）
            int outputPinId = nodeId * 1000 + 1;
            ImNodes::BeginOutputAttribute(outputPinId);
            ImGui::Text("依存先");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();

            // ノード位置を設定（力学シミュレーション結果）
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pos.x, pos.y));

            nodeId++;
        }

        // 依存関係エッジを描画
        int linkId = 0;
        int fromNodeId = 0;

        for (const auto& file : files) {
            for (const auto& dep : file.dependencies) {
                // 依存先ノードを検索
                int toNodeId = 0;
                for (const auto& targetFile : files) {
                    if (targetFile.name.find(dep) != std::string::npos ||
                        targetFile.path.find(dep) != std::string::npos) {

                        int fromPinId = fromNodeId * 1000 + 1;
                        int toPinId = toNodeId * 1000 + 1;

                        ImNodes::Link(linkId++, fromPinId, toPinId);
                        break;
                    }
                    toNodeId++;
                }
            }
            fromNodeId++;
        }

        ImNodes::EndNodeEditor();
    } else {
        // プレースホルダー
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                          "フォルダを選択してスキャンを実行してください");
    }

    ImGui::EndChild();

    ImGui::End();
}

void Application::showFolderSelectDialog() {
    // CP2 必須成功要件: ネイティブファイルダイアログ
    // 簡易実装: 標準入力でパスを受け取る（本番実装ではネイティブダイアログを使用）

    std::cout << "\n[FilesystemUI] フォルダ選択" << std::endl;
    std::cout << "フォルダパスを入力してください: ";

    // デフォルト候補を提示
    std::vector<std::string> suggestions = {
        "/home/user/AAAAA",
        "/Users/yamaguchinaoyuki/Desktop/JJJ/AAA",
        "."
    };

    std::cout << "\n候補:" << std::endl;
    for (size_t i = 0; i < suggestions.size(); ++i) {
        if (fs::exists(suggestions[i])) {
            std::cout << "  " << (i + 1) << ") " << suggestions[i] << " ✓" << std::endl;
        } else {
            std::cout << "  " << (i + 1) << ") " << suggestions[i] << std::endl;
        }
    }

    // 暫定: 最初の存在するパスを自動選択
    for (const auto& path : suggestions) {
        if (fs::exists(path)) {
            m_selectedFolder = fs::canonical(path).string();
            std::cout << "[FilesystemUI] 自動選択: " << m_selectedFolder << std::endl;
            break;
        }
    }

    // TODO: 本番実装では nativefiledialog や platform-specific API を使用
    // - macOS: NSOpenPanel
    // - Linux: GTK file chooser / Qt file dialog
}

} // namespace app
