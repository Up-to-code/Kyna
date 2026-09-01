#pragma once

#include "../cli_commands.hpp"
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

namespace kyna::cli {

namespace fs = std::filesystem;

std::string projectShellQuote(const std::string &value);
bool projectWrite(const fs::path &path, std::string_view contents, std::string &error);
std::string projectNameOf(const fs::path &path);
std::optional<std::string> promptProjectPath(const Options &options);
std::string selectTemplate(const Options &options, std::istream &input, std::ostream &errors);
bool scaffoldProject(const fs::path &root, const std::string &kind, const Options &options,
                     std::ostream &output, std::ostream &errors);

int generateRoute(const Options &options, std::ostream &output, std::ostream &errors);

std::vector<fs::path> formatFiles(const std::vector<std::string> &inputs);
int runFormat(const Options &options, std::istream &input, std::ostream &output,
              std::ostream &errors);

fs::path cacheRoot();
toml::table loadManifest(const fs::path &root, std::string &error);
bool saveManifest(const fs::path &root, const toml::table &table, std::string &error);
int runDependencies(const Options &options, std::ostream &output, std::ostream &errors);

int doctor(const Options &options, std::ostream &output);

int serveProject(const Options &options, std::istream &input, std::ostream &output,
                 std::ostream &errors);

int devProject(const Options &options, std::ostream &errors);

int selfManage(const Options &options, std::ostream &errors);

} // namespace kyna::cli
