#pragma once
#include <string>
#include <vector>

namespace gpu_bench {
// Scope of one CLI suite. Cancellation forwards to the worker process group.
class SuiteProcessScope {
public:
    SuiteProcessScope();
    ~SuiteProcessScope();
    SuiteProcessScope(const SuiteProcessScope&) = delete;
    SuiteProcessScope& operator=(const SuiteProcessScope&) = delete;
private:
    using Handler = void (*)(int);
    Handler oldTerm_ = nullptr;
    Handler oldInt_ = nullptr;
};
int RunSuiteChild(const std::vector<std::string>& arguments);
}  // namespace gpu_bench
