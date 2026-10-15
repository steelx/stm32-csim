#include "firmware.h"
#include "stm32/f446/memory_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Decode explicitly: the host need not have the target's byte order. */
static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}
void firmware_free(firmware_t *f) { free(f->bytes); memset(f, 0, sizeof(*f)); }
bool firmware_load(firmware_t *f, const char *path, char *error, size_t capacity)
{
    memset(f, 0, sizeof(*f));
    FILE *file = fopen(path, "rb");
    if (!file) { snprintf(error, capacity, "Cannot open firmware: %s", path); return false; }
    if (fseek(file, 0, SEEK_END) != 0) {
        snprintf(error, capacity, "Cannot seek firmware file"); fclose(file); return false;
    }
    long length = ftell(file);
    if (length < 8 || (unsigned long)length > F446_FLASH_SIZE) {
        snprintf(error, capacity, "Firmware must contain vectors and fit 512 KiB Flash");
        fclose(file); return false;
    }
    rewind(file);
    f->size = (size_t)length;
    f->bytes = malloc(f->size);
    if (!f->bytes || fread(f->bytes, 1, f->size, file) != f->size) {
        snprintf(error, capacity, "Cannot read firmware"); fclose(file); firmware_free(f); return false;
    }
    fclose(file);
    f->stack = read_le32(f->bytes);
    f->reset = read_le32(f->bytes + 4);
    if (f->stack <= F446_SRAM_BASE || f->stack > F446_SRAM_BASE + F446_SRAM_SIZE ||
        (f->stack & 7U) != 0U || (f->reset & 1U) == 0U ||
        (f->reset & ~1U) < F446_FLASH_BASE ||
        (uint64_t)(f->reset & ~1U) >= (uint64_t)F446_FLASH_BASE + f->size) {
        snprintf(error, capacity, "Invalid initial stack or Thumb Reset_Handler vector");
        firmware_free(f); return false;
    }
    return true;
}
