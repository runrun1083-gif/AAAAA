#pragma once
#include "../models/NodeData.hpp"
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace aaaaa::core {

class ASTParser {
public:
  // Zero-Copy Parsing Logic
  // Eliminates std::stringstream and intermediate allocations.
  // Uses raw pointers and std::string_view for maximum throughput.
  static std::vector<std::string> ParseImports(std::string_view content,
                                               models::NodeType type) {
    std::vector<std::string> imports;
    imports.reserve(16); // Pre-allocate optimization

    const char *cursor = content.data();
    const char *end = cursor + content.size();

    while (cursor < end) {
      // 1. Find end of current line
      // memchr is highly optimized (SIMD on many platforms)
      const char *eol =
          static_cast<const char *>(std::memchr(cursor, '\n', end - cursor));
      const char *next_cursor = eol ? eol + 1 : end;
      size_t line_len = eol ? (eol - cursor) : (end - cursor);

      // 2. Skip leading whitespace (manual pointer advancement is faster than
      // string logic)
      const char *line_start = cursor;
      const char *line_end = cursor + line_len;

      while (line_start < line_end &&
             (*line_start == ' ' || *line_start == '\t')) {
        line_start++;
      }

      if (line_start == line_end) {
        cursor = next_cursor;
        continue; // Empty line
      }

      // 3. Create view for the trimmed line
      std::string_view line(line_start, line_end - line_start);

      // 4. Parse based on type
      if (type == models::NodeType::Python) {
        // "import ..."
        if (line.starts_with("import ")) {
          std::string_view rest = line.substr(7); // Skip "import "
          // Extract first module: "import os.path" -> "os.path"
          size_t stop = rest.find_first_of(",#");
          // Note: Python allows "import a, b". Simplified: grab first.
          // For "Instant" visualizer, strict grammar parsing is overkill.

          if (stop == std::string_view::npos)
            imports.emplace_back(rest);
          else {
            // Trim trailing spaces before the comma/comment
            std::string_view mod = rest.substr(0, stop);
            while (!mod.empty() && (mod.back() == ' ' || mod.back() == '\t'))
              mod.remove_suffix(1);
            imports.emplace_back(mod);
          }
        }
        // "from ..."
        else if (line.starts_with("from ")) {
          std::string_view rest = line.substr(5); // Skip "from "
          size_t import_kw = rest.find(" import");
          if (import_kw != std::string_view::npos) {
            imports.emplace_back(rest.substr(0, import_kw));
          }
        }
      } else if (type == models::NodeType::CppSource ||
                 type == models::NodeType::CppHeader) {
        // "#include ..."
        if (line.starts_with("#include")) {
          std::string_view rest = line.substr(8);
          // Skip spaces after #include
          size_t start_quote = rest.find_first_not_of(" \t");
          if (start_quote != std::string_view::npos) {
            char quote_char = rest[start_quote];
            if (quote_char == '<' || quote_char == '"') {
              char close_char = (quote_char == '<') ? '>' : '"';
              size_t end_quote = rest.find(close_char, start_quote + 1);
              if (end_quote != std::string_view::npos) {
                imports.emplace_back(
                    rest.substr(start_quote + 1, end_quote - start_quote - 1));
              }
            }
          }
        }
      }

      cursor = next_cursor;
    }

    return imports;
  }
};

} // namespace aaaaa::core
