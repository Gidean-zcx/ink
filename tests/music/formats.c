/* Host decoder/reference checks; no device or real codec is involved. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "ink/osal.h"
#include "ink/hal_codec.h"
#include "../../system/services/audio/ogg.h"
#include "../../system/services/audio/vendor/kiikoread/codec_formats.h"
#include "../../system/services/audio/vendor/kiikoread/music_lyrics.h"
void ink_music_tags(const char *, char *, char *, char *, size_t);
void music_tags_read(const char *, char *, size_t, char *, size_t, uint32_t *);
size_t music_tags_lyrics(const char *, char *, size_t);
struct ink_fs_file { FILE *file; };
typedef union { max_align_t align; size_t size; } allocation_t;
static size_t live, peak;
void *osal_malloc(size_t n, uint32_t caps) {
    (void)caps; allocation_t *a = malloc(sizeof *a + n);
    if (!a) return NULL;
    a->size = n; live += n; if (live > peak) peak = live; return a + 1;
}
void *osal_calloc(size_t n, size_t s, uint32_t c) { void *p = osal_malloc(n * s, c); if (p) memset(p, 0, n * s); return p; }
void osal_free(void *p) { if (p) { allocation_t *a = (allocation_t *)p - 1; live -= a->size; free(a); } }
void *osal_realloc(void *p, size_t n, uint32_t c) {
    if (!p) return osal_malloc(n, c);
    allocation_t *a = (allocation_t *)p - 1; size_t old = a->size;
    void *q = osal_malloc(n, c); if (q) { memcpy(q, p, n < old ? n : old); osal_free(p); } return q;
}
int ink_fs_open(const char *path, int mode, ink_fs_file_t **out) {
    (void)mode; *out = NULL; FILE *f = fopen(path, "rb"); if (!f) return INK_FS_ERR_NOT_FOUND;
    *out = malloc(sizeof **out); assert(*out); (*out)->file = f; return 0;
}
int ink_fs_open_for(const char *m, const char *p, int mode, ink_fs_file_t **o) { (void)m; return ink_fs_open(p, mode, o); }
int64_t ink_fs_read(ink_fs_file_t *f, void *p, size_t n) { size_t got = fread(p, 1, n, f->file); return ferror(f->file) ? -1 : (int64_t)got; }
int ink_fs_seek(ink_fs_file_t *f, int64_t off, int origin) { return fseek(f->file, (long)off, origin); }
int64_t ink_fs_tell(ink_fs_file_t *f) { return ftell(f->file); }
int ink_fs_close(ink_fs_file_t *f) { int e = fclose(f->file); free(f); return e; }
int ogg_open_for(const char *m, const char *p, ogg_reader_t **r, int *e) { (void)m; (void)p; *r = NULL; *e = 0; return OGG_ERR_FORMAT; }
void ogg_close(ogg_reader_t *r) { (void)r; }
uint32_t ogg_duration_ms(const ogg_reader_t *r) { (void)r; return 0; }
uint32_t ogg_position_ms(const ogg_reader_t *r) { (void)r; return 0; }
uint32_t ogg_seek(ogg_reader_t *r, uint32_t ms) { (void)r; return ms; }
int ogg_next_packet(ogg_reader_t *r, const uint8_t **p, size_t *n) { (void)r; (void)p; (void)n; return 0; }
hal_opus_enc_t *hal_opus_enc_new(int bitrate, int complexity) { (void)bitrate; (void)complexity; return osal_malloc(1, 0); }
void hal_opus_enc_free(hal_opus_enc_t *e) { osal_free(e); }
int hal_opus_encode(hal_opus_enc_t *e, const int16_t *p, uint8_t *out, int cap) { (void)e; (void)p; (void)cap; out[0] = 1; return 1; }
static void check_file(const char *root, const char *name, int tolerance) {
    char path[1024], ref[1100]; snprintf(path, sizeof path, "%s/%s", root, name); snprintf(ref, sizeof ref, "%s.pcm", path);
    ink_format_t *r = NULL; int error; size_t baseline = live; peak = live;
    assert(ink_format_open_for("test", path, &r, &error) == 0 && r);
    FILE *f = fopen(ref, "rb"); assert(f); size_t channels = ink_format_channels(r), total = 0; int worst = 0;
    int16_t pcm[2048], expected[2048]; clock_t start = clock(), max_block = 0; size_t n;
    for (;;) {
        clock_t block_start = clock(); n = ink_format_pcm(r, pcm, 1024); clock_t elapsed = clock() - block_start;
        if (elapsed > max_block) max_block = elapsed;
        if (!n) break;
        assert(fread(expected, channels * sizeof *expected, n, f) == n);
        for (size_t i = 0; i < n * channels; i++) { int d = abs((int)pcm[i] - expected[i]); if (d > worst) worst = d; }
        total += n;
    }
    double per_s = (double)(clock() - start) / CLOCKS_PER_SEC * ink_format_rate(r) / total;
    printf("%s: frames=%zu max_error=%d tolerance=%d decode_ms_per_audio_s=%.4f peak_bytes=%zu max_1024_frame_ms=%.4f\n", name, total, worst, tolerance, per_s * 1000, peak - baseline, (double)max_block * 1000 / CLOCKS_PER_SEC);
    assert(worst <= tolerance && total > 0 && fgetc(f) == EOF); assert(fclose(f) == 0);
    assert(ink_format_seek(r, 1000) == 1000);
    n = ink_format_pcm(r, pcm, 128); assert(n == 128);
    f = fopen(ref, "rb"); assert(f); assert(fseek(f, (long)(ink_format_rate(r) * channels * 2), SEEK_SET) == 0);
    assert(fread(expected, channels * 2, 128, f) == 128); assert(fclose(f) == 0);
    for (size_t i = 0; i < 128 * channels; i++) assert(abs((int)pcm[i] - expected[i]) <= tolerance);
    ink_format_close(r); assert(live == baseline);
    char title[96], artist[96], album[96]; ink_music_tags(path, title, artist, album, sizeof title);
    if (!strstr(name, ".wav")) {
        assert(!strcmp(title, "合成音调")); assert(!strcmp(artist, "测试歌手")); assert(!strcmp(album, "原创测试"));
    }
    if (strstr(name, ".flac")) { /* music-3: the STREAMINFO duration, and embedded lyrics asked without a title buffer */
        uint32_t ms = 0; char lyrics[256]; music_tags_read(path, title, sizeof title, artist, sizeof artist, &ms);
        assert(ms >= 2900 && ms <= 3100); (void)music_tags_lyrics(path, lyrics, sizeof lyrics);
    }
    music_lyrics_load(path, 3000); assert(!strcmp(music_lyrics_at(0), "第一句")); assert(!strcmp(music_lyrics_at(1500), "第二句")); assert(!strcmp(music_lyrics_at(2500), "第三句"));
    music_lyrics_clear(); assert(!music_lyrics_at(1000)[0]);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *const names[] = {"tone-16000-1.wav", "tone-16000-1.flac", "tone-16000-1.ogg", "tone-16000-1.mp3", "tone-44100-2.wav", "tone-44100-2.flac", "tone-44100-2.ogg", "tone-44100-2.mp3"};
    /* The upstream lyric pool is retained by design; verify decoder releases independently. */
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        check_file(argv[1], names[i], strstr(names[i], ".mp3") ? 3 : strstr(names[i], ".ogg") ? 1 : 0);
    }
    ink_format_t *r = NULL; int error; char path[1024]; snprintf(path, sizeof path, "%s/bad.mp3", argv[1]); size_t before = live;
    assert(ink_format_open_for("test", path, &r, &error) != 0 && !r && live == before);
    char a[96], b[96], c[96]; ink_music_tags(path, a, b, c, sizeof a); assert(!b[0] && !c[0]);
    puts("music formats: passed"); return 0;
}
