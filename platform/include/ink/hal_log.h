#ifndef INK_HAL_LOG_H
#define INK_HAL_LOG_H
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define INK_LOG_BYTES (512u * 1024u)
#define INK_LOG_SECTOR 4096u
#define INK_LOG_TAIL 16384u
#define INK_LOG_MAGIC 0x31474f4cu
/* Little endian, one CRC-protected record per erase sector. CRC covers header and payload. */
typedef struct { uint32_t magic, seq, boot, len, crc; } ink_log_record_t;
typedef struct { uint32_t magic, boot, head, used; char data[INK_LOG_TAIL]; } hal_log_tail_t;
static inline uint32_t ink_log_crc(const void *data, size_t n, uint32_t crc) {
    const uint8_t *p = data;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static inline bool ink_log_valid(const void *sector) {
    const ink_log_record_t *r = sector;
    return r->magic == INK_LOG_MAGIC && r->len <= INK_LOG_SECTOR - sizeof *r &&
        r->crc == ~ink_log_crc(r + 1, r->len, ink_log_crc(r, offsetof(ink_log_record_t, crc), ~0u));
}
hal_log_tail_t *hal_log_tail(void);
bool hal_log_read(bool core, uint32_t off, void *out, size_t n);
bool hal_log_erase(uint32_t off);
bool hal_log_program(uint32_t off, const void *data, size_t n);
int hal_log_format(char *out, size_t n, const char *fmt, va_list args);
void hal_log_boot_info(char *out, size_t n);
void hal_log_tasks(char *out, size_t n);
#endif
