#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace kyna {
struct Object;
struct Array;
struct Value;
struct VmCaptureCell;
struct VmClosure;
struct VmBoundMethod;
struct ErrorObject;
class Environment;

struct HeapStats {
  std::size_t live{0};
  std::size_t allocated{0};
  std::size_t reclaimed{0};
  std::size_t collections{0};
  std::size_t peakLive{0};
  std::size_t nextThreshold{0};
};

struct HeapRoots {
  std::vector<Environment *> environments;
  std::vector<const Value *> values;
  std::vector<VmCaptureCell *> captureCells;
};

// Tracing heap for runtime objects. Object fields are non-owning Value edges;
// the heap owns object storage and collects unreachable cycles automatically.
class Heap {
public:
  class RootScope {
  public:
    RootScope(const RootScope &) = delete;
    RootScope &operator=(const RootScope &) = delete;
    RootScope(RootScope &&) = delete;
    RootScope &operator=(RootScope &&) = delete;
    ~RootScope();
    void protect(const Value &value);

  private:
    friend class Heap;
    explicit RootScope(Heap &owner);
    Heap &heap;
    std::size_t firstRoot;
  };

  Heap() = default;
  ~Heap();
  Object *allocate();
  Array *allocateArray();
  VmCaptureCell *allocateCaptureCell(const Value &value);
  VmClosure *allocateClosure(std::uint32_t function,
                             std::vector<VmCaptureCell *> captures);
  VmBoundMethod *allocateBoundMethod(Object *receiver, std::uint32_t function);
  ErrorObject *allocateError(std::string message, std::string code, const Value &cause);
  void collect(const std::vector<Environment *> &roots);
  void collectRoots(const HeapRoots &roots);
  void maybeCollect(const std::vector<Environment *> &roots);
  void maybeCollectRoots(const HeapRoots &roots);
  void setThreshold(std::size_t threshold);
  std::size_t allocated() const { return allocatedCount; }
  std::size_t live() const {
    return objects.size() + arrays.size() + captureCells.size() + closures.size() +
           boundMethods.size() + errors.size();
  }
  std::size_t collections() const { return collectionCount; }
  HeapStats stats() const;
  [[nodiscard]] RootScope rootScope() { return RootScope(*this); }

private:
  std::vector<std::unique_ptr<Object>> objects;
  std::vector<std::unique_ptr<Array>> arrays;
  std::vector<std::unique_ptr<VmCaptureCell>> captureCells;
  std::vector<std::unique_ptr<VmClosure>> closures;
  std::vector<std::unique_ptr<VmBoundMethod>> boundMethods;
  std::vector<std::unique_ptr<ErrorObject>> errors;
  std::size_t allocatedCount{0};
  std::size_t collectionCount{0};
  std::size_t reclaimedCount{0};
  std::size_t peakLiveCount{0};
  std::size_t minimumThreshold{256};
  std::size_t nextThreshold{256};
  std::vector<const Value *> temporaryRoots;
};
} // namespace kyna
