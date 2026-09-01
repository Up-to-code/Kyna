#pragma once

#include "../../cli_commands.hpp"
#include <array>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kyna::cli {

struct ReplProject {
  std::filesystem::path root;
  std::string workspace;
  std::string name;
  std::string version;
  std::string entry;
  std::string templateName;
  bool initialized{false};
};

struct ReplCommandInfo {
  std::string_view command;
  std::string_view description;
};

extern const std::array<ReplCommandInfo, 11> replCommands;

ReplProject detectReplProject();
void showProject(const ReplProject &project, std::ostream &output);
void showReplHelp(std::ostream &output);
void showReplKeys(std::ostream &output);

class ReplLineEditor {
public:
  ReplLineEditor(std::vector<std::string> &entries, bool colorEnabled,
                 const ReplProject &projectContext);
  std::optional<std::string> read(bool continuation, std::ostream &output);

private:
  std::vector<std::string> &history;
  bool colors{false};
  const ReplProject &project;
};

} // namespace kyna::cli
