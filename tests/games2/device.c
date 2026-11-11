/* Acceptance on the real bus, member database and headless screen. */
#include <stdlib.h>
#include "../../system/apps/memory/engine.h"
#include "../../system/apps/snake/engine.h"
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"
static char phase[16], board[INK_SNAKE_CELLS + 1], old_board[INK_SNAKE_CELLS + 1], deck[INK_MEMORY_CARDS];
static ink_json_t parsed;
static int parent_moves, parent_seconds, parent_score, max_area, measured, a2;
static bool measuring;
static void observed_commit(const ink_region_t *regions, int n, uint64_t at, const char *tag) {
    if (measuring) for (int i = 0; i < n; i++) {
        int area = regions[i].rect.w * regions[i].rect.h;
        if (area > max_area) max_area = area;
        if (regions[i].intent == INK_REFRESH_A2) a2++;
        measured++;
    }
    ink_display_commit_tagged(regions, n, at, tag);
}
#define ink_display_commit_tagged observed_commit
#include "../../system/ui/ui_task.c"
#undef ink_display_commit_tagged
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "page settles"); }
static void ok(const char *id, const char *args) { CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s: %s %.100s", id, ink_rc_name(g_rc.status), g_rc.say); }
static int number(const char *key) {
    int64_t n = -1;
    if (!ink_json_parse(&parsed, g_result, strlen(g_result))) ink_json_int(&parsed, ink_json_find(&parsed, 0, key), &n);
    return (int)n;
}
static bool flag(const char *key) { ink_json_parse(&parsed, g_result, strlen(g_result)); return ink_json_eq(&parsed, ink_json_find(&parsed, 0, key), "true"); }
static void read_board(void) {
    CHECK(!ink_json_parse(&parsed, g_result, strlen(g_result)) && ink_json_str(&parsed, ink_json_find(&parsed, 0, "board"), board, sizeof board) > 0, "visible board JSON");
}
static void state(const char *game) { ok(!strcmp(game, "memory") ? "games.memory.state" : "games.snake.state", NULL); read_board(); }
static void same_page(const char *name) { on_ui(read_screen); CHECK(!strcmp(g_top, name) && strstr(g_snap, board), "capability and page agree: %.150s", g_snap); }
static void capture_event(void *arg) { CHECK(settled(), "capture after presentation"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5); }
static void rules(void) {
    ink_memory_t m; ink_memory_start(&m, 123); int counts[8] = {0};
    for (int i = 0; i < INK_MEMORY_CARDS; i++) counts[m.cards[i]]++;
    for (int i = 0; i < 8; i++) CHECK(counts[i] == 2, "shuffle keeps exactly one pair per icon");
    int other = 1; while (m.cards[other] == m.cards[0]) other++;
    CHECK(!ink_memory_flip(&m, 0) && ink_memory_flip(&m, 0) < 0 && !m.moves, "one card cannot pair with itself");
    CHECK(!ink_memory_flip(&m, other) && m.moves == 1 && m.pairs == 0, "mismatch counts one attempt");
    CHECK(ink_memory_flip(&m, (other + 1) % 16) < 0, "third card refused while mismatch visible");
    ink_memory_hide(&m); CHECK(!m.open[0] && !m.open[other], "mismatch flips back");
    for (int value = 0; value < 8; value++) for (int i = 0; i < 16; i++) if (m.cards[i] == value) CHECK(!ink_memory_flip(&m, i), "pair accepted");
    CHECK(m.pairs == 8 && m.moves == 9, "all pairs complete with counted attempts");
    for (int i = 0; i < 16; i++) CHECK(m.open[i] == 2, "matched cards remain visible");
    ink_snake_t s; ink_snake_start(&s, 1); int head = s.body[0];
    ink_snake_step(&s, 2); CHECK(s.body[0] == head + 1 && s.length == 3, "snake moves one cell");
    s.food = s.body[0] + 1; ink_snake_step(&s, 3); CHECK(s.length == 4 && s.score == 1, "food grows snake and scores");
    CHECK(ink_snake_turn(&s, 3) < 0, "reverse direction refused");
    s.paused = true; head = s.body[0]; ink_snake_step(&s, 4); CHECK(s.body[0] == head, "paused engine stays still");
    s.paused = false; ink_snake_step(&s, 4); CHECK(s.body[0] != head, "resume moves again");
    for (int i = 0; i < 12; i++) ink_snake_step(&s, 5);
    CHECK(s.over, "wall collision ends game");
    s = (ink_snake_t){.body = {65, 66, 54, 53, 52}, .length = 5, .food = 1, .dir = 0, .next = 0};
    ink_snake_step(&s, 1); CHECK(s.over, "self collision ends game");
    s = (ink_snake_t){.body = {65, 66, 54, 53}, .length = 4, .food = 1, .dir = 0, .next = 0};
    ink_snake_step(&s, 1); CHECK(!s.over && s.body[0] == 53, "vacated tail is a legal destination");
}
static void flip(int card) { char args[24]; snprintf(args, sizeof args, "{\"card\":%d}", card); ok("games.memory.flip", args); read_board(); }
static bool hidden(int a, int b) { state("memory"); return board[a] == '?' && board[b] == '?'; }
static lv_area_t target;
static bool found;
static const char *touch_cap;
static void find_touch(void) {
    found = false; lv_obj_update_layout(lv_screen_active());
    for (uint32_t i = 0; i < lv_obj_get_child_count(lv_screen_active()); i++) {
        lv_obj_t *o = lv_obj_get_child(lv_screen_active(), (int)i); const char *cap = ink_shell_bound_cap(o);
        if (!found && cap && !strcmp(cap, touch_cap)) { lv_obj_get_coords(o, &target); found = true; }
    }
}
static void touch(const char *cap) { touch_cap = cap; on_ui(find_touch); CHECK(found, "bound touch control exists"); if (found) tap((target.x1 + target.x2) / 2, (target.y1 + target.y2) / 2); }
static void memory_game(void) {
    ok("app.open", "{\"id\":\"memory\"}"); state("memory"); capture("MemoryStart");
    ok("games.memory.start", NULL); quiet(); state("memory"); CHECK(!strcmp(board, "????????????????"), "AI sees no hidden pattern");
    max_area = measured = 0; measuring = true; touch("games.memory.flip"); quiet(); measuring = false;
    state("memory"); CHECK(board[0] != '?' && number("moves") == 0, "touch and AI use same flip");
    CHECK(measured > 0 && max_area < INK_CANVAS_W * INK_CANVAS_H / 4, "flip refresh is local, max=%d", max_area);
    ink_probe_value("test.memory_refresh_max_px", max_area); capture("MemoryPlaying");
    CHECK(AI("games.memory.flip", "{\"card\":0}") && g_rc.status == INK_RC_INVALID, "same card rejected through bus");
    state("memory"); deck[0] = board[0];
    for (int i = 1; i < 16; i++) {
        flip(i); deck[i] = board[i];
        if (i % 2 && board[i] != board[i - 1]) CHECK(WAIT_FOR(hidden(i - 1, i), 3000), "mismatch turns back on page timer");
    }
    state("memory");
    for (int value = 'A'; value <= 'H'; value++) {
        int first = -1, second = -1;
        for (int i = 0; i < 16; i++) if (deck[i] == value) { if (first < 0) first = i; else second = i; }
        CHECK(first >= 0 && second >= 0, "visible observations find pair %c", value);
        if (first >= 0 && second >= 0 && board[first] == '?') { flip(first); flip(second); }
    }
    state("memory"); CHECK(number("pairs") == 8 && flag("over"), "memory game completed through AI capabilities");
    CHECK(number("moves") >= 8 && number("seconds") >= 1 && number("best_moves") == number("moves"), "best moves and elapsed seconds stored");
    parent_moves = number("best_moves"); parent_seconds = number("best_seconds"); same_page("MemoryGame"); capture("MemoryEnd");
    ok("games.memory.reset", NULL); state("memory"); CHECK(number("moves") == 0 && number("best_moves") == parent_moves, "reset retains best score");
}
static void turn(int dir) { char args[32]; snprintf(args, sizeof args, "{\"direction\":%d}", dir); ok("games.snake.turn", args); }
static bool moved(int head) { state("snake"); return !board[head] || board[head] != '@' || flag("over"); }
static bool ended(void) { state("snake"); return flag("over"); }
static void eat(void) {
    state("snake");
    for (int step = 0; step < 48 && number("score") < 1 && !flag("over"); step++) {
        int head = (int)(strchr(board, '@') - board), food = (int)(strchr(board, '*') - board), dir = number("direction");
        const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0}; int best = -1, distance = 1000;
        for (int d = 0; d < 4; d++) {
            int x = head % 12 + dx[d], y = head / 12 + dy[d], cell = y * 12 + x;
            if ((d + 2) % 4 == dir || x < 0 || x >= 12 || y < 0 || y >= 12 || board[cell] == 'o') continue;
            int cost = abs(x - food % 12) + abs(y - food / 12);
            if (cost < distance) { distance = cost; best = d; }
        }
        CHECK(best >= 0, "a safe direction exists"); if (best < 0) break;
        turn(best); CHECK(WAIT_FOR(moved(head), 1500), "page timer advances snake");
    }
    CHECK(number("score") >= 1 && number("length") >= 4 && number("best") >= 1, "AI-guided eating updates length and member high score");
}
static void snake_game(void) {
    ok("app.open", "{\"id\":\"snake\"}"); state("snake"); capture("SnakeStart");
    ok("games.snake.start", NULL); state("snake"); int head = (int)(strchr(board, '@') - board);
    max_area = measured = a2 = 0; measuring = true;
    CHECK(WAIT_FOR(moved(head), 1500), "snake advances without input"); ok("games.snake.pause", NULL); quiet(); measuring = false;
    CHECK(measured > 0 && a2 > 0 && max_area < INK_CANVAS_W * INK_CANVAS_H / 4, "snake uses local fast refresh: max=%d a2=%d", max_area, a2);
    ink_probe_value("test.snake_refresh_max_px", max_area); state("snake"); snprintf(old_board, sizeof old_board, "%s", board);
    delay_ms(1500); state("snake"); CHECK(flag("paused") && !strcmp(board, old_board), "pause preserves snake for multiple ticks");
    ok("games.snake.resume", NULL); state("snake"); int dir = number("direction"); key(HAL_KEY_PREV); state("snake");
    CHECK(number("next") == (dir + 3) % 4 || number("direction") == (dir + 3) % 4, "previous key turns counterclockwise through capability");
    CHECK(WAIT_FOR(moved(head + 1), 1500), "key turn reaches next tick");
    ok("games.snake.reset", NULL); touch("games.snake.turn"); state("snake");
    CHECK(number("next") == 0 || number("direction") == 0, "screen direction control shares AI action");
    eat(); ok("games.snake.pause", NULL); state("snake"); parent_score = number("best"); same_page("SnakeGame"); capture("SnakePlaying");
    ok("games.snake.resume", NULL); CHECK(WAIT_FOR(ended(), 10000), "wall ends running game"); same_page("SnakeGame"); capture("SnakeEnd");
    ok("games.snake.reset", NULL); ok("system.home", NULL); state("snake"); snprintf(old_board, sizeof old_board, "%s", board);
    delay_ms(1500); state("snake"); CHECK(flag("paused") && !strcmp(board, old_board), "leaving page stops snake");
}
static volatile bool pin_done;
static int pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void approve(void) {
    unsigned token = 0; CHECK(g_rc.status == INK_RC_PENDING_CONFIRMATION && sscanf(g_rc.object, "confirm:%u", &token) == 1, "parent policy confirmation");
    pin_done = false; CHECK(!ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "local synthetic parent approval"); quiet();
}
static void parent(void) { pin_done = false; CHECK(!ink_member_unlock("owner", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "parent unlock"); quiet(); }
static void child_snapshot(void) { CHECK(!ink_app_open("memory", NULL, NULL), "child opens memory"); read_screen(); CHECK(!strstr(g_snap, "best_moves") && !strstr(g_snap, "已通关"), "new member first snapshot has no adult result"); }
static void isolation(void) {
    CHECK(!ink_member_switch("kid"), "switch to second family member"); quiet(); on_ui(child_snapshot);
    state("memory"); CHECK(number("best_moves") == 0 && number("pairs") == 0, "memory best scores do not cross members");
    state("snake"); CHECK(number("best") == 0 && number("score") == 0, "snake high scores do not cross members");
    CHECK(!ink_member_switch("owner"), "return to first member"); quiet();
    state("memory"); CHECK(number("best_moves") == parent_moves && number("best_seconds") == parent_seconds, "memory best restored for first member");
    state("snake"); CHECK(number("best") == parent_score, "snake high score restored for first member");
    pin_done = false; CHECK(!ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set synthetic parent verifier");
    ok("games.snake.start", NULL); ok("games.snake.pause", NULL);
    const char *const games[] = {"memory", "snake"};
    for (int i = 0; i < 2; i++) {
        char args[80]; snprintf(args, sizeof args, "{\"id\":\"kid\",\"app\":\"%s\",\"mode\":0}", games[i]);
        CHECK(AI("kid.app_set", args), "set child app list"); approve();
    }
    CHECK(!ink_member_switch("kid"), "enter child restricted app list"); quiet();
    CHECK(AI("games.memory.start", NULL) && g_rc.status == INK_RC_REJECTED, "child app list gates memory");
    CHECK(AI("games.snake.start", NULL) && g_rc.status == INK_RC_REJECTED, "child app list gates snake"); parent();
    for (int i = 0; i < 2; i++) {
        char args[80]; snprintf(args, sizeof args, "{\"id\":\"kid\",\"app\":\"%s\",\"mode\":1}", games[i]);
        CHECK(AI("kid.app_set", args), "restore child app list"); approve();
    }
    ink_tm_t time = {.year = 2026, .month = 11, .day = 12, .hour = 12}; ink_time_set(ink_time_from_local(&time), false);
    CHECK(AI("kid.period_set", "{\"id\":\"kid\",\"index\":0,\"start\":0,\"end\":1439,\"days\":127}"), "set child restricted period"); approve();
    CHECK(!ink_member_switch("kid"), "enter child restricted time"); quiet();
    CHECK(AI("games.memory.start", NULL) && g_rc.status == INK_RC_REJECTED, "period gates memory");
    CHECK(AI("games.snake.start", NULL) && g_rc.status == INK_RC_REJECTED, "period gates snake"); parent();
    pin_done = false; CHECK(!ink_member_set_pin("owner", "24681357", "", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "restore parent fixture");
}
static void durable(void) {
    const char *const names[] = {"memory.best", "snake.best"};
    for (int i = 0; i < 2; i++) { ink_db_t *db = ink_db_open(ink_db_find_def(names[i]), "owner"); CHECK(db && !ink_db_wait(db, ink_db_last_ver(db), 10000000), "best score durable"); ink_db_close(db); }
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens"); quiet(); test_start_home(); quiet();
    if (!strcmp(phase, "reboot")) {
        state("memory"); CHECK(number("best_moves") >= 8 && number("best_seconds") >= 1, "memory best survives process restart");
        state("snake"); CHECK(number("best") >= 1, "snake high score survives process restart");
    } else {
        if (!strcmp(phase, "big")) ok("settings.set", "{\"id\":\"display.font_size\",\"value\":3}");
        else rules();
        memory_game(); snake_game(); if (!strcmp(phase, "main")) isolation(); durable();
    }
    ink_probe_count("test.finished");
}
static void environment(void *arg) { (void)arg; const char *p = getenv("INK_GAMES2_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "main"); }
void ink_main(void) {
    vtk_host_call(environment, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "games2-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start games acceptance driver");
}
