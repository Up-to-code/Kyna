#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "bytecode_compiler_private.hpp"
#include "kyna/bytecode/bytecode_validator.hpp"
#include "kyna/mir/mir_verifier.hpp"

namespace kyna {

BytecodeCompileResult compileMirToBytecode(const MirProgram &program) {
  auto mirValidation = verifyMir(program);
  if (!mirValidation.ok())
    return {std::nullopt, std::move(mirValidation.diagnostics)};

  BytecodeModule module;
  module.name = program.name;
  module.functions.reserve(program.functions.size() + 1);
  module.functions.push_back({"<module>", program.temporaryCount, {}, 0, 0, {}});
  for (const auto &function : program.functions)
    module.functions.push_back(
        {function.name, function.temporaryCount, {}, function.parameterCount,
         static_cast<std::uint32_t>(function.captures.size()), {}});
  for (const auto &sourceClass : program.classes) {
    BytecodeClass target{sourceClass.name, sourceClass.parent, sourceClass.fields, {},
                         sourceClass.constructor
                             ? std::optional<std::uint32_t>{*sourceClass.constructor + 1}
                             : std::nullopt};
    for (const auto &method : sourceClass.methods)
      target.methods.push_back({method.name, method.function + 1});
    module.classes.push_back(std::move(target));
  }

  bytecode_generation_detail::compileBody(module, module.functions.front(), program.blocks,
                                           program.exceptionRegions);
  for (std::size_t index = 0; index < program.functions.size(); ++index)
    bytecode_generation_detail::compileBody(module, module.functions[index + 1],
                                             program.functions[index].blocks,
                                             program.functions[index].exceptionRegions);

  auto validation = validateBytecode(module);
  if (!validation.ok())
    return {std::nullopt, std::move(validation.diagnostics)};
  return {std::move(module), {}};
}

} // namespace kyna
