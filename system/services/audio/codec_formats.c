/* Decode on the existing media executor; the output queue accepts Opus only. */
#include <math.h>
#include <strings.h>
#include "ink/hal_codec.h"
#include "ogg.h"
#include "vendor/kiikoread/codec_formats.h"
#define DR_MP3_NO_SIMD
#define DR_FLAC_NO_SIMD
#define DR_MP3_NO_STDIO
#define DR_FLAC_NO_STDIO
#define DR_WAV_NO_STDIO
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#define STB_VORBIS_NO_PUSHDATA_API
#include "vendor/kiikoread/codec_io.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../../../third_party/audio_formats/dr_mp3.h"
#include "../../../third_party/audio_formats/dr_flac.h"
#include "../../../third_party/audio_formats/dr_wav.h"
#include "../../../third_party/audio_formats/stb_vorbis.c"
#pragma GCC diagnostic pop

struct ink_format {
    ink_music_file_t *file;
    ogg_reader_t *opus;
    int kind;
    union { drmp3 mp3; drwav wav; drflac *flac; stb_vorbis *vorbis; } dec;
    unsigned rate, channels;
    uint64_t frames, at;
    hal_opus_enc_t *encoder;
    int16_t *samples;
    void *scratch;
    int16_t mono[HAL_OPUS_FRAME_SAMPLES];
    uint8_t packet[HAL_OPUS_MAX_PACKET];
};
static void *alloc_cb(size_t n, void *ctx) { (void)ctx; return ink_music_malloc(n); }
static void *realloc_cb(void *p, size_t n, void *ctx) { (void)ctx; return ink_music_realloc(p, n); }
static void free_cb(void *p, void *ctx) { (void)ctx; osal_free(p); }
static size_t read_cb(void *ctx, void *p, size_t n) { return ink_music_fread(p, 1, n, ctx); }
static drmp3_bool32 mp3_seek(void *f, int off, drmp3_seek_origin origin) { return !ink_music_fseek(f, off, origin == DRMP3_SEEK_SET ? SEEK_SET : origin == DRMP3_SEEK_CUR ? SEEK_CUR : SEEK_END); }
static drwav_bool32 wav_seek(void *f, int off, drwav_seek_origin origin) { return !ink_music_fseek(f, off, origin == DRWAV_SEEK_SET ? SEEK_SET : origin == DRWAV_SEEK_CUR ? SEEK_CUR : SEEK_END); }
static drflac_bool32 flac_seek(void *f, int off, drflac_seek_origin origin) { return !ink_music_fseek(f, off, origin == DRFLAC_SEEK_SET ? SEEK_SET : origin == DRFLAC_SEEK_CUR ? SEEK_CUR : SEEK_END); }
static drmp3_bool32 mp3_tell(void *f, drmp3_int64 *p) { *p = ink_music_ftell(f); return *p >= 0; }
static drflac_bool32 flac_tell(void *f, drflac_int64 *p) { *p = ink_music_ftell(f); return *p >= 0; }
static drwav_bool32 wav_tell(void *f, drwav_int64 *p) { *p = ink_music_ftell(f); return *p >= 0; }
void ink_format_close(ink_format_t *r) {
    if (!r) return;
    if (r->opus) ogg_close(r->opus);
    if (r->kind == 1) drmp3_uninit(&r->dec.mp3);
    if (r->kind == 2) drwav_uninit(&r->dec.wav);
    if (r->kind == 3) drflac_close(r->dec.flac);
    if (r->kind == 4) stb_vorbis_close(r->dec.vorbis);
    if (r->file) ink_music_fclose(r->file);
    hal_opus_enc_free(r->encoder); osal_free(r->samples); osal_free(r->scratch); osal_free(r);
}
int ink_format_open_for(const char *member, const char *path, ink_format_t **out, int *fs_err) {
    *out = NULL; *fs_err = 0;
    ink_format_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL);
    if (!r) return OGG_ERR_MEMORY;
    int e = ogg_open_for(member, path, &r->opus, fs_err);
    if (!e) { *out = r; return OGG_OK; }
    if (e == OGG_ERR_FILE) { osal_free(r); return e; }
    r->file = ink_music_open_for(member, path, fs_err);
    if (*fs_err) { osal_free(r); return OGG_ERR_FILE; }
    const drmp3_allocation_callbacks ma = {NULL, alloc_cb, realloc_cb, free_cb};
    const drwav_allocation_callbacks wa = {NULL, alloc_cb, realloc_cb, free_cb};
    const drflac_allocation_callbacks fa = {NULL, alloc_cb, realloc_cb, free_cb};
    const char *ext = strrchr(path, '.');
    if (ext && !strcasecmp(ext, ".mp3") && drmp3_init(&r->dec.mp3, read_cb, mp3_seek, mp3_tell, NULL, r->file, &ma)) {
        r->kind = 1; r->rate = r->dec.mp3.sampleRate; r->channels = r->dec.mp3.channels; r->frames = drmp3_get_pcm_frame_count(&r->dec.mp3);
    } else if (ext && !strcasecmp(ext, ".wav") && drwav_init(&r->dec.wav, read_cb, wav_seek, wav_tell, r->file, &wa)) {
        r->kind = 2; r->rate = r->dec.wav.sampleRate; r->channels = r->dec.wav.channels; r->frames = r->dec.wav.totalPCMFrameCount;
    } else if (ext && !strcasecmp(ext, ".flac") && (r->dec.flac = drflac_open(read_cb, flac_seek, flac_tell, r->file, &fa))) {
        r->kind = 3; r->rate = r->dec.flac->sampleRate; r->channels = r->dec.flac->channels; r->frames = r->dec.flac->totalPCMFrameCount;
    } else if (ext && (!strcasecmp(ext, ".ogg") || !strcasecmp(ext, ".oga"))) {
        int err = 0; r->scratch = osal_malloc(512 * 1024, OSAL_MEM_EXTERNAL);
        stb_vorbis_alloc arena = {.alloc_buffer = r->scratch, .alloc_buffer_length_in_bytes = 512 * 1024};
        if (r->scratch) r->dec.vorbis = stb_vorbis_open_file(r->file, 0, &err, &arena);
        if (r->dec.vorbis) {
            stb_vorbis_info info = stb_vorbis_get_info(r->dec.vorbis);
            r->kind = 4; r->rate = info.sample_rate; r->channels = (unsigned)info.channels; r->frames = stb_vorbis_stream_length_in_samples(r->dec.vorbis);
        }
    }
    if (!r->kind || r->channels > 2 || !r->channels || r->rate < 8000 || r->rate > 96000 || !r->frames) { ink_format_close(r); return OGG_ERR_FORMAT; }
    r->samples = osal_malloc((size_t)r->rate * 60 / 1000 * r->channels * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    r->encoder = hal_opus_enc_new(24000, 0);
    if (!r->samples || !r->encoder) { ink_format_close(r); return OGG_ERR_MEMORY; }
    *out = r; return OGG_OK;
}
unsigned ink_format_rate(const ink_format_t *r) { return r->rate; }
unsigned ink_format_channels(const ink_format_t *r) { return r->channels; }
uint32_t ink_format_duration_ms(const ink_format_t *r) { return r->opus ? ogg_duration_ms(r->opus) : (uint32_t)(r->frames * 1000 / r->rate); }
uint32_t ink_format_position_ms(const ink_format_t *r) { return r->opus ? ogg_position_ms(r->opus) : (uint32_t)(r->at * 1000 / r->rate); }
size_t ink_format_pcm(ink_format_t *r, int16_t *pcm, size_t frames) {
    size_t n = 0;
    if (r->kind == 1) n = (size_t)drmp3_read_pcm_frames_s16(&r->dec.mp3, frames, pcm);
    if (r->kind == 2) n = (size_t)drwav_read_pcm_frames_s16(&r->dec.wav, frames, pcm);
    if (r->kind == 3) n = (size_t)drflac_read_pcm_frames_s16(r->dec.flac, frames, pcm);
    if (r->kind == 4) n = (size_t)stb_vorbis_get_samples_short_interleaved(r->dec.vorbis, (int)r->channels, pcm, (int)(frames * r->channels));
    r->at += n; return n;
}
uint32_t ink_format_seek(ink_format_t *r, uint32_t ms) {
    if (r->opus) return ogg_seek(r->opus, ms);
    uint64_t at = (uint64_t)ms * r->rate / 1000; bool ok = false;
    if (at > r->frames) at = r->frames;
    if (r->kind == 1) ok = drmp3_seek_to_pcm_frame(&r->dec.mp3, at);
    if (r->kind == 2) ok = drwav_seek_to_pcm_frame(&r->dec.wav, at);
    if (r->kind == 3) ok = drflac_seek_to_pcm_frame(r->dec.flac, at);
    if (r->kind == 4) ok = stb_vorbis_seek(r->dec.vorbis, (unsigned)at);
    if (ok) r->at = at;
    return ink_format_position_ms(r);
}
int ink_format_next_packet(ink_format_t *r, const uint8_t **data, size_t *len) {
    if (r->opus) return ogg_next_packet(r->opus, data, len);
    size_t frames = (size_t)r->rate * 60 / 1000, n = ink_format_pcm(r, r->samples, frames);
    if (!n) return 0;
    memset(r->mono, 0, sizeof r->mono);
    for (unsigned i = 0; i < HAL_OPUS_FRAME_SAMPLES; i++) {
        size_t at = (size_t)i * frames / HAL_OPUS_FRAME_SAMPLES;
        if (at < n) r->mono[i] = r->channels == 1 ? r->samples[at] : (int16_t)(((int)r->samples[2 * at] + r->samples[2 * at + 1]) / 2);
    }
    int size = hal_opus_encode(r->encoder, r->mono, r->packet, sizeof r->packet);
    if (size <= 0) return -1;
    *data = r->packet; *len = (size_t)size; return 1;
}
