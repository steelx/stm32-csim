#include "app/webview_bridge.h"
#include "app/platform.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char firmware[4096];
    if (!app_asset_path(firmware, sizeof(firmware), "firmware.bin")) return 1;
    int smoke_test = 0, bundled_source = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke-test") == 0) smoke_test = 1;
        else if (strcmp(argv[i], "--firmware") == 0 && i + 1 < argc) {
            bundled_source = 0;
            int n = snprintf(firmware, sizeof(firmware), "%s", argv[++i]);
            if (n < 0 || (size_t)n >= sizeof(firmware)) return 1;
        } else {
            fprintf(stderr, "Usage: stm32-csim [--firmware image.bin] [--smoke-test]\n");
            return 1;
        }
    }
    return app_run(firmware, smoke_test, bundled_source);
}
