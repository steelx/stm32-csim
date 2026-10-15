#ifndef CSIM_FIRMWARE_H
#define CSIM_FIRMWARE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint8_t *bytes; size_t size; uint32_t stack, reset; } firmware_t;
bool firmware_load(firmware_t *firmware, const char *path, char *error, size_t capacity);
void firmware_free(firmware_t *firmware);
#endif
