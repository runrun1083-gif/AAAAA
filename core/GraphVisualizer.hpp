#pragma once

#include <entt/entt.hpp>
#include <filesystem>
#include <imgui.h>
#include <imnodes.h>
#include <string>
#include <unordered_map>

namespace aaaaa {
namespace core {

class GraphVisualizer {
public:
  GraphVisualizer();
  ~GraphVisualizer();

  void Init(float pixel_scale = 1.0f);
  void UpdateGraph(entt::registry &registry); // Pre-calculate links
  void Render(entt::registry &registry);
  void Shutdown();

  struct LinkData {
    int id;
    int src;
    int dst;
    bool is_cycle;
    unsigned int color; // ImU32
  };

private:
  ImNodesContext *context_ = nullptr;
  bool initialized_ = false;

  // Layout state
  std::unordered_map<std::string, ImVec2> directory_positions_;
  std::unordered_map<entt::entity, bool> position_set_;

  // Entity -> UI ID Map (Safe small ints)
  std::unordered_map<entt::entity, int> entity_to_id_;
  std::vector<entt::entity> id_to_entity_;

  // Optimization: Reverse Lookup & Link Cache
  // Path -> Entity ID (Exact Match)
  std::unordered_map<std::string, entt::entity> reverse_lookup_;
  // Filename -> Entity ID (Heuristic Match for fast O(1))
  std::unordered_map<std::string, entt::entity> filename_lookup_;

  // Cached list of links
  std::vector<LinkData> links_cache_;

  // Cycle Detection
  void DetectCycles();

  // Helper to determine initial node position based on directory structure
  ImVec2 GetInitialPosition(const std::filesystem::path &path);
};

} // namespace core
} // namespace aaaaa
