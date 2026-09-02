#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace kyna::semantics {

// Resolution state for the three-color (White/Grey/Black) cycle detector. A
// node is White until it is visited, Grey while its dependencies are being
// explored, and Black once fully resolved. Re-entering a Grey node is a cycle.
enum class Color : uint8_t {
  White,
  Grey,
  Black,
};

// A constraints helper that detects cycles in a directed dependency graph by
// name (e.g. class inheritance or type-alias expansion) without re-running a
// full DFS from scratch. `advance(name, dependencies)` returns the name that
// completes a cycle, or an empty string when the node is acyclic.
class CycleDetector {
public:
  // Records that `name` depends on `dependencies`; returns the name of the
  // node whose edge closes a cycle (typically a dependency already in the
  // Grey set), or an empty string when no cycle is found.
  std::string advance(const std::string &name, const std::vector<std::string> &dependencies);

  // Marks `name` (and transitively-satisfied dependencies) as fully resolved.
  // Call after all of a node's dependencies have been advanced successfully.
  void finish(const std::string &name);

  // True when `name` is still being resolved (Grey).
  bool isResolving(const std::string &name) const;

private:
  void markBlack(const std::string &name);

  std::unordered_map<std::string, Color> colors_;
};

} // namespace kyna::semantics
