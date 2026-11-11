/* CrossMux rules and search, member storage and the shared human/AI actions. */
#include "engine.h"
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static const ink_db_def_t DB = {.name = "gomoku.game", .title = "五子棋棋局", .scope = INK_DB_MEMBER,
    .schema = 1, .max_records = 1, .max_bytes = sizeof(ink_gomoku_t)};
static struct {
    ink_gomoku_t s, before;
    ink_db_t *db;
    ink_call_t *call;
    ink_exec_job_t job;
    uint32_t epoch, loading_epoch;
    char member[16], board[226], json[640], status[80];
    bool saved, thinking;
    int move, cursor;
    uint8_t drawn[225];
    lv_obj_t *cells[225], *label, *resume;
} G INK_PSRAM_BSS;
static const ink_activity_t MENU, GAME;
static void run(ink_call_t *c);
static bool over(void) { return G.s.winner || G.s.count == G.s.size * G.s.size; }
static bool ai_turn(void) { return G.s.mode && (G.s.count & 1) && !over(); }
static void describe(void) {
    for (int i = 0; i < G.s.size * G.s.size; i++) G.board[i] = (char)('0' + G.s.cells[i]);
    G.board[G.s.size * G.s.size] = 0;
    snprintf(G.json, sizeof G.json, "{\"size\":%u,\"mode\":\"%s\",\"difficulty\":%u,\"board\":\"%s\",\"moves\":%u,\"turn\":%u,\"legal\":%u,\"winner\":%u,\"over\":%s,\"thinking\":%s}",
        G.s.size, G.s.mode ? "ai" : "two", G.s.level, G.board, G.s.count, over() ? 0 : 1 + (G.s.count & 1),
        over() ? 0 : G.s.size * G.s.size - G.s.count, G.s.winner, over() ? "true" : "false", G.thinking ? "true" : "false");
    snprintf(G.status, sizeof G.status, "%s", G.thinking ? TR("思考中") : G.s.winner == 1 ? TR("黑棋获胜") :
        G.s.winner == 2 ? TR("白棋获胜") : over() ? TR("平局") : (G.s.count & 1) ? TR("轮到白棋") : TR("轮到黑棋"));
}
static int cursor(void) { return G.s.count ? G.s.history[G.s.count - 1] : G.s.size * (G.s.size / 2) + G.s.size / 2; }
static void show(void) {
    describe();
    int next = cursor();
    if (G.label && strcmp(lv_label_get_text(G.label), G.status)) lv_label_set_text(G.label, G.status);
    if (G.resume) {
        if (G.saved) lv_obj_remove_state(G.resume, LV_STATE_DISABLED);
        else lv_obj_add_state(G.resume, LV_STATE_DISABLED);
    }
    for (int i = 0; i < G.s.size * G.s.size; i++) {
        if (G.cells[i] && (G.drawn[i] != G.s.cells[i] || (next != G.cursor && (i == next || i == G.cursor)))) lv_obj_invalidate(G.cells[i]);
        G.drawn[i] = G.s.cells[i];
    }
    G.cursor = next;
}
static void answer(ink_call_t *c) { describe(); ink_cap_result(c, G.json); snprintf(c->rc.say, sizeof c->rc.say, "%s", G.status); }
static void fail(ink_call_t *c, ink_rc_t code, const char *why) { c->rc.status = code; snprintf(c->rc.say, sizeof c->rc.say, "%s", why); }
static bool save(ink_call_t *c) {
    ink_db_write_t w = {.id = 1, .data = &G.s, .len = sizeof G.s, .src = c->src};
    if (ink_db_put(G.db, &w, NULL)) { G.s = G.before; fail(c, INK_RC_FAILED, TR("棋局未保存，请重试")); return false; }
    G.saved = true;
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "moves", G.s.count); ink_args_int(&a, "winner", G.s.winner);
    ink_cap_emit_for("games.changed", &a, G.member);
    if (G.label) ink_ui_content_changed();
    return true;
}
static void ai_done(void *arg) {
    (void)arg; ink_call_t *c = G.call; G.call = NULL; G.thinking = false;
    if (G.epoch != ink_member_epoch() || !ink_member_space_open() || !ink_member_app_time_allows("gomoku")) {
        fail(c, INK_RC_CANCELLED, TR("使用者或可用时段已改变"));
    } else {
        G.before = G.s;
        if (G.move < 0 || ink_gomoku_play(&G.s, 1, G.move / G.s.size, G.move % G.s.size)) fail(c, INK_RC_FAILED, TR("电脑未能落子"));
        else if (save(c)) { show(); answer(c); }
    }
    if (G.epoch == ink_member_epoch()) show();
    ink_cap_done(c);
}
static void search(void *arg) { (void)arg; G.move = ink_gomoku_choose(&G.s); ink_exec_post(INK_EXEC_UI, &G.job, ai_done, NULL); }
static void loaded(void *arg) {
    (void)arg; ink_call_t *c = G.call; G.call = NULL;
    if (G.loading_epoch != ink_member_epoch() || !ink_member_space_open()) {
        ink_db_close(G.db); G.db = NULL; fail(c, INK_RC_CANCELLED, TR("使用者已改变"));
    } else if (!G.db) fail(c, INK_RC_FAILED, TR("无法读取棋局"));
    else {
        ink_rec_t rec;
        G.saved = ink_db_get(G.db, 1, &rec, &G.s, sizeof G.s) == 0 && rec.len == sizeof G.s && !ink_gomoku_play(&G.s, 0, 0, 0);
        if (!G.saved) G.s = (ink_gomoku_t){.size = 15, .level = 1};
        G.epoch = G.loading_epoch;
        run(c);
    }
    if (!G.call) ink_cap_done(c);
}
static void load(void *arg) { (void)arg; G.db = ink_db_open_member(&DB, G.member); ink_exec_post(INK_EXEC_UI, &G.job, loaded, NULL); }
static void run(ink_call_t *c) {
    bool state = !strcmp(c->cap->id, "games.state"), start = !strcmp(c->cap->id, "games.start");
    if (G.call) {
        if (state && G.epoch == ink_member_epoch() && G.db) answer(c);
        else fail(c, INK_RC_FAILED, TR("请稍后再试"));
        return;
    }
    if (!G.db || G.epoch != ink_member_epoch()) {
        if (ink_exec_ready(INK_EXEC_BG)) { fail(c, INK_RC_FAILED, TR("无法读取棋局")); return; }
        ink_db_close(G.db); G.db = NULL; G.loading_epoch = ink_member_epoch();
        snprintf(G.member, sizeof G.member, "%s", ink_cap_call_member(c));
        G.call = c; ink_cap_defer(c); ink_exec_post(INK_EXEC_BG, &G.job, load, NULL); return;
    }
    if (state) { show(); answer(c); return; }
    G.before = G.s;
    bool resume = false; ink_args_get_bool(&c->args, "resume", &resume);
    if (start && resume && !G.saved) { fail(c, INK_RC_INVALID, TR("没有上一局")); return; }
    if ((start && !resume) || !strcmp(c->cap->id, "games.reset")) {
        int64_t level = G.s.level, size = G.s.size; char mode[8] = "";
        ink_args_get_int(&c->args, "difficulty", &level); ink_args_get_int(&c->args, "size", &size);
        ink_args_get_str(&c->args, "mode", mode, sizeof mode);
        G.s = (ink_gomoku_t){.size = (uint8_t)size, .level = (uint8_t)level, .mode = mode[0] ? !strcmp(mode, "ai") : G.s.mode};
    } else if (!strcmp(c->cap->id, "games.move")) {
        int64_t r = -1, col = -1; ink_args_get_int(&c->args, "row", &r); ink_args_get_int(&c->args, "col", &col);
        if (ai_turn() || ink_gomoku_play(&G.s, 1, (int)r, (int)col)) { fail(c, INK_RC_INVALID, TR("这里不能落子")); return; }
    } else if (!start && ink_gomoku_play(&G.s, 2, 0, 0)) { fail(c, INK_RC_INVALID, TR("没有可以悔的棋")); return; }
    if (!save(c)) return;
    if (start && strcmp(ink_ui_page_name(), "GomokuGame")) { ink_app_open("gomoku", NULL, NULL); ink_ui_push(&GAME, NULL); }
    else if (start && G.s.size != G.before.size) ink_ui_redraw();
    if (ai_turn()) {
        if (ink_exec_ready(INK_EXEC_BG)) { fail(c, INK_RC_FAILED, TR("电脑暂时不可用")); return; }
        G.call = c; G.thinking = true; show(); ink_cap_defer(c); ink_exec_post(INK_EXEC_BG, &G.job, search, NULL);
    } else { show(); answer(c); }
}
static void draw_cell(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e), r = i / G.s.size, col = i % G.s.size;
    lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line); line.color = ink_ui_ink(); line.width = ink_ui_px(1);
    line.p1 = (lv_point_precise_t){col ? a.x1 : x, y}; line.p2 = (lv_point_precise_t){col + 1 == G.s.size ? x : a.x2 + 1, y}; lv_draw_line(layer, &line);
    line.p1 = (lv_point_precise_t){x, r ? a.y1 : y}; line.p2 = (lv_point_precise_t){x, r + 1 == G.s.size ? y : a.y2 + 1}; lv_draw_line(layer, &line);
    int pad = (a.x2 - a.x1) / 7; lv_area_t stone = {a.x1 + pad, a.y1 + pad, a.x2 - pad, a.y2 - pad};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.radius = LV_RADIUS_CIRCLE; d.border_width = ink_ui_px(INK_STROKE);
    d.border_color = ink_ui_ink(); d.bg_color = G.s.cells[i] == 1 ? ink_ui_ink() : ink_ui_paper();
    if (G.s.cells[i]) lv_draw_rect(layer, &d, &stone);
    if (i == cursor()) { d.radius = 0; d.bg_opa = LV_OPA_TRANSP; lv_draw_rect(layer, &d, &a); }
}
static void destroy(void) { memset(G.cells, 0, sizeof G.cells); G.label = G.resume = NULL; }
static lv_obj_t *button(lv_obj_t *s, const char *text, int x, int y, int w, const char *cap, const char *args) {
    lv_obj_t *b = ink_ui_pill(s, text, x, y, w, INK_CONTROL, false, NULL, NULL); ink_shell_bind(b, cap, args); return b;
}
static void menu(lv_obj_t *s, void *arg) {
    (void)arg; destroy(); ink_shell_statusbar(s, true); ink_shell_topbar(s, TR("五子棋"));
    const char *const labels[] = {TR("继续上一局"), TR("双人 · 15 路"), TR("双人 · 9 路"), TR("人机 · 简单"), TR("人机 · 普通"), TR("人机 · 困难")};
    const char *const args[] = {"{\"resume\":true}", "{\"mode\":\"two\",\"size\":15}", "{\"mode\":\"two\",\"size\":9}",
        "{\"mode\":\"ai\",\"size\":15,\"difficulty\":0}", "{\"mode\":\"ai\",\"size\":15,\"difficulty\":1}", "{\"mode\":\"ai\",\"size\":15,\"difficulty\":2}"};
    for (unsigned i = 0; i < sizeof labels / sizeof *labels; i++) {
        lv_obj_t *b = button(s, labels[i], INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_GAP + i * (INK_CONTROL + INK_GAP),
            INK_CANVAS_W - 2 * INK_MARGIN, "games.start", args[i]);
        if (!i) G.resume = b;
    }
    if (G.epoch == ink_member_epoch() && G.db) show();
    else { lv_obj_add_state(G.resume, LV_STATE_DISABLED); ink_call_opts_t o = {.src = INK_SRC_TOUCH}; ink_cap_call("games.state", NULL, &o); }
}
static void game(lv_obj_t *s, void *arg) {
    (void)arg; destroy(); ink_shell_statusbar(s, true); ink_shell_topbar(s, TR("五子棋"));
    G.label = ink_ui_line(s, "", INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
    int pitch = (INK_CANVAS_W - 2 * INK_MARGIN) / G.s.size, left = (INK_CANVAS_W - pitch * G.s.size) / 2;
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_BLOCK_L;
    for (int i = 0; i < G.s.size * G.s.size; i++) {
        char args[48]; snprintf(args, sizeof args, "{\"row\":%d,\"col\":%d}", i / G.s.size, i % G.s.size);
        G.cells[i] = ink_ui_box(s, left + i % G.s.size * pitch, top + i / G.s.size * pitch, pitch, pitch, 0, INK_SURFACE_WHITE, false);
        lv_obj_add_event_cb(G.cells[i], draw_cell, LV_EVENT_DRAW_MAIN, (void *)(intptr_t)i); ink_shell_bind(G.cells[i], "games.move", args);
    }
    ink_ui_label(s, TR("点交叉点落子，方框标出最后一手"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, top + pitch * G.s.size + INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN);
    int w = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2, y = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    button(s, TR("悔棋"), INK_MARGIN, y, w, "games.undo", NULL); button(s, TR("重开"), INK_MARGIN + w + INK_GAP, y, w, "games.reset", NULL);
    show();
}
static int snapshot(char *out, size_t cap) {
    if (G.epoch != ink_member_epoch()) return snprintf(out, cap, "{}");
    describe();
    const ink_ui_field_spec_t fields[] = {{.id = "board", .role = "text", .label = TR("棋盘，按行，0 空 1 黑 2 白"), .value = G.board}, {.id = "turn", .role = "status", .label = TR("当前回合"), .value = G.status}};
    const ink_ui_action_spec_t actions[] = {{.id = "start", .label = TR("开始或继续"), .capability = "games.start", .enabled = true},
        {.id = "state", .label = TR("棋局"), .capability = "games.state", .enabled = true}, {.id = "move", .label = TR("落子"), .capability = "games.move", .enabled = !G.call && !over()},
        {.id = "undo", .label = TR("悔棋"), .capability = "games.undo", .enabled = !G.call && G.s.count}, {.id = "reset", .label = TR("重开"), .capability = "games.reset", .enabled = !G.call}};
    ink_ui_page_spec_t page = {.id = ink_ui_page_name(), .title = TR("五子棋"), .object = {"gomoku", 1}, .fields = fields, .field_count = 2, .actions = actions, .action_count = 5};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static const ink_activity_t MENU = {.name = "GomokuMenu", .app = "gomoku", .create = menu, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t GAME = {.name = "GomokuGame", .app = "gomoku", .create = game, .destroy = destroy, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "gomoku", .title = TR_NOOP("五子棋"), .icon = INK_UI_ICON(grid), .entry = &MENU, .child_ok = true, .system_app = true};
static const ink_cap_t CAPS[] = {
    {.id = "games.start", .app = "gomoku", .title = "开始五子棋或继续上一局，难度0简单1普通2困难，黑棋先手",
     .input = "{\"type\":\"object\",\"properties\":{\"mode\":{\"type\":\"string\",\"enum\":[\"ai\",\"two\"]},\"difficulty\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":2},\"size\":{\"type\":\"integer\",\"enum\":[9,15]},\"resume\":{\"type\":\"boolean\"}},\"additionalProperties\":false}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .timeout_ms = 15000, .run = run},
    {.id = "games.state", .app = "gomoku", .title = "读取五子棋棋盘（按行0空1黑2白）、回合、合法着法数与胜负", .effect = INK_FX_READ, .child_ok = true, .run = run},
    {.id = "games.move", .app = "gomoku", .title = "五子棋落子，行列从0开始；人机时自动接电脑的一手",
     .input = "{\"type\":\"object\",\"properties\":{\"row\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":14},\"col\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":14}},\"required\":[\"row\",\"col\"],\"additionalProperties\":false}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .timeout_ms = 15000, .run = run},
    {.id = "games.undo", .app = "gomoku", .title = "悔棋，人机模式退回到人的回合", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.reset", .app = "gomoku", .title = "按当前模式和难度重开五子棋", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.changed", .app = "gomoku", .title = "当前成员的棋局已变化", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true}};
int ink_gomoku_init(void) { return ink_db_register(&DB) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ? -1 : 0; }
