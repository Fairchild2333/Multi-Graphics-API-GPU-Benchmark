#include "suite_process.h"
#include <cstdlib>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#ifdef __APPLE__
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <crt_externs.h>
#include <cerrno>
#endif

namespace gpu_bench {
#if !defined(__APPLE__) && !defined(_WIN32)
static std::string QuoteSuiteProcessArg(const std::string& value) {
    std::string escaped = "'";
    for (char c : value) escaped += c == '\'' ? "'\\''" : std::string(1, c);
    return escaped + "'";
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
#elif defined(_WIN32)
    if (arguments.empty()) return ERROR_INVALID_PARAMETER;
    auto wide = [](const std::string& value) {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(length, L'\0');
        if (length) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
        return result;
    };
    // Use CreateProcess quoting, never cmd.exe; paths with spaces and trailing
    // slashes must arrive unchanged. A killed parent closes the job handle and
    // terminates every outstanding worker, including its descendants.
    auto quote = [](const std::wstring& value) {
        std::wstring result = L"\"";
        std::size_t slashes = 0;
        for (wchar_t c : value) {
            if (c == L'\\') { ++slashes; continue; }
            result.append(c == L'"' ? 2 * slashes + 1 : slashes, L'\\');
            result += c; slashes = 0;
        }
        result.append(2 * slashes, L'\\');
        return result + L"\"";
    };
    std::wstring command;
    for (const auto& arg : arguments) {
        if (!command.empty()) command += L' ';
        command += quote(wide(arg));
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return static_cast<int>(GetLastError());
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        const auto error = GetLastError(); CloseHandle(job); return static_cast<int>(error);
    }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED, nullptr, nullptr, &startup, &process)) {
        const auto error = GetLastError(); CloseHandle(job); return static_cast<int>(error);
    }
    if (!AssignProcessToJobObject(job, process.hProcess)) {
        const auto error = GetLastError(); TerminateProcess(process.hProcess, error);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job);
        return static_cast<int>(error);
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        const auto error = GetLastError(); TerminateJobObject(job, error);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job);
        return static_cast<int>(error);
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess); CloseHandle(job);
    return static_cast<int>(code);
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
