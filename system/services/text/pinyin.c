/*
 * The pinyin engine: full pinyin, whole-key matches of the longest prefix first (system services design §9.2). No
 * syllable splitting: the data decides ("xian" is both 先 and 西安). The dictionary is a file on the card, read into
 * external RAM the first time it is needed, searched in place, given back when memory is short and nobody is typing.
 * Without a card copy the firmware's asset pack has one, searched in place in mapped flash (no RAM; trial report
 * 2026-11-11: the device had no Chinese because nothing put the file on the card). Without either (no pack entry,
 * a bad file) a small table in the program does.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/crc.h"
#include "ink/sys/file.h"
#include "ink/sys/exec.h"
#include "ink/sys/res.h"
#include "ink/sys/svc.h"
#include "text_internal.h"

#define DICT_MAX (1024u * 1024u)

typedef struct __attribute__((packed)) {
    uint32_t text_off;
    uint16_t key_len, count;
    uint32_t first;
} dict_key_t;

typedef struct __attribute__((packed)) {
    uint32_t text_off;
    uint16_t word_len, freq;
} dict_entry_t;

typedef struct __attribute__((packed)) {
    char magic[4];
    uint32_t nkeys, nentries, keys_off, entries_off, text_off, text_len, crc;
} dict_head_t;

static struct {
    uint8_t *block;
    const dict_key_t *keys;
    const dict_entry_t *entries;
    const char *text;
    uint32_t nkeys;
    bool loading, tried, mapped; /* mapped: the asset pack's copy, nothing to give back */
    void (*ready)(void);
    ink_exec_job_t load_job, ready_job;
} PY INK_PSRAM_BSS;

/* When there is no dictionary: the commonest characters, sorted by key. */
static const struct {
    const char *key, *word;
    uint16_t freq;
} SMALL[] = {
    {"bu", "不", 8700}, {"da", "大", 7800}, {"de", "的", 9000}, {"ge", "个", 8000}, {"guo", "国", 7400}, {"hao", "好", 7500}, {"lai", "来", 7500}, {"ni", "你", 8000},
    {"ren", "人", 8600}, {"shang", "上", 7600}, {"shi", "是", 8900}, {"ta", "他", 8200}, {"wo", "我", 8500}, {"xia", "下", 7000}, {"yi", "一", 8800}, {"you", "有", 8400},
    {"zai", "在", 8300}, {"zhe", "这", 8100}, {"zhong", "中", 7000},
};

/* Everything in it is within it: a bad file must not take the device down. */
static bool dict_ok(const uint8_t *b, size_t n) {
    if (n < sizeof(dict_head_t)) return false;
    dict_head_t h;
    memcpy(&h, b, sizeof h);
    if (memcmp(h.magic, "IKD1", 4) || h.keys_off != sizeof h) return false;
    uint64_t keys_end = (uint64_t)h.keys_off + (uint64_t)h.nkeys * sizeof(dict_key_t), entries_end = (uint64_t)h.entries_off + (uint64_t)h.nentries * sizeof(dict_entry_t);
    if (keys_end != h.entries_off || entries_end != h.text_off || (uint64_t)h.text_off + h.text_len != n) return false;
    if (ink_crc32(0, b + sizeof h, n - sizeof h) != h.crc) return false;
    const dict_key_t *keys = (const dict_key_t *)(b + h.keys_off);
    const dict_entry_t *entries = (const dict_entry_t *)(b + h.entries_off);
    for (uint32_t i = 0; i < h.nkeys; i++) {
        if ((uint64_t)keys[i].text_off + keys[i].key_len > h.text_len || (uint64_t)keys[i].first + keys[i].count > h.nentries) return false;
    }
    for (uint32_t i = 0; i < h.nentries; i++)
        if ((uint64_t)entries[i].text_off + entries[i].word_len > h.text_len) return false;
    return true;
}

static void ready_job(void *arg) { /* UI task */
    (void)arg;
    void (*fn)(void) = PY.ready;
    if (fn) fn();
}

static void load_job(void *arg) { /* exec.bg */
    (void)arg;
    uint8_t *b = NULL;
    bool held = ink_fs_card_hold() == INK_FS_OK; /* on a computer now: the small table until next time */
    int64_t size = held ? hal_fs_size(PY_DICT_PATH) : -1;
    if (size > 0 && size <= DICT_MAX) {
        hal_file_t *f = hal_fs_open(PY_DICT_PATH, HAL_O_READ);
        b = f ? ink_mem_alloc("ime", INK_MEM_PSRAM, (size_t)size) : NULL;
        size_t got = 0;
        while (b && got < (size_t)size) {
            ssize_t r = hal_fs_read(f, b + got, (size_t)size - got);
            if (r <= 0) break;
            got += (size_t)r;
        }
        if (f) hal_fs_close(f);
        ink_fs_card_release();
        held = false;
        if (b && (got != (size_t)size || !dict_ok(b, got))) {
            ink_probe_count("text.dict_bad");
            ink_mem_free(b);
            b = NULL;
        }
    }
    if (held) ink_fs_card_release();
    bool mapped = false;
    if (!b) {
        size_t n = 0;
        const uint8_t *m = hal_asset_map(PY_DICT_ASSET, &n);
        if (m) hal_asset_access(m, n);
        if (m && n <= DICT_MAX && dict_ok(m, n)) {
            b = (uint8_t *)m; /* read only: searched, never written */
            mapped = true;
        } else if (m) {
            ink_probe_count("text.dict_bad");
            hal_asset_unmap(m);
        }
    }
    dict_head_t h = {0};
    if (b) memcpy(&h, b, sizeof h);
    ink_svc_lock();
    PY.loading = false;
    if (b) {
        PY.block = b;
        PY.mapped = mapped;
        PY.nkeys = h.nkeys;
        PY.keys = (const dict_key_t *)(b + h.keys_off);
        PY.entries = (const dict_entry_t *)(b + h.entries_off);
        PY.text = (const char *)(b + h.text_off);
    }
    ink_svc_unlock();
    if (b) {
        ink_probe_count("text.dict_loaded");
        if (mapped) ink_probe_count("text.dict_mapped");
        ink_exec_post(INK_EXEC_UI, &PY.ready_job, ready_job, NULL);
    }
}

void py_want_dict(void (*ready)(void)) {
    ink_svc_lock();
    bool go = !PY.block && !PY.loading;
    if (go) PY.loading = true;
    PY.ready = ready;
    ink_svc_unlock();
    if (go && ink_exec_post(INK_EXEC_BG, &PY.load_job, load_job, NULL) != 0) {
        ink_svc_lock();
        PY.loading = false;
        ink_svc_unlock();
    }
}

bool py_dict_loaded(void) {
    ink_svc_lock();
    bool b = PY.block != NULL;
    ink_svc_unlock();
    return b;
}

/* -1, 0, 1: the key at i against these letters. */
static int key_cmp(uint32_t i, const char *s, size_t n) {
    const dict_key_t *k = &PY.keys[i];
    size_t m = k->key_len < n ? k->key_len : n;
    int c = memcmp(PY.text + k->text_off, s, m);
    return c ? c : k->key_len < n ? -1 : k->key_len > n ? 1 : 0;
}

int py_lookup(const char *composing, py_cand_t *out, int max) {
    size_t len = strlen(composing);
    int got = 0;
    for (size_t n = len; n > 0 && got < max; n--) {
        if (PY.block) {
            uint32_t lo = 0, hi = PY.nkeys;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                int c = key_cmp(mid, composing, n);
                if (c == 0) {
                    const dict_key_t *k = &PY.keys[mid];
                    for (uint32_t e = 0; e < k->count && got < max; e++) {
                        const dict_entry_t *d = &PY.entries[k->first + e];
                        out[got++] = (py_cand_t){.word = PY.text + d->text_off, .word_len = d->word_len, .consumed = (uint8_t)n, .freq = d->freq};
                    }
                    break;
                }
                if (c < 0) lo = mid + 1;
                else hi = mid;
            }
        } else {
            for (size_t i = 0; i < sizeof SMALL / sizeof SMALL[0] && got < max; i++)
                if (strlen(SMALL[i].key) == n && !memcmp(SMALL[i].key, composing, n))
                    out[got++] = (py_cand_t){.word = SMALL[i].word, .word_len = (uint16_t)strlen(SMALL[i].word), .consumed = (uint8_t)n, .freq = SMALL[i].freq};
        }
    }
    return got;
}

static size_t reclaim(void *ctx, size_t want) { /* memory is short: it can be read again */
    (void)ctx;
    (void)want;
    ink_svc_lock();
    if (PY.mapped) { /* flash, not RAM: nothing to give back */
        ink_svc_unlock();
        return 0;
    }
    void *b = PY.block;
    PY.block = NULL;
    PY.keys = NULL;
    PY.entries = NULL;
    PY.text = NULL;
    PY.nkeys = 0;
    ink_svc_unlock();
    if (!b) return 0;
    size_t n = ink_mem_used("ime", INK_MEM_PSRAM);
    ink_mem_free(b);
    ink_probe_count("text.dict_reclaimed");
    return n;
}

void py_init(void) {
    ink_mem_budget("ime", INK_MEM_PSRAM, DICT_MAX);
    ink_mem_reclaimer("ime-dict", 30, reclaim, NULL);
}
