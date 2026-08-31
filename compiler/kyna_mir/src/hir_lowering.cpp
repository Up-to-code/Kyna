#include "kyna/mir/hir_lowering.hpp"
#include "kyna/mir/mir_verifier.hpp"
#include <type_traits>
#include <unordered_map>

namespace kyna {
namespace {

class HirLowerer {
public:
  explicit HirLowerer(const HirProgram &source) : hir(source) {
    mir.name = source.name;
    for (const auto &sourceClass : source.classes) {
      MirClass target{sourceClass.name,
                      sourceClass.parent
                          ? std::optional<std::uint32_t>{sourceClass.parent->value}
                          : std::nullopt,
                      {}, {},
                      sourceClass.constructor
                          ? std::optional<std::uint32_t>{sourceClass.constructor->value}
                          : std::nullopt};
      for (const auto &field : sourceClass.fields)
        target.fields.push_back(field.name);
      for (const auto &method : sourceClass.methods)
        target.methods.push_back({method.name, method.function.value});
      mir.classes.push_back(std::move(target));
    }
    mir.blocks.emplace_back();
    activate(mir.temporaryCount, mir.blocks, mir.exceptionRegions);
  }

  MirLoweringResult lower() {
    for (const auto statement : hir.body)
      lowerStatement(statement);
    finishFunction();

    for (const auto &sourceFunction : hir.functions) {
      mir.functions.push_back(
          {sourceFunction.name, static_cast<std::uint32_t>(sourceFunction.parameters.size()), 0,
           {}, {MirBasicBlock{}}, {}, sourceFunction.span, sourceFunction.captures});
      auto &target = mir.functions.back();
      activate(target.temporaryCount, target.blocks, target.exceptionRegions);
      for (std::size_t capture = 0; capture < sourceFunction.captures.size(); ++capture)
        captureIndexes.insert_or_assign(sourceFunction.captures[capture].value,
                                        static_cast<std::uint32_t>(capture));
      for (const auto parameter : sourceFunction.parameters)
        locals.insert_or_assign(parameter.value, temporary());
      lowerStatement(sourceFunction.body);
      finishFunction();
    }

    auto verification = verifyMir(mir);
    if (!verification.ok())
      return {std::nullopt, std::move(verification.diagnostics)};
    return {std::move(mir), {}};
  }

private:
  const HirProgram &hir;
  MirProgram mir;
  std::uint32_t *activeTemporaryCount{nullptr};
  std::vector<MirBasicBlock> *activeBlocks{nullptr};
  std::vector<MirExceptionRegion> *activeExceptionRegions{nullptr};
  MirBlockId currentBlock{0};
  std::unordered_map<std::uint32_t, MirTemporary> values;
  std::unordered_map<std::uint32_t, MirTemporary> locals;
  std::unordered_map<std::uint32_t, std::uint32_t> captureIndexes;
  struct LoopContext {
    std::string label;
    MirBlockId breakTarget;
    MirBlockId continueTarget;
  };
  std::vector<LoopContext> loops;
  std::vector<HirStatementId> cleanups;

  void activate(std::uint32_t &temporaryCount, std::vector<MirBasicBlock> &blocks,
                std::vector<MirExceptionRegion> &exceptionRegions) {
    activeTemporaryCount = &temporaryCount;
    activeBlocks = &blocks;
    activeExceptionRegions = &exceptionRegions;
    currentBlock = {};
    values.clear();
    locals.clear();
    captureIndexes.clear();
    loops.clear();
    cleanups.clear();
  }

  MirTemporary temporary() { return MirTemporary{(*activeTemporaryCount)++}; }
  MirBasicBlock &current() { return activeBlocks->at(currentBlock.value); }
  MirBlockId addBlock() {
    const auto id = MirBlockId{static_cast<std::uint32_t>(activeBlocks->size())};
    activeBlocks->emplace_back();
    return id;
  }
  void terminate(MirTerminator::Node node, SourceSpan span) {
    current().terminator = MirTerminator{std::move(node), span};
  }
  void finishFunction() {
    if (current().terminator)
      return;
    const auto nullValue = temporary();
    current().instructions.push_back(
        {MirInstructionKind::Constant, nullValue, {}, {}, nullptr, {}, 0, {}});
    terminate(MirReturnTerminator{nullValue}, {});
  }

  bool lowerActiveCleanups() {
    const auto saved = cleanups;
    for (std::size_t count = saved.size(); count > 0; --count) {
      cleanups.assign(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(count - 1));
      lowerStatement(saved[count - 1]);
      if (current().terminator) {
        cleanups = saved;
        return false;
      }
    }
    cleanups = saved;
    return true;
  }

  std::vector<MirBlockId> blocksFrom(std::size_t first) const {
    std::vector<MirBlockId> result;
    result.reserve(activeBlocks->size() - first);
    for (std::size_t index = first; index < activeBlocks->size(); ++index)
      result.push_back(MirBlockId{static_cast<std::uint32_t>(index)});
    return result;
  }

  MirInstructionKind instructionFor(HirBinaryOperator operation) const {
    switch (operation) {
    case HirBinaryOperator::Add: return MirInstructionKind::Add;
    case HirBinaryOperator::Subtract: return MirInstructionKind::Subtract;
    case HirBinaryOperator::Multiply: return MirInstructionKind::Multiply;
    case HirBinaryOperator::Divide: return MirInstructionKind::Divide;
    case HirBinaryOperator::Remainder: return MirInstructionKind::Remainder;
    case HirBinaryOperator::Equal: return MirInstructionKind::Equal;
    case HirBinaryOperator::NotEqual: return MirInstructionKind::NotEqual;
    case HirBinaryOperator::Less: return MirInstructionKind::Less;
    case HirBinaryOperator::LessEqual: return MirInstructionKind::LessEqual;
    case HirBinaryOperator::Greater: return MirInstructionKind::Greater;
    case HirBinaryOperator::GreaterEqual: return MirInstructionKind::GreaterEqual;
    case HirBinaryOperator::And:
    case HirBinaryOperator::Or: return MirInstructionKind::Move;
    }
    return MirInstructionKind::Move;
  }

  const LoopContext &loopTarget(const std::string &label) const {
    if (label.empty())
      return loops.back();
    for (auto loop = loops.rbegin(); loop != loops.rend(); ++loop)
      if (loop->label == label)
        return *loop;
    return loops.back();
  }

  MirTemporary lowerExpression(HirExpressionId id) {
    const auto &expression = hir.expressions.at(id.value);
    const auto result = std::visit(
        [&](const auto &node) -> MirTemporary {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, HirConstantExpression>) {
            const auto target = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Constant, target, {}, {}, node.value, expression.span, 0, {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirLocalExpression>) {
            if (const auto local = locals.find(node.local.value); local != locals.end())
              return local->second;
            const auto target = temporary();
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::LoadCapture;
            instruction.destination = target;
            instruction.capture = captureIndexes.at(node.local.value);
            instruction.span = expression.span;
            current().instructions.push_back(std::move(instruction));
            return target;
          } else if constexpr (std::is_same_v<T, HirFunctionReferenceExpression>) {
            const auto target = temporary();
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::FunctionReference;
            instruction.destination = target;
            instruction.span = expression.span;
            instruction.function = node.function.value + 1;
            current().instructions.push_back(std::move(instruction));
            return target;
          } else if constexpr (std::is_same_v<T, HirAssignIndexExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> operands{lowerExpression(node.object),
                                               lowerExpression(node.index),
                                               lowerExpression(node.value)};
            current().instructions.push_back({MirInstructionKind::StoreIndex, target, {}, {},
                                               nullptr, expression.span, 0,
                                               std::move(operands)});
            return target;
          } else if constexpr (std::is_same_v<T, HirAssignMemberExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> operands{lowerExpression(node.object),
                                               lowerExpression(node.value)};
            current().instructions.push_back({MirInstructionKind::StoreMember, target, {}, {},
                                               node.member, expression.span, 0,
                                               std::move(operands)});
            return target;
          } else if constexpr (std::is_same_v<T, HirMemberExpression>) {
            const auto target = temporary();
            const auto object = lowerExpression(node.object);
            current().instructions.push_back({MirInstructionKind::LoadMember, target, object, {},
                                               node.member, expression.span, 0, {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirBoundMethodExpression>) {
            const auto target = temporary();
            const auto receiver = lowerExpression(node.receiver);
            current().instructions.push_back({MirInstructionKind::BindMethod, target, receiver,
                                               {}, nullptr, expression.span,
                                               node.function.value + 1, {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirNativeCallExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> arguments;
            arguments.reserve(node.arguments.size());
            for (const auto argument : node.arguments)
              arguments.push_back(lowerExpression(argument));
            current().instructions.push_back({MirInstructionKind::CallNative, target, {}, {},
                                               node.name, expression.span, 0,
                                               std::move(arguments)});
            return target;
          } else if constexpr (std::is_same_v<T, HirIndexExpression>) {
            const auto target = temporary();
            const auto object = lowerExpression(node.object);
            const auto index = lowerExpression(node.index);
            current().instructions.push_back({MirInstructionKind::LoadIndex, target, object, index,
                                               nullptr, expression.span, 0, {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirArrayExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> elements;
            elements.reserve(node.elements.size());
            for (const auto element : node.elements)
              elements.push_back(lowerExpression(element));
            current().instructions.push_back({MirInstructionKind::MakeArray, target, {}, {},
                                               nullptr, expression.span, 0,
                                               std::move(elements)});
            return target;
          } else if constexpr (std::is_same_v<T, HirObjectExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> values;
            std::vector<std::string> names;
            values.reserve(node.fields.size());
            names.reserve(node.fields.size());
            for (const auto &field : node.fields) {
              names.push_back(field.name);
              values.push_back(lowerExpression(field.value));
            }
            current().instructions.push_back(
                {MirInstructionKind::MakeObject, target, {}, {}, nullptr, expression.span, 0,
                 std::move(values), 0, {}, std::move(names)});
            return target;
          } else if constexpr (std::is_same_v<T, HirNewExpression>) {
            const auto target = temporary();
            std::vector<MirTemporary> arguments;
            arguments.reserve(node.arguments.size());
            for (const auto argument : node.arguments)
              arguments.push_back(lowerExpression(argument));
            current().instructions.push_back({MirInstructionKind::MakeInstance, target, {}, {},
                                               nullptr, expression.span, node.klass.value,
                                               std::move(arguments)});
            return target;
          } else if constexpr (std::is_same_v<T, HirClosureExpression>) {
            const auto target = temporary();
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::Closure;
            instruction.destination = target;
            instruction.function = node.function.value + 1;
            instruction.span = expression.span;
            for (const auto capture : hir.functions.at(node.function.value).captures) {
              if (const auto local = locals.find(capture.value); local != locals.end())
                instruction.captureSources.push_back(
                    {MirCaptureSource::Kind::Local, local->second.value});
              else
                instruction.captureSources.push_back(
                    {MirCaptureSource::Kind::Capture, captureIndexes.at(capture.value)});
            }
            current().instructions.push_back(std::move(instruction));
            return target;
          } else if constexpr (std::is_same_v<T, HirUnaryExpression>) {
            const auto operand = lowerExpression(node.operand);
            const auto target = temporary();
            current().instructions.push_back(
                {node.operation == HirUnaryOperator::Negate ? MirInstructionKind::Negate
                                                            : MirInstructionKind::Not,
                 target, operand, {}, nullptr, expression.span, 0, {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirBinaryExpression>) {
            const auto left = lowerExpression(node.left);
            if (node.operation == HirBinaryOperator::And ||
                node.operation == HirBinaryOperator::Or) {
              const auto target = temporary();
              const auto rightBlock = addBlock();
              const auto shortCircuitBlock = addBlock();
              const auto continuationBlock = addBlock();
              const bool isAnd = node.operation == HirBinaryOperator::And;
              terminate(MirBranchTerminator{left,
                                            isAnd ? rightBlock : shortCircuitBlock,
                                            isAnd ? shortCircuitBlock : rightBlock},
                        expression.span);

              currentBlock = shortCircuitBlock;
              const auto shortCircuitValue = temporary();
              current().instructions.push_back(
                  {MirInstructionKind::Constant, shortCircuitValue, {}, {}, !isAnd,
                   expression.span, 0, {}});
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, shortCircuitValue, {}, nullptr,
                   expression.span, 0, {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);

              currentBlock = rightBlock;
              const auto right = lowerExpression(node.right);
              const auto inverted = temporary();
              const auto booleanRight = temporary();
              current().instructions.push_back(
                  {MirInstructionKind::Not, inverted, right, {}, nullptr, expression.span, 0, {}});
              current().instructions.push_back(
                  {MirInstructionKind::Not, booleanRight, inverted, {}, nullptr, expression.span, 0,
                   {}});
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, booleanRight, {}, nullptr, expression.span, 0,
                   {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);

              currentBlock = continuationBlock;
              return target;
            }
            const auto right = lowerExpression(node.right);
            const auto target = temporary();
            current().instructions.push_back(
                {instructionFor(node.operation), target, left, right, nullptr, expression.span, 0,
                 {}});
            return target;
          } else if constexpr (std::is_same_v<T, HirAssignLocalExpression>) {
            const auto source = lowerExpression(node.value);
            if (const auto local = locals.find(node.local.value); local != locals.end()) {
              current().instructions.push_back(
                  {MirInstructionKind::Move, local->second, source, {}, nullptr, expression.span, 0,
                   {}});
              return local->second;
            }
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::StoreCapture;
            instruction.destination = source;
            instruction.first = source;
            instruction.capture = captureIndexes.at(node.local.value);
            instruction.span = expression.span;
            current().instructions.push_back(std::move(instruction));
            return source;
          } else if constexpr (std::is_same_v<T, HirCallExpression>) {
            std::vector<MirTemporary> arguments;
            arguments.reserve(node.arguments.size());
            for (const auto argument : node.arguments)
              arguments.push_back(lowerExpression(argument));
            const auto target = temporary();
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::Call;
            instruction.destination = target;
            instruction.span = expression.span;
            instruction.function = node.function.value + 1;
            instruction.arguments = std::move(arguments);
            current().instructions.push_back(std::move(instruction));
            return target;
          } else if constexpr (std::is_same_v<T, HirIndirectCallExpression>) {
            const auto callee = lowerExpression(node.callee);
            std::vector<MirTemporary> arguments;
            arguments.reserve(node.arguments.size());
            for (const auto argument : node.arguments)
              arguments.push_back(lowerExpression(argument));
            const auto target = temporary();
            MirInstruction instruction;
            instruction.kind = MirInstructionKind::CallIndirect;
            instruction.destination = target;
            instruction.first = callee;
            instruction.span = expression.span;
            instruction.arguments = std::move(arguments);
            current().instructions.push_back(std::move(instruction));
            return target;
          } else if constexpr (std::is_same_v<T, HirIfExpression>) {
            const auto condition = lowerExpression(node.condition);
            const auto target = temporary();
            const auto thenBlock = addBlock();
            const auto elseBlock = addBlock();
            const auto continuationBlock = addBlock();
            terminate(MirBranchTerminator{condition, thenBlock, elseBlock}, expression.span);

            currentBlock = thenBlock;
            lowerStatement(node.thenPrelude);
            if (!current().terminator) {
              const auto value = lowerExpression(node.thenValue);
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);
            }

            currentBlock = elseBlock;
            lowerStatement(node.elsePrelude);
            if (!current().terminator) {
              const auto value = lowerExpression(node.elseValue);
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);
            }
            currentBlock = continuationBlock;
            return target;
          } else {
            const auto subject = lowerExpression(node.subject);
            const auto target = temporary();
            const auto continuationBlock = addBlock();
            bool endedWithWildcard = false;
            for (const auto &arm : node.arms) {
              if (!arm.pattern) {
                const auto value = lowerExpression(arm.value);
                current().instructions.push_back(
                    {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0,
                     {}});
                terminate(MirGotoTerminator{continuationBlock}, expression.span);
                endedWithWildcard = true;
                break;
              }
              const auto pattern = lowerExpression(*arm.pattern);
              const auto matches = temporary();
              current().instructions.push_back(
                  {MirInstructionKind::Equal, matches, subject, pattern, nullptr, expression.span,
                   0, {}});
              const auto armBlock = addBlock();
              const auto nextArmBlock = addBlock();
              terminate(MirBranchTerminator{matches, armBlock, nextArmBlock}, expression.span);

              currentBlock = armBlock;
              const auto value = lowerExpression(arm.value);
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);
              currentBlock = nextArmBlock;
            }
            if (!endedWithWildcard && !current().terminator) {
              const auto fallback = temporary();
              current().instructions.push_back(
                  {MirInstructionKind::Constant, fallback, {}, {}, nullptr, expression.span, 0,
                   {}});
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, fallback, {}, nullptr, expression.span, 0,
                   {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);
            }
            currentBlock = continuationBlock;
            return target;
          }
        },
        expression.node);
    return result;
  }

  void lowerStatement(HirStatementId id) {
    if (current().terminator)
      return;
    const auto &statement = hir.statements.at(id.value);
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, HirBindLocalStatement>) {
            const auto destination = temporary();
            locals.insert_or_assign(node.local.value, destination);
            const auto source = lowerExpression(node.initializer);
            current().instructions.push_back(
                {MirInstructionKind::Move, destination, source, {}, nullptr, statement.span, 0,
                 {}});
          } else if constexpr (std::is_same_v<T, HirEvaluateStatement>) {
            lowerExpression(node.expression);
          } else if constexpr (std::is_same_v<T, HirReturnStatement>) {
            const auto value = lowerExpression(node.expression);
            if (lowerActiveCleanups())
              terminate(MirReturnTerminator{value}, statement.span);
          } else if constexpr (std::is_same_v<T, HirBlockStatement>) {
            for (const auto child : node.statements)
              lowerStatement(child);
          } else if constexpr (std::is_same_v<T, HirIfStatement>) {
            const auto condition = lowerExpression(node.condition);
            const auto thenBlock = addBlock();
            const auto elseBlock = addBlock();
            const auto continuation = addBlock();
            terminate(MirBranchTerminator{condition, thenBlock, elseBlock}, statement.span);

            currentBlock = thenBlock;
            lowerStatement(node.thenBranch);
            if (!current().terminator)
              terminate(MirGotoTerminator{continuation}, statement.span);

            currentBlock = elseBlock;
            if (node.elseBranch)
              lowerStatement(*node.elseBranch);
            if (!current().terminator)
              terminate(MirGotoTerminator{continuation}, statement.span);
            currentBlock = continuation;
          } else if constexpr (std::is_same_v<T, HirWhileStatement>) {
            const auto conditionBlock = addBlock();
            const auto bodyBlock = addBlock();
            const auto exitBlock = addBlock();
            terminate(MirGotoTerminator{conditionBlock}, statement.span);

            currentBlock = conditionBlock;
            const auto condition = lowerExpression(node.condition);
            terminate(MirBranchTerminator{condition, bodyBlock, exitBlock}, statement.span);

            currentBlock = bodyBlock;
            loops.push_back({node.label, exitBlock, conditionBlock});
            lowerStatement(node.body);
            loops.pop_back();
            if (!current().terminator)
              terminate(MirGotoTerminator{conditionBlock}, statement.span);
            currentBlock = exitBlock;
          } else if constexpr (std::is_same_v<T, HirLoopStatement>) {
            if (node.initializer)
              lowerStatement(*node.initializer);
            if (current().terminator)
              return;
            const auto conditionBlock = addBlock();
            const auto bodyBlock = addBlock();
            const auto incrementBlock = addBlock();
            const auto exitBlock = addBlock();
            terminate(MirGotoTerminator{conditionBlock}, statement.span);

            currentBlock = conditionBlock;
            const auto condition = lowerExpression(node.condition);
            terminate(MirBranchTerminator{condition, bodyBlock, exitBlock}, statement.span);

            currentBlock = bodyBlock;
            loops.push_back({node.label, exitBlock, incrementBlock});
            lowerStatement(node.body);
            loops.pop_back();
            if (!current().terminator)
              terminate(MirGotoTerminator{incrementBlock}, statement.span);

            currentBlock = incrementBlock;
            if (node.increment)
              lowerExpression(*node.increment);
            terminate(MirGotoTerminator{conditionBlock}, statement.span);
            currentBlock = exitBlock;
          } else if constexpr (std::is_same_v<T, HirBreakStatement>) {
            const auto target = loopTarget(node.label).breakTarget;
            if (lowerActiveCleanups())
              terminate(MirGotoTerminator{target}, statement.span);
          } else if constexpr (std::is_same_v<T, HirContinueStatement>) {
            const auto target = loopTarget(node.label).continueTarget;
            if (lowerActiveCleanups())
              terminate(MirGotoTerminator{target}, statement.span);
          } else if constexpr (std::is_same_v<T, HirThrowStatement>) {
            terminate(MirThrowTerminator{lowerExpression(node.value)}, statement.span);
          } else {
            const auto continuation = addBlock();
            const auto tryEntry = addBlock();
            terminate(MirGotoTerminator{tryEntry}, statement.span);
            currentBlock = tryEntry;
            const auto tryFirstBlock = static_cast<std::size_t>(tryEntry.value);

            if (node.finallyBranch)
              cleanups.push_back(*node.finallyBranch);
            lowerStatement(node.tryBranch);
            if (node.finallyBranch)
              cleanups.pop_back();
            const auto protectedTryBlocks = blocksFrom(tryFirstBlock);
            if (!current().terminator) {
              if (node.finallyBranch)
                lowerStatement(*node.finallyBranch);
              if (!current().terminator)
                terminate(MirGotoTerminator{continuation}, statement.span);
            }

            const auto error = temporary();
            const auto handler = addBlock();
            activeExceptionRegions->push_back({protectedTryBlocks, handler, error});
            currentBlock = handler;

            if (node.catchBranch) {
              const auto catchFirstBlock = static_cast<std::size_t>(handler.value);
              locals.insert_or_assign(node.catchLocal->value, error);
              if (node.finallyBranch)
                cleanups.push_back(*node.finallyBranch);
              lowerStatement(*node.catchBranch);
              if (node.finallyBranch)
                cleanups.pop_back();
              const auto protectedCatchBlocks = blocksFrom(catchFirstBlock);
              if (!current().terminator) {
                if (node.finallyBranch)
                  lowerStatement(*node.finallyBranch);
                if (!current().terminator)
                  terminate(MirGotoTerminator{continuation}, statement.span);
              }
              if (node.finallyBranch) {
                const auto rethrown = temporary();
                const auto rethrowHandler = addBlock();
                activeExceptionRegions->push_back(
                    {protectedCatchBlocks, rethrowHandler, rethrown});
                currentBlock = rethrowHandler;
                lowerStatement(*node.finallyBranch);
                if (!current().terminator)
                  terminate(MirThrowTerminator{rethrown}, statement.span);
              }
            } else {
              if (node.finallyBranch)
                lowerStatement(*node.finallyBranch);
              if (!current().terminator)
                terminate(MirThrowTerminator{error}, statement.span);
            }
            currentBlock = continuation;
          }
        },
        statement.node);
  }
};

} // namespace

const char *mirInstructionName(MirInstructionKind kind) {
  switch (kind) {
  case MirInstructionKind::Constant: return "constant";
  case MirInstructionKind::FunctionReference: return "function";
  case MirInstructionKind::Closure: return "closure";
  case MirInstructionKind::LoadCapture: return "load_capture";
  case MirInstructionKind::StoreCapture: return "store_capture";
  case MirInstructionKind::Move: return "move";
  case MirInstructionKind::Negate: return "negate";
  case MirInstructionKind::Not: return "not";
  case MirInstructionKind::Add: return "add";
  case MirInstructionKind::Subtract: return "subtract";
  case MirInstructionKind::Multiply: return "multiply";
  case MirInstructionKind::Divide: return "divide";
  case MirInstructionKind::Remainder: return "remainder";
  case MirInstructionKind::Equal: return "equal";
  case MirInstructionKind::NotEqual: return "not_equal";
  case MirInstructionKind::Less: return "less";
  case MirInstructionKind::LessEqual: return "less_equal";
  case MirInstructionKind::Greater: return "greater";
  case MirInstructionKind::GreaterEqual: return "greater_equal";
  case MirInstructionKind::Call: return "call";
  case MirInstructionKind::CallIndirect: return "call_indirect";
  case MirInstructionKind::CallNative: return "call_native";
  case MirInstructionKind::LoadMember: return "load_member";
  case MirInstructionKind::BindMethod: return "bind_method";
  case MirInstructionKind::MakeArray: return "make_array";
  case MirInstructionKind::MakeObject: return "make_object";
  case MirInstructionKind::MakeInstance: return "make_instance";
  case MirInstructionKind::LoadIndex: return "load_index";
  case MirInstructionKind::StoreIndex: return "store_index";
  case MirInstructionKind::StoreMember: return "store_member";
  }
  return "unknown";
}

MirLoweringResult lowerHirToMir(const HirProgram &program) { return HirLowerer(program).lower(); }

} // namespace kyna
