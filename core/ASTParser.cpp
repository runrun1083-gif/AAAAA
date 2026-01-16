/**
 * Module: core/ASTParser.cpp
 */

#include "ASTParser.hpp"
#include <sstream>

namespace aaaaa {
namespace core {

ParseResult ASTParser::Parse(std::string_view source) {
  ParseResult result;

  // 行ごとに分割して解析
  // Note: C++20以前では string_view の split は標準ではないため、自前で回す
  size_t start = 0;
  size_t end = source.find('\n');

  while (end != std::string_view::npos) {
    std::string_view line = source.substr(start, end - start);
    ParseLine(line, result);
    start = end + 1;
    end = source.find('\n', start);
  }

  // 最後の行
  if (start < source.length()) {
    ParseLine(source.substr(start), result);
  }

  return result;
}

size_t ASTParser::SkipWhitespace(std::string_view sv, size_t pos) {
  while (pos < sv.length() && (sv[pos] == ' ' || sv[pos] == '\t')) {
    pos++;
  }
  return pos;
}

void ASTParser::ParseLine(std::string_view line, ParseResult &result) {
  // コメント除去 (# 以降は無視)
  size_t comment_pos = line.find('#');
  if (comment_pos != std::string_view::npos) {
    line = line.substr(0, comment_pos);
  }

  // 先頭の空白スキップ
  size_t pos = SkipWhitespace(line, 0);
  if (pos >= line.length())
    return;

  std::string_view trimmed = line.substr(pos);

  // "import " で始まるか
  if (trimmed.starts_with("import ")) {
    // 例: "import numpy", "import os, sys"
    // 簡易実装:
    // 最初のモジュール名だけ取る（カンマ区切り対応は複雑になるためCP2初期では省略）
    std::string_view content = trimmed.substr(7); // "import " の長さ
    size_t next_space = SkipWhitespace(content, 0);
    content = content.substr(next_space);

    // モジュール名の終端を探す (スペース、カンマ、改行のいずれか)
    size_t end_token = content.find_first_of(" ,\r\n");
    if (end_token == std::string_view::npos)
      end_token = content.length();

    if (end_token > 0) {
      result.imports.emplace_back(content.substr(0, end_token));
    }
  }
  // "from " で始まるか
  else if (trimmed.starts_with("from ")) {
    // 例: "from numpy import array"
    std::string_view content = trimmed.substr(5); // "from " の長さ
    size_t next_space = SkipWhitespace(content, 0);
    content = content.substr(next_space);

    // モジュール名抽出
    size_t mod_end = content.find_first_of(" ");
    if (mod_end != std::string_view::npos) {
      result.from_imports.emplace_back(content.substr(0, mod_end));
    }
  }
}

} // namespace core
} // namespace aaaaa
