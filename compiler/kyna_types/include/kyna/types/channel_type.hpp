#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

enum class ChannelDirection : uint8_t { SendReceive, SendOnly, ReceiveOnly };

// A typed synchronization channel. Direction participates in type identity.
class ChannelType final : public Type {
public:
  static const ChannelType *make(TypePtr element,
                                 ChannelDirection direction = ChannelDirection::SendReceive);

  TypePtr element() const { return element_; }
  ChannelDirection direction() const { return direction_; }

  TypeKind kind() const override { return TypeKind::Channel; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  ChannelType(TypePtr element, ChannelDirection direction)
      : element_(element), direction_(direction) {}

  TypePtr element_;
  ChannelDirection direction_;
};

} // namespace kyna::types
