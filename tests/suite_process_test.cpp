#include "suite_process.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--worker") {
        const pid_t grandchild = fork();
        if (grandchild == 0) { execl("/bin/sleep", "sleep", "20", nullptr); _exit(1); }
        { std::ofstream out(argv[2]); out << getpid() << ' ' << grandchild; }
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--controller") {
        gpu_bench::SuiteProcessScope scope;
        return gpu_bench::RunSuiteChild({argv[0], "--worker", argv[2]});
    }
    if (gpu_bench::RunSuiteChild({"/usr/bin/true"}) != 0 ||
        gpu_bench::RunSuiteChild({"/usr/bin/false"}) == 0) return 1;
    const std::string path = "/tmp/mangekyo-suite-process-" + std::to_string(getpid());
    const pid_t controller = fork();
    if (controller == 0) { execl(argv[0], argv[0], "--controller", path.c_str(), nullptr); _exit(2); }
    pid_t worker = 0, grandchild = 0;
    for (int i = 0; i < 200; ++i) {
        std::ifstream input(path);
        if (input >> worker >> grandchild && worker > 0 && grandchild > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(controller, SIGTERM);
    int status = 0;
    waitpid(controller, &status, 0);
    bool stopped = false;
    for (int i = 0; worker > 0 && grandchild > 0 && i < 200; ++i) {
        stopped = kill(worker, 0) != 0 && kill(grandchild, 0) != 0;
        if (stopped) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (worker > 0) kill(worker, SIGKILL);
    if (grandchild > 0) kill(grandchild, SIGKILL);
    unlink(path.c_str());
    if (!stopped || !WIFEXITED(status) || WEXITSTATUS(status) != 128 + SIGTERM) return 3;
    std::cout << "Cancelling the controller stopped both worker and grandchild.\n";
}
