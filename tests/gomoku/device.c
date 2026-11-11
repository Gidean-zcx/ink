/* Acceptance uses the real bus, member store, UI and imported rules. */
#include <stdlib.h>
#include "../../system/apps/gomoku/engine.h"
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], saved_board[226];
static ink_json_t parsed;
static ink_gomoku_t board;
static bool measuring;
static int max_area, measured;
static void observed_commit(const ink_region_t *regions, int n, uint64_t at, const char *tag) {
    if (measuring) for (int i = 0; i < n; i++) {
        int area = regions[i].rect.w * regions[i].rect.h;
        if (area > max_area) max_area = area;
        measured++;
    }
    ink_display_commit_tagged(regions, n, at, tag);
}
#define ink_display_commit_tagged observed_commit
#include "../../system/ui/ui_task.c"
#undef ink_display_commit_tagged
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "page settles"); }
static void ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s: %s %.100s", id, ink_rc_name(g_rc.status), g_rc.say);
    quiet();
}
static int number(const char *key) {
    int64_t value = -1;
    if (!ink_json_parse(&parsed, g_result, strlen(g_result))) ink_json_int(&parsed, ink_json_find(&parsed, 0, key), &value);
    return (int)value;
}
static void state(int moves) { ok("games.state", NULL); CHECK(number("moves") == moves, "move count %d: %.150s", moves, g_result); }
static void move(int row, int col) {
    char args[48]; snprintf(args, sizeof args, "{\"row\":%d,\"col\":%d}", row, col); ok("games.move", args);
}
static void read_board(void) {
    CHECK(!ink_json_parse(&parsed, g_result, strlen(g_result)), "state JSON parses");
    CHECK(ink_json_str(&parsed, ink_json_find(&parsed, 0, "board"), saved_board, sizeof saved_board) > 0, "state exposes board");
}
static void same_page(void) {
    read_board(); on_ui(read_screen);
    CHECK(!strcmp(g_top, "GomokuGame") && strstr(g_snap, saved_board), "AI board equals page snapshot: %.150s", g_snap);
    CHECK(strstr(g_snap, "games.state") && strstr(g_snap, "games.reset"), "page actions exposed to AI");
    CHECK((strstr(g_snap, "games.move") != NULL) == (number("winner") == 0), "only a live game exposes move");
}
static void capture_event(void *arg) { CHECK(settled(), "frame presented before capture"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5); }
static void rules(void) {
    const int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {-1, 1}};
    for (int d = 0; d < 4; d++) {
        board = (ink_gomoku_t){.size = 15};
        for (int i = 0; i < 5; i++) {
            CHECK(!ink_gomoku_play(&board, 1, 7 + i * dirs[d][0], 5 + i * dirs[d][1]), "legal black move direction %d", d);
            if (i < 4) CHECK(!ink_gomoku_play(&board, 1, 0, 2 * i), "legal separated white move");
        }
        CHECK(board.winner == 1, "five in a row wins direction %d", d);
        CHECK(ink_gomoku_play(&board, 1, 14, 14) < 0, "cannot continue after win");
        CHECK(!ink_gomoku_play(&board, 2, 0, 0) && !board.winner && board.count == 8, "undo winning move");
        CHECK(ink_gomoku_play(&board, 1, 0, 0) < 0 && board.count == 8, "occupied intersection refused");
    }
    for (int level = 0; level < 3; level++) {
        board = (ink_gomoku_t){.size = 15, .mode = 1, .level = (uint8_t)level};
        for (int turn = 0; turn < 4; turn++) {
            int human = turn;
            while (board.cells[human]) human++;
            CHECK(!ink_gomoku_play(&board, 1, human / 15, human % 15), "human legal");
            int chosen = ink_gomoku_choose(&board);
            CHECK(chosen >= 0 && chosen < 225 && !board.cells[chosen], "level %d chooses a legal move", level);
            if (chosen >= 0 && chosen < 225) CHECK(!ink_gomoku_play(&board, 1, chosen / 15, chosen % 15), "AI move accepted");
        }
        CHECK(!ink_gomoku_play(&board, 2, 0, 0) && board.count == 6, "human/AI undo removes a pair");
    }
}
static void win_game(void) {
    ok("games.start", "{\"mode\":\"two\",\"size\":15}");
    for (int i = 0; i < 5; i++) { move(7, 5 + i); if (i < 4) move(0, 2 * i); }
    CHECK(number("winner") == 1 && number("legal") == 0 && number("turn") == 0, "winning state and no legal continuations");
    same_page(); on_ui(read_screen); CHECK(strstr(g_snap, "黑棋获胜"), "page declares the same winner"); capture("GomokuWin");
}
static int target_index, seen;
static lv_area_t target;
static bool found;
static void find_cell(void) {
    found = false; seen = 0; lv_obj_update_layout(lv_screen_active());
    for (uint32_t i = 0; i < lv_obj_get_child_count(lv_screen_active()); i++) {
        lv_obj_t *obj = lv_obj_get_child(lv_screen_active(), (int)i);
        const char *cap = ink_shell_bound_cap(obj);
        if (cap && !strcmp(cap, "games.move") && seen++ == target_index) { lv_obj_get_coords(obj, &target); found = true; }
    }
}
static void touch_move(void) {
    target_index = 112; on_ui(find_cell); CHECK(found, "physical intersection exists");
    max_area = measured = 0; measuring = true;
    if (found) tap((target.x1 + target.x2) / 2, (target.y1 + target.y2) / 2);
    quiet(); measuring = false; state(1);
    CHECK(measured > 0 && max_area < INK_CANVAS_W * INK_CANVAS_H / 4, "move refresh is local, max=%d regions=%d", max_area, measured);
    ink_probe_value("test.move_refresh_max_px", max_area);
    CHECK(AI("games.move", "{\"row\":7,\"col\":7}") && g_rc.status == INK_RC_INVALID, "occupied move rejected through bus");
    state(1);
    same_page(); capture("GomokuGame");
}
static volatile bool pin_done;
static int pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void parent(void) {
    pin_done = false;
    CHECK(!ink_member_unlock("owner", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "parent unlocks own space"); quiet();
}
static void approve(void) {
    unsigned token = 0;
    CHECK(g_rc.status == INK_RC_PENDING_CONFIRMATION && sscanf(g_rc.object, "confirm:%u", &token) == 1, "parent policy asks locally");
    pin_done = false;
    CHECK(!ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "parent approves synthetic policy change locally"); quiet();
}
static void isolation(void) {
    state(1); read_board(); char parent_board[226]; snprintf(parent_board, sizeof parent_board, "%s", saved_board);
    CHECK(!ink_member_switch("kid"), "switch to child"); quiet(); state(0);
    ok("games.start", "{\"mode\":\"two\",\"size\":9}"); move(1, 2); state(1);
    CHECK(!ink_member_switch("owner"), "return to parent"); quiet(); state(1); read_board();
    CHECK(!strcmp(saved_board, parent_board) && number("size") == 15, "member save is isolated");
    pin_done = false;
    CHECK(!ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set synthetic parent verifier");
    CHECK(AI("kid.app_set", "{\"id\":\"kid\",\"app\":\"gomoku\",\"mode\":0}"), "request child app policy"); approve();
    CHECK(WAIT_FOR(ink_member_app_mode("kid", "gomoku") == 0, 6000), "child app policy applied");
    CHECK(!ink_member_switch("kid"), "switch to restricted child"); quiet();
    CHECK(AI("games.start", "{\"mode\":\"two\"}") && g_rc.status == INK_RC_REJECTED, "child app list gates game capabilities");
    parent();
    CHECK(AI("kid.app_set", "{\"id\":\"kid\",\"app\":\"gomoku\",\"mode\":1}"), "restore child policy"); approve();
    CHECK(WAIT_FOR(ink_member_app_mode("kid", "gomoku") == 1, 6000), "child app policy restored");
    ink_tm_t time = {.year = 2026, .month = 11, .day = 12, .hour = 12}; ink_time_set(ink_time_from_local(&time), false);
    CHECK(AI("kid.period_set", "{\"id\":\"kid\",\"index\":0,\"start\":0,\"end\":1439,\"days\":127}"), "set synthetic restricted period"); approve();
    CHECK(!ink_member_switch("kid"), "enter child in restricted hours"); quiet();
    CHECK(AI("games.move", "{\"row\":2,\"col\":2}") && g_rc.status == INK_RC_REJECTED, "restricted period gates game capabilities");
    parent(); pin_done = false;
    CHECK(!ink_member_set_pin("owner", "24681357", "", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "restore password-free parent fixture"); quiet();
}
static void flush(void) {
    ink_db_t *db = ink_db_open(ink_db_find_def("gomoku.game"), "owner");
    CHECK(db && !ink_db_wait(db, ink_db_last_ver(db), 10000000), "save durable before process exits"); ink_db_close(db);
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens"); quiet(); test_start_home(); quiet();
    if (!strcmp(phase, "reboot")) { state(9); CHECK(number("winner") == 1, "winner survives process restart"); ok("games.start", "{\"resume\":true}"); same_page(); }
    else {
        if (!strcmp(phase, "big")) ok("settings.set", "{\"id\":\"display.font_size\",\"value\":3}");
        else rules();
        ok("app.open", "{\"id\":\"gomoku\"}"); state(0); capture("GomokuMenu");
        ok("games.start", "{\"mode\":\"two\",\"size\":15}"); touch_move();
        ok("app.close", "{\"id\":\"gomoku\"}"); ok("app.open", "{\"id\":\"gomoku\"}"); ok("games.start", "{\"resume\":true}"); state(1); same_page();
        if (strcmp(phase, "big")) {
            isolation();
            for (int level = 0; level < 3; level++) {
                char args[64]; snprintf(args, sizeof args, "{\"mode\":\"ai\",\"difficulty\":%d,\"size\":15}", level);
                ok("games.start", args); move(7, 7); state(2); CHECK(number("turn") == 1, "human/AI round returns to black"); same_page();
                ok("games.undo", NULL); state(0); ok("games.reset", NULL); state(0);
            }
        }
        win_game(); flush();
    }
    ink_probe_count("test.finished");
}
static void environment(void *arg) { (void)arg; const char *p = getenv("INK_GOMOKU_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "main"); }
void ink_main(void) {
    vtk_host_call(environment, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "gomoku-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
