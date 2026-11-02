/* newlib system-call stubs and bump allocators for the iss simulator. */
#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#include "iss_rt.h"

#undef errno
extern int errno;

/* The Espressif newlib is built with dynamic reentrancy (ESP-IDF supplies a
 * per-task struct _reent); a single-threaded program uses the static one. */
struct _reent *__getreent(void) { return _impure_ptr; }

int _write(int fd, const void *buf, size_t n) {
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = (long)buf;
    register long a2 __asm__("a2") = (long)n;
    register long a7 __asm__("a7") = 64;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return (int)a0;
}

void _exit(int code) {
    iss_ecall(93, code);
    for (;;) {
    }
}

void *_sbrk(ptrdiff_t incr) {
    extern char __heap_start[], __heap_end[];
    static char *brk = __heap_start;
    if (brk + incr > __heap_end || brk + incr < __heap_start) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *old = brk;
    brk += incr;
    return old;
}

int _close(int fd) { return -1; }
int _fstat(int fd, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int fd) { return 1; }
int _lseek(int fd, int off, int whence) { return 0; }
int _read(int fd, void *buf, size_t n) { return 0; }
int _open(const char *path, int flags, int mode) { return -1; }
int _kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}
int _getpid(void) { return 1; }
int _gettimeofday(void *tv, void *tz) { return -1; }
int _unlink(const char *p) { return -1; }
int _link(const char *a, const char *b) { return -1; }

/* ---------------------------------------------------------------- pools */

#define SRAM_POOL_BYTES (288 * 1024)
static uint8_t sram_pool[SRAM_POOL_BYTES] __attribute__((aligned(64)));
static size_t sram_used, psram_used;

static void *bump(uint8_t *base, size_t cap, size_t *used, size_t n, size_t align) {
    uintptr_t p = ((uintptr_t)base + *used + align - 1) & ~(uintptr_t)(align - 1);
    if (p + n > (uintptr_t)base + cap) return NULL;
    *used = p + n - (uintptr_t)base;
    return (void *)p;
}

void *iss_sram_alloc(size_t n, size_t align) { return bump(sram_pool, SRAM_POOL_BYTES, &sram_used, n, align ? align : 4); }

void *iss_psram_alloc(size_t n, size_t align) {
    extern char __psram_free[], __psram_end[];
    return bump((uint8_t *)__psram_free, (size_t)(__psram_end - __psram_free), &psram_used, n, align ? align : 4);
}

void iss_alloc_reset(void) { sram_used = psram_used = 0; }
