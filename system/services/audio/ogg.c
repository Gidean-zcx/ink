/*
 * Ogg Opus, reading and writing (RFC 3533, RFC 7845): pages with their own boundaries and checksums, so a file cut off by
 * a power failure plays up to where it stops, a damaged page is skipped, and a place is found by bisecting the file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/crc.h"
#include "ogg.h"

#define PAGE_MAX (27 + 255 + 255 * 255)
#define HEAD_SIZE 27
#define PAGE_TARGET_MS 1000 /* recording: one page a second */
#define PAGE_BYTES (8 * 1024)
#define BISECT_STEPS 16
#define TAIL_SCAN 70000 /* the last page starts within this of the end */

struct ogg_reader {
    ink_fs_file_t *f;
    int64_t size;
    int64_t data_start;
    uint32_t pre_skip;
    uint32_t duration_ms;
    /* the page in hand */
    uint8_t *page;
    size_t page_len;
    int nseg;
    int seg;          /* next lacing value */
    size_t body_off;  /* of the next packet */
    uint64_t granule; /* at the end of this page */
    uint64_t pos_ms;  /* where the next packet is */
    /* a packet that goes on over pages */
    uint8_t *packet;
    size_t packet_len;
    size_t packet_cap;
};

static uint64_t rd64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

static void wr32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void wr64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* The page at the file's place, whole and with a good checksum: its size. 0: there is none here (the place does not move). < 0: read error. */
static int64_t read_page_here(ogg_reader_t *r) {
    int64_t at = ink_fs_tell(r->f);
    int64_t n = ink_fs_read(r->f, r->page, HEAD_SIZE);
    if (n < 0) return n;
    if (n < HEAD_SIZE || memcmp(r->page, "OggS", 4) || r->page[4] != 0) {
        ink_fs_seek(r->f, at, 0);
        return 0;
    }
    int nseg = r->page[26];
    n = ink_fs_read(r->f, r->page + HEAD_SIZE, (size_t)nseg);
    if (n < 0) return n;
    if (n < nseg) {
        ink_fs_seek(r->f, at, 0);
        return 0;
    }
    size_t body = 0;
    for (int i = 0; i < nseg; i++) body += r->page[HEAD_SIZE + i];
    n = ink_fs_read(r->f, r->page + HEAD_SIZE + nseg, body);
    if (n < 0) return n;
    if ((size_t)n < body) {
        ink_fs_seek(r->f, at, 0);
        return 0;
    }
    size_t total = HEAD_SIZE + (size_t)nseg + body;
    uint32_t want = (uint32_t)r->page[22] | (uint32_t)r->page[23] << 8 | (uint32_t)r->page[24] << 16 | (uint32_t)r->page[25] << 24;
    r->page[22] = r->page[23] = r->page[24] = r->page[25] = 0;
    uint32_t got = ink_crc32_ogg(0, r->page, total);
    if (got != want) {
        ink_probe_count("media.ogg_bad_page");
        ink_fs_seek(r->f, at, 0);
        return 0;
    }
    return (int64_t)total;
}

/* Forward to the next good page: its size, 0 at the end, < 0 on a read error. A byte at a time would be slow on a card: a block at a time looking for the mark. */
static int64_t next_page(ogg_reader_t *r) {
    uint8_t *scan = NULL;
    for (;;) {
        int64_t n = read_page_here(r);
        if (n != 0) {
            osal_free(scan);
            return n;
        }
        int64_t at = ink_fs_tell(r->f);
        if (at >= r->size) {
            osal_free(scan);
            return 0;
        }
        if (!scan) scan = osal_malloc(4096, OSAL_MEM_EXTERNAL);
        if (!scan) return -1;
        ink_fs_seek(r->f, at + 1, 0);
        int64_t got = ink_fs_read(r->f, scan, 4096);
        if (got < 0) {
            osal_free(scan);
            return got;
        }
        int64_t found = -1;
        for (int64_t i = 0; i + 3 < got && found < 0; i++)
            if (scan[i] == 'O' && scan[i + 1] == 'g' && scan[i + 2] == 'g' && scan[i + 3] == 'S') found = i;
        if (found < 0) {
            if (got < 4) {
                ink_fs_seek(r->f, r->size, 0);
                osal_free(scan);
                return 0;
            }
            ink_fs_seek(r->f, at + 1 + got - 3, 0);
        } else ink_fs_seek(r->f, at + 1 + found, 0);
    }
}

static uint64_t granule_ms(const ogg_reader_t *r, uint64_t g) {
    uint64_t samples = g > r->pre_skip ? g - r->pre_skip : 0;
    return samples / OGG_SAMPLES_PER_MS;
}

static void take_page(ogg_reader_t *r) { /* it has just been read */
    r->nseg = r->page[26];
    r->seg = 0;
    r->body_off = HEAD_SIZE + (size_t)r->nseg;
    r->granule = rd64(r->page + 6);
}

/* From the end: the last good page says how long it is. */
static void find_duration(ogg_reader_t *r) {
    int64_t from = r->size > TAIL_SCAN ? r->size - TAIL_SCAN : r->data_start;
    uint64_t best = 0;
    ink_fs_seek(r->f, from, 0);
    for (;;) {
        int64_t n = next_page(r);
        if (n <= 0) break;
        uint64_t g = rd64(r->page + 6);
        if (g != UINT64_MAX) best = g;
    }
    r->duration_ms = (uint32_t)granule_ms(r, best);
}

static int open_for(const char *member, const char *path, ogg_reader_t **out, int *fs_err) {
    ogg_reader_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL);
    uint8_t *page = osal_malloc(PAGE_MAX, OSAL_MEM_EXTERNAL);
    uint8_t *packet = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    if (!r || !page || !packet) {
        osal_free(r);
        osal_free(page);
        osal_free(packet);
        return OGG_ERR_MEMORY;
    }
    r->page = page;
    r->packet = packet;
    r->packet_cap = 4096;
    int e = member ? ink_fs_open_for(member, path, INK_FS_READ, &r->f) : ink_fs_open(path, INK_FS_READ, &r->f);
    if (e != INK_FS_OK) {
        if (fs_err) *fs_err = e;
        ogg_close(r);
        return OGG_ERR_FILE;
    }
    r->size = ink_fs_seek(r->f, 0, 2) == 0 ? ink_fs_tell(r->f) : -1;
    if (r->size < 0 || ink_fs_seek(r->f, 0, 0) != 0) {
        if (fs_err) *fs_err = INK_FS_ERR_IO;
        ogg_close(r);
        return OGG_ERR_FILE;
    }
    /* the first page: OpusHead, one channel; the second: OpusTags */
    int64_t n = read_page_here(r);
    if (n <= 0 || n < HEAD_SIZE + 1 + 19 || memcmp(r->page + HEAD_SIZE + r->page[26], "OpusHead", 8) || r->page[HEAD_SIZE + r->page[26] + 9] != 1) {
        ogg_close(r);
        return OGG_ERR_FORMAT;
    }
    const uint8_t *head = r->page + HEAD_SIZE + r->page[26];
    r->pre_skip = (uint32_t)head[10] | (uint32_t)head[11] << 8;
    n = next_page(r);
    if (n <= 0 || memcmp(r->page + HEAD_SIZE + r->page[26], "OpusTags", 8)) {
        ogg_close(r);
        return OGG_ERR_FORMAT;
    }
    r->data_start = ink_fs_tell(r->f);
    find_duration(r);
    ink_fs_seek(r->f, r->data_start, 0);
    r->nseg = r->seg = 0;
    r->pos_ms = 0;
    *out = r;
    return OGG_OK;
}

int ogg_open(const char *path, ogg_reader_t **out, int *fs_err) { return open_for(NULL, path, out, fs_err); }

int ogg_open_for(const char *member, const char *path, ogg_reader_t **out, int *fs_err) {
    return open_for(member, path, out, fs_err);
}

void ogg_close(ogg_reader_t *r) {
    if (!r) return;
    if (r->f) ink_fs_close(r->f);
    osal_free(r->page);
    osal_free(r->packet);
    osal_free(r);
}

uint32_t ogg_duration_ms(const ogg_reader_t *r) { return r->duration_ms; }
uint32_t ogg_position_ms(const ogg_reader_t *r) { return (uint32_t)r->pos_ms; }

int ogg_next_packet(ogg_reader_t *r, const uint8_t **data, size_t *len) {
    for (;;) {
        if (r->seg >= r->nseg) {
            int64_t n = next_page(r);
            if (n < 0) return (int)n;
            if (n == 0) return 0;
            take_page(r);
        }
        /* the lacing values of one packet */
        size_t size = 0;
        int s = r->seg;
        bool ended = false;
        while (s < r->nseg) {
            uint8_t lace = r->page[HEAD_SIZE + s++];
            size += lace;
            if (lace < 255) {
                ended = true;
                break;
            }
        }
        const uint8_t *p = r->page + r->body_off;
        r->seg = s;
        r->body_off += size;
        if (!ended || r->packet_len) { /* over pages: put together */
            if (r->packet_len + size > r->packet_cap) {
                r->packet_len = 0; /* longer than any frame of ours: not one */
                continue;
            }
            memcpy(r->packet + r->packet_len, p, size);
            r->packet_len += size;
            if (!ended) continue;
            *data = r->packet;
            *len = r->packet_len;
            r->packet_len = 0;
        } else {
            *data = p;
            *len = size;
        }
        if (!*len) continue;
        r->pos_ms += OGG_FRAME_MS;
        if (r->seg >= r->nseg && r->granule != UINT64_MAX) r->pos_ms = granule_ms(r, r->granule); /* the page says */
        return 1;
    }
}

uint32_t ogg_seek(ogg_reader_t *r, uint32_t ms) {
    int64_t lo = r->data_start, hi = r->size;
    int64_t best = r->data_start;
    uint64_t best_end_ms = 0;
    for (int step = 0; step < BISECT_STEPS && hi - lo > 2048; step++) {
        int64_t mid = lo + (hi - lo) / 2;
        ink_fs_seek(r->f, mid, 0);
        int64_t page_at = -1;
        int64_t n = next_page(r);
        if (n > 0) page_at = ink_fs_tell(r->f) - n;
        if (n <= 0) {
            hi = mid;
            continue;
        }
        uint64_t end_ms = granule_ms(r, rd64(r->page + 6));
        if (end_ms <= ms) {
            lo = page_at + n;
            best = lo;
            best_end_ms = end_ms;
        } else hi = mid;
    }
    /* from the last page that ends before it, forward */
    ink_fs_seek(r->f, best, 0);
    r->nseg = r->seg = 0;
    r->packet_len = 0;
    r->pos_ms = best_end_ms;
    for (;;) {
        int64_t at = ink_fs_tell(r->f);
        int64_t n = next_page(r);
        if (n <= 0) break;
        uint64_t end_ms = granule_ms(r, rd64(r->page + 6));
        if (end_ms > ms) {
            ink_fs_seek(r->f, at, 0);
            break;
        }
        r->pos_ms = end_ms;
    }
    r->nseg = r->seg = 0;
    return (uint32_t)r->pos_ms;
}

/* ---- writing ---- */

struct ogg_writer {
    ink_fs_file_t *f;
    uint32_t serial, seq;
    uint8_t lace[255];
    int nlace;
    uint8_t *body;
    size_t body_len;
    uint32_t page_ms, total_ms;
};

static int write_page(ogg_writer_t *w, uint8_t flags, uint64_t granule, const uint8_t *lace, int nlace, const uint8_t *body, size_t body_len) {
    uint8_t *p = osal_malloc(HEAD_SIZE + (size_t)nlace + body_len, OSAL_MEM_EXTERNAL);
    if (!p) return INK_FS_ERR_IO;
    memcpy(p, "OggS", 4);
    p[4] = 0;
    p[5] = flags;
    wr64(p + 6, granule);
    wr32(p + 14, w->serial);
    wr32(p + 18, w->seq++);
    wr32(p + 22, 0);
    p[26] = (uint8_t)nlace;
    memcpy(p + HEAD_SIZE, lace, (size_t)nlace);
    memcpy(p + HEAD_SIZE + nlace, body, body_len);
    size_t total = HEAD_SIZE + (size_t)nlace + body_len;
    wr32(p + 22, ink_crc32_ogg(0, p, total));
    int64_t n = ink_fs_write(w->f, p, total);
    osal_free(p);
    if (n < 0) return (int)n;
    return (size_t)n == total ? 0 : INK_FS_ERR_NO_SPACE;
}

static int flush_page(ogg_writer_t *w, uint8_t flags) {
    if (!w->nlace && !(flags & 4)) return 0;
    int r = write_page(w, flags, (uint64_t)w->total_ms * OGG_SAMPLES_PER_MS, w->lace, w->nlace, w->body, w->body_len);
    w->nlace = 0;
    w->body_len = 0;
    w->page_ms = 0;
    return r;
}

static int create_for(const char *member, const char *path, ogg_writer_t **out, int *fs_err) {
    ogg_writer_t *w = osal_calloc(1, sizeof *w, OSAL_MEM_EXTERNAL);
    uint8_t *body = osal_malloc(PAGE_BYTES, OSAL_MEM_EXTERNAL);
    if (!w || !body) {
        osal_free(w);
        osal_free(body);
        return OGG_ERR_MEMORY;
    }
    w->body = body;
    w->serial = (uint32_t)(osal_now_us() & 0xffffffffu) | 1u;
    int mode = INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL;
    int e = member ? ink_fs_open_for(member, path, mode, &w->f) : ink_fs_open(path, mode, &w->f);
    if (e != INK_FS_OK) {
        if (fs_err) *fs_err = e;
        osal_free(body);
        osal_free(w);
        return OGG_ERR_FILE;
    }
    static const uint8_t HEAD[19] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 1, 0, 0, 0x80, 0x3e, 0, 0, 0, 0, 0}; /* one channel, 16 kHz */
    static const uint8_t TAGS[24] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'i', 'n', 'k', ' ', 'o', 's', ' ', 0, 0, 0, 0, 0};
    uint8_t lace1 = sizeof HEAD, lace2 = sizeof TAGS;
    int r = write_page(w, 2, 0, &lace1, 1, HEAD, sizeof HEAD);
    if (!r) r = write_page(w, 0, 0, &lace2, 1, TAGS, sizeof TAGS);
    if (r) {
        if (fs_err) *fs_err = r;
        ink_fs_close(w->f);
        osal_free(body);
        osal_free(w);
        return OGG_ERR_FILE;
    }
    *out = w;
    return OGG_OK;
}

int ogg_create(const char *path, ogg_writer_t **out, int *fs_err) { return create_for(NULL, path, out, fs_err); }

int ogg_create_for(const char *member, const char *path, ogg_writer_t **out, int *fs_err) {
    return create_for(member, path, out, fs_err);
}

int ogg_write_packet(ogg_writer_t *w, const uint8_t *data, size_t len, uint32_t ms) {
    int laces = (int)(len / 255) + 1;
    if (w->nlace + laces > 255 || w->body_len + len > PAGE_BYTES) {
        int r = flush_page(w, 0);
        if (r) return r;
    }
    if (len > PAGE_BYTES) return 0; /* not a frame */
    for (int i = 0; i < laces - 1; i++) w->lace[w->nlace++] = 255;
    w->lace[w->nlace++] = (uint8_t)(len % 255);
    memcpy(w->body + w->body_len, data, len);
    w->body_len += len;
    w->total_ms += ms;
    w->page_ms += ms;
    return w->page_ms >= PAGE_TARGET_MS ? flush_page(w, 0) : 0;
}

int ogg_sync(ogg_writer_t *w) {
    int r = flush_page(w, 0);
    if (r) return r;
    return ink_fs_sync(w->f);
}

int ogg_finish(ogg_writer_t *w) {
    int r = flush_page(w, 4); /* the last page */
    int s = ink_fs_sync(w->f);
    int c = ink_fs_close(w->f);
    osal_free(w->body);
    osal_free(w);
    return r ? r : s ? s : c;
}

uint32_t ogg_written_ms(const ogg_writer_t *w) { return w->total_ms; }
