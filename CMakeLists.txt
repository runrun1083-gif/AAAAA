cmake_minimum_required(VERSION 3.20)
project(AAAAA_Core LANGUAGES CXX OBJCXX)

# M1 Mac Optimization
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-m1 -O3")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# Enable ImGui Math Operators (Required for ImNodes)
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# --- Python Environment Discovery (Robust) ---
if(NOT DEFINED ENV{VIRTUAL_ENV})
    # Try to find .venv in project root or parent
    find_file(VENV_ROOT_DIR NAMES ".venv" PATHS ${CMAKE_CURRENT_SOURCE_DIR} NO_DEFAULT_PATH)
    if(VENV_ROOT_DIR)
        set(ENV{VIRTUAL_ENV} "${VENV_ROOT_DIR}")
        message(STATUS "Auto-detected Virtual Environment: ${VENV_ROOT_DIR}")
    else()
        message(WARNING "VIRTUAL_ENV not set and .venv not found. Python embedding might fail.")
    endif()
endif()

# 仮想環境のパスを明示的に定義
set(VENV_PATH "${CMAKE_SOURCE_DIR}/.venv")

# システム標準ではなく、仮想環境内のPythonを最優先で探させる
set(Python3_EXECUTABLE "${VENV_PATH}/bin/python3")
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)

# pybind11も仮想環境内から探すように指定
set(pybind11_DIR "${VENV_PATH}/lib/python3.12/site-packages/pybind11/share/cmake/pybind11")
find_package(pybind11 REQUIRED)


# --- Dear ImGui & Backend (FetchContent) ---
include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        docking  # Use docking branch for modern features
)
FetchContent_MakeAvailable(imgui)

# --- EnTT (Entity Component System) ---
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.13.0
)
FetchContent_MakeAvailable(entt)

# --- ImNodes (Node Graph Visualization) ---
FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG        master
)
FetchContent_GetProperties(imnodes)
if(NOT imnodes_POPULATED)
  FetchContent_Populate(imnodes)
endif()

message(STATUS "IMGUI_SOURCE_DIR: ${imgui_SOURCE_DIR}")
message(STATUS "IMNODES_SOURCE_DIR: ${imnodes_SOURCE_DIR}")
message(STATUS "ENTT_SOURCE_DIR: ${entt_SOURCE_DIR}")

# --- Main Executable ---
add_executable(core_app 
    core/main.mm
    core/PythonManager.cpp
    core/FileScanner.cpp
    core/GraphVisualizer.cpp
    ${imnodes_SOURCE_DIR}/imnodes.cpp
    # ImGui Sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
)

# Find GUI Dependencies
find_package(glfw3 REQUIRED)
find_library(METAL_LIB Metal)
find_library(COCOA_LIB Cocoa)
find_library(IOKIT_LIB IOKit)
find_library(COREVIDEO_LIB CoreVideo)
find_library(QUARTZCORE_LIB QuartzCore)

target_include_directories(core_app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VENV_PATH}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imnodes_SOURCE_DIR}
    ${entt_SOURCE_DIR}/src
)

target_link_libraries(core_app PRIVATE 
    EnTT::EnTT
    glfw
    pybind11::embed  # Restore this!
    ${PYTHON_LIBRARIES}
    "-framework Metal"
    ${METAL_LIB}
    ${COCOA_LIB}
    ${IOKIT_LIB}
    ${COREVIDEO_LIB}
    ${QUARTZCORE_LIB}
)

# No post-build command to avoid shell escaping issues
