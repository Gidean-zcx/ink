#include <cstdio>
#include <cstring>
#include <ink/hal_storage.h>
#include <ink/sys/app.h>
#ifndef ESP_PLATFORM
extern "C" {
#include <vtk.h>
}
#endif
extern "C" void ink_reader_host_call(void (*fn)(void *), void *arg) {
#ifdef ESP_PLATFORM
    fn(arg);
#else
    vtk_host_call(fn, arg);
#endif
}
struct ink_reader_file { hal_file_t *file; bool error; };
namespace std {
FILE *ink_reader_fopen(const char *path, const char *mode) {
    if (strcmp(mode, "rb") || strncmp(path, "/content/", 9) || strstr(path, "..")) return nullptr;
    auto *f = static_cast<ink_reader_file *>(ink_app_alloc(sizeof(ink_reader_file), false));
    if (!f) return nullptr;
    f->file = hal_fs_open(path, HAL_O_READ); f->error = false;
    if (!f->file) { ink_app_free(f); return nullptr; }
    return reinterpret_cast<FILE *>(f);
}
int ink_reader_fclose(FILE *fp) {
    auto *f = reinterpret_cast<ink_reader_file *>(fp);
    if (f) { hal_fs_close(f->file); ink_app_free(f); }
    return 0;
}
size_t ink_reader_fread(void *out, size_t size, size_t count, FILE *fp) {
    auto *f = reinterpret_cast<ink_reader_file *>(fp);
    if (!size || count > SIZE_MAX / size) return 0;
    auto n = hal_fs_read(f->file, out, size * count);
    if (n < 0) f->error = true;
    return n < 0 ? 0 : static_cast<size_t>(n) / size;
}
int ink_reader_fseek(FILE *fp, long off, int whence) {
    auto *f = reinterpret_cast<ink_reader_file *>(fp);
    int r = hal_fs_seek(f->file, off, whence);
    if (r < 0) f->error = true;
    return r;
}
long ink_reader_ftell(FILE *fp) { return static_cast<long>(hal_fs_tell(reinterpret_cast<ink_reader_file *>(fp)->file)); }
int ink_reader_ferror(FILE *fp) { return reinterpret_cast<ink_reader_file *>(fp)->error; }
}
