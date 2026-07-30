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

// Embedded Metal host (mirrors Android NativeBridge).
// shader_dir / data_dir must be absolute UTF-8 paths; shader_dir should contain
// particle.metal and gpu_burn.metal (usually copied from the app bundle).
void gpb_init_paths(const char* shader_dir, const char* data_dir);
void gpb_set_metal_layer(void* ca_metal_layer);  // CAMetalLayer* as __bridge void*
bool gpb_start_workload(const char* workload_id, double seconds);  // stream|gpu_burn
void gpb_stop_workload(void);   // -> gpu_bench::RequestStop()
bool gpb_is_running(void);
char* gpb_last_error(void);     // malloc'd; caller gpb_free()
char* gpb_engine_version(void); // malloc'd; caller gpb_free()

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
