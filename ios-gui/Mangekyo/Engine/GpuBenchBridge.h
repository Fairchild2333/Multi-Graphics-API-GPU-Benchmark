// GpuBenchBridge.h — Pure C interface for Swift to execute in-process.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Line-by-line callback function pointer for live stdout forwarding
typedef void (*gpb_line_callback)(const char* line, void* context);

// Platform path setup
void gpb_set_working_dir(const char* dir);

// Probe operations: returns allocated JSON array of GPUs. Must call gpb_free().
char* gpb_list_gpus(void);

// Run the benchmark suite in-process. Mutex-protected to guarantee one run at a time.
// argv/argc are standard command-line args. The live output callback context pointer
// can be used by Swift to match callbacks to objects.
int gpb_run(const char* const argv[], int argc, gpb_line_callback callback, void* context);

// Load, delete and clear JSON results from the sandbox storage.
char* gpb_load_results(void);
bool gpb_delete_result(const char* id);
bool gpb_clear_results(void);

// Path inquiries: returns allocated paths, caller must call gpb_free().
char* gpb_results_dir(void);
char* gpb_captures_dir(void);

// Releases allocated C-strings returned by query methods
void gpb_free(char* ptr);

#ifdef __cplusplus
}
#endif
