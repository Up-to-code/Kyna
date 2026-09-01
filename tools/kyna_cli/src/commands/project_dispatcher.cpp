#include "project_internals.hpp"

namespace kyna::cli {

int runProjectCommand(const Options &options, std::istream &input, std::ostream &output,
                      std::ostream &errors) {
  switch (options.command) {
  case Command::New: {
    auto target = options.input;
    if (target.empty()) {
      const auto prompted = promptProjectPath(options);
      if (!prompted) {
        errors << "ky new: project name is required in non-interactive mode\n";
        return 2;
      }
      if (prompted->empty())
        return 130;
      target = *prompted;
    }
    const auto kind = selectTemplate(options, input, errors);
    if (kind.empty())
      return 130;
    return scaffoldProject(fs::absolute(target).lexically_normal(), kind, options, output, errors)
               ? 0
               : 2;
  }
  case Command::Init: {
    const auto kind = selectTemplate(options, input, errors);
    if (kind.empty())
      return 130;
    return scaffoldProject(
               fs::absolute(options.input.empty() ? "." : options.input).lexically_normal(), kind,
               options, output, errors)
               ? 0
               : 2;
  }
  case Command::Generate:
    return generateRoute(options, output, errors);
  case Command::Format:
    return runFormat(options, input, output, errors);
  case Command::Add:
  case Command::Remove:
  case Command::Install:
    return runDependencies(options, output, errors);
  case Command::Doctor:
    return doctor(options, output);
  case Command::Serve:
    return serveProject(options, input, output, errors);
  case Command::Dev:
    return devProject(options, errors);
  case Command::SelfUpdate:
  case Command::SelfUninstall:
    return selfManage(options, errors);
  default:
    errors << "ky: command is not implemented by project dispatcher\n";
    return 2;
  }
}

} // namespace kyna::cli
