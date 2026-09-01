#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "kyna/execution/tree_walk_engine.hpp"

namespace kyna::detail {

std::pair<ArrayPtr, FunctionPtr> arrayAndFunction(const std::vector<Value> &arguments,
                                                  const std::string &name);

std::vector<Value> elementArguments(const FunctionPtr &function, const Value &value,
                                    std::size_t index);

std::shared_ptr<Function> makeMapOp(Interpreter &interpreter);
std::shared_ptr<Function> makeReduceOp(Interpreter &interpreter);
std::shared_ptr<Function> makeFindOp(Interpreter &interpreter);
std::shared_ptr<Function> makeAnyOp(Interpreter &interpreter);
std::shared_ptr<Function> makeAllOp(Interpreter &interpreter);
std::shared_ptr<Function> makeUniqueOp(Interpreter &interpreter);

} // namespace kyna::detail
