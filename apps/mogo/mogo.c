/* 阅读: the grafted Mogo reader, the only reader (D-139). Touch, keys and the AI call the same reader.* capabilities;
 * the engine runs on the BG executor, one operation at a time, and what is missing is explained on the page (D-138). */
#include <page.h>
#include <book_progress.h>
#include "ink/sys/i18n.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/settings.h"
#include "ink/probe.h"
#include <ink/ui_theme.h>
#include <ink/ui_shell.h>
#define INK_MOGO_PORT_IMPL
#include <port/runtime.h>
#include <settings.h>

void ink_mogo_book_title(const char *path, char *out, size_t cap);
static struct {
    ink_member_identity_t owner;
    ink_exec_job_t work, done;
    ink_call_t *call, *opening; /* the running call; an open_book waiting for the page to be created */
    uint8_t *pixels;
    int w, h, px, status, reason, turns; /* turns: presses that came while busy, turned in the next pass */
    ink_mogo_request_t req;
    bool active, busy, loaded, root, back, drawn, card_new, closing, has_next, reattach; /* closing: a closed session's cleanup is due */
    ink_mogo_request_t next; /* the page opened again before that cleanup ran: its first operation, after it */
    ink_call_t *next_call;
    char card_member[16]; /* whose card M.card is (kept after the session closes) */
    uint64_t opened_us, saved_us; /* saved_us: the place was last written (or the book opened) */
    ink_mogo_card_t card;
    lv_obj_t *back_button, *note, *screen; /* screen: the page's, while the activity lives */
    char screen_member[16], book[288];      /* whose page it is and the book on it: built again (a page above closed, a
                                             * redraw, a member gate) the page shows that book, not what it was opened with */
    char want[288];                         /* the book the page opens when it is built next (reader.open_book and the like) */
    char description[3072], explanation[400], json[4096]; /* json: a longer result is in M.req.json (ink_mogo_json_free) */
} M INK_PSRAM_BSS;
/* list_books, continue and open by title run without the page: their own node, member and buffers. */
static struct {
    ink_exec_job_t work, done;
    ink_call_t *call;
    ink_member_identity_t owner;
    bool busy, cont, alone, layout_op;
    int index, found, layout[INK_MOGO_LAYOUT_COUNT];
    char title[96], path[288], json[4096], *list; /* list: list_books' result, S.json or a bigger one */
    size_t list_cap;
} S INK_PSRAM_BSS;
/* The home card mirrors each member's last book, read ahead on BG: .card runs on UI and never reads flash, and the
 * lock screen shows the right book at once after a member switch. */
typedef struct { char member[16]; bool have, migrated; ink_mogo_card_t card; } ink_mogo_slot_t; /* migrated: looked at since boot */
static struct {
    ink_exec_job_t work, done, migrate, migrated;
    ink_member_identity_t owner;
    ink_mogo_card_t read;
    ink_mogo_slot_t slot[INK_MEMBERS_MAX];
    bool busy, found, migrating, again;
} C INK_PSRAM_BSS;
/* The member's slot (NULL: not read yet); `take` reuses the oldest one for a new member. */
static ink_mogo_slot_t *ink_mogo_slot(const char *member, bool take) {
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (!strcmp(C.slot[i].member, member)) return &C.slot[i];
    if (!take) return NULL;
    memmove(&C.slot[1], &C.slot[0], sizeof C.slot - sizeof *C.slot);
    memset(&C.slot[0], 0, sizeof *C.slot); snprintf(C.slot[0].member, sizeof C.slot[0].member, "%s", member);
    return &C.slot[0];
}

static void ink_mogo_reply(ink_call_t *call, ink_rc_t status) { if (call) { call->rc.status = status; ink_cap_done(call); } }
/* A result that did not fit its 4 KB is in a buffer of its own until the reply has copied it. */
static void ink_mogo_json_free(char **json, char *own) { if (*json && *json != own) ink_app_free(*json); *json = NULL; }
static bool ink_mogo_start(const ink_mogo_request_t *req, ink_call_t *call);
static void ink_mogo_reattach(void);
/* The place is written after a pause in reading (8 s, as the old reader did), not on every turn. */
static lv_timer_t *ink_mogo_idle;
static void ink_mogo_save(lv_timer_t *t) {
    ink_mogo_request_t req = {.op = INK_MOGO_OP_SAVE};
    if (M.busy) { lv_timer_reset(t); lv_timer_resume(t); }
    else (void)ink_mogo_start(&req, NULL);
}
static void ink_mogo_card_read(void);
static void ink_mogo_whole(char *s);
static void ink_mogo_work(void *arg);
static void ink_mogo_unbound(lv_event_t *e) { (void)e; } /* replaced by the transfer.open binding */
static void ink_mogo_overlays(void) {
    /* only what changed is redrawn: a hidden button or the same note left alone costs the panel no refresh */
    if (M.back_button && M.back == lv_obj_has_flag(M.back_button, LV_OBJ_FLAG_HIDDEN)) {
        if (M.back) lv_obj_remove_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
        ink_ui_hint_obj(M.back_button, INK_REFRESH_UI);
        lv_obj_invalidate(M.back_button);
    }
    static uint32_t shown;
    uint32_t now = M.reason ? ink_crc32((uint32_t)M.reason, M.explanation, strlen(M.explanation)) : 0;
    if (M.note && now == shown) return;
    if (M.note) lv_obj_delete(M.note);
    M.note = NULL; shown = 0;
    if (!M.reason || !M.back_button) return;
    shown = now;
    bool child = ink_member_current()->role == INK_ROLE_CHILD;
    M.note = ink_ui_empty(lv_obj_get_parent(M.back_button), M.explanation, M.reason == INK_MOGO_R_EMPTY && !child ? TR("传书") : NULL,
                          M.reason == INK_MOGO_R_EMPTY && !child ? ink_mogo_unbound : NULL, NULL);
    /* The explanation wraps instead of being cut: the box grows to its lines, centred like ink_ui_empty's. */
    int w = INK_CANVAS_W - 2 * INK_MARGIN, text = ink_ui_text_height(M.explanation, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    lv_obj_t *label = lv_obj_get_child(M.note, 0), *button = lv_obj_get_child(M.note, 1);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP); lv_obj_set_height(label, ink_ui_px(text));
    int h = text + (button ? INK_GAP + INK_CONTROL : 0);
    lv_obj_set_height(M.note, ink_ui_px(h)); lv_obj_set_y(M.note, ink_ui_px((INK_CANVAS_H - h) / 2));
    if (button) {
        lv_obj_remove_event_cb(button, ink_mogo_unbound);
        lv_obj_set_y(button, ink_ui_px(text + INK_GAP));
        ink_shell_bind(button, "transfer.open", NULL);
    }
    ink_ui_hint_obj(M.note, INK_REFRESH_UI);
}
static void ink_mogo_finished(void *arg) {
    (void)arg;
    ink_call_t *call = M.call;
    M.call = NULL;
    if (M.active && ink_graft_current()) {
        if (M.drawn) { /* a read or a save changes nothing on the page: the content version stays */
            lv_area_t a = {0, 0, M.w - 1, M.h - 1};
            if (ink_graft_present(M.pixels, M.w, &a, INK_GRAFT_PAGE)) M.status = -1;
            ink_mogo_overlays();
            ink_ui_content_changed();
        }
        int op = M.req.op;
        if (op == INK_MOGO_OP_LOAD || op == INK_MOGO_OP_OPEN) snprintf(M.book, sizeof M.book, "%s", M.req.text); /* an explanation too */
        if (M.root && !M.reason) M.book[0] = 0; /* on the shelf */
        if (!M.status && ink_mogo_idle && (op == INK_MOGO_OP_TURN || op == INK_MOGO_OP_GOTO_PAGE || op == INK_MOGO_OP_ENTRY || op == INK_MOGO_OP_CHAPTER ||
            op == INK_MOGO_OP_GOTO_MARK || op == INK_MOGO_OP_LAYOUT || op == INK_MOGO_OP_OPEN || op == INK_MOGO_OP_LOAD)) {
            lv_timer_set_repeat_count(ink_mogo_idle, 1); lv_timer_reset(ink_mogo_idle); lv_timer_resume(ink_mogo_idle);
            /* reading on without a pause: at most a minute is lost (SR-PL-08) */
            if (ink_now_us() - M.saved_us > 60000000ull) lv_timer_set_period(ink_mogo_idle, 1);
        }
        if (op == INK_MOGO_OP_SAVE || op == INK_MOGO_OP_LOAD || op == INK_MOGO_OP_OPEN) { M.saved_us = ink_now_us(); if (ink_mogo_idle) lv_timer_set_period(ink_mogo_idle, 8000); }
        if (!M.status && M.req.op == INK_MOGO_OP_TURN) for (int i = 0; i < (M.req.value < 0 ? -M.req.value : M.req.value); i++) ink_probe_count("reader.page_turns"); /* each press a turn did */
        if (M.drawn && (M.req.op == INK_MOGO_OP_LOAD || M.req.op == INK_MOGO_OP_OPEN) && !M.reason && M.req.text[0]) {
            ink_probe_value("reader.open_ms", (double)(ink_now_us() - M.opened_us) / 1000.0);
            ink_probe_value("reader.restored_page", (double)(M.req.revision ? M.req.revision - 1 : 0));
        }
    } else M.status = -1;
    if (M.card_new && M.card_member[0]) { /* also from the close: leaving the book writes its place */
        ink_mogo_slot_t *slot = ink_mogo_slot(M.card_member, true);
        slot->card = M.card; slot->have = true; M.card_new = false;
        ink_app_card_changed("reader");
    }
    M.busy = false;
    if (call) {
        call->rc.revision = M.req.revision;
        if (M.req.json && M.req.json[0] && !M.status) ink_cap_result(call, M.req.json);
        if (M.status) { /* the explanation, as much of it as the reply holds, never half a character */
            snprintf(call->rc.say, sizeof call->rc.say, "%.*s", (int)sizeof call->rc.say - 1, M.reason ? M.explanation : TR("现在做不了，请稍后再试"));
            ink_mogo_whole(call->rc.say);
        }
        else if (M.req.say[0]) snprintf(call->rc.say, sizeof call->rc.say, "%s", M.req.say);
        ink_mogo_reply(call, M.status ? INK_RC_FAILED : INK_RC_DONE);
    }
    ink_mogo_json_free(&M.req.json, M.json);
    if (M.reattach && !M.busy && M.screen && !M.active) { M.reattach = false; ink_mogo_reattach(); }
    if (M.has_next && M.active && !M.busy) {
        M.has_next = false;
        if (!ink_mogo_start(&M.next, M.next_call)) ink_mogo_reply(M.next_call, INK_RC_FAILED);
    }
    if (M.turns && M.active && !M.busy) {
        static ink_mogo_request_t queued;
        queued = (ink_mogo_request_t){.op = INK_MOGO_OP_TURN, .value = M.turns};
        M.turns = 0;
        M.req = queued; M.busy = true;
        if (ink_exec_post(INK_EXEC_BG, &M.work, ink_mogo_work, NULL)) M.busy = false;
    }
}
static void ink_mogo_work(void *arg) {
    (void)arg;
    if (M.active && !M.closing) {
        uint64_t t0 = ink_now_us();
        M.drawn = M.card_new = false;
        if (!M.loaded) {
            ink_mogo_page_close();
            M.status = ink_mogo_page_start(&M.owner, M.w, M.h, M.px);
            if (M.status != INK_MOGO_R_NO_CARD) (void)ink_mogo_migrate_step(&M.owner, M.req.text);
            M.loaded = true;
            M.status = ink_mogo_page_command(&M.req);
        } else M.status = ink_mogo_page_command(&M.req);
        int op = M.req.op;
        if (op != INK_MOGO_OP_CHAPTERS && op != INK_MOGO_OP_GET_LAYOUT && op != INK_MOGO_OP_READ && op != INK_MOGO_OP_SAVE && !(op == INK_MOGO_OP_MARKS && !M.req.value)) {
            ink_mogo_page_draw(M.pixels);
            M.drawn = true;
        }
        ink_mogo_page_snapshot(M.description, sizeof M.description);
        M.reason = ink_mogo_page_reason(); M.root = ink_mogo_page_root(); M.back = ink_mogo_page_back_shown();
        snprintf(M.explanation, sizeof M.explanation, "%s", ink_mogo_page_explanation());
        if (op == INK_MOGO_OP_TURN) ink_probe_value("reader.page_render_ms", (double)(ink_now_us() - t0) / 1000.0);
    }
    bool cleanup = !M.active || M.closing;
    if (cleanup) ink_mogo_page_close();
    ink_mogo_card_t card;
    if (ink_mogo_page_card(&card) && memcmp(&card, &M.card, sizeof card)) {
        M.card = card; M.card_new = true; snprintf(M.card_member, sizeof M.card_member, "%s", M.owner.member);
    }
    if (cleanup) {
        if (!M.active) { ink_app_free(M.pixels); M.pixels = NULL; memset(&M.owner, 0, sizeof M.owner); } /* reopened: the new page keeps them */
        M.loaded = false; M.turns = 0; M.closing = false;
    }
    ink_exec_post(INK_EXEC_UI, &M.done, ink_mogo_finished, NULL);
}
/* UI: one engine operation for `call` (deferred here, answered when the page is on the screen). */
static bool ink_mogo_start(const ink_mogo_request_t *req, ink_call_t *call) {
    if (M.busy || !M.active || !ink_graft_current()) return false;
    M.req = *req; M.req.json = req->json ? M.json : NULL; M.req.cap = sizeof M.json; M.json[0] = 0;
    M.busy = true; M.call = call;
    if (call) ink_cap_defer(call);
    if (ink_exec_post(INK_EXEC_BG, &M.work, ink_mogo_work, NULL)) {
        M.busy = false; M.call = NULL;
        ink_mogo_reply(call, INK_RC_FAILED);
    }
    return true;
}
static bool ink_mogo_opened(void *ctx, const ink_member_identity_t *owner, int w, int h) {
    (void)ctx;
    if (M.busy && (!M.closing || (M.pixels && (w != M.w || h != M.h)))) return false; /* only an old page's cleanup may still be due */
    M.owner = *owner; M.w = w; M.h = h;
    int level = ink_member_current()->role == INK_ROLE_CHILD ? 2 : (int)ink_setting_get("display.font_size");
    static const int percent[] = {90, 100, 110, 120};
    M.px = ink_ui_px(percent[level < 0 ? 0 : level > 3 ? 3 : level]); M.active = true; M.loaded = false;
    if (!M.pixels) M.pixels = ink_app_alloc((size_t)w * h, true);
    return M.pixels != NULL;
}
static void ink_mogo_closed(void *ctx) {
    (void)ctx; M.active = false; M.closing = true; M.description[0] = 0; M.back_button = M.note = NULL;
    if (M.has_next) { M.has_next = false; ink_mogo_reply(M.next_call, INK_RC_FAILED); }
    if (ink_mogo_idle) lv_timer_delete(ink_mogo_idle);
    ink_mogo_idle = NULL;
    if (!M.busy) {
        M.busy = true;
        ink_exec_post(INK_EXEC_BG, &M.work, ink_mogo_work, NULL);
    }
}
static int ink_mogo_snapshot(void *ctx, char *out, size_t cap) {
    (void)ctx;
    return snprintf(out, cap, "%s", M.busy ? TR("阅读 正在排版") : M.description);
}
static bool ink_mogo_call(const char *id, const char *args, ink_source_t src) {
    ink_call_opts_t opts = {.src = src, .args_json = args && args[0] ? args : NULL};
    return !ink_cap_call(id, NULL, &opts);
}
/* Keys, taps and swipes become reader.* calls. BACK on the shelf or an explanation page is the shell's: no loop. */
static bool ink_mogo_input(void *ctx, const ink_graft_input_t *e) {
    (void)ctx;
    /* a book that would not open is still a book page: its page keys are refused by reader.next_page/prev_page */
    bool reading = (!M.root && !M.back) || M.reason == INK_MOGO_R_BAD_BOOK, lists = M.root ? !M.reason : M.back;
    if (e->kind == INK_GRAFT_KEY && e->value == INK_KEY_BACK) return M.root ? false : M.busy || ink_mogo_call("reader.back", NULL, e->source);
    if (M.busy) { /* page keys while a page is being laid out are turned together with the next pass */
        bool turn = e->kind == INK_GRAFT_KEY && !e->held && (e->value == INK_KEY_NEXT || e->value == INK_KEY_PREV);
        return turn ? !reading || ink_mogo_call(e->value == INK_KEY_NEXT ? "reader.next_page" : "reader.prev_page", NULL, e->source) : e->kind != INK_GRAFT_KEY;
    }
    if (e->kind == INK_GRAFT_TAP) {
        char args[64];
        const char *id = ink_mogo_page_hit(e->x, e->y, args, sizeof args);
        return id && (!*id || ink_mogo_call(id, args, e->source));
    }
    int dir = 0;
    if (e->kind == INK_GRAFT_HOLD || (e->kind == INK_GRAFT_SWIPE && e->value == INK_SWIPE_UP))
        return reading && ink_mogo_call("reader.menu", "{\"view\":\"menu\"}", e->source);
    if (e->kind == INK_GRAFT_SWIPE) dir = e->value == INK_SWIPE_LEFT ? 1 : e->value == INK_SWIPE_RIGHT ? -1 : 0;
    else if (e->kind == INK_GRAFT_SHAKE) dir = e->value;
    else if (e->kind == INK_GRAFT_KEY && !e->held) dir = e->value == INK_KEY_NEXT ? 1 : e->value == INK_KEY_PREV ? -1 : 0;
    if (!dir || (!reading && !lists)) return false;
    return ink_mogo_call(reading ? (dir > 0 ? "reader.next_page" : "reader.prev_page") : (dir > 0 ? "reader.list_next" : "reader.list_prev"), NULL, e->source);
}
static const ink_graft_client_t INK_MOGO_CLIENT = {.app = "mogo", .open = ink_mogo_opened, .close = ink_mogo_closed,
    .input = ink_mogo_input, .snapshot = ink_mogo_snapshot};
static void ink_mogo_build(lv_obj_t *screen, const char *path) {
    ink_call_t *call = M.opening;
    M.opening = NULL; M.screen = screen; snprintf(M.screen_member, sizeof M.screen_member, "%s", ink_member_current()->id);
    if (ink_graft_attach(screen, &INK_MOGO_CLIENT)) { ink_mogo_reply(call, INK_RC_FAILED); return; }
    /* The shell's own back button: hidden on the bare body (Mogo and Kindle keep the page clean), a white box over the corner. */
    M.back_button = ink_ui_circle(screen, "back", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    lv_obj_set_style_bg_opa(M.back_button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(M.back_button, ink_ui_paper(), 0);
    ink_shell_bind(M.back_button, "reader.back", NULL);
    lv_obj_add_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
    ink_mogo_idle = lv_timer_create(ink_mogo_save, 8000, NULL);
    lv_timer_set_repeat_count(ink_mogo_idle, 1); lv_timer_set_auto_delete(ink_mogo_idle, false); lv_timer_pause(ink_mogo_idle);
    M.reason = 0; M.root = true; M.back = false; M.description[0] = 0; M.opened_us = ink_now_us();
    ink_mogo_request_t req = {.op = INK_MOGO_OP_LOAD};
    snprintf(req.text, sizeof req.text, "%s", path);
    if (M.busy && M.closing) { M.next = req; M.next_call = call; M.has_next = true; if (call) ink_cap_defer(call); return; } /* after the old page's cleanup */
    if (!ink_mogo_start(&req, call)) ink_mogo_reply(call, INK_RC_FAILED);
}
/* Opened: the book asked for, else the shelf. Built again: the book that was on the page for the same member. The
 * frame's arg is not used: it would outlive what it pointed at. */
static void ink_mogo_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    char path[288];
    bool same = ink_ui_shown_again() && !strcmp(M.screen_member, ink_member_current()->id);
    snprintf(path, sizeof path, "%s", M.want[0] ? M.want : same ? M.book : "");
    M.want[0] = 0;
    ink_mogo_build(screen, path);
}
/* Open a path in the page (opening the app first): the call is answered when the first page is on the screen. */
static void ink_mogo_open_path(ink_call_t *call, const char *path) {
    if (M.active && ink_graft_current()) {
        ink_mogo_request_t req = {.op = INK_MOGO_OP_OPEN};
        snprintf(req.text, sizeof req.text, "%s", path);
        (void)ink_app_open("reader", NULL, NULL);
        M.opened_us = ink_now_us();
        if (!ink_mogo_start(&req, call)) ink_mogo_reply(call, INK_RC_STALE);
        return;
    }
    M.opening = call; snprintf(M.want, sizeof M.want, "%s", path);
    if (ink_app_open("reader", NULL, NULL) && M.opening) { M.opening = NULL; M.want[0] = 0; ink_mogo_reply(call, INK_RC_FAILED); }
}
static void ink_mogo_shelf_done(void *arg) {
    (void)arg;
    ink_call_t *call = S.call;
    S.call = NULL; S.busy = false;
    if (!call) return;
    if (S.layout_op) { /* the layout without the page: the member's saved values */
        ink_cap_result(call, S.json);
        ink_mogo_reply(call, S.found < 0 ? INK_RC_FAILED : INK_RC_DONE);
    } else if (!strcmp(call->cap->id, "reader.list_books")) {
        if (S.found != -3) ink_cap_result(call, S.list); /* -3: the list could not be written: no half answer */
        ink_mogo_json_free(&S.list, S.json);
        snprintf(call->rc.say, sizeof call->rc.say, "%s", S.found == -3 ? TR("现在做不了，请稍后再试") : S.title);
        snprintf(call->rc.object, sizeof call->rc.object, "shelf:books");
        ink_mogo_reply(call, S.found == -2 || S.found == -3 ? INK_RC_FAILED : INK_RC_DONE);
    } else if (S.found < 0 || !S.path[0]) {
        snprintf(call->rc.detail, sizeof call->rc.detail, S.cont ? "no book to continue" : "no such book on the shelf");
        ink_mogo_reply(call, INK_RC_FAILED);
    } else {
        snprintf(call->rc.say, sizeof call->rc.say, S.cont ? TR("接着读《%.60s》") : TR("打开《%.60s》"), S.title);
        snprintf(call->rc.object, sizeof call->rc.object, "book:%.40s", S.title);
        ink_mogo_open_path(call, S.path);
    }
}
static void ink_mogo_shelf_work(void *arg) {
    (void)arg;
    S.found = -1;
    if (S.layout_op) {
        ink_mogo_owner(&S.owner); ink_mogo_settings_load();
        ink_mogo_settings_t *p = &ink_mogo_settings; bool set = false;
        for (int i = 0; i < INK_MOGO_LAYOUT_COUNT; i++) set = set || S.layout[i] != INK_MOGO_UNSET;
        int v[INK_MOGO_LAYOUT_COUNT] = {p->px, p->line, p->paragraph, p->margin[0], p->weight, p->indent, p->header, p->footer};
        for (int i = 0; i < INK_MOGO_LAYOUT_COUNT; i++) if (S.layout[i] != INK_MOGO_UNSET) v[i] = S.layout[i];
        p->px = (uint8_t)v[0]; p->line = (uint8_t)v[1]; p->paragraph = (uint8_t)v[2]; p->margin[0] = p->margin[1] = (uint8_t)v[3];
        p->weight = (int8_t)v[4]; p->indent = (uint8_t)v[5]; p->header = (uint8_t)v[6]; p->footer = (uint8_t)v[7];
        S.found = !set || ink_mogo_settings_save() ? 0 : -1;
        snprintf(S.json, sizeof S.json, "{\"member\":\"%s\",\"font_size\":%d,\"line_spacing\":%d,\"paragraph_spacing\":%d,\"margins\":%d,\"weight\":%d,\"indent\":%d,\"header\":%d,\"footer\":%d}",
                 S.owner.member, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    } else if (S.cont) {
        if (S.alone) (void)ink_mogo_migrate_step(&S.owner, ""); /* the book source is the open page's otherwise */
        ink_mogo_owner(&S.owner);
        ink_fs_file_t *f = NULL;
        if (book_progress_last_path(S.path, sizeof S.path) && (!strncmp(S.path, "asset:", 6) || !ink_graft_file_open(&S.owner, S.path, INK_FS_READ, &f))) {
            if (f) ink_fs_close(f);
            ink_mogo_book_title(S.path, S.title, sizeof S.title);
            S.found = 0;
        }
    } else {
        bool list = !strcmp(S.call->cap->id, "reader.list_books");
        S.list = S.json; S.list_cap = sizeof S.json;
        S.found = ink_mogo_shelf_query(&S.owner, list ? &S.list : NULL, &S.list_cap, S.title, S.index, S.path, sizeof S.path);
        if (list) snprintf(S.title, sizeof S.title, "%s", TR("书架上有这些书"));
        else if (S.found >= 0) ink_mogo_book_title(S.path, S.title, sizeof S.title);
    }
    ink_exec_post(INK_EXEC_UI, &S.done, ink_mogo_shelf_done, NULL);
}
static void ink_mogo_shelf_start(ink_call_t *call, bool cont) {
    if (S.busy) { call->rc.status = INK_RC_STALE; return; }
    S.busy = true; S.call = call; S.cont = cont; S.alone = !M.active; S.path[0] = S.json[0] = 0;
    ink_member_identity(&S.owner);
    ink_cap_defer(call);
    if (ink_exec_post(INK_EXEC_BG, &S.work, ink_mogo_shelf_work, NULL)) { S.busy = false; S.call = NULL; ink_mogo_reply(call, INK_RC_FAILED); }
}
static int ink_mogo_value(ink_call_t *call, const char *key, int fallback) {
    int64_t v;
    return ink_args_get_int(&call->args, key, &v) && v > -100000 && v <= INT32_MAX ? (int)v : fallback;
}
static void ink_mogo_action(ink_call_t *call) {
    const char *id = call->cap->id + strlen("reader.");
    ink_mogo_request_t req = {.value = 0};
    for (int i = 0; i < INK_MOGO_LAYOUT_COUNT; i++) req.layout[i] = INK_MOGO_UNSET;
    if (!strcmp(id, "open_book") || !strcmp(id, "continue") || !strcmp(id, "list_books")) {
        char path[288] = "";
        if (call->src == INK_SRC_TOUCH && !strcmp(id, "open_book")) ink_ui_cause_tag("ui.open_to_stable_ms");
        if (ink_args_get_str(&call->args, "path", path, sizeof path) > 0) {
            if (strncmp(path, "/me/", 4) && strncmp(path, "/family/books/", 14) && strcmp(path, "asset:sample/welcome.txt")) { call->rc.status = INK_RC_INVALID; return; }
            ink_cap_defer(call); ink_mogo_open_path(call, path); return;
        }
        S.index = ink_mogo_value(call, "index", -1); S.title[0] = 0; S.layout_op = false;
        ink_args_get_str(&call->args, "title", S.title, sizeof S.title);
        ink_mogo_shelf_start(call, !strcmp(id, "continue"));
        return;
    }
    if (!strcmp(id, "open_sample")) { snprintf(call->rc.object, sizeof call->rc.object, "book:sample"); ink_cap_defer(call); ink_mogo_open_path(call, "asset:sample/welcome.txt"); return; }
    if (!strcmp(id, "back")) ink_probe_count("reader.back"); /* each call, by hand, key or AI: the tests count that BACK never loops */
    if (!strcmp(id, "back") && M.root && !M.busy) { ink_mogo_call("shell.back", NULL, call->src); return; }
    static const struct { const char *id; int op, value; bool json; } OPS[] = {
        {"next_page", INK_MOGO_OP_TURN, 1, false}, {"prev_page", INK_MOGO_OP_TURN, -1, false}, {"list_next", INK_MOGO_OP_LIST, 1, false}, {"list_prev", INK_MOGO_OP_LIST, -1, false},
        {"goto_page", INK_MOGO_OP_GOTO_PAGE, 0, false}, {"read_page", INK_MOGO_OP_READ, 0, false}, {"chapters", INK_MOGO_OP_CHAPTERS, 0, true},
        {"show_chapters", INK_MOGO_OP_TOC, 0, false}, {"goto_chapter", INK_MOGO_OP_CHAPTER, 0, false}, {"toc_percent", INK_MOGO_OP_TOC_PERCENT, 0, false},
        {"bookmarks", INK_MOGO_OP_MARKS, 0, true}, {"add_bookmark", INK_MOGO_OP_ADD_MARK, 0, false}, {"remove_bookmark", INK_MOGO_OP_REMOVE_MARK, 0, false},
        {"goto_bookmark", INK_MOGO_OP_GOTO_MARK, 0, false}, {"get_layout", INK_MOGO_OP_GET_LAYOUT, 0, true}, {"set_layout", INK_MOGO_OP_LAYOUT, 0, true},
        {"menu", INK_MOGO_OP_MENU, 1, false}, {"back", INK_MOGO_OP_BACK, 0, false}};
    unsigned k = 0;
    while (k < sizeof OPS / sizeof *OPS && strcmp(OPS[k].id, id)) k++;
    if (k == sizeof OPS / sizeof *OPS) { call->rc.status = INK_RC_INVALID; return; }
    req.op = OPS[k].op; req.value = OPS[k].value; req.json = OPS[k].json ? M.json : NULL;
    if (req.op == INK_MOGO_OP_GOTO_PAGE) req.value = ink_mogo_value(call, "page", 0);
    else if (req.op == INK_MOGO_OP_CHAPTER) {
        req.value = ink_mogo_value(call, "entry", 0);
        if (req.value) req.op = INK_MOGO_OP_ENTRY; else req.value = ink_mogo_value(call, "chapter", 0);
        if (req.value < 1) { call->rc.status = INK_RC_INVALID; return; }
    } else if (req.op == INK_MOGO_OP_TOC_PERCENT) req.value = ink_mogo_value(call, "value", 0);
    else if (req.op == INK_MOGO_OP_REMOVE_MARK || req.op == INK_MOGO_OP_GOTO_MARK) req.value = ink_mogo_value(call, "id", 0);
    else if (req.op == INK_MOGO_OP_MARKS) { bool open = false; ink_args_get_bool(&call->args, "open", &open); req.value = open; if (open) req.json = NULL; }
    else if (req.op == INK_MOGO_OP_MENU) {
        char view[16] = ""; bool open = true;
        ink_args_get_bool(&call->args, "open", &open);
        ink_args_get_str(&call->args, "view", view, sizeof view);
        req.value = !open ? 0 : !strcmp(view, "settings") ? 2 : !strcmp(view, "shelf") ? 3 : 1;
    } else if (req.op == INK_MOGO_OP_LAYOUT) {
        static const int range[INK_MOGO_LAYOUT_COUNT][2] = {{18, 90}, {110, 150}, {0, 100}, {0, 96}, {-32, 32}, {0, 3}, {0, 1}, {0, 1}};
        for (int i = 0; i < INK_MOGO_LAYOUT_COUNT; i++) {
            req.layout[i] = ink_mogo_value(call, ink_mogo_layout_keys[i], INK_MOGO_UNSET);
            if (ink_args_find(&call->args, ink_mogo_layout_keys[i]) && (req.layout[i] < range[i][0] || req.layout[i] > range[i][1])) { call->rc.status = INK_RC_INVALID; return; }
        }
    }
    if ((req.op == INK_MOGO_OP_LAYOUT || req.op == INK_MOGO_OP_GET_LAYOUT) && !M.active) {
        memcpy(S.layout, req.layout, sizeof S.layout); S.layout_op = true;
        ink_mogo_shelf_start(call, false);
        return;
    }
    if (req.op == INK_MOGO_OP_TURN) {
        ink_ui_content_changed(); /* a call prepared for this page no longer applies (F-6), from the press on */
        if (M.busy && M.active && M.loaded) { M.turns += req.value; return; } /* turned with the next pass, after what runs now */
    }
    if (!ink_mogo_start(&req, call)) call->rc.status = M.active ? INK_RC_STALE : INK_RC_FAILED;
}
static const ink_cap_t INK_MOGO_CAPS[] = {
    {.id = "reader.open_book", .app = "reader", .title = "打开一本书", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"index\":{\"type\":\"integer\",\"minimum\":0},\"path\":{\"type\":\"string\"}},\"required\":[\"title\"]}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.continue", .app = "reader", .title = "接着读上次的书", .effect = INK_FX_NAVIGATE, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.list_books", .app = "reader", .title = "书架上有哪些书", .effect = INK_FX_READ, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.open_sample", .app = "reader", .title = "打开离线原创阅读示例", .effect = INK_FX_NAVIGATE, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.next_page", .app = "reader", .title = "下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.prev_page", .app = "reader", .title = "上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.list_next", .app = "reader", .title = "书架、目录或书签下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.list_prev", .app = "reader", .title = "书架、目录或书签上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.goto_page", .app = "reader", .title = "跳到本章第几页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book",
     .input = "{\"type\":\"object\",\"properties\":{\"page\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"page\"]}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.read_page", .app = "reader", .title = "读出这一页", .effect = INK_FX_READ, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.chapters", .app = "reader", .title = "列出目录", .effect = INK_FX_READ, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.show_chapters", .app = "reader", .title = "打开目录", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.goto_chapter", .app = "reader", .title = "跳到章节", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"chapter\":{\"type\":\"integer\",\"minimum\":1},\"entry\":{\"type\":\"integer\",\"minimum\":1}}}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.toc_percent", .app = "reader", .title = "目录翻到全书的百分之几", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book",
     .input = "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"value\"]}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.bookmarks", .app = "reader", .title = "列出书签", .effect = INK_FX_READ, .child_ok = true,
     .input = "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"open\":{\"type\":\"boolean\"}}}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.add_bookmark", .app = "reader", .title = "加书签", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.remove_bookmark", .app = "reader", .title = "删书签", .effect = INK_FX_LOCAL_WRITE, .child_ok = true,
     .input = "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}}}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.goto_bookmark", .app = "reader", .title = "跳到书签", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.get_layout", .app = "reader", .title = "查看阅读排版", .effect = INK_FX_READ, .child_ok = true, .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.set_layout", .app = "reader", .title = "修改阅读排版", .effect = INK_FX_LOCAL_WRITE, .child_ok = true,
     .input = "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{" "\"font_size\":{\"type\":\"integer\",\"minimum\":18,\"maximum\":90},\"line_spacing\":{\"type\":\"integer\",\"minimum\":110,\"maximum\":150}," "\"paragraph_spacing\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100},\"margins\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":96}," "\"weight\":{\"type\":\"integer\",\"minimum\":-32,\"maximum\":32},\"indent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3}," "\"header\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1},\"footer\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1}}}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.menu", .app = "reader", .title = "阅读菜单", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book",
     .input = "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"open\":{\"type\":\"boolean\"},\"view\":{\"type\":\"string\",\"enum\":[\"menu\",\"settings\",\"shelf\"]}}}", .timeout_ms = 120000, .run = ink_mogo_action},
    {.id = "reader.back", .app = "reader", .title = "阅读里退一步", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "book", .timeout_ms = 120000, .run = ink_mogo_action},
};
static void ink_mogo_destroy(void) { M.screen = NULL; ink_graft_detach(); }
/* The page keys reach the top page only past sheets that let them through (the AI's answer): they turn the page there
 * as they always did, though the carrier holds other input while a sheet is up. */
static bool ink_mogo_key(ink_key_t key, bool held) {
    if (!held && (key == INK_KEY_NEXT || key == INK_KEY_PREV) && M.active && ink_graft_current()) {
        ink_graft_input_t e = {.kind = INK_GRAFT_KEY, .value = key, .source = INK_SRC_KEY};
        return ink_mogo_input(NULL, &e);
    }
    return ink_graft_key(key, held);
}
/* The member gate closed the space and the carrier let the page go; the same member back: the same book again. */
static void ink_mogo_reattach(void) {
    char book[288]; snprintf(book, sizeof book, "%s", M.book);
    lv_obj_clean(M.screen);
    ink_mogo_build(M.screen, book);
}
static const ink_activity_t INK_MOGO_ACTIVITY = {.name = "book", .app = "reader", .create = ink_mogo_create,
    .destroy = ink_mogo_destroy, .on_key = ink_mogo_key, .on_swipe = ink_graft_swipe, .settle = ink_graft_settle, .snapshot = ink_graft_snapshot};

/* What is being read: home's "继续读" and the lock screen, from the mirror only. */
/* Drop a character that a byte limit cut in two. */
static void ink_mogo_whole(char *s) {
    size_t n = strlen(s), p = n;
    while (p && ((unsigned char)s[p - 1] & 0xc0) == 0x80) p--;
    if (!p || (unsigned char)s[p - 1] < 0xc0) return;
    unsigned char lead = (unsigned char)s[p - 1];
    if (n - (p - 1) < (lead >= 0xf0 ? 4u : lead >= 0xe0 ? 3u : 2u)) s[p - 1] = 0;
}
static int ink_mogo_card(int index, ink_card_t *out) {
    const ink_mogo_slot_t *slot = ink_mogo_slot(ink_member_current()->id, false);
    if (index || !slot || !slot->have) return 0;
    const ink_mogo_card_t *c = &slot->card;
    char title[40], chapter[36];
    snprintf(title, sizeof title, "%.36s", c->title); ink_mogo_whole(title);
    if (c->chapter[0]) snprintf(chapter, sizeof chapter, "%.33s", c->chapter);
    else snprintf(chapter, sizeof chapter, TR("第 %u 章"), (unsigned)c->chapter_no + 1);
    ink_mogo_whole(chapter);
    snprintf(out->id, sizeof out->id, "reading");
    snprintf(out->title, sizeof out->title, "%s", TR("继续读"));
    snprintf(out->text, sizeof out->text, TR("《%s》%s · 读到 %u%%"), title, chapter, (unsigned)c->pct);
    ink_mogo_whole(out->text);
    snprintf(out->action, sizeof out->action, "reader.continue");
    snprintf(out->member, sizeof out->member, "%s", slot->member);
    out->value = c->pct; out->has_value = true; out->bookmark = true;
    return 1;
}
static void ink_mogo_card_done(void *arg) {
    (void)arg;
    C.busy = false;
    ink_mogo_slot_t *slot = ink_mogo_slot(C.owner.member, true);
    bool same = slot->have == C.found && (!C.found || !memcmp(&slot->card, &C.read, sizeof C.read));
    slot->have = C.found; slot->card = C.read;
    if (!same) ink_app_card_changed("reader"); /* home redraws only for news */
    if (C.again) { C.again = false; ink_mogo_card_read(); }
}
static void ink_mogo_card_work(void *arg) {
    (void)arg;
    memset(&C.read, 0, sizeof C.read);
    C.found = ink_mogo_card_load(C.owner.member, &C.read);
    ink_exec_post(INK_EXEC_UI, &C.done, ink_mogo_card_done, NULL);
}
static void ink_mogo_card_read(void) {
    if (C.busy) { C.again = true; return; }
    if (!ink_member_space_open()) return;
    ink_member_identity(&C.owner);
    C.busy = true;
    if (ink_exec_post(INK_EXEC_BG, &C.work, ink_mogo_card_work, NULL)) C.busy = false;
}
/* Old reader data moves over once per member, one book per BG pass; the card is read again after it. */
static void ink_mogo_migrate_work(void *arg);
static void ink_mogo_migrated(void *arg) {
    int more = (int)(intptr_t)arg;
    if (more > 0 && !M.active && !strcmp(C.owner.member, ink_member_current()->id)) {
        if (!ink_exec_post(INK_EXEC_BG, &C.migrate, ink_mogo_migrate_work, NULL)) return;
    }
    if (!more) ink_mogo_slot(C.owner.member, true)->migrated = true;
    C.migrating = false;
    ink_mogo_card_read();
}
static void ink_mogo_migrate_work(void *arg) {
    (void)arg;
    int more = ink_mogo_migrate_step(&C.owner, NULL);
    ink_exec_post(INK_EXEC_UI, &C.migrated, ink_mogo_migrated, (void *)(intptr_t)more);
}
static void ink_mogo_kick(void *ctx, int h, const char *id);
static void ink_mogo_settled(lv_timer_t *t) { (void)t; ink_mogo_kick(NULL, 0, NULL); }
static void ink_mogo_kick(void *ctx, int h, const char *id) {
    (void)ctx; (void)h;
    if (!ink_member_space_open()) return;
    if (M.screen && !M.active && !strcmp(M.screen_member, ink_member_current()->id)) {
        if (M.busy) M.reattach = true; /* after the old session's cleanup */
        else ink_mogo_reattach();
    }
    /* A member already looked at since boot: their card is kept current by the page; only the card coming back is news. */
    const ink_mogo_slot_t *slot = ink_mogo_slot(ink_member_current()->id, false);
    if (slot && slot->migrated && (!id || strcmp(id, "storage.card"))) return;
    /* A member who just came in: the flash is busy with their switch, and a load would hold the BG executor behind it.
     * Their card and old data are looked at once things have settled. */
    static lv_timer_t *settle;
    if (id && !strncmp(id, "member.", 7)) {
        if (!settle) { settle = lv_timer_create(ink_mogo_settled, 3000, NULL); if (settle) lv_timer_set_auto_delete(settle, false); }
        if (settle) { lv_timer_set_repeat_count(settle, 1); lv_timer_reset(settle); lv_timer_resume(settle); return; }
    }
    if (M.active || C.migrating) { ink_mogo_card_read(); return; }
    ink_member_identity(&C.owner);
    C.migrating = true;
    if (ink_exec_post(INK_EXEC_BG, &C.migrate, ink_mogo_migrate_work, NULL)) { C.migrating = false; ink_mogo_card_read(); }
}
/* A book deleted through the file service: the card checks its book again. */
static void ink_mogo_file_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    const ink_mogo_slot_t *slot = ink_mogo_slot(ink_member_current()->id, false);
    if (slot && slot->have) ink_mogo_card_read();
}
static ink_exec_job_t ink_mogo_boot_job;
static void ink_mogo_boot(void *arg) { (void)arg; ink_mogo_kick(NULL, 0, NULL); }

const ink_app_def_t ink_appdef_mogo = {.id = "reader", .title = TR_NOOP("阅读"), .icon = INK_UI_ICON(book), .entry = &INK_MOGO_ACTIVITY,
    .system_app = true, .child_ok = true, .card = ink_mogo_card};
int ink_mogo_init(void) {
    if (ink_mogo_migrate_init() || ink_cap_register(INK_MOGO_CAPS, sizeof INK_MOGO_CAPS / sizeof *INK_MOGO_CAPS) ||
        ink_state_watch("storage.card", ink_mogo_kick, NULL, INK_EXEC_UI) || ink_state_watch("member.", ink_mogo_kick, NULL, INK_EXEC_UI) ||
        ink_state_watch("power.state", ink_mogo_kick, NULL, INK_EXEC_UI) || /* a new incarnation after a lock: the page comes back */
        ink_cap_subscribe("file.changed", ink_mogo_file_changed, NULL, INK_EXEC_UI)) return -1;
    return ink_exec_post(INK_EXEC_UI, &ink_mogo_boot_job, ink_mogo_boot, NULL);
}
