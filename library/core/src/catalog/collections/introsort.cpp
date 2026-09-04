#include "introsort.hpp"

#include <cstddef>
#include <vector>

namespace kyna::detail {
namespace {

using Compare = std::function<bool(const Value &, const Value &)>;

constexpr std::size_t kInsertionThreshold = 16;

constexpr std::size_t kLog2Floor(std::size_t value) {
  std::size_t result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

void insertionSort(std::vector<Value> &elements, std::size_t begin, std::size_t end,
                   const Compare &less) {
  for (std::size_t i = begin + 1; i < end; ++i) {
    const Value pivot = elements[i];
    std::size_t pos = i;
    while (pos > begin && less(pivot, elements[pos - 1])) {
      elements[pos] = elements[pos - 1];
      --pos;
    }
    elements[pos] = pivot;
  }
}

void siftDown(std::vector<Value> &elements, std::size_t begin, std::size_t start,
              std::size_t count, const Compare &less) {
  std::size_t root = start;
  while (root * 2 + 1 <= count - 1) {
    std::size_t child = root * 2 + 1;
    if (child + 1 <= count - 1 && less(elements[begin + child], elements[begin + child + 1]))
      ++child;
    if (!less(elements[begin + root], elements[begin + child]))
      return;
    std::swap(elements[begin + root], elements[begin + child]);
    root = child;
  }
}

void heapSort(std::vector<Value> &elements, std::size_t begin, std::size_t count,
              const Compare &less) {
  for (std::size_t start = count / 2; start > 0; --start)
    siftDown(elements, begin, start - 1, count, less);
  for (std::size_t end = count; end > 1; --end) {
    std::swap(elements[begin], elements[begin + end - 1]);
    siftDown(elements, begin, 0, end - 1, less);
  }
}

void introspectiveSort(std::vector<Value> &elements, std::size_t begin, std::size_t end,
                       std::size_t depthLimit, const Compare &less) {
  const std::size_t count = end - begin;
  if (count <= 1)
    return;
  if (count <= kInsertionThreshold) {
    insertionSort(elements, begin, end, less);
    return;
  }
  if (depthLimit == 0) {
    heapSort(elements, begin, count, less);
    return;
  }

  // Median-of-three pivot selection (copied by value so it stays stable while
  // the partition swaps elements around it).
  const Value pivot =
      less(elements[begin + count / 2], elements[begin])
          ? (less(elements[begin], elements[end - 1]) ? elements[begin] : elements[end - 1])
          : (less(elements[begin + count / 2], elements[end - 1]) ? elements[begin + count / 2]
                                                                  : elements[end - 1]);

  std::size_t left = begin;
  std::size_t right = end - 1;
  while (true) {
    while (less(elements[left], pivot))
      ++left;
    while (less(pivot, elements[right]))
      --right;
    if (left >= right) {
      if (left == right)
        ++left;
      break;
    }
    std::swap(elements[left], elements[right]);
    ++left;
    --right;
  }

  introspectiveSort(elements, begin, left, depthLimit - 1, less);
  introspectiveSort(elements, left, end, depthLimit - 1, less);
}

} // namespace

void introsort(std::vector<Value> &elements,
               const std::function<bool(const Value &, const Value &)> &less) {
  if (elements.size() <= 1)
    return;
  const std::size_t depthLimit = 2 * kLog2Floor(elements.size());
  introspectiveSort(elements, 0, elements.size(), depthLimit, less);
}

} // namespace kyna::detail
