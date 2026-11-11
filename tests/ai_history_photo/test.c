/* Synthetic member conversations and photos through the real product. */
#include <stdlib.h>
#include "../../system/services/ai/ai_history.c"
#include "../../system/services/ai/ai_service.c"
#include "../../system/apps/gallery/gallery.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "image_fixtures.h"

static char mode[32], snapshot[8192], raw[INK_DB_RECORD_MAX + 1], long_answer[INK_AI_ANSWER_MAX];
static uint32_t first, photo_id;
static bool ready;
static int replay_x, replay_y;
static lv_obj_t *replay_button(lv_obj_t *root) {
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, "ai.panel.replay")) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *b = replay_button(lv_obj_get_child(root, (int)i)); if (b) return b;
    }
    return NULL;
}
static void locate_replay(void) {
    lv_obj_t *b = replay_button(lv_layer_sys()); CHECK(b, "panel contains the bound replay button");
    if (b) { lv_area_t a; lv_obj_update_layout(b); lv_obj_get_coords(b, &a); replay_x = (a.x1 + a.x2) / 2; replay_y = (a.y1 + a.y2) / 2; }
}
static void inspect_offline(void) {
    lv_obj_t *b = replay_button(lv_layer_sys());
    CHECK(b && lv_obj_has_state(b, LV_STATE_DISABLED) && strstr(lv_label_get_text(lv_obj_get_child(b, 0)), "离线"), "visible replay button is disabled and explains offline status");
}
static void get_mode(void *arg) {
    (void)arg; const char *s = getenv("INK_HISTORY_CASE"); snprintf(mode, sizeof mode, "%s", s ? s : "history");
}
static ink_db_t *history_db(const char *member) { return ink_db_open_cached_member(&HISTORY, member); }
static int count(const char *member) { ink_db_t *db = history_db(member); int n = db ? ink_db_count(db) : 0; if (db) ink_db_close(db); return n; }
static uint32_t version(const char *member) { ink_db_t *db = history_db(member); uint32_t n = db ? ink_db_last_ver(db) : 0; if (db) ink_db_close(db); return n; }
static void quiet(void) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 12000), "frame settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "complete page snapshot"); }
static void take_snapshot(void *arg) { sim_display_snapshot((const char *)arg); }
static void offline_event(void *arg) { (void)arg; sim_net_set_link("none"); }
static void capture(const char *name) {
    quiet(); on_ui(read_snapshot); vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, (void *)name); delay_ms(5);
}
static void ask(void) { ink_ai_ask_text("合成私有问题"); }
static void newest(void) {
    ink_db_t *db = history_db("owner"); ink_rec_t rec; ink_db_cursor_t cur = {.desc = true};
    CHECK(db && ink_db_next(db, &cur, &rec, raw, INK_DB_RECORD_MAX) == 1, "completed turn writes a record");
    if (db) { raw[rec.len] = 0; first = rec.id; ink_db_close(db); }
}
static unsigned confirmation(void) { unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token); return token; }
static void records(void) {
    on_ui(ask); CHECK(WAIT_FOR(count("owner") == 1, 20000), "answer is stored automatically"); newest();
    CHECK(strstr(raw, "合成私有问题") && strstr(raw, "合成回答") && strstr(raw, "\"page\":") && strstr(raw, "\"at\":"), "record includes question answer page and time");
    for (int i = 0; i < 51; i++) {
        char q[80]; snprintf(q, sizeof q, "合成问题%02d", i); uint32_t v = version("owner");
        ink_ai_history_save("owner", q, "合成答案", "合成页面");
        CHECK(WAIT_FOR(version("owner") > v, 12000), "history write %d accepted", i);
    }
    CHECK(count("owner") == 50, "retain at most fifty rounds");
    ink_db_t *db = history_db("owner"); ink_rec_t rec;
    CHECK(db && ink_db_get(db, first, &rec, NULL, 0) == INK_DB_ERR_NOT_FOUND, "oldest round was removed"); if (db) ink_db_close(db);
    CHECK(AI("ai.history.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "合成问题50") && !strstr(g_result, "合成问题46"), "first page is newest first, four rounds");
    CHECK(AI("ai.history.list", "{\"offset\":4}") && strstr(g_result, "合成问题46") && !strstr(g_result, "合成问题50"), "second page has no duplicate from first");
    CHECK(AI("shell.ai_home", NULL) && g_rc.status == INK_RC_DONE, "history app opens"); capture("AiHistory");
    CHECK(strstr(snapshot, "pagination") && strstr(snapshot, "合成问题50"), "history has a paginated AI snapshot");
    CHECK(AI("ai.history.next", NULL) && g_rc.status == INK_RC_DONE, "AI turns the visible history page"); on_ui(read_snapshot);
    CHECK(strstr(snapshot, "合成问题46") && !strstr(snapshot, "合成问题50"), "visible second page matches the service");
    newest(); char args[80]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)first);
    CHECK(AI("ai.history.open", args) && g_rc.status == INK_RC_DONE, "AI opens its own round"); capture("AiHistoryAnswer");
    CHECK(strstr(snapshot, "合成答案") && strstr(snapshot, "ai.panel.replay"), "stored answer opens in the actual AI panel");
    CHECK(AI("ai.history.delete", args) && g_rc.status == INK_RC_DONE && count("owner") == 49, "delete one through the common capability");
    CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
    CHECK(AI("ai.history.list", "{\"member\":\"owner\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"total\":0"), "AI cannot list another member by supplying a member argument");
    CHECK(AI("ai.history.open", args) && g_rc.status != INK_RC_DONE, "child cannot open adult round");
    CHECK(AI("ai.history.delete", args) && g_rc.status != INK_RC_DONE && count("owner") == 49, "child cannot delete adult round");
    ink_ai_history_save("kid", "孩子自己的问题", "孩子自己的答案", "主页"); CHECK(WAIT_FOR(count("kid") == 1, 12000), "child has its own history");
    CHECK(AI("ai.history.list", NULL) && strstr(g_result, "孩子自己的答案") && !strstr(g_result, "合成问题"), "child can only list its own round");
    CHECK(BY_HAND("ai.history.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION && count("kid") == 1, "touch cannot clear without confirmation");
    CHECK(ink_confirm_decide(confirmation(), false, INK_SRC_TOUCH) == 0 && count("kid") == 1, "cancel keeps history");
    CHECK(AI("ai.history.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION && count("kid") == 1, "AI clear also asks the human");
    unsigned token = confirmation(); CHECK(ink_confirm_decide(token, true, INK_SRC_AI) != 0, "AI cannot confirm the clear itself");
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(count("kid") == 0, 12000), "human confirmation clears only child history");
    CHECK(count("owner") == 49, "clearing child keeps adult rounds");
    CHECK(ink_member_switch("owner") == 0, "return to adult"); quiet();
    memset(long_answer, 'x', sizeof long_answer - 1); long_answer[sizeof long_answer - 1] = 0;
    for (int i = 0; i < 20; i++) {
        uint32_t v = version("owner"); ink_ai_history_save("owner", "大记录", long_answer, "合成页面");
        CHECK(WAIT_FOR(version("owner") > v, 12000), "large bounded record %d accepted", i);
    }
    db = history_db("owner"); size_t bytes = 0; ink_db_cursor_t cur = {0};
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) bytes += rec.len;
    CHECK(bytes <= 128 * 1024 && count("owner") < 50, "byte limit also removes oldest records"); if (db) ink_db_close(db);
    newest(); CHECK(strstr(raw, "记录太长，只保存了前面部分"), "oversized saved answer explicitly says it was cut");
    CHECK(WAIT_FOR(ink_db_idle(), 20000), "history becomes durable");
}
static void replay(void) {
    on_ui(ask); CHECK(WAIT_FOR(!AI.asking && AI.answer_len, 20000), "online typed answer arrives");
    CHECK(WAIT_FOR(count("owner") == 1, 12000), "original round is stored before replay");
    int n = count("owner"); CHECK(AI("ai.panel.replay", NULL) && g_rc.status == INK_RC_DONE, "AI and person share replay capability");
    CHECK(WAIT_FOR(sim_metric_value("ai.release_to_first_audio_heard_ms") > 0, 20000), "online replay produces actual synthetic audio");
    CHECK(WAIT_FOR(!AI.asking, 20000), "replay finishes"); CHECK(count("owner") == n, "replay does not create a duplicate round");
    CHECK(strstr(AI.answer_text, "合成回答") && strlen(AI.answer_text) == strlen("合成回答。"), "replay leaves answer text unchanged");
    quiet(); on_ui(locate_replay); tap(replay_x, replay_y);
    CHECK(WAIT_FOR(sim_metric_value("action.user.ai.panel.replay") == 1 && !AI.asking, 20000), "physical tap reaches the same replay capability and finishes");
    double requests = sim_metric_value("hub.requests.ai.replay");
    vtk_event_at(vtk_now() + SIM_MS(1), offline_event, NULL); CHECK(WAIT_FOR(!ink_hub_online(), 5000), "network is offline"); capture("AiReplayOffline");
    CHECK(strstr(snapshot, "离线，暂时不能朗读") && !ink_ai_can_replay(), "offline snapshot explains why replay is disabled");
    on_ui(inspect_offline);
    CHECK(AI("ai.panel.replay", NULL) && g_rc.status != INK_RC_DONE && sim_metric_value("hub.requests.ai.replay") == requests, "offline replay sends nothing");
}
static void inspect_ready(void) { ready = G.path[0] && G.box && lv_obj_get_child_count(G.box); }
static void oversized_photo(void) { ink_ai_ask_photo("{\"width\":513,\"height\":1,\"gray4\":\"f\"}"); }
static void stale_photo(void) {
    photo_question_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL); CHECK(p, "allocate prepared photo callback"); if (!p) return;
    *p = (photo_question_t){ink_member_epoch(), ink_ui_generation(), G.photo, ink_fs_revision()};
    if (!strcmp(mode, "photo-member")) p->epoch--;
    else if (!strcmp(mode, "photo-changed")) p->revision--;
    else p->photo++;
    ink_receipt_t rc = {.status = INK_RC_DONE, .result = "{\"width\":1,\"height\":1,\"gray4\":\"f\"}"};
    double requests = sim_metric_value("hub.requests.ai.ask"); photo_question_ready(p, &rc);
    CHECK(sim_metric_value("hub.requests.ai.ask") == requests && !AI.asking, "obsolete member/photo/revision discards attachment before request");
}
static void photo(void) {
    CHECK(ink_fs_mkdir("/me/photos") == 0 && ink_fs_write_atomic("/me/photos/synthetic.png", black_png, sizeof black_png) == 0, "write synthetic photo");
    uint32_t cursor = 0; ink_fs_entry_t e;
    CHECK(ink_fs_index("/me/photos", INK_FS_IMAGE, &cursor, &e) == 1, "index synthetic photo"); photo_id = e.id;
    char args[80]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)photo_id);
    CHECK(AI("photo.open", args) && g_rc.status == INK_RC_DONE, "open authorized photo");
    for (int i = 0; i < 1000; i++) { on_ui(inspect_ready); if (ready) break; delay_ms(20); }
    CHECK(ready, "photo pixels loaded"); quiet();
    if (strcmp(mode, "photo")) { on_ui(stale_photo); return; }
    CHECK(AI("photo.ask", NULL) && g_rc.status == INK_RC_DONE, "ask with current photo");
    CHECK(WAIT_FOR(sim_metric_value("hub.photo.valid") == 1, 20000), "simulated hub validates received gray attachment");
    CHECK(WAIT_FOR(!AI.asking && AI.answer_len, 20000), "photo question answered"); capture("AiPhotoAnswer");
    CHECK(sim_metric_value("hub.photo.invalid") == 0, "photo is within dimension byte and encoding limits");
    on_ui(oversized_photo); CHECK(WAIT_FOR(sim_metric_value("hub.photo.invalid") == 1 && !AI.asking, 12000), "simulated hub rejects an oversized attachment");
    CHECK(ink_member_switch("kid") == 0, "child enters own space"); quiet();
    CHECK(AI("photo.open", args) && g_rc.status != INK_RC_DONE, "child cannot attach adult photo by opening its id");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); test_start_home(); quiet(); vtk_host_call(get_mode, NULL);
    if (!strcmp(mode, "history")) records();
    else if (!strcmp(mode, "restart")) {
        CHECK(AI("ai.history.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "大记录"), "conversation survives process restart from flash");
        CHECK(count("owner") > 0 && count("owner") <= 50, "persisted collection retains its count bound");
    }
    else if (!strcmp(mode, "replay")) replay();
    else photo();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "history-test", .fn = run, .stack_bytes = 32768, .priority = 1, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&cfg, NULL);
}
