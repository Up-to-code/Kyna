#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <kyna/execution/runtime_capabilities.hpp>
#include "../host_private.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#define KYNA_HOST_POSIX 1
#endif

#if defined(KYNA_HOST_POSIX)
extern char **environ;
#endif

namespace kyna::detail {
namespace {

#if defined(KYNA_HOST_POSIX)

// Owns both the string storage and the char* pointer array so the pointers
// remain valid for the duration of the posix_spawn call.
struct OwnedCStringArray {
  std::vector<std::string> storage;
  std::vector<char *> pointers;
};

// Builds a stable argv array: [program, args..., nullptr] plus the storage that
// keeps the char* entries alive.
OwnedCStringArray buildArgv(const std::string &program, const std::vector<std::string> &args) {
  OwnedCStringArray owned;
  owned.storage.reserve(args.size() + 1);
  owned.storage.push_back(program);
  for (const auto &argument : args)
    owned.storage.push_back(argument);
  owned.pointers.reserve(owned.storage.size() + 1);
  for (auto &entry : owned.storage)
    owned.pointers.push_back(entry.data());
  owned.pointers.push_back(nullptr);
  return owned;
}

OwnedCStringArray buildEnv(const std::map<std::string, std::string> &overrides) {
  // Inherit the current environment, then apply explicit overrides. This keeps
  // PATH and other variables intact while still honoring the requested values.
  std::map<std::string, std::string> merged;
  if (environ) {
    for (char **entry = environ; *entry; ++entry) {
      std::string text(*entry);
      const auto eq = text.find('=');
      if (eq != std::string::npos)
        merged[text.substr(0, eq)] = text.substr(eq + 1);
    }
  }
  for (const auto &[name, value] : overrides)
    merged[name] = value;

  OwnedCStringArray owned;
  owned.storage.reserve(merged.size() + 1);
  for (const auto &[name, value] : merged)
    owned.storage.push_back(name + "=" + value);
  owned.storage.push_back({});
  owned.pointers.reserve(owned.storage.size());
  for (auto &entry : owned.storage)
    owned.pointers.push_back(entry.empty() ? nullptr : entry.data());
  return owned;
}

// Reads both pipes concurrently with poll so large output on either stream
// cannot deadlock on a full pipe buffer while the other is being drained.
void readBothPipes(int stdoutFd, int stderrFd, std::string &stdoutText, std::string &stderrText) {
  bool stdoutOpen = stdoutFd >= 0;
  bool stderrOpen = stderrFd >= 0;
  char buffer[4096];
  while (stdoutOpen || stderrOpen) {
    pollfd descriptors[2];
    nfds_t count = 0;
    if (stdoutOpen) {
      descriptors[count].fd = stdoutFd;
      descriptors[count].events = POLLIN;
      descriptors[count++].revents = 0;
    }
    if (stderrOpen) {
      descriptors[count].fd = stderrFd;
      descriptors[count].events = POLLIN;
      descriptors[count++].revents = 0;
    }
    if (::poll(descriptors, count, -1) < 0)
      break;
    for (nfds_t i = 0; i < count; ++i) {
      const int fd = descriptors[i].fd;
      const auto events = descriptors[i].revents;
      if (events & POLLIN) {
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n > 0) {
          (fd == stdoutFd ? stdoutText : stderrText).append(buffer, static_cast<std::size_t>(n));
          continue;
        }
      }
      if ((events & (POLLHUP | POLLERR | POLLNVAL)) || (events & POLLIN)) {
        if (fd == stdoutFd)
          stdoutOpen = false;
        else
          stderrOpen = false;
      }
    }
  }
}

#endif

} // namespace

class LocalProcess final : public ProcessPort {
public:
  int run(const std::string &command) override { return std::system(command.c_str()); }
  std::optional<std::string> environment(const std::string &name) override {
    const auto *value = std::getenv(name.c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
  }

  ProcessResult spawn(const ProcessConfig &config) override {
    ProcessResult result;
#if defined(KYNA_HOST_POSIX)
    int stdoutPipe[2]{-1, -1};
    int stderrPipe[2]{-1, -1};
    if (config.captureOutput) {
      if (pipe(stdoutPipe) != 0) {
        result.failedToStart = true;
        result.startError = "create stdout pipe: " + std::string(std::strerror(errno));
        return result;
      }
      if (pipe(stderrPipe) != 0) {
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        result.failedToStart = true;
        result.startError = "create stderr pipe: " + std::string(std::strerror(errno));
        return result;
      }
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
      result.failedToStart = true;
      result.startError = "initialize spawn file actions";
      return result;
    }
    if (config.captureOutput) {
      posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);
      posix_spawn_file_actions_adddup2(&actions, stderrPipe[1], STDERR_FILENO);
      posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
      posix_spawn_file_actions_addclose(&actions, stderrPipe[0]);
    }
    if (!config.workingDir.empty())
#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&                         \
    __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
      posix_spawn_file_actions_addchdir(&actions, config.workingDir.c_str());
#else
      posix_spawn_file_actions_addchdir_np(&actions, config.workingDir.c_str());
#endif

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);

    const auto argv = buildArgv(config.program, config.args);
    const auto envp = buildEnv(config.env);
    pid_t child = -1;
    const int spawnError =
        posix_spawn(&child, config.program.c_str(), &actions, &attributes, argv.pointers.data(),
                    envp.pointers.data());
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if (config.captureOutput) {
      close(stdoutPipe[1]);
      close(stderrPipe[1]);
    }

    if (spawnError != 0) {
      result.failedToStart = true;
      result.startError = "spawn '" + config.program + "': " +
                          std::string(std::strerror(spawnError));
      if (config.captureOutput) {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
      }
      return result;
    }

    if (config.captureOutput) {
      readBothPipes(stdoutPipe[0], stderrPipe[0], result.stdoutText, result.stderrText);
      close(stdoutPipe[0]);
      close(stderrPipe[0]);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
      result.exitCode = -1;
      return result;
    }
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#else
    (void)config;
    result.failedToStart = true;
    result.startError = "vector spawning is unsupported on this platform";
#endif
    return result;
  }
};

std::shared_ptr<ProcessPort> makeLocalProcess() {
  return std::make_shared<LocalProcess>();
}

} // namespace kyna::detail

namespace kyna {

ProcessResult ProcessPort::spawn(const ProcessConfig &) {
  ProcessResult result;
  result.failedToStart = true;
  result.startError = "process adapter does not support vector spawning";
  return result;
}

} // namespace kyna
