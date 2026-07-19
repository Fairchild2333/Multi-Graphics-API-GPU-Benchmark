// GpuBenchBridge.h — Pure C interface for Swift ↔ C++ engine communication.
// This header is included via the Bridging-Header.h so Swift can call these.

#ifndef GpuBenchBridge_h
#define GpuBenchBridge_h

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Set the working directory to the repository root (needed for results/ path).
void gpb_set_working_dir(const char* path);

/// Enumerate GPUs. Returns a JSON string (caller must free with gpb_free).
/// Format: [{"index":0,"name":"Apple M4 Pro","vram":38338,"metal":true,"vulkan":false,"opengl":true}]
char* gpb_list_gpus(void);

/// Run the benchmark engine with the given argv.
/// stdout/stderr are captured and sent line-by-line via onLine callback.
/// This function blocks until the benchmark completes.
typedef void (*gpb_line_callback)(const char* line, void* ctx);
int gpb_run(const char* const* argv, int argc,
            gpb_line_callback onLine, void* ctx);

/// Load saved benchmark results as JSON string (caller must free with gpb_free).
char* gpb_load_results(void);

/// Delete a single result by ID. Returns true on success.
bool gpb_delete_result(const char* resultId);

/// Delete all results. Returns true on success.
bool gpb_clear_results(void);

/// Free a string returned by gpb_list_gpus / gpb_load_results / path helpers.
void gpb_free(char* ptr);

/// Platform data-root subfolders (caller must free with gpb_free).
/// macOS default: ~/Library/Application Support/GpuComputeBenchmark/{results,captures}
char* gpb_results_dir(void);
char* gpb_captures_dir(void);

#ifdef __cplusplus
}
#endif

#endif /* GpuBenchBridge_h */
