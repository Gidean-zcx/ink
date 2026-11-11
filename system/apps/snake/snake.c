/* Slow, page-owned stepping; touch, keys and AI share the same capabilities. */
/* ui-scale-runtime: dimension x,y,w Forwarded pill geometry; callers use theme tokens. */
#include "engine.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>
static const ink_db_def_t DB = {.name = "snake.best", .title = "贪吃蛇最高分", .scope = INK_DB_MEMBER,
    .schema = 1, .max_records = 1, .max_bytes = sizeof(uint32_t)};
static struct {
    ink_snake_t s;
    uint32_t best, epoch, loading;
    bool unsaved;
    ink_db_t *db;
    ink_call_t *call;
    ink_exec_job_t job;
    char member[16], board[INK_SNAKE_CELLS + 1], json[512], status[64], score[96];
    uint8_t drawn[INK_SNAKE_CELLS];
    lv_obj_t *cells[INK_SNAKE_CELLS], *label, *record, *start;
    lv_timer_t *timer;
} S INK_PSRAM_BSS;
static void run(ink_call_t *c);
static void describe(void) {
    memset(S.board, '.', INK_SNAKE_CELLS); S.board[INK_SNAKE_CELLS] = 0;
    if (S.s.length && S.s.food >= 0) S.board[S.s.food] = '*';
    for (int i = 0; i < S.s.length; i++) S.board[S.s.body[i]] = i ? 'o' : '@';
    snprintf(S.status, sizeof S.status, "%s", S.unsaved ? TR("成绩未保存，请重试") : !S.s.length ? TR("按开始玩一局") : S.s.over ? TR("本局结束") : S.s.paused ? TR("已暂停") : TR("每半秒前进一步"));
    snprintf(S.score, sizeof S.score, TR("得分 %d · 最高 %u"), S.s.score, (unsigned)S.best);
    snprintf(S.json, sizeof S.json, "{\"side\":12,\"board\":\"%s\",\"length\":%d,\"score\":%d,\"best\":%u,\"direction\":%d,\"next\":%d,\"paused\":%s,\"over\":%s}",
        S.board, S.s.length, S.s.score, (unsigned)S.best, S.s.dir, S.s.next, S.s.paused ? "true" : "false", S.s.over ? "true" : "false");
}
static void show(void) {
    describe();
    if (S.timer) { if (!S.s.length || S.s.paused || S.s.over) lv_timer_pause(S.timer); else lv_timer_resume(S.timer); }
    if (S.label && strcmp(lv_label_get_text(S.label), S.status)) lv_label_set_text(S.label, S.status);
    if (S.record && strcmp(lv_label_get_text(S.record), S.score)) lv_label_set_text(S.record, S.score);
    if (S.start) {
        lv_obj_t *label = lv_obj_get_child(S.start, 0); const char *text = S.s.length ? TR("重开") : TR("开始");
        if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
    }
    for (int i = 0; i < INK_SNAKE_CELLS; i++) if (S.cells[i] && S.drawn[i] != S.board[i]) {
        S.drawn[i] = (uint8_t)S.board[i]; ink_ui_hint_obj(S.cells[i], INK_REFRESH_A2); lv_obj_invalidate(S.cells[i]);
    }
}
static void changed(void) {
    show();
    if (S.label) ink_ui_content_changed();
    ink_cap_emit_for("games.snake.changed", NULL, S.member);
}
static void fail(ink_call_t *c, ink_rc_t rc) { c->rc.status = rc; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("暂时不能操作，请重试")); }
static bool save(ink_source_t src) {
    if ((uint32_t)S.s.score <= S.best) return true;
    uint32_t best = (uint32_t)S.s.score; ink_db_write_t w = {.id = 1, .data = &best, .len = sizeof best, .src = src};
    if (ink_db_put(S.db, &w, NULL)) return false;
    S.best = best; return true;
}
static void loaded(void *arg) {
    (void)arg; ink_call_t *c = S.call; S.call = NULL;
    if (S.loading != ink_member_epoch() || !ink_member_space_open()) { ink_db_close(S.db); S.db = NULL; fail(c, INK_RC_CANCELLED); }
    else if (!S.db) fail(c, INK_RC_FAILED);
    else {
        ink_rec_t rec; S.best = 0;
        if (ink_db_get(S.db, 1, &rec, &S.best, sizeof S.best) || rec.len != sizeof S.best) S.best = 0;
        S.s = (ink_snake_t){.paused = true}; S.unsaved = false; S.epoch = S.loading; show(); run(c);
    }
    ink_cap_done(c);
}
static void load(void *arg) { (void)arg; S.db = ink_db_open_member(&DB, S.member); ink_exec_post(INK_EXEC_UI, &S.job, loaded, NULL); }
static void run(ink_call_t *c) {
    if (S.call) { fail(c, INK_RC_FAILED); return; }
    if (!S.db || S.epoch != ink_member_epoch()) {
        if (ink_exec_ready(INK_EXEC_BG)) { fail(c, INK_RC_FAILED); return; }
        ink_db_close(S.db); S.db = NULL; S.loading = ink_member_epoch(); snprintf(S.member, sizeof S.member, "%s", ink_cap_call_member(c));
        S.call = c; ink_cap_defer(c); ink_exec_post(INK_EXEC_BG, &S.job, load, NULL); return;
    }
    const char *id = c->cap->id;
    if (!strcmp(id, "games.snake.state")) { describe(); ink_cap_result(c, S.json); return; }
    if (!strcmp(id, "games.snake.start") || !strcmp(id, "games.snake.reset")) {
        uint32_t seed; hal_random_fill(&seed, sizeof seed); ink_snake_start(&S.s, seed); S.unsaved = false; ink_app_open("snake", NULL, NULL); S.s.paused = false;
    } else if (!strcmp(id, "games.snake.turn")) {
        int64_t dir = -1; ink_args_get_int(&c->args, "direction", &dir);
        if (ink_snake_turn(&S.s, (int)dir)) { fail(c, INK_RC_INVALID); return; }
    } else {
        if (!S.s.length || S.s.over) { fail(c, INK_RC_INVALID); return; }
        bool resume = !strcmp(id, "games.snake.resume");
        if (resume) ink_app_open("snake", NULL, NULL);
        S.s.paused = !resume;
    }
    if (S.timer && strcmp(id, "games.snake.turn")) lv_timer_reset(S.timer);
    changed(); describe(); ink_cap_result(c, S.json);
}
static void tick(lv_timer_t *t) {
    (void)t;
    if (S.epoch != ink_member_epoch() || !ink_member_space_open()) return;
    if (!ink_member_app_time_allows("snake") || ink_ui_overlay_up()) { S.s.paused = true; show(); return; }
    if (!S.s.length || S.s.paused || S.s.over) return;
    uint32_t seed; hal_random_fill(&seed, sizeof seed); ink_snake_step(&S.s, seed);
    S.unsaved = !save(INK_SRC_SYSTEM);
    if (S.unsaved) S.s.paused = true;
    changed();
}
static void draw_cell(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e); char cell = S.board[i];
    if (cell == '.') return;
    lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    int pad = ink_ui_px(INK_GAP_XS); a.x1 += pad; a.y1 += pad; a.x2 -= pad; a.y2 -= pad;
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color = ink_ui_ink(); d.radius = cell == '*' ? LV_RADIUS_CIRCLE : 0;
    if (cell == '@') { d.border_color = ink_ui_ink(); d.border_width = ink_ui_px(INK_STROKE); d.bg_color = ink_ui_paper(); }
    lv_draw_rect(lv_event_get_layer(e), &d, &a);
}
static void destroy(void) {
    if (S.timer) lv_timer_delete(S.timer);
    S.timer = NULL; S.s.paused = true; memset(S.cells, 0, sizeof S.cells); S.label = S.record = S.start = NULL;
}
static lv_obj_t *button(lv_obj_t *s, const char *text, int x, int y, int w, const char *cap, const char *args) {
    lv_obj_t *b = ink_ui_pill(s, text, x, y, w, INK_CONTROL, false, NULL, NULL); ink_shell_bind(b, cap, args); return b;
}
static void create(lv_obj_t *s, void *arg) {
    (void)arg; ink_shell_statusbar(s, true); ink_shell_topbar(s, TR("贪吃蛇"));
    if (S.epoch != ink_member_epoch()) memset(S.board, '.', INK_SNAKE_CELLS);
    S.label = ink_ui_line(s, "", INK_FONT_BODY, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
    S.record = ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_LINE_BODY, INK_CANVAS_W - 2 * INK_MARGIN);
    int pitch = (INK_CANVAS_W - 2 * INK_MARGIN) / INK_SNAKE_SIDE, left = (INK_CANVAS_W - pitch * INK_SNAKE_SIDE) / 2;
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_BLOCK_L, size = pitch * INK_SNAKE_SIDE;
    ink_ui_box(s, left - INK_STROKE, top - INK_STROKE, size + 2 * INK_STROKE, size + 2 * INK_STROKE, 0, INK_SURFACE_WHITE, true);
    for (int i = 0; i < INK_SNAKE_CELLS; i++) {
        S.cells[i] = ink_ui_box(s, left + i % INK_SNAKE_SIDE * pitch, top + i / INK_SNAKE_SIDE * pitch, pitch, pitch, 0, INK_SURFACE_WHITE, false);
        lv_obj_add_event_cb(S.cells[i], draw_cell, LV_EVENT_DRAW_MAIN, (void *)(intptr_t)i); S.drawn[i] = 0;
    }
    int w = (INK_CANVAS_W - 2 * INK_MARGIN - 3 * INK_GAP_S) / 4, y = top + size + INK_GAP;
    const char *const names[] = {TR("上"), TR("右"), TR("下"), TR("左")};
    for (int i = 0; i < 4; i++) { char args[24]; snprintf(args, sizeof args, "{\"direction\":%d}", i); button(s, names[i], INK_MARGIN + i * (w + INK_GAP_S), y, w, "games.snake.turn", args); }
    w = (INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP_S) / 3; y = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    S.start = button(s, TR("开始"), INK_MARGIN, y, w, "games.snake.start", NULL);
    button(s, TR("暂停"), INK_MARGIN + w + INK_GAP_S, y, w, "games.snake.pause", NULL);
    button(s, TR("继续"), INK_MARGIN + 2 * (w + INK_GAP_S), y, w, "games.snake.resume", NULL);
    S.timer = lv_timer_create(tick, 500, NULL);
    if (S.epoch == ink_member_epoch() && S.db) show();
    else { ink_call_opts_t o = {.src = INK_SRC_TOUCH}; ink_cap_call("games.snake.state", NULL, &o); }
}
static bool key(ink_key_t key, bool long_press) {
    (void)long_press;
    if (key != INK_KEY_PREV && key != INK_KEY_NEXT) return false;
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "direction", (S.s.dir + (key == INK_KEY_PREV ? 3 : 1)) % 4);
    ink_receipt_t rc; ink_cap_invoke("games.snake.turn", &a, INK_SRC_KEY, 0, &rc); return true;
}
static int snapshot(char *out, size_t cap) {
    if (S.epoch != ink_member_epoch()) return snprintf(out, cap, "{}");
    describe();
    const ink_ui_field_spec_t fields[] = {{.id = "board", .role = "text", .label = TR("12×12 棋盘，@ 蛇头，o 蛇身，* 食物"), .value = S.board},
        {.id = "status", .role = "status", .label = TR("当前状态"), .value = S.status}, {.id = "score", .role = "text", .label = TR("得分"), .value = S.score}};
    const ink_ui_action_spec_t actions[] = {{.id = "start", .label = TR("开始 / 重开"), .capability = "games.snake.start", .enabled = true},
        {.id = "state", .label = TR("当前状态"), .capability = "games.snake.state", .enabled = true},
        {.id = "turn", .label = TR("转向"), .capability = "games.snake.turn", .enabled = S.s.length && !S.s.paused && !S.s.over},
        {.id = "pause", .label = TR("暂停"), .capability = "games.snake.pause", .enabled = S.s.length && !S.s.paused && !S.s.over},
        {.id = "resume", .label = TR("继续"), .capability = "games.snake.resume", .enabled = S.s.length && S.s.paused && !S.s.over},
        {.id = "reset", .label = TR("重开"), .capability = "games.snake.reset", .enabled = true}};
    ink_ui_page_spec_t page = {.id = "SnakeGame", .title = TR("贪吃蛇"), .object = {"snake", 1}, .fields = fields, .field_count = 3, .actions = actions, .action_count = 6};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static const ink_activity_t PAGE = {.name = "SnakeGame", .app = "snake", .create = create, .destroy = destroy, .on_key = key, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "snake", .title = TR_NOOP("贪吃蛇"), .icon = INK_UI_ICON(grid), .entry = &PAGE, .child_ok = true, .system_app = true};
static const ink_cap_t CAPS[] = {
    {.id = "games.snake.start", .app = "snake", .title = "开始每半秒一步的贪吃蛇", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.snake.state", .app = "snake", .title = "读取12×12蛇身、食物、分数与当前家人最高分", .effect = INK_FX_READ, .child_ok = true, .run = run},
    {.id = "games.snake.turn", .app = "snake", .title = "贪吃蛇转向，0上1右2下3左，每步至多转一次", .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3}},\"required\":[\"direction\"],\"additionalProperties\":false}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.snake.pause", .app = "snake", .title = "暂停贪吃蛇", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.snake.resume", .app = "snake", .title = "继续贪吃蛇并打开页面", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.snake.reset", .app = "snake", .title = "重开贪吃蛇", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.snake.changed", .app = "snake", .title = "当前家人的蛇身或成绩改变", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true}};
int ink_snake_init(void) { return ink_db_register(&DB) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ? -1 : 0; }
