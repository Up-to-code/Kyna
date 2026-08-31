#include "kyna/execution/runtime_object_model.hpp"
#include <algorithm>
#include <deque>
#include <set>

namespace kyna {

Heap::~Heap() = default;

Heap::RootScope::RootScope(Heap &owner)
    : heap(owner), firstRoot(owner.temporaryRoots.size()) {}

Heap::RootScope::~RootScope() { heap.temporaryRoots.resize(firstRoot); }

void Heap::RootScope::protect(const Value &value) { heap.temporaryRoots.push_back(&value); }

Object *Heap::allocate() {
  objects.push_back(std::make_unique<Object>());
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return objects.back().get();
}

Array *Heap::allocateArray() {
  arrays.push_back(std::make_unique<Array>());
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return arrays.back().get();
}

VmCaptureCell *Heap::allocateCaptureCell(const Value &value) {
  captureCells.push_back(std::make_unique<VmCaptureCell>(VmCaptureCell{value}));
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return captureCells.back().get();
}

VmClosure *Heap::allocateClosure(std::uint32_t function,
                                 std::vector<VmCaptureCell *> captures) {
  closures.push_back(
      std::make_unique<VmClosure>(VmClosure{function, std::move(captures)}));
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return closures.back().get();
}

VmBoundMethod *Heap::allocateBoundMethod(Object *receiver, std::uint32_t function) {
  boundMethods.push_back(
      std::make_unique<VmBoundMethod>(VmBoundMethod{receiver, function}));
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return boundMethods.back().get();
}

ErrorObject *Heap::allocateError(std::string message, std::string code, const Value &cause) {
  errors.push_back(
      std::make_unique<ErrorObject>(ErrorObject{std::move(message), std::move(code), cause}));
  ++allocatedCount;
  peakLiveCount = std::max(peakLiveCount, live());
  return errors.back().get();
}

void Heap::collect(const std::vector<Environment *> &roots) {
  collectRoots(HeapRoots{roots, {}, {}});
}

void Heap::collectRoots(const HeapRoots &roots) {
  const auto before = live();
  std::set<Object *> markedObjects;
  std::set<Array *> markedArrays;
  std::set<Environment *> markedEnvironments;
  std::set<Function *> markedFunctions;
  std::set<Class *> markedClasses;
  std::set<ModuleNamespace *> markedModules;
  std::set<VmCaptureCell *> markedCaptureCells;
  std::set<VmClosure *> markedClosures;
  std::set<VmBoundMethod *> markedBoundMethods;
  std::set<ErrorObject *> markedErrors;
  std::deque<Value> pendingValues;
  std::deque<Environment *> pendingEnvironments;
  std::deque<VmCaptureCell *> pendingCaptureCells;

  for (auto *root : roots.environments)
    if (root)
      pendingEnvironments.push_back(root);
  for (const auto *root : roots.values)
    if (root)
      pendingValues.push_back(*root);
  for (auto *root : roots.captureCells)
    if (root)
      pendingCaptureCells.push_back(root);
  for (const auto *value : temporaryRoots)
    if (value)
      pendingValues.push_back(*value);

  while (!pendingEnvironments.empty() || !pendingValues.empty() ||
         !pendingCaptureCells.empty()) {
    while (!pendingEnvironments.empty()) {
      auto *environment = pendingEnvironments.front();
      pendingEnvironments.pop_front();
      if (!environment || !markedEnvironments.insert(environment).second)
        continue;
      for (const auto &[name, cell] : environment->values)
        pendingValues.push_back(cell.value);
      if (environment->enclosing)
        pendingEnvironments.push_back(environment->enclosing.get());
    }
    while (!pendingCaptureCells.empty()) {
      auto *cell = pendingCaptureCells.front();
      pendingCaptureCells.pop_front();
      if (cell && markedCaptureCells.insert(cell).second)
        pendingValues.push_back(cell->value);
    }
    if (pendingValues.empty())
      continue;
    auto value = std::move(pendingValues.front());
    pendingValues.pop_front();
    if (const auto *object = std::get_if<ObjectPtr>(&value.data)) {
      if (!*object || !markedObjects.insert(*object).second)
        continue;
      for (const auto &[name, field] : (*object)->fields)
        pendingValues.push_back(field);
      if ((*object)->klass)
        pendingValues.emplace_back((*object)->klass);
    } else if (const auto *array = std::get_if<ArrayPtr>(&value.data)) {
      if (!*array || !markedArrays.insert(*array).second)
        continue;
      for (const auto &element : (*array)->elements)
        pendingValues.push_back(element);
    } else if (const auto *function = std::get_if<FunctionPtr>(&value.data)) {
      if (!*function || !markedFunctions.insert(function->get()).second)
        continue;
      if ((*function)->boundThis)
        pendingValues.emplace_back((*function)->boundThis);
      if ((*function)->closure)
        pendingEnvironments.push_back((*function)->closure.get());
    } else if (const auto *klass = std::get_if<ClassPtr>(&value.data)) {
      if (!*klass || !markedClasses.insert(klass->get()).second)
        continue;
      if ((*klass)->parent)
        pendingValues.emplace_back((*klass)->parent);
      for (const auto &[name, field] : (*klass)->staticFields)
        pendingValues.push_back(field);
      for (const auto &[name, method] : (*klass)->methods)
        pendingValues.emplace_back(method);
    } else if (const auto *module = std::get_if<ModulePtr>(&value.data)) {
      if (!*module || !markedModules.insert(module->get()).second)
        continue;
      if ((*module)->environment)
        pendingEnvironments.push_back((*module)->environment.get());
    } else if (const auto *closure = std::get_if<VmClosure *>(&value.data)) {
      if (!*closure || !markedClosures.insert(*closure).second)
        continue;
      for (auto *capture : (*closure)->captures)
        if (capture)
          pendingCaptureCells.push_back(capture);
    } else if (const auto *method = std::get_if<VmBoundMethod *>(&value.data)) {
      if (!*method || !markedBoundMethods.insert(*method).second)
        continue;
      if ((*method)->receiver)
        pendingValues.emplace_back((*method)->receiver);
    } else if (const auto *error = std::get_if<ErrorPtr>(&value.data)) {
      if (!*error || !markedErrors.insert(*error).second)
        continue;
      pendingValues.push_back((*error)->cause);
    }
  }

  objects.erase(
      std::remove_if(objects.begin(), objects.end(),
                     [&](const auto &object) { return !markedObjects.contains(object.get()); }),
      objects.end());
  arrays.erase(
      std::remove_if(arrays.begin(), arrays.end(),
                     [&](const auto &array) { return !markedArrays.contains(array.get()); }),
      arrays.end());
  closures.erase(
      std::remove_if(closures.begin(), closures.end(),
                     [&](const auto &closure) { return !markedClosures.contains(closure.get()); }),
      closures.end());
  boundMethods.erase(
      std::remove_if(boundMethods.begin(), boundMethods.end(),
                     [&](const auto &method) {
                       return !markedBoundMethods.contains(method.get());
                     }),
      boundMethods.end());
  captureCells.erase(
      std::remove_if(captureCells.begin(), captureCells.end(),
                     [&](const auto &cell) { return !markedCaptureCells.contains(cell.get()); }),
      captureCells.end());
  errors.erase(
      std::remove_if(errors.begin(), errors.end(),
                     [&](const auto &error) { return !markedErrors.contains(error.get()); }),
      errors.end());
  reclaimedCount += before - live();
  ++collectionCount;
  nextThreshold = std::max(minimumThreshold, live() * 2 + 1);
}

void Heap::maybeCollect(const std::vector<Environment *> &roots) {
  if (live() >= nextThreshold)
    collect(roots);
}

void Heap::maybeCollectRoots(const HeapRoots &roots) {
  if (live() >= nextThreshold)
    collectRoots(roots);
}

void Heap::setThreshold(std::size_t threshold) {
  minimumThreshold = std::max<std::size_t>(1, threshold);
  nextThreshold = minimumThreshold;
}

HeapStats Heap::stats() const {
  return {live(), allocatedCount, reclaimedCount, collectionCount, peakLiveCount, nextThreshold};
}

} // namespace kyna
