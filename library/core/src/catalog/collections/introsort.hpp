#pragma once

#include <functional>
#include <vector>

#include <kyna/execution/runtime_object_model.hpp>

namespace kyna::detail {

// Sorts `elements` in place using an O(n log n) introsort (quicksort that
// degrades to heapsort on deep recursion, with insertion sort for small
// partitions). `less(a, b)` must provide a strict weak ordering: it returns
// true when `a` should appear before `b`. This is the standard-library sort
// core used by the tree-walk runtime.
void introsort(std::vector<Value> &elements,
               const std::function<bool(const Value &, const Value &)> &less);

} // namespace kyna::detail
