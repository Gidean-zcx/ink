#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ink/hal_log.h"
#include "sim.h"
static hal_log_tail_t tail;
hal_log_tail_t *hal_log_tail(void) { return &tail; }
typedef struct { bool core, erase, write, ok; uint32_t off; void *data; size_t n; } io_t;
static void io(void *arg) {
    io_t *q = arg;
    char path[1024];
    const char *base = g_sim.flash_image ? g_sim.flash_image : g_sim.out_dir;
    int n = snprintf(path, sizeof path, "%s.%s", base, q->core ? "coredump" : "logs");
    if (n < 0 || n >= (int)sizeof path) return;
    FILE *f = fopen(path, "r+b");
    if (!f) {
        f = fopen(path, "w+b");
        if (!f) return;
        uint8_t erased[INK_LOG_SECTOR]; memset(erased, 255, sizeof erased);
        for (unsigned i = 0; i < (q->core ? 16u : INK_LOG_BYTES / INK_LOG_SECTOR); i++)
            if (fwrite(erased, 1, sizeof erased, f) != sizeof erased) { fclose(f); return; }
    }
    if (!fseek(f, q->off, SEEK_SET)) {
        if (!q->write) q->ok = fread(q->data, 1, q->n, f) == q->n;
        else {
            uint8_t old[INK_LOG_SECTOR];
            q->ok = q->n <= sizeof old && fread(old, 1, q->n, f) == q->n && !fseek(f, q->off, SEEK_SET);
            for (size_t i = 0; q->ok && i < q->n; i++)
                if (!q->erase && (old[i] & ((uint8_t *)q->data)[i]) != ((uint8_t *)q->data)[i]) q->ok = false;
            if (q->ok) q->ok = fwrite(q->data, 1, q->n, f) == q->n && !fflush(f) && !fsync(fileno(f));
        }
    }
    if (fclose(f)) q->ok = false;
}
bool hal_log_read(bool core, uint32_t off, void *out, size_t n) {
    uint32_t size = core ? 65536u : INK_LOG_BYTES;
    io_t q = {.core = core, .off = off, .data = out, .n = n};
    if (off <= size && n <= size - off) vtk_host_call(io, &q);
    return q.ok;
}
bool hal_log_program(uint32_t off, const void *data, size_t n) {
    io_t q = {.write = true, .off = off, .data = (void *)data, .n = n};
    if (off <= INK_LOG_BYTES && n <= INK_LOG_BYTES - off) vtk_host_call(io, &q);
    return q.ok;
}
bool hal_log_erase(uint32_t off) {
    uint8_t *erased = sim_host_malloc(INK_LOG_SECTOR);
    if (!erased) return false;
    memset(erased, 255, INK_LOG_SECTOR);
    io_t q = {.write = true, .erase = true, .off = off, .data = erased, .n = INK_LOG_SECTOR};
    if (off % INK_LOG_SECTOR == 0 && off < INK_LOG_BYTES) vtk_host_call(io, &q);
    sim_host_free(erased);
    return q.ok;
}
typedef struct { char *out; size_t n; const char *fmt; va_list a; int result; } fmt_t;
static void format_on_host(void *arg) { fmt_t *f = arg; f->result = vsnprintf(f->out, f->n, f->fmt, f->a); }
int hal_log_format(char *out, size_t n, const char *fmt, va_list a) {
    fmt_t f = {.out = out, .n = n, .fmt = fmt};
    va_copy(f.a, a); vtk_host_call(format_on_host, &f); va_end(f.a);
    return f.result;
}
void hal_log_boot_info(char *out, size_t n) { sim_copy_name(out, n, "version=sim commit=" INK_BUILD_COMMIT " partition=sim"); }
static void tasks_on_host(void *arg) {
    fmt_t *f = arg;
    int count = vtk_task_count(), pos = snprintf(f->out, f->n, "tasks=%d stack_low=", count);
    vtk_task_info_t *t = sim_host_malloc((size_t)count * sizeof *t);
    if (!t) return;
    for (int i = 0; i < count; i++) vtk_task_info(i, &t[i]);
    for (int i = 0; i < count && i < 5 && pos >= 0 && (size_t)pos < f->n; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++)
            if (t[j].stack_target_bytes - t[j].stack_used_est_bytes < t[best].stack_target_bytes - t[best].stack_used_est_bytes) best = j;
        vtk_task_info_t tmp = t[i]; t[i] = t[best]; t[best] = tmp;
        pos += snprintf(f->out + pos, f->n - (size_t)pos, " %.16s:%u", t[i].name, t[i].stack_target_bytes > t[i].stack_used_est_bytes ? t[i].stack_target_bytes - t[i].stack_used_est_bytes : 0);
    }
    sim_host_free(t);
}
void hal_log_tasks(char *out, size_t n) { fmt_t f = {.out = out, .n = n}; vtk_host_call(tasks_on_host, &f); }
