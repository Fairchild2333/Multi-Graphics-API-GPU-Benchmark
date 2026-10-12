#include "suite_process.h"
#include <cstdlib>
#include <sstream>
#ifdef __APPLE__
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <crt_externs.h>
#include <cerrno>
#endif

namespace gpu_bench {
#ifndef __APPLE__
static std::string QuoteSuiteProcessArg(const std::string& value) {
#ifdef _WIN32
    std::string escaped = "\"";
    for (char c : value) { if (c == '\"') escaped += '\\'; escaped += c; }
    return escaped + "\"";
#else
    std::string escaped = "'";
    for (char c : value) escaped += c == '\'' ? "'\\''" : std::string(1, c);
    return escaped + "'";
#endif
}
#endif
#ifdef __APPLE__
// A cancelled suite must also stop the worker and its children. No shell is
// involved; arguments (including spaces/metacharacters) go straight to spawn.
static volatile std::sig_atomic_t suiteChildGroup = 0;
static void StopSuiteChild(int signal) {
    const auto group = suiteChildGroup;
    if (group > 0) kill(-group, signal);
    _exit(128 + signal);
}
SuiteProcessScope::SuiteProcessScope()
    : oldTerm_(std::signal(SIGTERM, StopSuiteChild)), oldInt_(std::signal(SIGINT, StopSuiteChild)) {}
SuiteProcessScope::~SuiteProcessScope() {
    std::signal(SIGTERM, oldTerm_);
    std::signal(SIGINT, oldInt_);
}
#else
SuiteProcessScope::SuiteProcessScope() = default;
SuiteProcessScope::~SuiteProcessScope() = default;
#endif

int RunSuiteChild(const std::vector<std::string>& arguments) {
#ifdef __APPLE__
    std::vector<char*> argv;
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    sigset_t stoppedSignals, previousSignals;
    sigemptyset(&stoppedSignals);
    sigaddset(&stoppedSignals, SIGTERM);
    sigaddset(&stoppedSignals, SIGINT);
    sigprocmask(SIG_BLOCK, &stoppedSignals, &previousSignals);
    // Children start with the parent's previous mask, rather than the spawn critical section's mask.
    posix_spawnattr_setsigmask(&attributes, &previousSignals);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
    pid_t child = 0;
    const int spawned = posix_spawn(&child, argv[0], nullptr, &attributes, argv.data(), *_NSGetEnviron());
    posix_spawnattr_destroy(&attributes);
    if (spawned == 0) suiteChildGroup = child;
    sigprocmask(SIG_SETMASK, &previousSignals, nullptr);
    if (spawned != 0) return spawned;
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    suiteChildGroup = 0;
    return waited < 0 ? 1 : status;
#else
    std::ostringstream command;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        if (i) command << ' ';
        command << QuoteSuiteProcessArg(arguments[i]);
    }
    return std::system(command.str().c_str());
#endif
}


}  // namespace gpu_bench
