#ifndef CSIM_PLATFORM_H
#define CSIM_PLATFORM_H
#include <stdbool.h>
#include <stddef.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION app_mutex_t;
typedef HANDLE app_thread_t;
#else
#include <pthread.h>
typedef pthread_mutex_t app_mutex_t;
typedef pthread_t app_thread_t;
#endif
void app_mutex_init(app_mutex_t *mutex);
void app_mutex_destroy(app_mutex_t *mutex);
void app_lock(app_mutex_t *mutex);
void app_unlock(app_mutex_t *mutex);
bool app_thread_start(app_thread_t *thread, void (*function)(void *), void *argument);
void app_thread_join(app_thread_t thread);
void app_sleep(unsigned milliseconds);
bool app_asset_path(char *path, size_t capacity, const char *relative);
#endif
