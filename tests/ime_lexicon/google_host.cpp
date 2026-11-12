/* The S31 host suite opens the exact shipping decoder file, without a simulator. */
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "ink/osal.h"
#include "ink/hal_storage.h"
#include "ink/sys/file.h"
#include "ink/probe.h"
#include "../../system/services/text/vendor/googlepinyin/include/pinyinime.h"
struct hal_file { FILE *fp; };
struct ink_fs_file { size_t pos; };
struct osal_mutex {};
static const char *dictionary;
static std::vector<unsigned char> user;
static const char *tag;
extern "C" {
void *osal_malloc(size_t n, uint32_t caps) { assert(caps == OSAL_MEM_EXTERNAL); return malloc(n); }
void *osal_calloc(size_t n, size_t size, uint32_t caps) { assert(caps == OSAL_MEM_EXTERNAL); return calloc(n, size); }
void *osal_realloc(void *p, size_t n, uint32_t caps) { assert(caps == OSAL_MEM_EXTERNAL); return realloc(p, n); }
void osal_free(void *p) { free(p); }
const char *osal_mem_tag(const char *next) { const char *old = tag; tag = next; return old; }
osal_mutex_t *osal_mutex_create(void) { static osal_mutex_t mutex; return &mutex; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout) { (void)m; (void)timeout; return OSAL_OK; }
void osal_mutex_unlock(osal_mutex_t *m) { (void)m; }
uint64_t osal_now_us(void) { static uint64_t now; return ++now; }
int64_t osal_wall_us(void) { return 0; }
void ink_probe_count(const char *name) { assert(strcmp(name, "text.google_write_failed")); }
int ink_fs_card_hold(void) { return 0; }
void ink_fs_card_release(void) {}
int64_t hal_fs_size(const char *path) { assert(!strcmp(path, "/content/ime/dict_pinyin.dat")); FILE *fp = fopen(dictionary, "rb"); if (!fp) return -1; int e = fseek(fp, 0, SEEK_END); long n = ftell(fp); int closed = fclose(fp); return e || closed ? -1 : n; }
hal_file_t *hal_fs_open(const char *path, int flags) { (void)flags; assert(!strcmp(path, "/content/ime/dict_pinyin.dat")); FILE *fp = fopen(dictionary, "rb"); if (!fp) return nullptr; return new hal_file_t{fp}; }
ssize_t hal_fs_read(hal_file_t *f, void *p, size_t n) { return (ssize_t)fread(p, 1, n, f->fp); }
int hal_fs_close(hal_file_t *f) { int e = fclose(f->fp); delete f; return e; }
int ink_fs_open(const char *path, int mode, ink_fs_file_t **out) { (void)mode; assert(!strcmp(path, "/me/ime/google.user")); if (user.empty()) return INK_FS_ERR_NOT_FOUND; *out = new ink_fs_file_t{}; return 0; }
int64_t ink_fs_read(ink_fs_file_t *f, void *p, size_t n) { n = std::min(n, user.size() - f->pos); memcpy(p, user.data() + f->pos, n); f->pos += n; return (int64_t)n; }
int ink_fs_seek(ink_fs_file_t *f, int64_t off, int whence) { int64_t at = off + (whence == SEEK_SET ? 0 : whence == SEEK_END ? (int64_t)user.size() : (int64_t)f->pos); if (at < 0 || (uint64_t)at > user.size()) return -1; f->pos = (size_t)at; return 0; }
int64_t ink_fs_tell(ink_fs_file_t *f) { return (int64_t)f->pos; }
int ink_fs_close(ink_fs_file_t *f) { delete f; return 0; }
int ink_fs_write_atomic(const char *path, const void *p, size_t n) { assert(!strcmp(path, "/me/ime/google.user")); const auto *b = static_cast<const unsigned char *>(p); user.assign(b, b + n); return 0; }
int ink_fs_remove(const char *path) { (void)path; user.clear(); return 0; }
}
int main(int argc, char **argv) {
    assert(argc == 2); dictionary = argv[1];
    using namespace ime_pinyin;
    assert(im_open_decoder("/content/ime/dict_pinyin.dat", "/me/ime/google.user"));
    const char *key = "woaibeijingtiananmen"; char16 word[41] = {};
    assert(im_search(key, strlen(key)) > 0);
    assert(im_get_candidate(0, word, 41));
    const char16 expected[] = {0x6211, 0x7231, 0x5317, 0x4eac, 0x5929, 0x5b89, 0x95e8, 0};
    assert(!memcmp(word, expected, sizeof expected));
    im_close_decoder();
    puts("same dictionary opened; 64-bit decode and full sentence passed");
}
