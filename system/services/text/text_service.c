/*
 * The input session (see ink/sys/textinput.h). One value, one composing string; the engine supplies candidates, what the
 * person picks is learned for them unless it is a password. All in the UI task, except what the shared lock guards.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/textinput.h"
#include "text_internal.h"

#define VALUE_DEFAULT 1024
#define VALUE_MAX 4095
#define COMPOSING_MAX 40
#define MAX_CANDS 60

/* What the member picked, by a hash of the word: the count, then the word. */
static const ink_db_def_t USER_FREQ = {.name = "text.user_freq", .title = "输入法学到的词频", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 512, .drop_oldest = true};

static struct {
    int session, next_session;
    ink_text_type_t type;
    ink_text_enter_t enter;
    bool ai_hidden;
    char *value;
    size_t cap;
    char hint[48];
    char composing[COMPOSING_MAX + 1];
    const ink_text_client_t *client;
    void *ctx;
    void (*presenter)(void);
    ink_db_t *freq;
    ink_text_cand_t cands[MAX_CANDS];
    int ncands;
    bool cands_valid;
    int h_focused;
} TS INK_PSRAM_BSS;

static void changed(void) {
    TS.cands_valid = false;
    if (TS.client && TS.client->changed) TS.client->changed(TS.ctx);
    if (TS.presenter) TS.presenter();
}

void text_presenter_kick(void) {
    if (TS.presenter) TS.presenter();
}

static void on_dict_ready(void) { /* UI task: the keyboard asks again */
    TS.cands_valid = false;
    if (TS.session && TS.presenter) TS.presenter();
}

int ink_text_focus(const ink_text_cfg_t *cfg, const ink_text_client_t *client, void *ctx) {
    if (!cfg) return -1;
    if (TS.session) ink_text_blur(TS.session);
    text_voice_clear_notice();
    size_t cap = cfg->max ? (cfg->max < VALUE_MAX ? cfg->max : VALUE_MAX) : VALUE_DEFAULT;
    char *v = osal_malloc(cap + 1, OSAL_MEM_EXTERNAL);
    if (!v) return -1;
    snprintf(v, cap + 1, "%s", cfg->initial ? cfg->initial : "");
    TS.value = v;
    TS.cap = cap;
    TS.type = cfg->type;
    TS.enter = cfg->enter;
    TS.ai_hidden = cfg->ai_hidden;
    snprintf(TS.hint, sizeof TS.hint, "%s", cfg->hint ? cfg->hint : "");
    TS.composing[0] = 0;
    TS.client = client;
    TS.ctx = ctx;
    TS.session = ++TS.next_session;
    TS.cands_valid = false;
    if (cfg->type == INK_TEXT_PLAIN) {
        TS.freq = ink_db_open(&USER_FREQ, NULL);
        py_want_dict(on_dict_ready);
    }
    ink_state_set_bool(TS.h_focused, true);
    ink_probe_count("text.sessions");
    if (TS.presenter) TS.presenter();
    return TS.session;
}

void ink_text_blur(int session) {
    if (!TS.session || (session && session != TS.session)) return;
    text_voice_on_blur();
    const ink_text_client_t *c = TS.client;
    void *ctx = TS.ctx;
    TS.session = 0;
    TS.client = NULL;
    ink_db_close(TS.freq);
    TS.freq = NULL;
    osal_free(TS.value);
    TS.value = NULL;
    TS.composing[0] = 0;
    ink_state_set_bool(TS.h_focused, false);
    if (c && c->ended) c->ended(ctx);
    if (TS.presenter) TS.presenter();
}

bool ink_text_focused(void) { return TS.session != 0; }
ink_text_type_t ink_text_type(void) { return TS.type; }
ink_text_enter_t ink_text_enter_kind(void) { return TS.enter; }
const char *ink_text_hint(void) { return TS.hint; }
bool ink_text_ai_hidden(void) { return TS.ai_hidden; }
const char *ink_text_value(void) { return TS.value ? TS.value : ""; }
const char *ink_text_composing(void) { return TS.composing; }

void ink_text_set_presenter(void (*fn)(void)) { TS.presenter = fn; }

/* As much as fits, whole characters. */
static void append(const char *s) {
    size_t have = strlen(TS.value), add = strlen(s);
    if (have + add > TS.cap) {
        add = TS.cap - have;
        while (add && (s[add] & 0xC0) == 0x80) add--;
        ink_probe_count("text.too_long");
    }
    memcpy(TS.value + have, s, add);
    TS.value[have + add] = 0;
}

static uint32_t word_id(const char *word) {
    uint32_t id = ink_crc32(0, word, strlen(word)) & 0x7FFFFFFF;
    return id ? id : 1;
}

/* How often this member took it. */
static uint32_t picked_count(const char *word) {
    uint32_t n = 0;
    if (!TS.freq) return 0;
    ink_rec_t r;
    if (ink_db_get(TS.freq, word_id(word), &r, &n, sizeof n) != INK_DB_OK) return 0;
    return n;
}

static void learn(const char *word) {
    if (!TS.freq || TS.type == INK_TEXT_PASSWORD) return;
    uint32_t n = picked_count(word) + 1;
    int64_t wall = osal_wall_us();
    ink_db_write_t w = {.id = word_id(word), .sort = wall > 0 ? wall / 1000000 : 0, .data = &n, .len = sizeof n, .flags = INK_DB_LAZY};
    ink_db_put(TS.freq, &w, NULL);
}

static void compute_cands(void) {
    if (TS.cands_valid) return;
    TS.cands_valid = true;
    TS.ncands = 0;
    if (!TS.composing[0]) return;
    py_cand_t *raw = osal_malloc(sizeof *raw * MAX_CANDS, OSAL_MEM_EXTERNAL);
    uint32_t *score = osal_malloc(sizeof *score * MAX_CANDS, OSAL_MEM_EXTERNAL);
    if (!raw || !score) {
        osal_free(raw);
        osal_free(score);
        return;
    }
    ink_svc_lock();
    int n = py_lookup(TS.composing, raw, MAX_CANDS);
    for (int i = 0; i < n; i++) {
        size_t len = raw[i].word_len < sizeof TS.cands[0].word - 1 ? raw[i].word_len : sizeof TS.cands[0].word - 1;
        memcpy(TS.cands[i].word, raw[i].word, len);
        TS.cands[i].word[len] = 0;
        TS.cands[i].consumed = raw[i].consumed;
        score[i] = raw[i].freq;
    }
    ink_svc_unlock();
    /* what this member picks comes first within what is as long; the database has its own lock */
    for (int i = 0; i < n; i++) score[i] += picked_count(TS.cands[i].word) * 2000u;
    for (int i = 1; i < n; i++) { /* a short list: insertion keeps the longest keys first and the order stable */
        ink_text_cand_t c = TS.cands[i];
        uint32_t s = score[i];
        int j = i - 1;
        while (j >= 0 && TS.cands[j].consumed == c.consumed && score[j] < s) {
            TS.cands[j + 1] = TS.cands[j];
            score[j + 1] = score[j];
            j--;
        }
        TS.cands[j + 1] = c;
        score[j + 1] = s;
    }
    TS.ncands = n;
    osal_free(raw);
    osal_free(score);
}

int ink_text_candidates(int first, ink_text_cand_t *out, int max) {
    if (!TS.session) return 0;
    compute_cands();
    int n = 0;
    for (int i = first; i < TS.ncands && n < max; i++) out[n++] = TS.cands[i];
    return n;
}

/* What is being composed goes in as typed. */
static void flush_composing(void) {
    if (!TS.composing[0]) return;
    append(TS.composing);
    TS.composing[0] = 0;
}

void ink_text_pick(int index) {
    text_voice_clear_notice();
    if (!TS.session) return;
    compute_cands();
    if (index < 0 || index >= TS.ncands) return;
    ink_text_cand_t c = TS.cands[index];
    append(c.word);
    learn(c.word);
    memmove(TS.composing, TS.composing + c.consumed, strlen(TS.composing + c.consumed) + 1);
    ink_probe_count("text.picks");
    changed();
}

void ink_text_key(char c) {
    if (!TS.session) return;
    text_voice_clear_notice();
    char one[2] = {c, 0};
    switch (TS.type) {
    case INK_TEXT_PLAIN:
        if (c >= 'a' && c <= 'z') {
            size_t n = strlen(TS.composing);
            if (n < COMPOSING_MAX) {
                TS.composing[n] = c;
                TS.composing[n + 1] = 0;
            }
        } else if (c == ' ' && TS.composing[0]) {
            compute_cands();
            if (TS.ncands) {
                ink_text_pick(0); /* the first is what the space takes */
                return;
            }
            flush_composing();
        } else {
            flush_composing();
            append(one);
        }
        break;
    case INK_TEXT_NUMBER:
        if ((c < '0' || c > '9') && c != '.' && c != '-') return;
        append(one);
        break;
    case INK_TEXT_PHONE:
        if ((c < '0' || c > '9') && c != '+' && c != '*' && c != '#') return;
        append(one);
        break;
    case INK_TEXT_PASSWORD:
        if ((unsigned char)c < 0x20) return;
        append(one);
        break;
    }
    changed();
}

void ink_text_backspace(void) {
    text_voice_clear_notice();
    if (!TS.session) return;
    size_t n = strlen(TS.composing);
    if (n) TS.composing[n - 1] = 0;
    else {
        size_t v = strlen(TS.value);
        while (v && (TS.value[v - 1] & 0xC0) == 0x80) v--; /* continuation bytes */
        if (v) v--;
        TS.value[v] = 0;
    }
    changed();
}

void ink_text_enter(void) {
    text_voice_clear_notice();
    if (!TS.session) return;
    if (TS.composing[0]) { /* the enter key takes the letters as they are */
        flush_composing();
        changed();
        return;
    }
    ink_probe_count("text.entered");
    if (TS.client && TS.client->entered) TS.client->entered(TS.ctx, TS.value);
}

void ink_text_commit_utf8(const char *s) {
    if (!TS.session || !s) return;
    flush_composing();
    append(s);
    changed();
}

/* Someone else takes the device: what was typed is not theirs. */
static void on_member(void *ctx, int h, const char *id) { /* UI task */
    (void)ctx;
    (void)h;
    (void)id;
    ink_text_blur(0);
}

static const ink_state_def_t ST_FOCUSED = {.id = "text.focused", .type = INK_ST_BOOL, .title = "有文本框在输入"};
static const ink_setting_def_t TEXT_SETTINGS[] = {
    {.id = "ai.read_clipboard", .title = "AI 可以读剪贴板", .type = INK_SET_BOOL, .def = 0},
};

int ink_text_service_init(void) {
    TS.h_focused = ink_state_define(&ST_FOCUSED);
    ink_state_set_bool(TS.h_focused, false);
    py_init();
    clip_init();
    if (text_voice_init() != 0) return -1;
    if (ink_db_register(&USER_FREQ) != 0 || ink_settings_define(TEXT_SETTINGS, 1) != 0 || ink_state_watch("member.current", on_member, NULL, INK_EXEC_UI) != 0) return -1;
    return text_caps_init();
}
