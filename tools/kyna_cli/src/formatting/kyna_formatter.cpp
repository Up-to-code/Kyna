#include "kyna_formatter.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace kyna::cli {
namespace {
std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r");
  return value.substr(first, last - first + 1);
}

struct LineParts { std::string code; std::string comment; };
LineParts splitComment(std::string_view line) {
  char quote = 0; bool escaped = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (quote) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == quote) quote = 0;
    } else if (c == '"' || c == '\'') quote = c;
    else if (c == '#') return {trim(std::string(line.substr(0, i))), std::string(line.substr(i))};
  }
  return {trim(std::string(line)), {}};
}

bool word(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
std::vector<std::string> tokens(std::string_view code) {
  std::vector<std::string> result;
  for (std::size_t i = 0; i < code.size();) {
    if (std::isspace(static_cast<unsigned char>(code[i]))) { ++i; continue; }
    if (word(code[i])) {
      const auto start = i++; while (i < code.size() && word(code[i])) ++i;
      result.emplace_back(code.substr(start, i - start)); continue;
    }
    if (code[i] == '"' || code[i] == '\'') {
      const auto start = i; const char quote = code[i++]; bool escaped = false;
      while (i < code.size()) {
        const char c = code[i++];
        if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == quote) break;
      }
      result.emplace_back(code.substr(start, i - start)); continue;
    }
    if (i + 1 < code.size()) {
      const auto pair = code.substr(i, 2);
      if (pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" || pair == "&&" ||
          pair == "||" || pair == "->" || pair == "=>" || pair == "+=" || pair == "-=" ||
          pair == "*=" || pair == "/=") {
        result.emplace_back(pair); i += 2; continue;
      }
    }
    result.emplace_back(1, code[i++]);
  }
  return result;
}

bool noSpaceBefore(std::string_view token) {
  return token == "," || token == ";" || token == ")" || token == "]" || token == "." || token == ":";
}
bool noSpaceAfter(std::string_view token) { return token == "(" || token == "[" || token == "."; }
bool isOperator(std::string_view token) {
  static constexpr std::string_view ops[] = {"=", "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "->", "=>", "+=", "-=", "*=", "/="};
  return std::find(std::begin(ops), std::end(ops), token) != std::end(ops);
}
std::string formatCode(std::string_view code) {
  const auto parts = tokens(code); std::string result;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const auto &token = parts[i];
    bool space = !result.empty() && !noSpaceBefore(token) && !noSpaceAfter(parts[i - 1]);
    if (token == "(" && i > 0) {
      const auto &previous = parts[i - 1];
      space = previous == "if" || previous == "while" || previous == "for" || previous == "switch" || previous == "catch";
    }
    if (token == "{" || (i > 0 && parts[i - 1] == "}")) space = !result.empty();
    if (token == ":") space = false;
    if (i > 0 && parts[i - 1] == ":") space = true;
    if (isOperator(token) || (i > 0 && isOperator(parts[i - 1]))) space = !result.empty();
    if (space && result.back() != ' ') result.push_back(' ');
    result += token;
  }
  return result;
}

int braceDelta(std::string_view code) {
  int delta = 0; char quote = 0; bool escaped = false;
  for (char c : code) {
    if (quote) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == quote) quote = 0; }
    else if (c == '"' || c == '\'') quote = c;
    else if (c == '{') ++delta; else if (c == '}') --delta;
  }
  return delta;
}
} // namespace

FormatResult formatKyna(std::string_view source) {
  std::istringstream input{std::string(source)}; std::ostringstream output;
  std::string line; int indent = 0; bool previousBlank = false;
  while (std::getline(input, line)) {
    auto parts = splitComment(line); const auto code = formatCode(parts.code);
    if (code.empty() && parts.comment.empty()) {
      if (!previousBlank) output << '\n'; previousBlank = true; continue;
    }
    previousBlank = false;
    if (!code.empty() && code.front() == '}') indent = std::max(0, indent - 1);
    output << std::string(static_cast<std::size_t>(indent * 4), ' ') << code;
    if (!parts.comment.empty()) {
      if (!code.empty()) output << "  ";
      output << parts.comment;
    }
    output << '\n';
    int delta = braceDelta(code);
    if (!code.empty() && code.front() == '}' && delta < 0) ++delta;
    indent = std::max(0, indent + delta);
  }
  auto text = output.str();
  while (text.size() > 1 && text.ends_with("\n\n\n")) text.pop_back();
  if (text.empty() || text.back() != '\n') text.push_back('\n');
  return {std::move(text), {}};
}
} // namespace kyna::cli
