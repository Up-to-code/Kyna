#include "project_internals.hpp"
#if !defined(_WIN32)
#include <chrono>
#include <csignal>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/wait.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#else
#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <thread>
#include <atomic>
#endif

namespace kyna::cli {

#if !defined(_WIN32)
namespace {

std::atomic_bool devInterrupted{false};
void interruptDev(int) { devInterrupted = true; }

fs::path currentExecutable() {
#if defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) == 0) {
    path.resize(std::char_traits<char>::length(path.c_str()));
    return fs::weakly_canonical(path);
  }
#else
  std::string path(4096, '\0');
  const auto count = readlink("/proc/self/exe", path.data(), path.size());
  if (count > 0) {
    path.resize(static_cast<std::size_t>(count));
    return path;
  }
#endif
  return "ky";
}

std::uint64_t sourceFingerprint(const fs::path &root) {
  std::uint64_t fingerprint = 1469598103934665603ULL;
  std::error_code ec;
  for (const auto &entry :
       fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
    if (!entry.is_regular_file(ec))
      continue;
    const auto name = entry.path().filename().string();
    if (entry.path().extension() != ".kyna" && name != "kyna.toml" && name != "kyna.lock")
      continue;
    const auto stamp = entry.last_write_time(ec).time_since_epoch().count();
    fingerprint ^= static_cast<std::uint64_t>(stamp);
    fingerprint *= 1099511628211ULL;
    fingerprint ^= std::hash<std::string>{}(entry.path().string());
    fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}

pid_t spawnKy(const fs::path &root, const std::vector<std::string> &arguments, bool quiet) {
  const auto executable = currentExecutable();
  const auto child = fork();
  if (child != 0)
    return child;
  (void)chdir(root.c_str());
  std::vector<std::string> storage{executable.string()};
  storage.insert(storage.end(), arguments.begin(), arguments.end());
  if (quiet)
    storage.push_back("--quiet");
  storage.push_back("--no-color");
  storage.push_back("--no-interactive");
  std::vector<char *> argv;
  for (auto &value : storage)
    argv.push_back(value.data());
  argv.push_back(nullptr);
  execv(executable.c_str(), argv.data());
  _exit(127);
}

bool checkProjectProcess(const fs::path &root) {
  const auto child = spawnKy(root, {"check"}, true);
  int status = 0;
  return child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) &&
         WEXITSTATUS(status) == 0;
}

} // namespace

int devProject(const Options &options, std::ostream &errors) {
  const auto root = discoverProject();
  if (root.empty()) {
    errors << "ky dev: no kyna.toml found\n";
    return 2;
  }
  if (!checkProjectProcess(root)) {
    errors << "ky dev: initial check failed; server was not started\n";
    return 1;
  }
  devInterrupted = false;
  const auto previous = std::signal(SIGINT, interruptDev);
  auto child = spawnKy(root, {"serve"}, options.quiet);
  auto fingerprint = sourceFingerprint(root);
  if (!options.quiet)
    errors << "Kyna dev · watching " << root.string()
           << "\nSave a source, manifest, or lockfile to check and restart. Ctrl-C stops.\n";
  while (!devInterrupted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto current = sourceFingerprint(root);
    if (current == fingerprint)
      continue;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    fingerprint = sourceFingerprint(root);
    if (!checkProjectProcess(root)) {
      errors << "Check failed · keeping the last good server running.\n";
      continue;
    }
    if (!options.quiet)
      errors << "Check passed · restarting server.\n";
    if (child > 0) {
      kill(child, SIGTERM);
      (void)waitpid(child, nullptr, 0);
    }
    child = spawnKy(root, {"serve"}, options.quiet);
  }
  if (child > 0) {
    kill(child, SIGTERM);
    (void)waitpid(child, nullptr, 0);
  }
  std::signal(SIGINT, previous);
  return 130;
}

#else
namespace {

std::atomic_bool devInterrupted{false};
BOOL WINAPI interruptDev(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
    devInterrupted = true;
    return TRUE;
  }
  return FALSE;
}

std::uint64_t windowsSourceFingerprint(const fs::path &root) {
  std::uint64_t fingerprint = 1469598103934665603ULL;
  std::error_code ec;
  for (const auto &entry :
       fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
    if (!entry.is_regular_file(ec))
      continue;
    const auto name = entry.path().filename().string();
    if (entry.path().extension() != ".kyna" && name != "kyna.toml" && name != "kyna.lock")
      continue;
    fingerprint ^=
        static_cast<std::uint64_t>(entry.last_write_time(ec).time_since_epoch().count());
    fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}

PROCESS_INFORMATION spawnWindowsKy(const fs::path &root, std::string arguments, bool quiet) {
  char executable[MAX_PATH]{};
  GetModuleFileNameA(nullptr, executable, MAX_PATH);
  std::string command = "\"" + std::string(executable) + "\" " + arguments;
  if (quiet)
    command += " --quiet";
  command += " --no-color --no-interactive";
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  std::vector<char> mutableCommand(command.begin(), command.end());
  mutableCommand.push_back('\0');
  CreateProcessA(executable, mutableCommand.data(), nullptr, nullptr, TRUE,
                 CREATE_NEW_PROCESS_GROUP, nullptr, root.string().c_str(), &startup, &process);
  return process;
}

bool windowsCheck(const fs::path &root) {
  auto process = spawnWindowsKy(root, "check", true);
  if (!process.hProcess)
    return false;
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 2;
  GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return code == 0;
}

void stopWindowsChild(PROCESS_INFORMATION &process) {
  if (!process.hProcess)
    return;
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process.dwProcessId))
    TerminateProcess(process.hProcess, 130);
  if (WaitForSingleObject(process.hProcess, 3000) == WAIT_TIMEOUT)
    TerminateProcess(process.hProcess, 130);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  process = {};
}

} // namespace

int devProject(const Options &options, std::ostream &errors) {
  const auto root = discoverProject();
  if (root.empty()) {
    errors << "ky dev: no kyna.toml found\n";
    return 2;
  }
  if (!windowsCheck(root)) {
    errors << "ky dev: initial check failed; server was not started\n";
    return 1;
  }
  devInterrupted = false;
  SetConsoleCtrlHandler(interruptDev, TRUE);
  auto child = spawnWindowsKy(root, "serve", options.quiet);
  auto fingerprint = windowsSourceFingerprint(root);
  if (!options.quiet)
    errors << "Kyna dev · watching " << root.string()
           << "\nSave a source, manifest, or lockfile to check and restart. Ctrl-C stops.\n";
  while (!devInterrupted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto current = windowsSourceFingerprint(root);
    if (current == fingerprint)
      continue;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    fingerprint = windowsSourceFingerprint(root);
    if (!windowsCheck(root)) {
      errors << "Check failed · keeping the last good server running.\n";
      continue;
    }
    if (!options.quiet)
      errors << "Check passed · restarting server.\n";
    stopWindowsChild(child);
    child = spawnWindowsKy(root, "serve", options.quiet);
  }
  stopWindowsChild(child);
  SetConsoleCtrlHandler(interruptDev, FALSE);
  return 130;
}

#endif

} // namespace kyna::cli
