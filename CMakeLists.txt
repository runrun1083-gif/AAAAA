cmake_minimum_required(VERSION 3.18)

# プラットフォームに応じて言語を設定
if(APPLE)
    project(AIDevStation VERSION 0.1.0 LANGUAGES CXX OBJCXX)
else()
    project(AIDevStation VERSION 0.1.0 LANGUAGES CXX)
endif()

# ==============================================================================
# AI開発ステーション - 本格実装
# ==============================================================================
# アーキテクチャ:
#   - GUI: Dear ImGui + imnodes (C++ネイティブ)
#   - ECS: EnTT (ノード管理)
#   - AI: MLX C++ API (Python経由なし)
#   - 描画: Metal (macOS) / Vulkan (Linux)
# ==============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# プラットフォーム最適化
# ==============================================================================
if(APPLE)
    # Apple Silicon (M1/M2/M3) 最適化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        message(STATUS "✓ Apple Silicon 検出: ネイティブ最適化を有効化")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-m1")
    endif()

    # macOS 固有の設定
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc")

    # Metal フレームワーク
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(METALKIT_LIBRARY MetalKit REQUIRED)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)

    message(STATUS "✓ Metal フレームワーク検出")

elseif(UNIX)
    # Linux 最適化
    message(STATUS "✓ Linux 検出: ネイティブ最適化を有効化")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

    # OpenGL の検出（オプショナル）
    find_package(OpenGL)
    if(OpenGL_FOUND)
        message(STATUS "✓ OpenGL 検出")
        set(HAVE_OPENGL TRUE)
    else()
        message(WARNING "⚠ OpenGL が見つかりません。GUIは動作しません。")
        message(WARNING "  インストール: sudo apt install libgl1-mesa-dev libglu1-mesa-dev")
        set(HAVE_OPENGL FALSE)
    endif()
endif()

# 共通最適化フラグ
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# ==============================================================================
# 外部ライブラリの取得 (FetchContent)
# ==============================================================================
include(FetchContent)

# Dear ImGui
message(STATUS "Dear ImGui を取得中...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.7
)
FetchContent_MakeAvailable(imgui)

# imnodes
message(STATUS "imnodes を取得中...")
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG v0.5
)
FetchContent_MakeAvailable(imnodes)

# EnTT
message(STATUS "EnTT を取得中...")
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)
FetchContent_MakeAvailable(entt)

# GLFW (ウィンドウ管理)
message(STATUS "GLFW を取得中...")
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ==============================================================================
# Dear ImGui ライブラリの構築
# ==============================================================================
set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# Metal または OpenGL バックエンド
if(APPLE)
    target_sources(imgui PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_metal.mm
    )
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_METAL_CPP)
else()
    target_sources(imgui PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw)

if(APPLE)
    target_link_libraries(imgui PUBLIC
        ${METAL_LIBRARY}
        ${METALKIT_LIBRARY}
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${QUARTZCORE_LIBRARY}
    )
elseif(HAVE_OPENGL)
    target_link_libraries(imgui PUBLIC
        OpenGL::GL
    )
endif()

# ==============================================================================
# imnodes ライブラリの構築
# ==============================================================================
add_library(imnodes STATIC
    ${imnodes_SOURCE_DIR}/imnodes.cpp
)

target_include_directories(imnodes PUBLIC
    ${imnodes_SOURCE_DIR}
    ${IMGUI_DIR}
)

target_link_libraries(imnodes PUBLIC imgui)

# ==============================================================================
# MLX C++ API の検出
# ==============================================================================
# TODO: MLXのC++ APIが公式にリリースされたら統合
# 現在はスタブとして実装

# ==============================================================================
# ソースファイルの設定
# ==============================================================================
set(CORE_SOURCES
    core/Application.cpp
    core/NodeSystem.cpp
    core/MLXEngine.cpp
)

# macOS の場合は main.mm を使用、それ以外は main.cpp
if(APPLE)
    list(APPEND CORE_SOURCES core/main.mm)
else()
    list(APPEND CORE_SOURCES core/main.cpp)
endif()

# ==============================================================================
# 実行可能ファイルのビルド
# ==============================================================================
add_executable(ai_dev_station ${CORE_SOURCES})

target_include_directories(ai_dev_station PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${IMGUI_DIR}
    ${imnodes_SOURCE_DIR}
)

target_link_libraries(ai_dev_station PRIVATE
    imgui
    imnodes
    EnTT::EnTT
    glfw
)

if(APPLE)
    target_link_libraries(ai_dev_station PRIVATE
        ${METAL_LIBRARY}
        ${METALKIT_LIBRARY}
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${QUARTZCORE_LIBRARY}
    )
elseif(HAVE_OPENGL)
    target_link_libraries(ai_dev_station PRIVATE
        OpenGL::GL
    )
endif()

# ==============================================================================
# インストール設定
# ==============================================================================
install(TARGETS ai_dev_station DESTINATION bin)

# ==============================================================================
# ビルド情報の表示
# ==============================================================================
message(STATUS "==========================================")
message(STATUS "AI開発ステーション - ビルド設定完了")
message(STATUS "==========================================")
message(STATUS "プロジェクト: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "C++標準: C++${CMAKE_CXX_STANDARD}")
message(STATUS "ビルドタイプ: ${CMAKE_BUILD_TYPE}")
message(STATUS "コンパイラフラグ: ${CMAKE_CXX_FLAGS}")
message(STATUS "==========================================")
