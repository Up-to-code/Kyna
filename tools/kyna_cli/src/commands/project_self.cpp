#include "project_internals.hpp"
#include <cstdlib>
#include <fstream>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace kyna::cli {
namespace {
std::string shellQuote(const std::string &value) { return projectShellQuote(value); }
} // namespace

int selfManage(const Options &options, std::ostream &errors) {
  if (options.command == Command::SelfUpdate) {
#if defined(_WIN32)
    const auto powerShellQuote = [](const std::string &value) {
      std::string quoted{"'"};
      for (const char character : value)
        quoted += character == '\'' ? "''" : std::string(1, character);
      return quoted + "'";
    };
    std::string command =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \"$installer = irm "
        "'https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1'; & "
        "([scriptblock]::Create($installer)) -NonInteractive -Channel " +
        powerShellQuote(options.channel);
    if (!options.installVersion.empty())
      command += " -Version " + powerShellQuote(options.installVersion);
    if (!options.prefix.empty())
      command += " -Prefix " + powerShellQuote(options.prefix);
    command += "\"";
#else
    std::string command =
        "curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | "
        "sh -s -- --channel " +
        shellQuote(options.channel);
    if (!options.installVersion.empty())
      command += " --version " + shellQuote(options.installVersion);
    if (!options.prefix.empty())
      command += " --prefix " + shellQuote(options.prefix);
#endif
    return std::system(command.c_str()) == 0 ? 0 : 2;
  }
  fs::path prefix = options.prefix;
  if (prefix.empty()) {
#if defined(_WIN32)
    if (const char *base = std::getenv("LOCALAPPDATA"))
      prefix = fs::path(base) / "Kyna";
#else
    if (const char *home = std::getenv("HOME"))
      prefix = fs::path(home) / ".local";
#endif
  }
  if (prefix.empty()) {
    errors << "ky self uninstall: cannot determine installation prefix\n";
    return 2;
  }
  const auto manifest = prefix / "share/kyna/install-manifest.txt";
  std::vector<fs::path> installedFiles;
  if (std::ifstream input(manifest); input) {
    std::string line;
    while (std::getline(input, line)) {
      const fs::path relative(line);
      bool safe = !relative.empty() && !relative.is_absolute();
      for (const auto &component : relative)
        if (component == "..")
          safe = false;
      if (safe)
        installedFiles.push_back(prefix / relative);
    }
  }
  if (installedFiles.empty()) {
    installedFiles = {prefix / "bin/ky", prefix / "bin/kyna", prefix / "bin/ky.previous",
                      prefix / "bin/kyna.previous"};
#if defined(_WIN32)
    installedFiles.insert(installedFiles.end(),
                          {prefix / "bin/ky.exe", prefix / "bin/kyna.exe",
                           prefix / "bin/ky.exe.previous", prefix / "bin/kyna.exe.previous"});
#endif
  }
#if defined(_WIN32)
  const auto powerShellLiteral = [](const std::string &value) {
    std::string result{"'"};
    for (const char character : value)
      result += character == '\'' ? "''" : std::string(1, character);
    return result + "'";
  };
  const auto script =
      fs::temp_directory_path() / ("kyna-uninstall-" + std::to_string(GetCurrentProcessId()) + ".ps1");
  std::ofstream helper(script, std::ios::binary | std::ios::trunc);
  if (!helper) {
    errors << "ky self uninstall: cannot create cleanup helper\n";
    return 2;
  }
  helper << "\xEF\xBB\xBF$parent = Get-Process -Id " << GetCurrentProcessId()
         << " -ErrorAction SilentlyContinue\n"
         << "if ($parent) { $parent.WaitForExit() }\n";
  for (const auto &path : installedFiles)
    helper << "Remove-Item -LiteralPath " << powerShellLiteral(path.string())
           << " -Force -ErrorAction SilentlyContinue\n";
  helper << "Remove-Item -LiteralPath " << powerShellLiteral(manifest.string())
         << " -Force -ErrorAction SilentlyContinue\n"
         << "Remove-Item -LiteralPath $MyInvocation.MyCommand.Path -Force -ErrorAction "
            "SilentlyContinue\n";
  helper.close();
  std::string command =
      "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" +
      script.string() + "\"";
  std::vector<char> mutableCommand(command.begin(), command.end());
  mutableCommand.push_back('\0');
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, nullptr, &startup, &process)) {
    errors << "ky self uninstall: cannot launch cleanup helper\n";
    return 2;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  errors << "Scheduled removal of Kyna executables from " << (prefix / "bin").string() << "\n";
  return 0;
#else
  std::error_code ec;
  for (const auto &path : installedFiles)
    fs::remove(path, ec);
  fs::remove(manifest, ec);
  errors << "Removed Kyna executables from " << (prefix / "bin").string() << "\n";
  return 0;
#endif
}

} // namespace kyna::cli
