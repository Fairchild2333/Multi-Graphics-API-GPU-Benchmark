#ifndef MANGEKYO_WATCH_CPU_BRIDGE_H
#define MANGEKYO_WATCH_CPU_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void wcpu_initialize(const char* absolute_data_directory);
bool wcpu_start(int job, bool smoke_profile); // 0: compute; 1: memory/pages/storage
void wcpu_cancel(void);
bool wcpu_running(void);
int wcpu_exit_code(void);
char* wcpu_poll_output(void);
char* wcpu_load_results(void);
char* wcpu_error(void);
void wcpu_free(char* value);
#ifdef __cplusplus
}
#endif
#endif
