// GpuBenchBridge.h — Pure C interface for Swift to execute in-process.

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Keep the C embedding API visible to the hosted regression-test bundle.
#define GPB_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

// Line-by-line callback function pointer for live stdout forwarding
typedef void (*gpb_line_callback)(const char* line, void* context);

// Platform path setup
GPB_API void gpb_set_working_dir(const char* dir);

// Embedded Metal host (mirrors Android NativeBridge).
// shader_dir / data_dir must be absolute UTF-8 paths; shader_dir should contain
// particle.metal, gpu_burn.metal and cinematic_liquid_v2.metal (copied from the app bundle).
GPB_API void gpb_init_paths(const char* shader_dir, const char* data_dir);
GPB_API void gpb_set_metal_layer(void* ca_metal_layer);  // CAMetalLayer* as __bridge void*
// workload_id: stream, stream_headless, gpu_burn, cinematic_liquid or memory_sweep, with its size where it has one:
// "stream?particles=4194304", "gpu_burn?steps=64" (src/embedded_request.h).
GPB_API bool gpb_start_workload(const char* workload_id, double seconds);
GPB_API void gpb_stop_workload(void);   // -> gpu_bench::RequestStop()
GPB_API bool gpb_is_running(void);
// Lines an embedded run printed since the last call, each ending in a line feed.
// malloc'd; caller gpb_free().
GPB_API char* gpb_poll_output(void);
GPB_API char* gpb_last_error(void);     // malloc'd; caller gpb_free()
GPB_API char* gpb_engine_version(void); // malloc'd; caller gpb_free()

// Probe operations: returns allocated JSON array of GPUs. Must call gpb_free().
GPB_API char* gpb_list_gpus(void);

// Run CPU benchmarks in-process; GPU tests use gpb_start_workload. Mutex-protected to guarantee one run at a time.
// argv/argc are standard command-line args. The live output callback context pointer
// can be used by Swift to match callbacks to objects.
GPB_API int gpb_run(const char* const argv[], int argc, gpb_line_callback callback, void* context);

// Load, delete and clear JSON results from the sandbox storage.
GPB_API char* gpb_load_results(void);
GPB_API bool gpb_delete_result(const char* id);
GPB_API bool gpb_clear_results(void);

// Path inquiries: returns allocated paths, caller must call gpb_free().
GPB_API char* gpb_results_dir(void);
GPB_API char* gpb_captures_dir(void);

// Releases allocated C-strings returned by query methods
GPB_API void gpb_free(char* ptr);

#ifdef __cplusplus
}
#endif
