#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

typedef struct { void (*function)(void *); void *argument; } thread_start_t;
#ifdef _WIN32
static DWORD WINAPI thread_entry(LPVOID data)
#else
static void *thread_entry(void *data)
#endif
{
    thread_start_t start = *(thread_start_t *)data;
    free(data);
    start.function(start.argument);
    return 0;
}
void app_mutex_init(app_mutex_t *m)
{
#ifdef _WIN32
    InitializeCriticalSection(m);
#else
    (void)pthread_mutex_init(m, NULL);
#endif
}
void app_mutex_destroy(app_mutex_t *m)
{
#ifdef _WIN32
    DeleteCriticalSection(m);
#else
    (void)pthread_mutex_destroy(m);
#endif
}
void app_lock(app_mutex_t *m)
{
#ifdef _WIN32
    EnterCriticalSection(m);
#else
    (void)pthread_mutex_lock(m);
#endif
}
void app_unlock(app_mutex_t *m)
{
#ifdef _WIN32
    LeaveCriticalSection(m);
#else
    (void)pthread_mutex_unlock(m);
#endif
}
bool app_thread_start(app_thread_t *thread, void (*function)(void *), void *argument)
{
    thread_start_t *start = malloc(sizeof(*start));
    if (!start) return false;
    *start = (thread_start_t){function, argument};
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, thread_entry, start, 0, NULL);
    if (*thread) return true;
#else
    if (pthread_create(thread, NULL, thread_entry, start) == 0) return true;
#endif
    free(start);
    return false;
}
void app_thread_join(app_thread_t thread)
{
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    (void)pthread_join(thread, NULL);
#endif
}
void app_sleep(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec duration = {(time_t)(ms / 1000U), (long)(ms % 1000U) * 1000000L};
    while (nanosleep(&duration, &duration) != 0) {}
#endif
}
bool app_asset_path(char *path, size_t capacity, const char *relative)
{
    char executable[4096];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, executable, (DWORD)sizeof(executable));
    if (n == 0U || n >= sizeof(executable)) return false;
#elif defined(__APPLE__)
    uint32_t n = (uint32_t)sizeof(executable);
    if (_NSGetExecutablePath(executable, &n) != 0) return false;
#else
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (n < 0) return false;
    executable[n] = '\0';
#endif
    char *separator = strrchr(executable, '/');
#ifdef _WIN32
    char *backslash = strrchr(executable, '\\');
    if (backslash && (!separator || backslash > separator)) separator = backslash;
#endif
    if (!separator) return false;
    *separator = '\0';
    int length = snprintf(path, capacity, "%s/%s", executable, relative);
    return length >= 0 && (size_t)length < capacity;
}
