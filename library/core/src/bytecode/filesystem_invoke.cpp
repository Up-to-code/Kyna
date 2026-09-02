#include "bytecode_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> filesystemBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "readFile") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2001", "readFile expects exactly one string path");
    std::string message;
    auto contents = ctx.capabilities.files->read(std::get<std::string>(arguments[0].data), message);
    return contents ? NativeCallResult{RuntimeValue(std::move(*contents)), std::nullopt}
                    : bytecodeFailure("KFS2001", std::move(message), arguments[0]);
  }
  if (name == "writeFile") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KFS2002", "writeFile expects a string path and string content");
    std::string message;
    if (!ctx.capabilities.files->write(std::get<std::string>(arguments[0].data),
                                       std::get<std::string>(arguments[1].data), message))
      return bytecodeFailure("KFS2002", std::move(message), arguments[0]);
    return NativeCallResult{RuntimeValue(true), std::nullopt};
  }
  if (name == "fileExists") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2003", "fileExists expects exactly one string path");
    std::string message;
    const auto exists =
        ctx.capabilities.files->exists(std::get<std::string>(arguments[0].data), message);
    if (!message.empty())
      return bytecodeFailure("KFS2003", std::move(message), arguments[0]);
    return NativeCallResult{RuntimeValue(exists), std::nullopt};
  }
  if (name == "createDirectory") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2004", "createDirectory expects exactly one string path");
    std::string message;
    if (!ctx.capabilities.files->createDirectories(std::get<std::string>(arguments[0].data),
                                                    message))
      return bytecodeFailure("KFS2004", std::move(message), arguments[0]);
    return NativeCallResult{RuntimeValue(true), std::nullopt};
  }
  if (name == "removePath") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2005", "removePath expects exactly one string path");
    std::string message;
    const auto removed =
        ctx.capabilities.files->remove(std::get<std::string>(arguments[0].data), message);
    if (!message.empty())
      return bytecodeFailure("KFS2005", std::move(message), arguments[0]);
    return NativeCallResult{RuntimeValue(removed), std::nullopt};
  }
  if (name == "listDirectory") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2006", "listDirectory expects exactly one string path");
    std::string message;
    auto entries =
        ctx.capabilities.files->list(std::get<std::string>(arguments[0].data), message);
    if (!entries)
      return bytecodeFailure("KFS2006", std::move(message), arguments[0]);
    auto *result = ctx.heap.allocateArray();
    for (auto &entry : *entries)
      result->elements.emplace_back(std::move(entry));
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "readJsonFile") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2007", "readJsonFile expects exactly one string path");
    std::string message;
    auto contents = ctx.capabilities.files->read(std::get<std::string>(arguments[0].data), message);
    if (!contents)
      return bytecodeFailure("KFS2007", std::move(message), arguments[0]);
    try {
      return NativeCallResult{parseJsonValue(*contents, ctx.heap), std::nullopt};
    } catch (const KynaError &error) {
      return bytecodeFailure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                             error.diagnostic.message, arguments[0]);
    }
  }
  if (name == "writeJsonFile") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KFS2008", "writeJsonFile expects a string path and value");
    std::string encoded;
    try {
      encoded = stringifyJsonValue(arguments[1]);
    } catch (const KynaError &error) {
      return bytecodeFailure(error.diagnostic.code.empty() ? "K5101" : error.diagnostic.code,
                             error.diagnostic.message, arguments[1]);
    }
    std::string message;
    if (!ctx.capabilities.files->write(std::get<std::string>(arguments[0].data), encoded, message))
      return bytecodeFailure("KFS2008", std::move(message), arguments[0]);
    return NativeCallResult{RuntimeValue(true), std::nullopt};
  }
  if (name == "copyFile") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KFS2010", "copyFile expects source and destination path strings");
    std::string message;
    auto reader = ctx.capabilities.files->openRead(std::get<std::string>(arguments[0].data), message);
    if (!reader)
      return bytecodeFailure("KFS2010", std::move(message), arguments[0]);
    auto writer = ctx.capabilities.files->openWrite(std::get<std::string>(arguments[1].data), message);
    if (!writer)
      return bytecodeFailure("KFS2010", std::move(message), arguments[1]);
    std::array<std::uint8_t, 65536> buffer{};
    std::int64_t total = 0;
    for (;;) {
      const auto got = reader->read(buffer.data(), buffer.size());
      if (got == 0)
        break;
      total += static_cast<std::int64_t>(writer->write(buffer.data(), got));
    }
    reader->close();
    writer->close();
    return NativeCallResult{RuntimeValue(total), std::nullopt};
  }
  return std::nullopt;
}

} // namespace kyna::detail
