#pragma once
#include "../models/NodeData.hpp"
#include <entt/entt.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace aaaaa::core {

struct ScanStats {
  int file_count = 0;
  double duration_ms = 0.0;
};

class FileScanner {
public:
  explicit FileScanner(entt::registry &registry);

  // Scans the project root recursively and populates the registry.
  // Returns statistics about the scan.
  ScanStats ScanProject(const std::filesystem::path &root_path);

private:
  entt::registry &registry_;

  // Helper to determine node type from extension
  models::NodeType DetectType(const std::filesystem::path &path);
};

} // namespace aaaaa::core
