#pragma once

#include "kyna/semantics/symbol.hpp"
#include "kyna/source/source_span.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace kyna::semantics {

// A lexical scope: an ordered collection of declared symbols with an optional
// parent scope. Children form a tree that mirrors the nesting of blocks,
// functions, and classes so lookup can walk the parent chain in declaration
// order.
class Scope {
public:
  explicit Scope(Scope *parent = nullptr) : parent_(parent) {}

  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;
  ~Scope() = default;

  Scope *parent() const { return parent_; }
  Scope *createChild(SourceSpan extent = {}) {
    auto child = std::make_unique<Scope>(this);
    child->extent_ = extent;
    auto *raw = child.get();
    children_.push_back(std::move(child));
    return raw;
  }

  // Inserts `symbol` into this scope. Returns the previously existing symbol
  // (which the caller uses to emit a duplicate-declaration diagnostic), or
  // nullptr when the name was previously undeclared in this scope.
  SymbolPtr insert(SymbolPtr symbol);

  // Looks up `name`, searching this scope then walking up the parent chain.
  // Returns nullptr if the name is not in scope at all.
  SymbolPtr lookup(const std::string &name) const;

  // Looks up `name` only in the immediate scope, without walking parents.
  SymbolPtr lookupLocal(const std::string &name) const;

  // The number of symbols declared directly in this scope.
  std::size_t localCount() const { return symbols_.size(); }

private:
  Scope *parent_;
  std::vector<std::unique_ptr<Scope>> children_;
  std::unordered_map<std::string, SymbolPtr> symbols_;
  SourceSpan extent_;
};

} // namespace kyna::semantics
