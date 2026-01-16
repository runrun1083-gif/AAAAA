#include "GraphVisualizer.hpp"
#include "../models/NodeData.hpp"
#include <functional>
#include <imgui.h>
#include <iostream>

namespace aaaaa::core {

GraphVisualizer::GraphVisualizer() {}

GraphVisualizer::~GraphVisualizer() { Shutdown(); }

void GraphVisualizer::Init(float pixel_scale) {
  if (initialized_)
    return;

  context_ = ImNodes::CreateContext();
  ImNodes::SetNodeGridSpacePos(1, ImVec2(200.0f, 200.0f));

  // Professional Dark Style with Retina Scaling
  ImNodesStyle &style = ImNodes::GetStyle();

  // Scale all size-related style variables
  style.GridSpacing *= pixel_scale;
  style.NodeCornerRounding *= pixel_scale;
  style.NodePadding = ImVec2(style.NodePadding.x * pixel_scale,
                             style.NodePadding.y * pixel_scale);
  style.NodeBorderThickness *= pixel_scale;
  style.LinkThickness *= pixel_scale;
  style.LinkLineSegmentsPerLength *= pixel_scale;
  style.LinkHoverDistance *= pixel_scale;
  style.PinCircleRadius *= pixel_scale;
  style.PinQuadSideLength *= pixel_scale;
  style.PinTriangleSideLength *= pixel_scale;
  style.PinLineThickness *= pixel_scale;
  style.PinHoverRadius *= pixel_scale;
  style.PinOffset *= pixel_scale;
  style.MiniMapPadding = ImVec2(style.MiniMapPadding.x * pixel_scale,
                                style.MiniMapPadding.y * pixel_scale);
  style.MiniMapOffset = ImVec2(style.MiniMapOffset.x * pixel_scale,
                               style.MiniMapOffset.y * pixel_scale);

  // Colors (Dark & Modern)
  style.Colors[ImNodesCol_GridBackground] = IM_COL32(25, 25, 25, 255);
  style.Colors[ImNodesCol_GridLine] = IM_COL32(40, 40, 40, 255);
  style.Colors[ImNodesCol_TitleBar] = IM_COL32(40, 40, 40, 255);
  style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(60, 60, 60, 255);
  style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(80, 80, 80, 255);
  style.Colors[ImNodesCol_Link] = IM_COL32(100, 100, 255, 255);
  style.Colors[ImNodesCol_Pin] = IM_COL32(100, 100, 255, 255);
  style.Colors[ImNodesCol_PinHovered] = IM_COL32(150, 150, 255, 255);

  initialized_ = true;

  // Configure IO for Mac-friendly navigation
  ImNodesIO &io = ImNodes::GetIO();
  // io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyMeta; // Fix:
  // KeyMeta not found in some ImGui versions
  io.EmulateThreeButtonMouse.Modifier =
      nullptr; // Disable emulation to avoid conflict
}

void GraphVisualizer::Shutdown() {
  if (context_) {
    ImNodes::DestroyContext(context_);
    context_ = nullptr;
  }
  initialized_ = false;
}

ImVec2 GraphVisualizer::GetInitialPosition(const std::filesystem::path &path) {
  // Logic moved to Render for Global Grid
  return ImVec2(0, 0);
}

// Helper to extract filename stem (no extension)
static std::string GetStem(const std::string &filename) {
  size_t lastindex = filename.find_last_of(".");
  if (lastindex == std::string::npos)
    return filename;
  return filename.substr(0, lastindex);
}

void GraphVisualizer::UpdateGraph(entt::registry &registry) {
  // Logic is independent of ImNodes/GUI context, so we allow it to run
  // always.

  // 1. Build Reverse Lookup Caches
  reverse_lookup_.clear();
  filename_lookup_.clear();

  entity_to_id_.clear();
  id_to_entity_.clear();

  auto view = registry.view<models::NodeInfo>();
  int next_id = 0;
  for (auto entity : view) {
    // Assign Safe ID
    entity_to_id_[entity] = next_id;
    id_to_entity_.push_back(entity);
    next_id++;

    const auto &info = view.get<models::NodeInfo>(entity);
    reverse_lookup_[info.path] = entity;

    // Heuristic 1: Exact Filename ("utils.py" -> Entity)
    filename_lookup_[info.label] = entity;

    // Heuristic 2: Stem ("utils" -> Entity) - Warning: Overwrites duplicates,
    // acceptable for visualization
    filename_lookup_[GetStem(info.label)] = entity;
  }

  // 2. Pre-calculate Links (O(1) Lookup)
  links_cache_.clear();
  auto ast_view = registry.view<models::ASTData>();

  int link_counter = 0;

  for (auto entity : ast_view) {
    const auto &ast = ast_view.get<models::ASTData>(entity);
    // Safe ID used inside loop
    const auto &info = registry.get<models::NodeInfo>(entity);

    for (const auto &import_str : ast.imports) {
      entt::entity target_entity = entt::null;

      // Lookup Strategy (O(1) Map)

      // Attempt 1: Stem match (import "utils" -> matches "utils" key which
      // points to "utils.py") We need to handle paths like "core.utils" ->
      // "utils"
      std::filesystem::path import_path(import_str);
      std::string key = import_path.filename().string(); // "utils"

      if (filename_lookup_.find(key) != filename_lookup_.end()) {
        target_entity = filename_lookup_[key];
      }
      // Attempt 2: Exact check (if import has extension)
      else if (filename_lookup_.find(import_str) != filename_lookup_.end()) {
        target_entity = filename_lookup_[import_str];
      }

      if (target_entity != entt::null && target_entity != entity) {
        // Use Safe IDs
        if (entity_to_id_.find(entity) == entity_to_id_.end() ||
            entity_to_id_.find(target_entity) == entity_to_id_.end()) {
          continue;
        }

        int src_id = entity_to_id_[entity];
        int dst_id = entity_to_id_[target_entity];

        // Color Logic
        // Default: Blue (Python-like)
        // C++: Orange/Red
        unsigned int color = IM_COL32(100, 100, 255, 255);
        if (info.type == models::NodeType::CppSource ||
            info.type == models::NodeType::CppHeader) {
          color = IM_COL32(255, 100, 100, 255);
        }

        links_cache_.push_back({link_counter++, src_id, dst_id, false, color});
      }
    }
  }

  // 3. Cycle Detection
  DetectCycles();

  int cycle_count = 0;
  for (const auto &link : links_cache_) {
    if (link.is_cycle)
      cycle_count++;
  }

  std::cout << "[GraphVisualizer] Graph Updated. Nodes: "
            << reverse_lookup_.size() << ", Links: " << links_cache_.size()
            << ", Cycles: " << cycle_count << std::endl;
}

void GraphVisualizer::DetectCycles() {
  // DFS for Cycle Detection
  std::unordered_map<int, std::vector<int>> adj;
  for (size_t i = 0; i < links_cache_.size(); ++i) {
    adj[links_cache_[i].src].push_back(i); // Store index to links_cache_
  }

  std::unordered_set<int> visited;
  std::unordered_set<int> recursion_stack;

  // Recursive Lambda
  std::function<void(int)> dfs = [&](int u) {
    visited.insert(u);
    recursion_stack.insert(u);

    for (int link_idx : adj[u]) {
      int v = links_cache_[link_idx].dst;
      if (recursion_stack.count(v)) {
        // Cycle Detected!
        links_cache_[link_idx].is_cycle = true;
        links_cache_[link_idx].color = IM_COL32(255, 0, 0, 255); // Red Alert
      } else if (!visited.count(v)) {
        dfs(v);
      }
    }

    recursion_stack.erase(u);
  };

  // Run DFS from each node (using Safe IDs as index)
  for (const auto &[path, entity] : reverse_lookup_) {
    if (entity_to_id_.find(entity) == entity_to_id_.end())
      continue;
    int id = entity_to_id_[entity];
    if (id < 0)
      continue; // Should not happen

    if (!visited.count(id)) {
      dfs(id);
    }
  }
}

void GraphVisualizer::Render(entt::registry &registry) {
  if (!initialized_)
    return;

  ImNodes::BeginNodeEditor();

  // 1. Draw Nodes
  auto view = registry.view<models::NodeInfo>();
  for (auto entity : view) {
    const auto &info = view.get<models::NodeInfo>(entity);
    // int id = (int)entt::to_integral(entity); // Unsafe
    if (entity_to_id_.find(entity) == entity_to_id_.end())
      continue;
    int id = entity_to_id_[entity]; // Safe ID

    if (position_set_.find(entity) == position_set_.end()) {
      // FORCE GLOBAL GRID LAYOUT (Rectangular)
      int columns = 5; // 5 Columns
      float x = (id % columns) * 400.0f + 100.0f;
      float y = (id / columns) * 250.0f + 100.0f;

      ImNodes::SetNodeGridSpacePos(id, ImVec2(x, y));
      position_set_[entity] = true;
    }

    // Color Coding
    unsigned int title_color = IM_COL32(40, 100, 40, 255);
    if (info.type == models::NodeType::Python)
      title_color = IM_COL32(40, 40, 180, 255);
    else if (info.type == models::NodeType::CppSource)
      title_color = IM_COL32(180, 40, 40, 255);

    ImNodes::PushColorStyle(ImNodesCol_TitleBar, title_color);

    ImNodes::BeginNode(id);
    ImNodes::BeginNodeTitleBar();
    ImGui::Text("%s", info.label.c_str());
    ImNodes::EndNodeTitleBar();

    ImNodes::BeginInputAttribute((id << 8) | 0);
    ImNodes::EndInputAttribute();

    // Display Line Count if available (using ASTData)
    if (registry.all_of<models::ASTData>(entity)) {
      int lines = registry.get<models::ASTData>(entity).line_count;
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lines: %d", lines);
    }

    ImNodes::BeginOutputAttribute((id << 8) | 1);
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();
    ImNodes::PopColorStyle();
  }

  // 2. Draw Links (Cached with Colors)
  for (const auto &link : links_cache_) {
    ImNodes::PushColorStyle(ImNodesCol_Link, link.color);
    ImNodes::Link(link.id, (link.src << 8) | 1, (link.dst << 8) | 0);
    ImNodes::PopColorStyle();
  }

  ImNodes::EndNodeEditor();

  // --- Custom Navigation Logic (MacOS Style) ---
  bool is_editor_hovered = ImNodes::IsEditorHovered();
  ImGuiIO &io = ImGui::GetIO();

  // 1. Two-Finger Scroll (MouseWheel) -> PANNING (Not Zoom)
  // This gives the "Background move" feel with trackpad.
  // We only Zoom if CMD/Ctrl is held.
  if (is_editor_hovered) {
    bool is_zoom_key = io.KeySuper || io.KeyCtrl; // Cmd or Ctrl

    if (!is_zoom_key && (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)) {
      ImVec2 current_pan = ImNodes::EditorContextGetPanning();
      // Adjust sensitivity
      current_pan.x += io.MouseWheelH * 20.0f;
      current_pan.y += io.MouseWheel * 20.0f;
      ImNodes::EditorContextResetPanning(current_pan);
    }

    // Note: ImNodes handles Zoom with Ctrl+Scroll by default usually.
  }

  // 2. Pan with Right Click Drag (Standard Alternative)
  // 3. Pan with Left Click Drag (User Request - Conflicts with Box Select?)
  // We allow both.
  if (is_editor_hovered &&
      (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) ||
       ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))) {

    int hovered_node = -1;
    int hovered_link = -1;
    int hovered_pin = -1;

    bool node_hov = ImNodes::IsNodeHovered(&hovered_node);
    bool link_hov = ImNodes::IsLinkHovered(&hovered_link);
    bool pin_hov = ImNodes::IsPinHovered(&hovered_pin);

    // If dragging background
    if (!node_hov && !link_hov && !pin_hov) {
      ImVec2 delta = io.MouseDelta;
      ImVec2 current_pan = ImNodes::EditorContextGetPanning();
      current_pan.x += delta.x;
      current_pan.y += delta.y;
      ImNodes::EditorContextResetPanning(current_pan);
    }
  }
}

} // namespace aaaaa::core
