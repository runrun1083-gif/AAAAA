#include "FileScanner.hpp"
#include "ASTParser.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>

namespace aaaaa::core {

FileScanner::FileScanner(entt::registry &registry) : registry_(registry) {}

models::NodeType FileScanner::DetectType(const std::filesystem::path &path) {
  auto ext = path.extension().string();
  if (ext == ".py")
    return models::NodeType::Python;
  if (ext == ".cpp" || ext == ".mm" || ext == ".c" || ext == ".cc")
    return models::NodeType::CppSource;
  if (ext == ".hpp" || ext == ".h")
    return models::NodeType::CppHeader;
  if (ext == ".md")
    return models::NodeType::Markdown;
  return models::NodeType::Unknown;
}

ScanStats FileScanner::ScanProject(const std::filesystem::path &root_path) {
  ScanStats stats;
  auto start_time = std::chrono::high_resolution_clock::now();

  // 1. Clear previous NodeInfo entities
  auto view = registry_.view<models::NodeInfo>();
  registry_.destroy(view.begin(), view.end());

  std::vector<std::filesystem::path> all_files;
  all_files.reserve(1000);

  // 2. Iterate & Collect Paths (Single Threaded - IO Bound but fast on SSD)
  if (std::filesystem::exists(root_path)) {
    try {
      // Use explicit iterator to enable disable_recursion_pending()
      for (auto it = std::filesystem::recursive_directory_iterator(root_path);
           it != std::filesystem::recursive_directory_iterator(); ++it) {

        const auto &entry = *it;

        // Prune directories
        if (entry.is_directory()) {
          std::string dirname = entry.path().filename().string();
          if (dirname == ".venv" || dirname == "build" ||
              dirname == "__pycache__" || dirname.starts_with(".")) {
            it.disable_recursion_pending();
          }
          continue;
        }

        // File Processing
        auto path_str = entry.path().string();
        // (Double check if inside ignored dir, though pruning handles most)
        if (path_str.find("/.venv/") != std::string::npos)
          continue;

        // Pre-filter by extension
        if (DetectType(entry.path()) != models::NodeType::Unknown) {
          all_files.push_back(entry.path());
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Scan Error: " << e.what() << std::endl;
    }
  }

  stats.file_count = (int)all_files.size();
  std::mutex registry_mutex;

  // 3. Process
  // Threshold for parallelism. Thread creation has overhead (~20-50us +
  // scheduling). For small batch, sequential is strictly faster.
  if (all_files.size() < 64) {
    // Sequential Scan
    for (const auto &path : all_files) {
      auto type = DetectType(path);

      // Fast Read (Binary Mode)
      std::ifstream file(path, std::ios::binary | std::ios::ate);
      if (!file.is_open())
        continue;

      std::streamsize size = file.tellg();
      if (size <= 0)
        continue; // Empty file

      file.seekg(0, std::ios::beg);

      std::string content;
      content.resize(size);
      if (!file.read(content.data(), size))
        continue;

      // Stats
      // Fast Line Count (std::count is optimized)
      int lines = (int)std::count(content.begin(), content.end(), '\n') + 1;

      // Parse (Zero-Copy internal)
      // content is valid here, passes string_view
      auto imports = ASTParser::ParseImports(content, type);

      // Store (No lock needed)
      auto entity = registry_.create();
      auto p_str = path.string();
      auto hashed_id = entt::hashed_string(p_str.c_str());

      registry_.emplace<models::NodeInfo>(entity, hashed_id.value(), p_str,
                                          path.filename().string(), type);
      registry_.emplace<models::ASTData>(entity, std::move(imports), lines);
    }
  } else {
    // Parallel Scan (Chunked)
    size_t hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0)
      hardware_threads = 4;

    size_t chunk_size =
        (all_files.size() + hardware_threads - 1) / hardware_threads;
    std::vector<std::future<void>> futures;

    for (size_t t = 0; t < hardware_threads; ++t) {
      size_t start_idx = t * chunk_size;
      size_t end_idx = std::min(start_idx + chunk_size, all_files.size());

      if (start_idx >= end_idx)
        break;

      futures.emplace_back(std::async(std::launch::async, [this, &all_files,
                                                           start_idx, end_idx,
                                                           &registry_mutex]() {
        for (size_t i = start_idx; i < end_idx; ++i) {
          const auto &path = all_files[i];
          auto type = DetectType(path);

          std::ifstream file(path, std::ios::binary | std::ios::ate);
          if (!file.is_open())
            continue;

          std::streamsize size = file.tellg();
          if (size < 0)
            size = 0; // Handle errors
          file.seekg(0, std::ios::beg);

          std::string content;
          content.resize(size);
          file.read(content.data(), size);

          int lines = (int)std::count(content.begin(), content.end(), '\n') + 1;

          auto imports = ASTParser::ParseImports(content, type);

          {
            std::lock_guard<std::mutex> lock(registry_mutex);
            auto entity = registry_.create();
            auto p_str = path.string();
            auto hashed_id = entt::hashed_string(p_str.c_str());

            registry_.emplace<models::NodeInfo>(entity, hashed_id.value(),
                                                p_str, path.filename().string(),
                                                type);
            registry_.emplace<models::ASTData>(entity, std::move(imports),
                                               lines);
          }
        }
      }));
    }

    for (auto &f : futures) {
      f.get();
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  stats.duration_ms =
      std::chrono::duration<double, std::milli>(end_time - start_time).count();

  return stats;
}

} // namespace aaaaa::core
