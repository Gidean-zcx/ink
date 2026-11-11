/* Member scores and LVGL presentation of the imported matching rules. */
#include "engine.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>
typedef struct { uint32_t moves, seconds; } best_t;
static const ink_db_def_t DB = {.name = "memory.best", .title = "翻牌最好成绩", .scope = INK_DB_MEMBER,
    .schema = 1, .max_records = 1, .max_bytes = sizeof(best_t)};
static const char *const ICONS[] = {INK_UI_ICON(book), INK_UI_ICON(sun), INK_UI_ICON(moon), INK_UI_ICON(cloud), INK_UI_ICON(clock), INK_UI_ICON(camera), INK_UI_ICON(phone), INK_UI_ICON(bell)};
_Static_assert(sizeof ICONS / sizeof *ICONS == INK_MEMORY_CARDS / 2, "one icon per pair");
static struct {
    ink_memory_t s;
    best_t best;
    ink_db_t *db;
    ink_call_t *call;
    ink_exec_job_t job;
    uint32_t epoch, loading;
    uint64_t since, elapsed, hide_at;
    bool started;
    char member[16], board[INK_MEMORY_CARDS + 1], status[128], record[96], json[384];
    uint8_t drawn[INK_MEMORY_CARDS];
    lv_obj_t *cells[INK_MEMORY_CARDS], *icons[INK_MEMORY_CARDS], *label, *score, *note;
    lv_timer_t *timer;
} M INK_PSRAM_BSS;
static void run(ink_call_t *c);
static unsigned seconds(void) { return (unsigned)((M.elapsed + (M.since ? osal_now_us() - M.since : 0)) / 1000000); }
static void describe(void) {
    for (int i = 0; i < INK_MEMORY_CARDS; i++) M.board[i] = M.started && M.s.open[i] ? (char)('A' + M.s.cards[i]) : '?';
    M.board[INK_MEMORY_CARDS] = 0;
    snprintf(M.status, sizeof M.status, TR("%d 步 · %d/8 对 · %u 秒"), M.s.moves, M.s.pairs, seconds());
    snprintf(M.record, sizeof M.record, TR("最好：%u 步 · %u 秒"), (unsigned)M.best.moves, (unsigned)M.best.seconds);
    snprintf(M.json, sizeof M.json, "{\"board\":\"%s\",\"moves\":%d,\"pairs\":%d,\"seconds\":%u,\"best_moves\":%u,\"best_seconds\":%u,\"started\":%s,\"over\":%s}",
        M.board, M.s.moves, M.s.pairs, seconds(), (unsigned)M.best.moves, (unsigned)M.best.seconds, M.started ? "true" : "false", M.s.pairs == 8 ? "true" : "false");
}
static void show(void) {
    describe();
    if (M.timer) { if (!M.started || M.s.pairs == 8) lv_timer_pause(M.timer); else lv_timer_resume(M.timer); }
    if (M.label && strcmp(lv_label_get_text(M.label), M.status)) lv_label_set_text(M.label, M.status);
    if (M.score && strcmp(lv_label_get_text(M.score), M.record)) lv_label_set_text(M.score, M.record);
    const char *note = M.s.pairs == 8 ? TR("已通关") : TR("配成八对就通关");
    if (M.note && strcmp(lv_label_get_text(M.note), note)) lv_label_set_text(M.note, note);
    for (int i = 0; i < INK_MEMORY_CARDS; i++) if (M.cells[i] && M.drawn[i] != M.board[i]) {
        M.drawn[i] = (uint8_t)M.board[i];
        if (M.icons[i]) lv_obj_delete(M.icons[i]);
        M.icons[i] = NULL;
        if (M.board[i] != '?') {
            int side = (INK_CANVAS_W - 2 * INK_MARGIN + INK_GAP) / 4 - INK_GAP;
            M.icons[i] = ink_ui_icon(M.cells[i], ICONS[M.board[i] - 'A'], INK_ICON_LIST, INK_ICON_LINE, (side - INK_ICON_LIST) / 2, (side - INK_ICON_LIST) / 2, false);
        }
        ink_ui_hint_obj(M.cells[i], INK_REFRESH_FAST);
    }
}
static void changed(void) {
    show();
    if (M.label) ink_ui_content_changed();
    ink_cap_emit_for("games.memory.changed", NULL, M.member);
}
static void fail(ink_call_t *c, ink_rc_t rc) { c->rc.status = rc; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("暂时不能翻牌，请重试")); }
static void loaded(void *arg) {
    (void)arg; ink_call_t *c = M.call; M.call = NULL;
    if (M.loading != ink_member_epoch() || !ink_member_space_open()) { ink_db_close(M.db); M.db = NULL; fail(c, INK_RC_CANCELLED); }
    else if (!M.db) fail(c, INK_RC_FAILED);
    else {
        ink_rec_t rec; M.best = (best_t){0};
        if (ink_db_get(M.db, 1, &rec, &M.best, sizeof M.best) || rec.len != sizeof M.best) M.best = (best_t){0};
        M.s = (ink_memory_t){.first = -1, .second = -1}; M.started = false; M.since = M.elapsed = M.hide_at = 0;
        M.epoch = M.loading; show(); run(c);
    }
    ink_cap_done(c);
}
static void load(void *arg) { (void)arg; M.db = ink_db_open_member(&DB, M.member); ink_exec_post(INK_EXEC_UI, &M.job, loaded, NULL); }
static void run(ink_call_t *c) {
    if (M.call) { fail(c, INK_RC_FAILED); return; }
    if (!M.db || M.epoch != ink_member_epoch()) {
        if (ink_exec_ready(INK_EXEC_BG)) { fail(c, INK_RC_FAILED); return; }
        ink_db_close(M.db); M.db = NULL; M.loading = ink_member_epoch();
        snprintf(M.member, sizeof M.member, "%s", ink_cap_call_member(c));
        M.call = c; ink_cap_defer(c); ink_exec_post(INK_EXEC_BG, &M.job, load, NULL); return;
    }
    if (!strcmp(c->cap->id, "games.memory.state")) { describe(); ink_cap_result(c, M.json); return; }
    if (strcmp(c->cap->id, "games.memory.flip")) {
        uint32_t seed; hal_random_fill(&seed, sizeof seed); ink_memory_start(&M.s, seed);
        M.started = true; M.elapsed = M.hide_at = 0; M.since = osal_now_us(); ink_app_open("memory", NULL, NULL);
    } else {
        int64_t card = -1; ink_args_get_int(&c->args, "card", &card);
        if (!M.started || ink_memory_flip(&M.s, (int)card)) { fail(c, INK_RC_INVALID); return; }
        if (M.s.second >= 0) M.hide_at = osal_now_us() + 1200000;
        if (M.s.pairs == 8) {
            M.elapsed += M.since ? osal_now_us() - M.since : 0; M.since = 0;
            best_t best = {(uint32_t)M.s.moves, seconds()};
            if (!M.best.moves || best.moves < M.best.moves || (best.moves == M.best.moves && best.seconds < M.best.seconds)) {
                ink_db_write_t w = {.id = 1, .data = &best, .len = sizeof best, .src = c->src};
                if (ink_db_put(M.db, &w, NULL)) fail(c, INK_RC_FAILED);
                else M.best = best;
            }
        }
    }
    changed(); describe(); ink_cap_result(c, M.json);
}
static void tick(lv_timer_t *t) {
    (void)t;
    if (M.epoch != ink_member_epoch() || !ink_member_space_open() || !ink_member_app_time_allows("memory")) return;
    if (M.hide_at && osal_now_us() >= M.hide_at) { ink_memory_hide(&M.s); M.hide_at = 0; changed(); }
    else show();
}
static void destroy(void) {
    if (M.timer) lv_timer_delete(M.timer);
    M.timer = NULL; M.elapsed += M.since ? osal_now_us() - M.since : 0; M.since = 0;
    memset(M.cells, 0, sizeof M.cells); memset(M.icons, 0, sizeof M.icons); M.label = M.score = M.note = NULL;
}
static void create(lv_obj_t *s, void *arg) {
    (void)arg; ink_shell_statusbar(s, true); ink_shell_topbar(s, TR("记忆翻牌"));
    M.label = ink_ui_line(s, "", INK_FONT_BODY, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
    M.score = ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_LINE_BODY, INK_CANVAS_W - 2 * INK_MARGIN);
    int pitch = (INK_CANVAS_W - 2 * INK_MARGIN + INK_GAP) / 4, side = pitch - INK_GAP, top = INK_STATUS_BAR + INK_PRIMARY + INK_BLOCK_L;
    for (int i = 0; i < INK_MEMORY_CARDS; i++) {
        char args[24]; snprintf(args, sizeof args, "{\"card\":%d}", i);
        M.cells[i] = ink_ui_box(s, INK_MARGIN + i % 4 * pitch, top + i / 4 * pitch, side, side, INK_RADIUS_KEY, INK_SURFACE_WHITE, true);
        ink_shell_bind(M.cells[i], "games.memory.flip", args); M.drawn[i] = 0;
    }
    M.note = ink_ui_label(s, TR("配成八对就通关"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, top + 4 * pitch + INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN);
    ink_shell_bind(ink_ui_pill(s, TR("开始 / 重开"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL), "games.memory.start", NULL);
    M.timer = lv_timer_create(tick, 200, NULL);
    if (M.epoch == ink_member_epoch() && M.db) { if (M.started && M.s.pairs < 8) M.since = osal_now_us(); show(); }
    else { ink_call_opts_t o = {.src = INK_SRC_TOUCH}; ink_cap_call("games.memory.state", NULL, &o); }
}
static int snapshot(char *out, size_t cap) {
    if (M.epoch != ink_member_epoch()) return snprintf(out, cap, "{}");
    describe();
    const ink_ui_field_spec_t fields[] = {{.id = "board", .role = "text", .label = TR("卡牌，? 未翻开，A 到 H 为配对图案"), .value = M.board},
        {.id = "score", .role = "status", .label = M.s.pairs == 8 ? TR("已通关") : TR("步数和用时"), .value = M.status}, {.id = "best", .role = "text", .label = TR("最好成绩"), .value = M.record}};
    const ink_ui_action_spec_t actions[] = {{.id = "start", .label = TR("开始 / 重开"), .capability = "games.memory.start", .enabled = true},
        {.id = "state", .label = TR("当前状态"), .capability = "games.memory.state", .enabled = true},
        {.id = "flip", .label = TR("翻牌"), .capability = "games.memory.flip", .enabled = M.started && M.s.pairs < 8 && M.s.second < 0},
        {.id = "reset", .label = TR("重开"), .capability = "games.memory.reset", .enabled = true}};
    ink_ui_page_spec_t page = {.id = "MemoryGame", .title = TR("记忆翻牌"), .object = {"memory", 1}, .fields = fields, .field_count = 3, .actions = actions, .action_count = 4};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static const ink_activity_t PAGE = {.name = "MemoryGame", .app = "memory", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "memory", .title = TR_NOOP("记忆翻牌"), .icon = INK_UI_ICON(grid), .entry = &PAGE, .child_ok = true, .system_app = true};
static const ink_cap_t CAPS[] = {
    {.id = "games.memory.start", .app = "memory", .title = "开始4×4记忆翻牌，隐藏牌面不暴露给AI", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.memory.state", .app = "memory", .title = "读取可见牌面、步数、秒数与当前家人最好成绩", .effect = INK_FX_READ, .child_ok = true, .run = run},
    {.id = "games.memory.flip", .app = "memory", .title = "翻开一张牌，编号从0到15，按行排列", .input = "{\"type\":\"object\",\"properties\":{\"card\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":15}},\"required\":[\"card\"],\"additionalProperties\":false}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.memory.reset", .app = "memory", .title = "重新洗牌开始记忆翻牌", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = run},
    {.id = "games.memory.changed", .app = "memory", .title = "当前家人的可见牌面或成绩改变", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true}};
int ink_memory_init(void) { return ink_db_register(&DB) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ? -1 : 0; }
