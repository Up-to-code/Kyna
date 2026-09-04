// Implements directional channel identity, rendering, assignability, and interning.
#include <kyna/types/channel_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const ChannelType *left, const ChannelType *right) const {
    return left->str() < right->str();
  }
};
std::set<ChannelType *, Less> &pool() {
  static std::set<ChannelType *, Less> values;
  return values;
}
} // namespace

const ChannelType *ChannelType::make(TypePtr element, ChannelDirection direction) {
  auto *created = new ChannelType(element, direction);
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string ChannelType::str() const {
  const char *name = direction_ == ChannelDirection::SendReceive
                         ? "chan"
                         : direction_ == ChannelDirection::SendOnly ? "send" : "recv";
  return std::string(name) + "<" + element_->str() + ">";
}

bool ChannelType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target) || isBasic(target, BasicKind::Any))
    return true;
  if (!target || target->kind() != TypeKind::Channel)
    return false;
  const auto *channel = static_cast<const ChannelType *>(target);
  return direction_ == ChannelDirection::SendReceive &&
         channel->direction_ != ChannelDirection::SendReceive &&
         element_->isIdenticalTo(channel->element_);
}

bool ChannelType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Channel)
    return false;
  const auto *channel = static_cast<const ChannelType *>(other);
  return direction_ == channel->direction_ && element_->isIdenticalTo(channel->element_);
}
} // namespace kyna::types
