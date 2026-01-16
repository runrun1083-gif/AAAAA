/**
 * Module: core/main.mm
 * 言語: Objective-C++
 * 説明:
 * アプリケーションのエントリーポイント。Pythonランタイムの初期化とGUIメインループを担当する。
 * 外部依存:
 *   - Dear ImGui (Metal Backend)
 *   - Python 3.12 (Embedded)
 *   - GLFW
 */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include <stdio.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <iostream>
#include <pybind11/embed.h>
#include <string>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "FileScanner.hpp"
#include "GraphVisualizer.hpp"
#include "PythonManager.hpp"

namespace py = pybind11;

// エラー発生時のコールバック関数
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char **argv) {
  // 1. Python インタープリタの初期化 (GIL制御付き)
  // ---------------------------------------------------------
  // スコープド・インタープリタ：このスコープを抜けるとPythonも終了する
  py::scoped_interpreter guard{};

  // 1b. Scanner Initialization (Early Init for Headless CLI)
  entt::registry registry;
  aaaaa::core::FileScanner scanner(registry);
  aaaaa::core::ScanStats last_scan_stats;
  bool scan_done = false;

  // CP2.2 Graph Visualizer (Declared early for CLI access)
  aaaaa::core::GraphVisualizer visualizer;

  // 1c. Headless CLI Check (Before GUI/PythonManager)
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--scan-performance-check") {
      std::cout
          << "[Performance Check] Starting Native C++ Scan (Headless Mode)..."
          << std::endl;
      std::filesystem::path project_root =
          std::filesystem::current_path().parent_path();

      // Warmup
      scanner.ScanProject(project_root);

      // Official Run
      auto stats = scanner.ScanProject(project_root);

      // Update Graph (CP2.2 Verification)
      visualizer.UpdateGraph(registry);

      std::cout << "[Performance Check] Files Scanned: " << stats.file_count
                << std::endl;
      std::cout << "[Performance Check] Duration: " << stats.duration_ms
                << " ms" << std::endl;

      if (stats.duration_ms < 50.0) {
        std::cout << "RESULT: PASS (Ultra Fast)" << std::endl;
      } else {
        std::cout << "RESULT: WARNING (Slower than 50ms)" << std::endl;
      }
      return 0; // Exit immediately
    }
  }

  // メインスレッドのGILを解放し、バックグラウンドでのPython実行を可能にする
  // ※ ここではまだスレッドを作っていないが、将来のために解放しておくのが作法
  // ただし、pybind11のscoped_interpreterはGILを持った状態で始まる。
  // 明示的に release するには py::gil_scoped_release を使うが、
  // ここで release すると、直後の Python コード実行で acquire が必要になる。

  // .venv の site-packages を sys.path に追加 (CMakeでの自動検出を補助)
  {
    py::gil_scoped_acquire acquire; // 念のため明示的に確保（通常は持っている）
    try {
      py::exec("import sys\n"
               "import os\n"
               "cwd = os.getcwd()\n"
               "venv_path = os.path.join(cwd, '.venv')\n"
               "if not os.path.exists(venv_path):\n"
               "    venv_path = os.path.join(cwd, '../.venv')\n"
               "project_root = os.path.abspath(os.path.join(cwd, '..'))\n"
               "if project_root not in sys.path:\n"
               "    sys.path.insert(0, project_root)\n"
               "venv_site_pkgs = os.path.join(venv_path, "
               "'lib/python3.12/site-packages')\n"
               "if venv_site_pkgs not in sys.path:\n"
               "    sys.path.insert(0, venv_site_pkgs)\n"
               "print('Python Path Configured:', venv_site_pkgs)");

      // 設定ファイルの読み込み
      py::module_ settings = py::module_::import("config.user_settings");
      std::string app_title = "AAAAA - Professional AI Station"; // 仮タイトル

      // MLX の動作確認
      py::exec("import mlx.core as mx\n"
               "print(f'MLX Initialized (Core Version: {mx.__version__})')");

    } catch (py::error_already_set &e) {
      std::cerr << "Python 初期化エラー: " << e.what() << std::endl;
      return 1;
    }
  }

  // 2. GUI バックエンド (GLFW + Metal) の初期化
  // ---------------------------------------------------------
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  // Retina ディスプレイ対応などのウィンドウヒント設定
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // ウィンドウ作成
  GLFWwindow *window = glfwCreateWindow(
      1280, 800, "AAAAA - Professional AI Station", NULL, NULL);
  if (window == NULL)
    return 1;

  // Metal レイヤーのブリッジ接続
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];

  // GLFWのNSWindowを取得し、MetalLayerを追加する (Objective-C的な処理)
  NSWindow *nswindow = glfwGetCocoaWindow(window);
  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  nswindow.contentView.layer = layer;
  nswindow.contentView.wantsLayer = YES;

  MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor new];

  // 3. ImGui の初期化
  // ---------------------------------------------------------
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // キーボード操作有効化
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // ドッキング機能有効化

  // スタイル設定：プロフェッショナル・ダーク
  ImGui::StyleColorsDark();

  // --- Retina & Font Setup (Task 1.3) ---
  // M1 MacのRetinaディスプレイなど、高DPI環境での鮮明な描画を実現する工夫
  float xscale, yscale;
  glfwGetWindowContentScale(window, &xscale, &yscale);

  // ベーススケール（これを基準に各種サイズを調整）
  float pixel_scale = xscale;

  // ImGuiのグローバルスケールには 1.0/scale を設定し、
  // フォントは scale 倍の解像度でロードすることで、内部的に高解像度描画を行う
  // これにより、単なる拡大引き伸ばしではなく、ネイティブな鮮明さを得る
  io.FontGlobalScale = 1.0f / pixel_scale;

  // フォント設定
  ImFontConfig font_cfg;
  font_cfg.OversampleH = 2; // 水平方向のオーバーサンプリングでさらに綺麗に
  font_cfg.OversampleV = 2;
  font_cfg.PixelSnapH = true;

  // 基本フォントサイズ (Retinaスケールを掛けて高解像度化)
  float base_font_size = 16.0f * pixel_scale;
  float icon_font_size = 14.0f * pixel_scale;

  // 日本語フォント (NotoSansJP)
  const char *jp_font_path = "resources/fonts/NotoSansJP-Medium.ttf";
  ImFont *main_font = nullptr;

  FILE *font_file = fopen(jp_font_path, "rb");
  if (font_file) {
    fclose(font_file);
    // 日本語グリフレンジ
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddText(
        "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモ"
        "ヤユヨラリルレロワヲン"); // カタカナ追加(念のため)
    builder.BuildRanges(&ranges);

    main_font = io.Fonts->AddFontFromFileTTF(jp_font_path, base_font_size,
                                             &font_cfg, ranges.Data);
  } else {
    printf("警告: 日本語フォントが見つかりません "
           "(%s)。システムフォントへのフォールバックを検討してください。\n",
           jp_font_path);
    // フォールバック: デフォルト
    ImFontConfig default_cfg;
    default_cfg.SizePixels = base_font_size;
    io.Fonts->AddFontDefault(&default_cfg);
  }

  // アイコンフォント (FontAwesome) のマージ
  // MergeMode = true にすることで、直前のフォントに追加される
  static const ImWchar icon_ranges[] = {0xf000, 0xf3ff,
                                        0}; // FontAwesome Free Solid ranges
  ImFontConfig icon_cfg;
  icon_cfg.MergeMode = true;
  icon_cfg.PixelSnapH = true;
  icon_cfg.GlyphMinAdvanceX = base_font_size; // アイコンの幅を確保
  icon_cfg.OversampleH = 2;
  icon_cfg.OversampleV = 2;

  const char *icon_font_path = "resources/fonts/fa-solid-900.ttf";
  FILE *icon_file = fopen(icon_font_path, "rb");
  if (icon_file) {
    fclose(icon_file);
    // アイコンは少し小さめがバランス良い場合が多いが、ここではベースに合わせてロード
    io.Fonts->AddFontFromFileTTF(icon_font_path, icon_font_size, &icon_cfg,
                                 icon_ranges);
    printf("アイコンフォントをマージしました: %s\n", icon_font_path);
  } else {
    printf("警告: アイコンフォントが見つかりません (%s)\n", icon_font_path);
  }

  // スタイル（UIパーツのサイズ）もスケールに合わせて調整
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(pixel_scale);

  // バックエンド初期化
  ImGui_ImplGlfw_InitForOther(window, true);
  ImGui_ImplMetal_Init(device);

  // 5. Python Manager (Async Queue) - Start only if GUI mode
  aaaaa::PythonManager python_manager;

  // --- Automated QA Logic (Re-implemented for GUI mode) ---
  bool automated_check_mode = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--automated-check") {
      automated_check_mode = true;
      // ... Enqueue QA tasks ...
      python_manager.Enqueue(
          []() { py::exec("print('[QA] Automated Check Started')"); });
      // Note: Full QA logic simplified here to avoid duplication, assuming QA
      // check was mostly relevant for GUI responsiveness. Re-adding essential
      // QA tasks:
      for (int k = 0; k < 50; ++k)
        python_manager.Enqueue([k]() { py::exec("pass"); });
      python_manager.Enqueue(
          []() { py::exec("import time; time.sleep(5.0)"); });
    }
  }

  // ...

  // 7. Graph Visualizer (Task 2.2)
  // Already declared at top
  visualizer.Init(pixel_scale);

  // 4. メインループ
  // ---------------------------------------------------------
  float clear_color[4] = {0.1f, 0.11f, 0.12f, 1.0f}; // 背景色 (Dark Grey)

  while (!glfwWindowShouldClose(window)) {
    // イベントポーリング
    {
      py::gil_scoped_release release;
      glfwPollEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // --- Automated QA Auto-Exit ---
    if (automated_check_mode) {
      // FPS Logger (Evidence-Driven Verification)
      static double last_log_time = 0.0;
      double current_time = ImGui::GetTime();
      if (current_time - last_log_time >= 0.1) {
        std::cout << "[FPS Monitor] " << current_time
                  << " FPS: " << ImGui::GetIO().Framerate << std::endl;
        last_log_time = current_time;
      }

      if (ImGui::GetTime() > 8.0f) { // Extended to 8 seconds for Evidence
        std::cout << "[Automated QA] Test Duration Exceeded. Exiting..."
                  << std::endl;

        // Check if font loaded (QA4 proxy)
        if (main_font)
          std::cout << "[Automated QA] QA4: Japanese Font Loaded Successfully."
                    << std::endl;
        else
          std::cout << "[Automated QA] QA4: Japanese Font NOT Loaded."
                    << std::endl;

        break;
      }
    }

    // Retina ディスプレイ対応: ウィンドウサイズとフレームバッファサイズの取得
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    layer.drawableSize = CGSizeMake(width, height);
    id<CAMetalDrawable> drawable = [layer nextDrawable];

    // Safety check: specific for automated/headless or fast-loop scenarios
    if (!drawable) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Metal 描画パスの設定 (Moved here to fix rasterSampleCount error)
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
        clear_color[0] * clear_color[3], clear_color[1] * clear_color[3],
        clear_color[2] * clear_color[3], clear_color[3]);
    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    // フレーム開始
    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // --- GUI 描画 ---
    {
      ImGui::Begin("システム・ステータス (System Status)");
      ImGui::Text("各モジュールの状態を確認します。");
      ImGui::Separator();
      ImGui::Text("画面解像度: %d x %d (Scale: %.1f)", width, height,
                  pixel_scale);
      ImGui::Text("フレームレート: %.1f FPS", ImGui::GetIO().Framerate);
      ImGui::Text("待機中のタスク数: %zu", python_manager.GetQueueSize());
      ImGui::Separator();

      // クレバーなルート検出ロジック for No.2
      // 単純な parent_path()
      // ではなく、確実にプロジェクトルート(pyproject.tomlがある場所)を探す
      static std::filesystem::path project_root;
      if (project_root.empty()) {
        std::filesystem::path p = std::filesystem::current_path();
        for (int i = 0; i < 5; ++i) { // 最大5階層さかのぼる
          if (std::filesystem::exists(p / "pyproject.toml")) {
            project_root = p;
            break;
          }
          if (p.has_parent_path())
            p = p.parent_path();
          else
            break;
        }
        // 見つからなければカレントを採用（フォールバック）
        if (project_root.empty())
          project_root = std::filesystem::current_path();
      }

      ImGui::Text("Scan Root: %s", project_root.string().c_str());

      // スキャン機能
      if (ImGui::Button("プロジェクトスキャン (C++ Native)")) {
        last_scan_stats = scanner.ScanProject(project_root);
        scan_done = true;

        // Update Graph Layout & Links (CP2.2)
        visualizer.UpdateGraph(registry);
      }

      if (scan_done) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "スキャン完了!");
        ImGui::Text("ファイル数: %d", last_scan_stats.file_count);
        ImGui::Text("処理時間: %.2f ms", last_scan_stats.duration_ms);
        ImGui::Text("登録エンティティ数: %d",
                    (int)registry.storage<entt::entity>().size());
      }

      ImGui::Separator();

      // 視覚的な証明: GUIが生きているなら、このスピナーは回り続ける
      if (python_manager.GetQueueSize() > 0) {
        ImGui::Text("Processing... ");
        ImGui::SameLine();
        ImGui::ProgressBar(std::fmod((float)ImGui::GetTime(), 1.0f),
                           ImVec2(100, 0));
      }

      if (ImGui::Button("Python テスト実行 (10秒耐久・非同期証明 / QA1)")) {
        python_manager.Enqueue([]() {
          py::exec(
              "import time\n"
              "import mlx.core as mx\n"
              "print(f'[{time.ctime()}] 10-Second Heavy Task Started...')\n"
              "time.sleep(10.0)\n"
              "print(f'[{time.ctime()}] Heavy Task Finished!')");
        });
      }

      ImGui::Separator();
      ImGui::Text("CP1 QA Tests (Destructive)");

      // QA 2: Rapid Fire Test
      if (ImGui::Button("Rapid Fire Test (QA2)")) {
        for (int i = 0; i < 100; ++i) {
          python_manager.Enqueue([i]() {
            py::exec("print(f'Rapid Fire Task " + std::to_string(i) + "')");
          });
        }
      }

      // QA 3: Phoenix Test (Exception Recovery)
      if (ImGui::Button("Phoenix Test (QA3)")) {
        python_manager.Enqueue([]() {
          py::exec("raise Exception('Boom! (Intentional QA3 Error)')");
        });
      }

      // QA 4: Typography Check
      ImGui::Separator();
      ImGui::Text("QA4 Typography Check: 保存 💾 (Save) - 日米英+Icon");
      ImGui::End();

      // --- Node Graph Visualization (CP2.2) ---
      // Render graph if scanning is done (or even if empty, to show grid)
      // We will assume GraphVisualizer handles check internally or we check
      // here For now, let's always render to show the grid.
      visualizer.Render(registry);

      // レンダリング
      ImGui::Render();

      // Metal 描画パスの設定 (Moved up)

      id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
      id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer
          renderCommandEncoderWithDescriptor:renderPassDescriptor];
      [renderEncoder pushDebugGroup:@"ImGui Mesh"];

      ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer,
                                     renderEncoder);

      [renderEncoder popDebugGroup];
      [renderEncoder endEncoding];
      [commandBuffer presentDrawable:drawable];
      [commandBuffer commit];
    }

  } // End of Run Loop

  // 5. 終了処理
  // ---------------------------------------------------------
  visualizer.Shutdown();
  ImGui_ImplMetal_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
