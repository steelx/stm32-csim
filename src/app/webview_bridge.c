#include "webview_bridge.h"
#include "platform.h"
#include "emulator/cpu.h"
#include <webview/api.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    device_t device;
    uint32_t pc;
    uint64_t slices, generation;
    bool running, fault, ready;
    char error[256];
} snapshot_t;
typedef struct {
    webview_t view;
    app_mutex_t mutex;
    firmware_t firmware;
    bool quit, desired_run, reset;
    unsigned steps;
    snapshot_t snapshot;
    bool smoke_test, smoke_passed, bundled_source;
} app_t;

/* Exactly one thread owns Unicorn and the mutable MCU/circuit. Commands and
 * immutable snapshots cross the mutex; instruction execution never holds it. */
static void simulation_worker(void *argument)
{
    app_t *app = argument;
    cpu_t cpu;
    bool opened = cpu_open(&cpu, &app->firmware);
    uint64_t generation = 0;
    for (;;) {
        app_lock(&app->mutex);
        bool quit = app->quit, reset = app->reset, run = app->desired_run;
        bool step = app->steps > 0;
        app->reset = false;
        if (step) --app->steps;
        app_unlock(&app->mutex);
        if (quit) break;
        if (reset) {
            cpu_close(&cpu);
            opened = cpu_open(&cpu, &app->firmware);
            ++generation;
        }
        if (opened && !reset && (run || step)) (void)cpu_slice(&cpu, run ? 100000U : 1U);
        snapshot_t snapshot = {0};
        snapshot.device = cpu.device;
        snapshot.pc = cpu_pc(&cpu);
        snapshot.slices = cpu.slices;
        snapshot.generation = generation;
        snapshot.running = run && !cpu.fault && !reset;
        snapshot.fault = cpu.fault;
        snapshot.ready = true;
        memcpy(snapshot.error, cpu.error, sizeof(snapshot.error));
        app_lock(&app->mutex);
        app->snapshot = snapshot;
        if (cpu.fault) app->desired_run = false;
        app_unlock(&app->mutex);
        app_sleep(run ? 8U : 2U);
    }
    cpu_close(&cpu);
}

/* Small bounded JSON writer: no source text is evaluated as JavaScript. */
typedef struct { char *data; size_t used, capacity; bool failed; } json_t;
static void append(json_t *j, const char *format, ...)
{
    if (j->failed) return;
    va_list args;
    va_start(args, format);
    int n = vsnprintf(j->data + j->used, j->capacity - j->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= j->capacity - j->used) j->failed = true;
    else j->used += (size_t)n;
}
static void json_string(json_t *j, const char *text)
{
    append(j, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\') append(j, "\\%c", *p);
        else if (*p < 32U) append(j, "\\u%04x", (unsigned)*p);
        else append(j, "%c", *p);
    }
    append(j, "\"");
}
static void state_binding(const char *id, const char *request, void *argument)
{
    (void)request;
    app_t *app = argument;
    snapshot_t s;
    app_lock(&app->mutex); s = app->snapshot; app_unlock(&app->mutex);
    char buffer[32768];
    json_t j = {buffer, 0, sizeof(buffer), false};
    append(&j, "{\"ready\":%s,\"running\":%s,\"fault\":%s,\"led\":%s,\"pa5\":%u,\"pc\":%u,\"slices\":%llu,\"generation\":%llu,\"transitions\":%llu,\"accesses\":%llu,\"rcc\":%u,\"moder\":%u,\"odr\":%u,\"error\":",
        s.ready ? "true" : "false", s.running ? "true" : "false", s.fault ? "true" : "false",
        s.device.circuit.led_on ? "true" : "false", (unsigned)s.device.pa5, s.pc,
        (unsigned long long)s.slices, (unsigned long long)s.generation,
        (unsigned long long)s.device.transitions, (unsigned long long)s.device.accesses,
        s.device.rcc.ahb1enr, s.device.gpioa.moder, s.device.gpioa.odr);
    json_string(&j, s.error);
    append(&j, ",\"trace\":[");
    uint64_t first = s.device.accesses > TRACE_CAPACITY ? s.device.accesses - TRACE_CAPACITY : 0;
    for (uint64_t i = first; i < s.device.accesses; ++i) {
        const register_event_t *e = &s.device.trace[i % TRACE_CAPACITY];
        append(&j, "%s{\"sequence\":%llu,\"write\":%s,\"accepted\":%s,\"address\":%u,\"value\":%u,\"before\":%u,\"after\":%u,\"name\":",
               i == first ? "" : ",", (unsigned long long)e->sequence,
               e->write ? "true" : "false", e->accepted ? "true" : "false", e->address, e->value, e->before, e->after);
        json_string(&j, e->name);
        append(&j, ",\"meaning\":"); json_string(&j, e->meaning); append(&j, "}");
    }
    append(&j, "]}");
    (void)webview_return(app->view, id, j.failed ? 1 : 0, j.failed ? "\"State serialization overflow\"" : buffer);
}
static void command_binding(const char *id, const char *request, void *argument)
{
    app_t *app = argument;
    bool accepted = true;
    app_lock(&app->mutex);
    /* The binding has four exact string messages, not an arbitrary JS/C API. */
    if (strcmp(request, "[\"run\"]") == 0) app->desired_run = true;
    else if (strcmp(request, "[\"pause\"]") == 0) { app->desired_run = false; app->steps = 0; }
    else if (strcmp(request, "[\"reset\"]") == 0) { app->desired_run = false; app->steps = 0; app->reset = true; }
    else if (strcmp(request, "[\"step\"]") == 0) { app->desired_run = false; if (app->steps < 100U) ++app->steps; }
    else accepted = false;
    app_unlock(&app->mutex);
    (void)webview_return(app->view, id, accepted ? 0 : 1, accepted ? "true" : "\"Unknown command\"");
}
static char *read_asset(const char *relative)
{
    char path[4096];
    if (!app_asset_path(path, sizeof(path), relative)) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || n > 1024 * 1024) { fclose(f); return NULL; }
    rewind(f);
    char *text = malloc((size_t)n + 1U);
    if (!text) { fclose(f); return NULL; }
    if (fread(text, 1, (size_t)n, f) != (size_t)n) { free(text); fclose(f); return NULL; }
    text[n] = '\0'; fclose(f); return text;
}
static void source_binding(const char *id, const char *request, void *argument)
{
    (void)request;
    app_t *app = argument;
    if (!app->bundled_source) {
        (void)webview_return(app->view, id, 0, "\"External firmware binary loaded. Source is not bundled with this image.\"");
        return;
    }
    char *source = read_asset("main.c");
    if (!source) { (void)webview_return(app->view, id, 1, "\"Cannot read main.c\""); return; }
    size_t capacity = strlen(source) * 6U + 3U;
    char *buffer = malloc(capacity);
    if (buffer) {
        json_t j = {buffer, 0, capacity, false};
        json_string(&j, source);
        (void)webview_return(app->view, id, 0, buffer);
    } else (void)webview_return(app->view, id, 1, "\"Out of memory\"");
    free(buffer); free(source);
}
static void smoke_binding(const char *id, const char *request, void *argument)
{
    app_t *app = argument;
    app->smoke_passed = strcmp(request, "[true]") == 0;
    printf("WebView smoke test: %s (%s)\n", app->smoke_passed ? "PASS" : "FAIL", request);
    fflush(stdout);
    (void)webview_return(app->view, id, 0, "true");
    (void)webview_terminate(app->view);
}
int app_run(const char *firmware_path, int smoke_test, int bundled_source)
{
    app_t app = {0};
    char error[256] = {0};
    if (!firmware_load(&app.firmware, firmware_path, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error); return 1;
    }
    char *html = read_asset("web/index.html"), *css = read_asset("web/app.css"), *js = read_asset("web/app.js");
    if (!html || !css || !js) {
        fprintf(stderr, "Missing web assets beside executable\n");
        free(html); free(css); free(js); firmware_free(&app.firmware); return 1;
    }
    size_t capacity = strlen(html) + strlen(css) + strlen(js) + 256U;
    char *page = malloc(capacity);
    if (!page) { free(html); free(css); free(js); firmware_free(&app.firmware); return 1; }
    snprintf(page, capacity, "%s<style>%s</style><script>window.CSIM_SMOKE=%s;\n%s</script></body></html>",
             html, css, smoke_test ? "true" : "false", js);
    free(html); free(css); free(js);
    app.view = webview_create(1, NULL);
    if (!app.view) { fprintf(stderr, "Cannot create WebView (check WebView2 runtime)\n"); free(page); firmware_free(&app.firmware); return 1; }
    app.smoke_test = smoke_test != 0;
    app.bundled_source = bundled_source != 0;
    app_mutex_init(&app.mutex);
    (void)webview_set_title(app.view, "STM32-Csim | Bare-metal circuit lab");
    (void)webview_set_size(app.view, 1280, 900, WEBVIEW_HINT_NONE);
    (void)webview_set_size(app.view, 980, 740, WEBVIEW_HINT_MIN);
    (void)webview_bind(app.view, "sim_state", state_binding, &app);
    (void)webview_bind(app.view, "sim_command", command_binding, &app);
    (void)webview_bind(app.view, "sim_source", source_binding, &app);
    if (app.smoke_test) (void)webview_bind(app.view, "sim_smoke_done", smoke_binding, &app);
    (void)webview_set_html(app.view, page); free(page);
    app_thread_t worker;
    if (!app_thread_start(&worker, simulation_worker, &app)) {
        (void)webview_destroy(app.view); app_mutex_destroy(&app.mutex); firmware_free(&app.firmware); return 1;
    }
    /* All WebView calls above and binding callbacks run on the UI thread.
     * JS polls snapshots; the worker NEVER calls a WebView API. */
    (void)webview_run(app.view);
    app_lock(&app.mutex); app.quit = true; app_unlock(&app.mutex);
    app_thread_join(worker);
    (void)webview_destroy(app.view);
    app_mutex_destroy(&app.mutex);
    firmware_free(&app.firmware);
    return app.smoke_test && !app.smoke_passed ? 1 : 0;
}
