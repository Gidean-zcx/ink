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
#define UNDO_MAX 16
#define UNDO_GROUP_US 3000000ull
#define COMPOSING_MAX 40
#define MAX_CANDS 60

/* What the member picked, by a hash of the word: the count, then the word. */
static const ink_db_def_t USER_FREQ = {.name = "text.user_freq", .title = "输入法学到的词频", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 512, .drop_oldest = true};

typedef struct {
    char *text;
    size_t cursor, anchor;
} snap_t;

enum { EDIT_NONE = 0, EDIT_KEYS, EDIT_DELETE, EDIT_OTHER };

static struct {
    ink_member_identity_t owner;
    int session, next_session;
    uint32_t revision;
    ink_text_type_t type;
    ink_text_enter_t enter;
    bool ai_hidden;
    char *value;
    size_t cap;
    size_t cursor, anchor; /* bytes, on whole characters. They differ when something is selected */
    snap_t undo[UNDO_MAX], redo[UNDO_MAX];
    int nundo, nredo;
    int edit_kind;         /* what the last edit was: keys in a row are one thing to undo */
    uint64_t edit_at_us;
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
    if (++TS.revision == 0) TS.revision++;
    TS.cands_valid = false;
    if (TS.client && TS.client->changed) TS.client->changed(TS.ctx);
    if (TS.presenter) TS.presenter();
}

int ink_text_target_session(void) { return TS.session; }
uint32_t ink_text_edit_revision(void) { return TS.revision; }

void text_presenter_kick(void) {
    if (TS.session && ++TS.revision == 0) TS.revision++;
    if (TS.presenter) TS.presenter();
}

static void on_dict_ready(void) { /* UI task: the keyboard asks again */
    TS.cands_valid = false;
    if (TS.session && TS.presenter) TS.presenter();
}

int ink_text_focus(const ink_text_cfg_t *cfg, const ink_text_client_t *client, void *ctx) {
    if (!cfg) return -1;
    if (TS.session) ink_text_blur(TS.session);
    ink_member_identity(&TS.owner);
    text_voice_clear_notice();
    size_t cap = cfg->max ? (cfg->max < VALUE_MAX ? cfg->max : VALUE_MAX) : VALUE_DEFAULT;
    char *v = osal_malloc(cap + 1, OSAL_MEM_EXTERNAL);
    if (!v) return -1;
    const char *initial = cfg->initial ? cfg->initial : "";
    size_t n = strlen(initial);
    if (n > cap) {
        n = cap;
        while (n && ((unsigned char)initial[n] & 0xC0) == 0x80) n--;
    }
    memcpy(v, initial, n);
    v[n] = 0;
    TS.value = v;
    TS.cap = cap;
    TS.cursor = TS.anchor = strlen(v);
    TS.edit_kind = EDIT_NONE;
    TS.type = cfg->type;
    TS.enter = cfg->enter;
    TS.ai_hidden = cfg->ai_hidden;
    snprintf(TS.hint, sizeof TS.hint, "%s", cfg->hint ? cfg->hint : "");
    TS.composing[0] = 0;
    TS.client = client;
    TS.ctx = ctx;
    TS.session = ++TS.next_session;
    TS.revision = 1;
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

bool ink_text_session_current(void) {
    ink_member_identity_t identity;
    ink_member_identity(&identity);
    return !TS.session || (identity.authed && !strcmp(TS.owner.member, identity.member));
}

static void forget_snaps(snap_t *list, int *n) {
    for (int i = 0; i < *n; i++) {
        memset(list[i].text, 0, strlen(list[i].text));
        osal_free(list[i].text);
    }
    *n = 0;
}

static bool push_snap(snap_t *list, int *n) { /* what is there now; the oldest makes room */
    char *copy = osal_malloc(strlen(TS.value) + 1, OSAL_MEM_EXTERNAL);
    if (!copy) return false;
    strcpy(copy, TS.value);
    if (*n == UNDO_MAX) {
        memset(list[0].text, 0, strlen(list[0].text));
        osal_free(list[0].text);
        memmove(list, list + 1, sizeof list[0] * (UNDO_MAX - 1));
        (*n)--;
    }
    list[(*n)++] = (snap_t){copy, TS.cursor, TS.anchor};
    return true;
}

/* Before a change: keys in a row, soon after each other and in the same place, are undone together. Nothing of a password is kept. */
static void before_edit(int kind) {
    if (TS.type == INK_TEXT_PASSWORD) return;
    forget_snaps(TS.redo, &TS.nredo); /* A new branch invalidates redo even when a snapshot cannot fit. */
    uint64_t now = osal_now_us();
    if (kind != EDIT_OTHER && kind == TS.edit_kind && now - TS.edit_at_us < UNDO_GROUP_US) {
        TS.edit_at_us = now;
        return;
    }
    if (!push_snap(TS.undo, &TS.nundo)) {
        forget_snaps(TS.undo, &TS.nundo);
        TS.edit_kind = EDIT_NONE;
        return;
    }
    TS.edit_kind = kind;
    TS.edit_at_us = now;
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
    forget_snaps(TS.undo, &TS.nundo);
    forget_snaps(TS.redo, &TS.nredo);
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

/* In place of what is selected, or at the cursor. As much as fits, whole characters. */
static void put(const char *s) {
    size_t lo = TS.cursor < TS.anchor ? TS.cursor : TS.anchor, hi = TS.cursor < TS.anchor ? TS.anchor : TS.cursor;
    size_t have = strlen(TS.value), add = strlen(s), rest = have - (hi - lo);
    if (rest + add > TS.cap) {
        add = TS.cap - rest;
        while (add && (s[add] & 0xC0) == 0x80) add--;
        ink_probe_count("text.too_long");
    }
    memmove(TS.value + lo + add, TS.value + hi, have - hi + 1);
    memcpy(TS.value + lo, s, add);
    TS.cursor = TS.anchor = lo + add;
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
    before_edit(EDIT_KEYS);
    put(TS.composing);
    TS.composing[0] = 0;
}

void ink_text_pick(int index) {
    text_voice_clear_notice();
    if (!TS.session) return;
    compute_cands();
    if (index < 0 || index >= TS.ncands) return;
    ink_text_cand_t c = TS.cands[index];
    before_edit(EDIT_KEYS);
    put(c.word);
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
            before_edit(EDIT_KEYS);
            put(one);
        }
        break;
    case INK_TEXT_NUMBER:
        if ((c < '0' || c > '9') && c != '.' && c != '-') return;
        before_edit(EDIT_KEYS);
        put(one);
        break;
    case INK_TEXT_PHONE:
        if ((c < '0' || c > '9') && c != '+' && c != '*' && c != '#') return;
        before_edit(EDIT_KEYS);
        put(one);
        break;
    case INK_TEXT_PASSWORD:
        if ((unsigned char)c < 0x20) return;
        put(one);
        break;
    }
    changed();
}

void ink_text_backspace(void) {
    text_voice_clear_notice();
    if (!TS.session) return;
    size_t n = strlen(TS.composing);
    if (n) TS.composing[n - 1] = 0;
    else if (TS.cursor != TS.anchor) {
        before_edit(EDIT_OTHER);
        put("");
    } else if (TS.cursor) {
        before_edit(EDIT_DELETE);
        size_t v = TS.cursor;
        while (v && (TS.value[v - 1] & 0xC0) == 0x80) v--; /* continuation bytes */
        if (v) v--;
        TS.anchor = v;
        put("");
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
    if (++TS.revision == 0) TS.revision++;
    ink_probe_count("text.entered");
    if (TS.client && TS.client->entered) TS.client->entered(TS.ctx, TS.value);
}

void ink_text_commit_utf8(const char *s) {
    if (!TS.session || !s) return;
    flush_composing();
    before_edit(EDIT_OTHER);
    put(s);
    TS.edit_kind = EDIT_OTHER;
    changed();
}

/* ---- the cursor, the selection and undo: in characters, as a label and the AI count them ---- */

static size_t to_chars(size_t bytes) {
    size_t n = 0;
    for (size_t i = 0; i < bytes && TS.value[i]; i++) n += (TS.value[i] & 0xC0) != 0x80;
    return n;
}

static size_t to_bytes(size_t chars) { /* past the end: the end */
    size_t i = 0;
    while (TS.value[i] && chars) {
        i++;
        while ((TS.value[i] & 0xC0) == 0x80) i++;
        chars--;
    }
    return i;
}

static void moved(void) { /* the next key starts something new to undo */
    TS.edit_kind = EDIT_NONE;
    changed();
}

void ink_text_selection(size_t *start, size_t *end) {
    size_t a = TS.value ? to_chars(TS.cursor) : 0, b = TS.value ? to_chars(TS.anchor) : 0;
    if (start) *start = a < b ? a : b;
    if (end) *end = a < b ? b : a;
}

size_t ink_text_cursor(void) { return TS.value ? to_chars(TS.cursor) : 0; }

void ink_text_set_cursor(size_t at) {
    if (!TS.session) return;
    flush_composing();
    TS.cursor = TS.anchor = to_bytes(at);
    moved();
}

void ink_text_select(size_t start, size_t end) {
    if (!TS.session) return;
    flush_composing();
    TS.anchor = to_bytes(start < end ? start : end);
    TS.cursor = to_bytes(start < end ? end : start);
    moved();
}

static bool ascii_word(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

/* A wide character that is not punctuation: the blocks of Chinese characters. */
static bool han_at(size_t i) {
    unsigned char c0 = (unsigned char)TS.value[i];
    if (c0 < 0xE3 || c0 > 0xE9) return false;
    return c0 >= 0xE4 || (c0 == 0xE3 && (unsigned char)TS.value[i + 1] >= 0x90);
}

static size_t next_char(size_t i) {
    if (!TS.value[i]) return i;
    i++;
    while ((TS.value[i] & 0xC0) == 0x80) i++;
    return i;
}

static size_t prev_char(size_t i) {
    if (!i) return 0;
    i--;
    while (i && (TS.value[i] & 0xC0) == 0x80) i--;
    return i;
}

void ink_text_select_word(size_t at) {
    if (!TS.session) return;
    flush_composing();
    size_t i = to_bytes(at);
    if (!TS.value[i] && i) i = prev_char(i); /* past the end: the last one */
    size_t lo = i, hi = next_char(i);
    if (ascii_word((unsigned char)TS.value[i])) {
        while (lo && ascii_word((unsigned char)TS.value[lo - 1])) lo--;
        while (ascii_word((unsigned char)TS.value[hi])) hi++;
    } else if (han_at(i)) { /* no word list to go by yet: two characters, the commonest length; the arrows do the rest */
        if (han_at(hi)) hi = next_char(hi);
        else if (lo && han_at(prev_char(lo))) lo = prev_char(lo);
    }
    TS.anchor = lo;
    TS.cursor = hi;
    moved();
}

void ink_text_move(int chars, bool end) {
    if (!TS.session) return;
    flush_composing();
    bool selected = TS.cursor != TS.anchor;
    size_t lo = TS.cursor < TS.anchor ? TS.cursor : TS.anchor, hi = TS.cursor < TS.anchor ? TS.anchor : TS.cursor;
    size_t p = selected ? (end ? hi : lo) : TS.cursor;
    for (; chars > 0; chars--) p = next_char(p);
    for (; chars < 0; chars++) p = prev_char(p);
    if (!selected) TS.cursor = TS.anchor = p;
    else if (end) {
        TS.anchor = lo;
        TS.cursor = p < lo ? lo : p;
    } else {
        TS.anchor = p > hi ? hi : p;
        TS.cursor = hi;
    }
    moved();
}

int ink_text_selected(char *out, size_t cap) {
    if (!TS.session || TS.type == INK_TEXT_PASSWORD || !cap) return -1;
    size_t lo = TS.cursor < TS.anchor ? TS.cursor : TS.anchor, hi = TS.cursor < TS.anchor ? TS.anchor : TS.cursor;
    size_t n = hi - lo;
    if (n >= cap) n = cap - 1;
    while (n && (TS.value[lo + n] & 0xC0) == 0x80) n--;
    memcpy(out, TS.value + lo, n);
    out[n] = 0;
    return (int)n;
}

static void restore(const snap_t *s) {
    strcpy(TS.value, s->text);
    TS.cursor = s->cursor;
    TS.anchor = s->anchor;
    TS.composing[0] = 0;
    TS.edit_kind = EDIT_NONE;
    memset(s->text, 0, strlen(s->text));
    osal_free(s->text);
    changed();
}

bool ink_text_can_undo(void) { return TS.session && TS.nundo > 0; }
bool ink_text_can_redo(void) { return TS.session && TS.nredo > 0; }

bool ink_text_undo(void) {
    if (!ink_text_can_undo() || !push_snap(TS.redo, &TS.nredo)) return false;
    restore(&TS.undo[--TS.nundo]);
    return true;
}

bool ink_text_redo(void) {
    if (!ink_text_can_redo() || !push_snap(TS.undo, &TS.nundo)) return false;
    restore(&TS.redo[--TS.nredo]);
    return true;
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
    {.id = "ai.read_clipboard", .title = "AI 可以读剪贴板", .type = INK_SET_BOOL, .def = 0, .ask_by_hand = true},
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
