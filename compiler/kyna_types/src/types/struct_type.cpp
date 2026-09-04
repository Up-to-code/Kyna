// Implements structural field identity, lookup, validation, rendering, and interning.
#include <kyna/types/struct_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>
#include <stdexcept>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const StructType *left, const StructType *right) const {
    return left->str() < right->str();
  }
};
std::set<StructType *, Less> &pool() {
  static std::set<StructType *, Less> values;
  return values;
}
} // namespace

const StructType *StructType::make(std::vector<StructField> fields) {
  std::set<std::string> names;
  for (const auto &field : fields)
    if (field.name != "_" && !names.insert(field.name).second)
      throw std::invalid_argument("struct contains duplicate field '" + field.name + "'");
  auto *created = new StructType(std::move(fields));
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

const StructField *StructType::field(std::string_view name) const {
  for (const auto &candidate : fields_)
    if (candidate.name == name)
      return &candidate;
  return nullptr;
}

std::string StructType::str() const {
  std::string result = "struct{";
  for (std::size_t index = 0; index < fields_.size(); ++index) {
    if (index)
      result += "; ";
    const auto &field = fields_[index];
    if (field.embedded)
      result += "embedded ";
    result += field.name + ": " + field.type->str();
    if (!field.tag.empty())
      result += " `" + field.tag + "`";
  }
  return result + "}";
}

bool StructType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || isBasic(target, BasicKind::Object) ||
         isBasic(target, BasicKind::Any);
}

bool StructType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Struct)
    return false;
  const auto &fields = static_cast<const StructType *>(other)->fields_;
  if (fields_.size() != fields.size())
    return false;
  for (std::size_t index = 0; index < fields_.size(); ++index) {
    const auto &left = fields_[index];
    const auto &right = fields[index];
    if (left.name != right.name || left.tag != right.tag || left.embedded != right.embedded ||
        !left.type->isIdenticalTo(right.type))
      return false;
  }
  return true;
}
} // namespace kyna::types
