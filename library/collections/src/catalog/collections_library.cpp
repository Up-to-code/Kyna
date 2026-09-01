#include "kyna/stdlib/collections_library.hpp"

#include "../collections_private.hpp"

namespace kyna {

void installCollectionsLibrary(Interpreter &interpreter) {
  const auto transform = detail::makeMapOp(interpreter);
  const auto reduce = detail::makeReduceOp(interpreter);
  const auto find = detail::makeFindOp(interpreter);
  const auto any = detail::makeAnyOp(interpreter);
  const auto all = detail::makeAllOp(interpreter);
  const auto unique = detail::makeUniqueOp(interpreter);

  interpreter.globals()->define("map", Value(transform), false);
  interpreter.globals()->define("reduce", Value(reduce), false);
  interpreter.globals()->define("find", Value(find), false);
  interpreter.globals()->define("any", Value(any), false);
  interpreter.globals()->define("all", Value(all), false);
  interpreter.globals()->define("unique", Value(unique), false);

  auto collections = interpreter.heap().allocate();
  collections->fields["map"] = Value(transform);
  collections->fields["reduce"] = Value(reduce);
  collections->fields["find"] = Value(find);
  collections->fields["any"] = Value(any);
  collections->fields["all"] = Value(all);
  collections->fields["unique"] = Value(unique);
  interpreter.globals()->define("collections", Value(collections), false);
}

} // namespace kyna
