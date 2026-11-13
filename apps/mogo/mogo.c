/* 阅读: the grafted Mogo reader, the only reader (D-139). Touch, keys and the AI call the same reader.* capabilities;
 * the engine runs on the BG executor, one operation at a time, and what is missing is explained on the page (D-138). */
#include <page.h>
#include <book_progress.h>
#include "ink/sys/i18n.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/settings.h"
#include "ink/probe.h"
#include <ink/ui_theme.h>
#include <ink/ui_shell.h>
#define INK_MOGO_PORT_IMPL
#include <port/runtime.h>

void ink_mogo_book_title(const char *path, char *out, size_t cap);
static struct {
    ink_member_identity_t owner;
    ink_exec_job_t work, done;
    ink_call_t *call, *opening; /* the running call; an open_book waiting for the page to be created */
    uint8_t *pixels;
    int w, h, px, status, reason, turns; /* turns: presses that came while busy, turned in the next pass */
    ink_mogo_request_t req;
    bool active, busy, loaded, root, back, drawn, card_new;
    uint64_t opened_us;
    ink_mogo_card_t card;
    lv_obj_t *back_button, *note;
    char description[3072], explanation[400], json[4096];
} M INK_PSRAM_BSS;
/* list_books, continue and open by title run without the page: their own node, member and buffers. */
static struct {
    ink_exec_job_t work, done;
    ink_call_t *call;
    ink_member_identity_t owner;
    bool busy, cont;
    int index, found;
    char title[96], path[288], json[4096];
} S INK_PSRAM_BSS;
/* The home card mirrors the member's last book, read ahead on BG: .card runs on UI and never reads flash. */
static struct {
    ink_exec_job_t work, done, migrate, migrated;
    ink_member_identity_t owner;
    ink_mogo_card_t card, read;
    char member[16];
    bool ready, have, busy, found, migrating, again;
} C INK_PSRAM_BSS;

static void ink_mogo_reply(ink_call_t *call, ink_rc_t status) { if (call) { call->rc.status = status; ink_cap_done(call); } }
static void ink_mogo_card_read(void);
static void ink_mogo_work(void *arg);
static void ink_mogo_unbound(lv_event_t *e) { (void)e; } /* replaced by the transfer.open binding */
static void ink_mogo_overlays(void) {
    if (M.back_button) {
        if (M.back) lv_obj_remove_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
        ink_ui_hint_obj(M.back_button, INK_REFRESH_UI);
        lv_obj_invalidate(M.back_button);
    }
    if (M.note) lv_obj_delete(M.note);
    M.note = NULL;
    if (!M.reason || !M.back_button) return;
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
        if (M.drawn) {
            lv_area_t a = {0, 0, M.w - 1, M.h - 1};
            if (ink_graft_present(M.pixels, M.w, &a, INK_GRAFT_PAGE)) M.status = -1;
        }
        ink_mogo_overlays();
        ink_ui_content_changed();
        if (M.card_new && !strcmp(M.owner.member, ink_member_current()->id)) {
            C.card = M.card; C.have = C.ready = true; snprintf(C.member, sizeof C.member, "%s", M.owner.member);
            ink_app_card_changed("reader");
        }
        if (M.drawn && (M.req.op == INK_MOGO_OP_LOAD || M.req.op == INK_MOGO_OP_OPEN) && !M.reason && M.req.text[0]) {
            ink_probe_value("reader.open_ms", (double)(ink_now_us() - M.opened_us) / 1000.0);
            ink_probe_value("reader.restored_page", (double)(M.req.revision ? M.req.revision - 1 : 0));
        }
    } else M.status = -1;
    M.busy = false;
    if (call) {
        call->rc.revision = M.req.revision;
        if (M.req.json && M.json[0] && !M.status) ink_cap_result(call, M.json);
        if (M.status) snprintf(call->rc.say, sizeof call->rc.say, "%s", M.reason ? M.explanation : TR("现在做不了，请稍后再试"));
        else if (M.req.say[0]) snprintf(call->rc.say, sizeof call->rc.say, "%s", M.req.say);
        ink_mogo_reply(call, M.status ? INK_RC_FAILED : INK_RC_DONE);
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
    if (M.active) {
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
        if (op != INK_MOGO_OP_CHAPTERS && op != INK_MOGO_OP_GET_LAYOUT && op != INK_MOGO_OP_READ && !(op == INK_MOGO_OP_MARKS && !M.req.value)) {
            ink_mogo_page_draw(M.pixels);
            M.drawn = true;
        }
        ink_mogo_page_snapshot(M.description, sizeof M.description);
        M.reason = ink_mogo_page_reason(); M.root = ink_mogo_page_root(); M.back = ink_mogo_page_back_shown();
        snprintf(M.explanation, sizeof M.explanation, "%s", ink_mogo_page_explanation());
        ink_mogo_card_t card;
        if (ink_mogo_page_card(&card) && memcmp(&card, &M.card, sizeof card)) { M.card = card; M.card_new = true; }
        if (op == INK_MOGO_OP_TURN) ink_probe_value("reader.page_render_ms", (double)(ink_now_us() - t0) / 1000.0);
    }
    if (!M.active) {
        ink_mogo_page_close(); ink_app_free(M.pixels); M.pixels = NULL; M.loaded = false;
        memset(&M.owner, 0, sizeof M.owner); memset(&M.card, 0, sizeof M.card); M.turns = 0;
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
    if (M.busy) return false;
    M.owner = *owner; M.w = w; M.h = h;
    int level = ink_member_current()->role == INK_ROLE_CHILD ? 2 : (int)ink_setting_get("display.font_size");
    static const int percent[] = {90, 100, 110, 120};
    M.px = ink_ui_px(percent[level < 0 ? 0 : level > 3 ? 3 : level]); M.active = true; M.loaded = false;
    M.pixels = ink_app_alloc((size_t)w * h, true);
    return M.pixels != NULL;
}
static void ink_mogo_closed(void *ctx) {
    (void)ctx; M.active = false; M.description[0] = 0; M.back_button = M.note = NULL;
    if (!M.busy) {
        M.busy = true;
        ink_exec_post(INK_EXEC_BG, &M.work, ink_mogo_work, NULL);
    }
}
static int ink_mogo_snapshot(void *ctx, char *out, size_t cap) {
    (void)ctx;
    return snprintf(out, cap, "%s", M.busy && !M.description[0] ? TR("阅读 正在排版") : M.description);
}
static bool ink_mogo_call(const char *id, const char *args, ink_source_t src) {
    ink_call_opts_t opts = {.src = src, .args_json = args && args[0] ? args : NULL};
    return !ink_cap_call(id, NULL, &opts);
}
/* Keys, taps and swipes become reader.* calls. BACK on the shelf or an explanation page is the shell's: no loop. */
static bool ink_mogo_input(void *ctx, const ink_graft_input_t *e) {
    (void)ctx;
    bool reading = !M.root && !M.back, lists = M.root ? !M.reason : M.back;
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
static void ink_mogo_create(lv_obj_t *screen, void *arg) {
    ink_call_t *call = M.opening;
    M.opening = NULL;
    if (ink_graft_attach(screen, &INK_MOGO_CLIENT)) { ink_mogo_reply(call, INK_RC_FAILED); return; }
    /* The shell's own back button: hidden on the bare body (Mogo and Kindle keep the page clean), a white box over the corner. */
    M.back_button = ink_ui_circle(screen, "back", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    lv_obj_set_style_bg_opa(M.back_button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(M.back_button, ink_ui_paper(), 0);
    ink_shell_bind(M.back_button, "reader.back", NULL);
    lv_obj_add_flag(M.back_button, LV_OBJ_FLAG_HIDDEN);
    M.reason = 0; M.root = true; M.back = false; M.description[0] = 0; M.opened_us = ink_now_us();
    ink_mogo_request_t req = {.op = INK_MOGO_OP_LOAD};
    snprintf(req.text, sizeof req.text, "%s", arg ? (const char *)arg : "");
    if (!ink_mogo_start(&req, call)) ink_mogo_reply(call, INK_RC_FAILED);
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
    M.opening = call;
    if (ink_app_open("reader", NULL, (void *)path) && M.opening) { M.opening = NULL; ink_mogo_reply(call, INK_RC_FAILED); }
}
static void ink_mogo_shelf_done(void *arg) {
    (void)arg;
    ink_call_t *call = S.call;
    S.call = NULL; S.busy = false;
    if (!call) return;
    if (!strcmp(call->cap->id, "reader.list_books")) {
        ink_cap_result(call, S.json);
        snprintf(call->rc.say, sizeof call->rc.say, "%s", S.title);
        snprintf(call->rc.object, sizeof call->rc.object, "shelf:books");
        ink_mogo_reply(call, S.found == -2 ? INK_RC_FAILED : INK_RC_DONE);
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
    if (S.cont) {
        (void)ink_mogo_migrate_step(&S.owner, "");
        ink_mogo_owner(&S.owner);
        ink_fs_file_t *f = NULL;
        if (book_progress_last_path(S.path, sizeof S.path) && (!strncmp(S.path, "asset:", 6) || !ink_graft_file_open(&S.owner, S.path, INK_FS_READ, &f))) {
            if (f) ink_fs_close(f);
            ink_mogo_book_title(S.path, S.title, sizeof S.title);
            S.found = 0;
        }
    } else {
        bool list = !strcmp(S.call->cap->id, "reader.list_books");
        S.found = ink_mogo_shelf_query(&S.owner, list ? S.json : NULL, sizeof S.json, S.title, S.index, S.path, sizeof S.path);
        if (list) snprintf(S.title, sizeof S.title, "%s", TR("书架上有这些书"));
        else if (S.found >= 0) ink_mogo_book_title(S.path, S.title, sizeof S.title);
    }
    ink_exec_post(INK_EXEC_UI, &S.done, ink_mogo_shelf_done, NULL);
}
static void ink_mogo_shelf_start(ink_call_t *call, bool cont) {
    if (S.busy) { call->rc.status = INK_RC_STALE; return; }
    S.busy = true; S.call = call; S.cont = cont; S.path[0] = S.json[0] = 0;
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
        S.index = ink_mogo_value(call, "index", -1); S.title[0] = 0;
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
    if (req.op == INK_MOGO_OP_TURN) {
        ink_probe_count("reader.page_turns");
        if (M.busy && M.active && M.loaded && M.req.op == INK_MOGO_OP_TURN) { M.turns += req.value; return; } /* turned with the next pass */
    }
    if (!ink_mogo_start(&req, call)) call->rc.status = M.active ? INK_RC_STALE : INK_RC_FAILED;
}
#define INK_MOGO_CAP(name, title_, fx, act, schema) \
    {.id = "reader." name, .app = "reader", .title = title_, .effect = fx, .child_ok = true, .activity = act, .input = schema, .timeout_ms = 120000, .run = ink_mogo_action}
static const ink_cap_t INK_MOGO_CAPS[] = {
    INK_MOGO_CAP("open_book", "打开一本书", INK_FX_NAVIGATE, NULL, "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"index\":{\"type\":\"integer\",\"minimum\":0},\"path\":{\"type\":\"string\"}},\"required\":[\"title\"]}"),
    INK_MOGO_CAP("continue", "接着读上次的书", INK_FX_NAVIGATE, NULL, NULL),
    INK_MOGO_CAP("list_books", "书架上有哪些书", INK_FX_READ, NULL, NULL),
    INK_MOGO_CAP("open_sample", "打开离线原创阅读示例", INK_FX_NAVIGATE, NULL, NULL),
    INK_MOGO_CAP("next_page", "下一页", INK_FX_NAVIGATE, "book", NULL),
    INK_MOGO_CAP("prev_page", "上一页", INK_FX_NAVIGATE, "book", NULL),
    INK_MOGO_CAP("list_next", "书架、目录或书签下一页", INK_FX_NAVIGATE, NULL, NULL),
    INK_MOGO_CAP("list_prev", "书架、目录或书签上一页", INK_FX_NAVIGATE, NULL, NULL),
    INK_MOGO_CAP("goto_page", "跳到本章第几页", INK_FX_NAVIGATE, "book", "{\"type\":\"object\",\"properties\":{\"page\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"page\"]}"),
    INK_MOGO_CAP("read_page", "读出这一页", INK_FX_READ, "book", NULL),
    INK_MOGO_CAP("chapters", "列出目录", INK_FX_READ, NULL, NULL),
    INK_MOGO_CAP("show_chapters", "打开目录", INK_FX_NAVIGATE, "book", NULL),
    INK_MOGO_CAP("goto_chapter", "跳到章节", INK_FX_NAVIGATE, NULL, "{\"type\":\"object\",\"properties\":{\"chapter\":{\"type\":\"integer\",\"minimum\":1},\"entry\":{\"type\":\"integer\",\"minimum\":1}}}"),
    INK_MOGO_CAP("toc_percent", "目录翻到全书的百分之几", INK_FX_NAVIGATE, "book", "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"value\"]}"),
    INK_MOGO_CAP("bookmarks", "列出书签", INK_FX_READ, NULL, "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"open\":{\"type\":\"boolean\"}}}"),
    INK_MOGO_CAP("add_bookmark", "加书签", INK_FX_LOCAL_WRITE, "book", NULL),
    INK_MOGO_CAP("remove_bookmark", "删书签", INK_FX_LOCAL_WRITE, NULL, "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}}}"),
    INK_MOGO_CAP("goto_bookmark", "跳到书签", INK_FX_NAVIGATE, NULL, "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}"),
    INK_MOGO_CAP("get_layout", "查看阅读排版", INK_FX_READ, NULL, NULL),
    INK_MOGO_CAP("set_layout", "修改阅读排版", INK_FX_LOCAL_WRITE, NULL, "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{"
        "\"font_size\":{\"type\":\"integer\",\"minimum\":18,\"maximum\":90},\"line_spacing\":{\"type\":\"integer\",\"minimum\":110,\"maximum\":150},"
        "\"paragraph_spacing\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100},\"margins\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":96},"
        "\"weight\":{\"type\":\"integer\",\"minimum\":-32,\"maximum\":32},\"indent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3},"
        "\"header\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1},\"footer\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1}}}"),
    INK_MOGO_CAP("menu", "阅读菜单", INK_FX_NAVIGATE, "book", "{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{\"open\":{\"type\":\"boolean\"},\"view\":{\"type\":\"string\",\"enum\":[\"menu\",\"settings\",\"shelf\"]}}}"),
    INK_MOGO_CAP("back", "阅读里退一步", INK_FX_NAVIGATE, "book", NULL),
};
static const ink_activity_t INK_MOGO_ACTIVITY = {.name = "book", .app = "reader", .create = ink_mogo_create,
    .destroy = ink_graft_detach, .on_key = ink_graft_key, .on_swipe = ink_graft_swipe, .settle = ink_graft_settle, .snapshot = ink_graft_snapshot};

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
    if (index || !C.ready || !C.have || strcmp(C.member, ink_member_current()->id)) return 0;
    char title[40], chapter[36];
    snprintf(title, sizeof title, "%.36s", C.card.title); ink_mogo_whole(title);
    if (C.card.chapter[0]) snprintf(chapter, sizeof chapter, "%.33s", C.card.chapter);
    else snprintf(chapter, sizeof chapter, TR("第 %u 章"), (unsigned)C.card.chapter_no + 1);
    ink_mogo_whole(chapter);
    snprintf(out->id, sizeof out->id, "reading");
    snprintf(out->title, sizeof out->title, "%s", TR("继续读"));
    snprintf(out->text, sizeof out->text, TR("《%s》%s · 读到 %u%%"), title, chapter, (unsigned)C.card.pct);
    ink_mogo_whole(out->text);
    snprintf(out->action, sizeof out->action, "reader.continue");
    snprintf(out->member, sizeof out->member, "%s", C.member);
    out->value = C.card.pct; out->has_value = true; out->bookmark = true;
    return 1;
}
static void ink_mogo_card_done(void *arg) {
    (void)arg;
    C.busy = false;
    if (!strcmp(C.owner.member, ink_member_current()->id)) {
        C.have = C.found; C.card = C.read; C.ready = true; snprintf(C.member, sizeof C.member, "%s", C.owner.member);
        ink_app_card_changed("reader");
    }
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
    C.migrating = false;
    ink_mogo_card_read();
}
static void ink_mogo_migrate_work(void *arg) {
    (void)arg;
    int more = ink_mogo_migrate_step(&C.owner, NULL);
    ink_exec_post(INK_EXEC_UI, &C.migrated, ink_mogo_migrated, (void *)(intptr_t)more);
}
static void ink_mogo_kick(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_member_space_open()) return;
    if (M.active || C.migrating) { ink_mogo_card_read(); return; }
    ink_member_identity(&C.owner);
    C.migrating = true;
    if (ink_exec_post(INK_EXEC_BG, &C.migrate, ink_mogo_migrate_work, NULL)) { C.migrating = false; ink_mogo_card_read(); }
}
static ink_exec_job_t ink_mogo_boot_job;
static void ink_mogo_boot(void *arg) { (void)arg; ink_mogo_kick(NULL, 0, NULL); }

const ink_app_def_t ink_appdef_mogo = {.id = "reader", .title = TR_NOOP("阅读"), .icon = INK_UI_ICON(book), .entry = &INK_MOGO_ACTIVITY,
    .system_app = true, .child_ok = true, .card = ink_mogo_card};
int ink_mogo_init(void) {
    if (ink_mogo_migrate_init() || ink_cap_register(INK_MOGO_CAPS, sizeof INK_MOGO_CAPS / sizeof *INK_MOGO_CAPS) ||
        ink_state_watch("storage.card", ink_mogo_kick, NULL, INK_EXEC_UI) || ink_state_watch("member.", ink_mogo_kick, NULL, INK_EXEC_UI)) return -1;
    return ink_exec_post(INK_EXEC_UI, &ink_mogo_boot_job, ink_mogo_boot, NULL);
}
