/* Five AI surfaces, their real service paths and member isolation. */
#include <stdlib.h>
#include "cJSON.h"
#include <errno.h>
#include <sys/stat.h>
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/textinput.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char snapshot[8192], visible[4096], all[4096];
static const char *scene;
static int x, y, page, pages;
static bool found;
static uint32_t send_version;

static void remember_send_version(void) { send_version = ink_ui_content_version(); }
static void stale_send(void) {
    CHECK(send_version != ink_ui_content_version(), "editing advances the question page version");
    ink_receipt_t rc;
    ink_cap_invoke("shell.ai_send", NULL, INK_SRC_AI, send_version, &rc);
    CHECK(rc.status == INK_RC_STALE, "an AI send prepared before editing is rejected");
    ink_cap_invoke("shell.ai_send", NULL, INK_SRC_REMOTE, send_version, &rc);
    CHECK(rc.status == INK_RC_STALE, "a remote send prepared before editing is rejected");
    CHECK(ink_text_focused() && strstr(ink_text_value(), "后来补充"), "stale sends preserve the edited question and keyboard");
}
static void current_send(void) {
    ink_receipt_t rc;
    ink_cap_invoke("shell.ai_send", NULL, INK_SRC_AI, ink_ui_content_version(), &rc);
    CHECK(rc.status == INK_RC_DONE, "an AI send with the current page version succeeds");
}

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool speaking(void) {
    char state[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), state, sizeof state);
    return !strcmp(state, "voice");
}
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "screen settles"); }
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "complete AI snapshot");
}
static lv_obj_t *bound(lv_obj_t *root, const char *id) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return NULL;
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, id)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = bound(lv_obj_get_child(root, (int32_t)i), id);
        if (o) return o;
    }
    return NULL;
}
static const char *button_cap;
static void find_button(void) {
    lv_obj_t *o = bound(lv_layer_sys(), button_cap);
    if (!o) o = bound(lv_screen_active(), button_cap);
    found = o != NULL;
    if (o) {
        lv_obj_update_layout(o);
        lv_area_t a; lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
}
static void press(const char *cap) {
    button_cap = cap; on_ui(find_button);
    CHECK(found, "visible bound control: %s", cap);
    if (found) tap(x, y);
}
static void host_save(void *arg) {
    (void)arg;
    char dir[600], path[650];
    int n = snprintf(dir, sizeof dir, "%s/%s", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof dir) abort();
    if (mkdir(dir, 0700) && errno != EEXIST) abort();
    n = snprintf(path, sizeof path, "%s/snapshot.json", dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void take_snapshot(void *arg) {
    CHECK(settled(), "capture is a settled frame");
    if (!settled()) return;
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", (const char *)arg);
    ink_probe_count(metric);
    sim_display_snapshot((const char *)arg);
}
static void capture(const char *name) {
    quiet(); scene = name; on_ui(read_snapshot);
    vtk_host_call(host_save, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, (void *)name); delay_ms(5);
}
static bool valid_panel;
static void parse_answer(void *arg) {
    (void)arg;
    cJSON *root = cJSON_Parse(snapshot), *f;
    valid_panel = cJSON_IsObject(root); page = pages = 0; visible[0] = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItem(root, "fields")) {
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(f, "id"));
        const char *value = cJSON_GetStringValue(cJSON_GetObjectItem(f, "value"));
        if (!id || !value) { valid_panel = false; continue; }
        if (!strcmp(id, "answer")) snprintf(visible, sizeof visible, "%s", value);
        if (!strcmp(id, "page")) valid_panel = sscanf(value, "页码=%d/%d", &page, &pages) == 2;
    }
    cJSON_Delete(root);
}
static void read_answer(void) {
    read_snapshot(); vtk_host_call(parse_answer, NULL);
    CHECK(valid_panel && pages > 0, "complete F1 panel JSON and page count");
}
static bool has_text(lv_obj_t *root, const char *text) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) if (has_text(lv_obj_get_child(root, (int32_t)i), text)) return true;
    return false;
}
static void check_cleared(void) {
    read_snapshot();
    CHECK(!strstr(snapshot, "合成") && !strstr(snapshot, "PRIVATE_QUESTION"), "new member snapshot contains no old conversation");
    CHECK(!has_text(lv_layer_sys(), "合成") && !has_text(lv_screen_active(), "PRIVATE_QUESTION"), "old conversation and draft disappear visually");
    CHECK(strcmp(ink_ui_top_name(), "Ai"), "old conversation panel closes on member switch");
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800); quiet();
    CHECK(AI("app.open", "{\"id\":\"ai\"}") && g_rc.status == INK_RC_DONE, "open real AI app");
    capture("AiHome"); CHECK(strstr(snapshot, "AiHome") && strstr(snapshot, "还没有对话记录"), "honest history empty state and F1 snapshot");
    press("shell.ai_ask"); capture("AiAsk");
    CHECK(strstr(snapshot, "AiAsk") && ink_text_focused(), "question field and real keyboard are open");
    CHECK(AI("text.insert", "{\"text\":\"PRIVATE_QUESTION：这一页讲了什么？\"}") && g_rc.status == INK_RC_DONE, "AI and keyboard share the input service");
    on_ui(remember_send_version);
    CHECK(AI("text.insert", "{\"text\":\"后来补充的问题\"}") && g_rc.status == INK_RC_DONE, "edit after preparing a send request");
    on_ui(stale_send);
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "submit through the real enter action");
    capture("AiWait");
    CHECK(strstr(snapshot, "正在看这一页"), "typed question waits on the service");
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 1, 20000), "typed answer arrives");
    capture("AiLong"); on_ui(read_answer);
    CHECK(pages > 3 && page == 1, "long answer has enough pages to exercise touch, hardware and AI navigation");
    all[0] = 0; int total = pages;
    for (int i = 1; i <= total && i < 100; i++) {
        CHECK(page == i && visible[0], "page %d has text", i);
        size_t used = strlen(all), len = strlen(visible);
        CHECK(used + len < sizeof all, "fixture fits");
        if (used + len < sizeof all) memcpy(all + used, visible, len + 1);
        if (i == total) break;
        if (i == 1) { press("ai.panel.next"); CHECK(sim_metric_value("action.user.ai.panel.next") == 1, "touch uses next capability"); }
        else if (i == 2) { key(HAL_KEY_NEXT); CHECK(sim_metric_value("action.user.ai.panel.next") == 2, "hardware uses next capability"); }
        else CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "AI uses next capability");
        quiet(); on_ui(read_answer);
    }
    CHECK(strstr(all, "第一段") && strstr(all, "最后一段") && sim_metric_value("action.user.ai.panel.next") + sim_metric_value("action.ai.ai.panel.next") == total - 1, "all pages reached through the bus");
    CHECK(AI("ai.panel.prev", NULL) && g_rc.status == INK_RC_DONE, "previous page works");
    CHECK(AI("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "tap-to-listen uses real capture");
    capture("AiListenTap"); CHECK(strstr(snapshot, "停一下自动结束") && ink_audio_capture_busy(), "tap status and capture agree");
    CHECK(AI("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "second tap ends the utterance");
    CHECK(WAIT_FOR(sim_metric_value("ai.release_to_first_audio_heard_ms") > 0, 20000), "real synthetic TTS is heard");
    capture("AiSpeak"); CHECK(strstr(snapshot, "AiSpeak") && strstr(snapshot, "正在读给你听"), "speech page follows audio service state");
    press("shell.ai_stop");
    CHECK(sim_metric_value("action.user.shell.ai_stop") == 1, "stop button uses capability");
    quiet(); on_ui(read_snapshot); CHECK(!strstr(snapshot, "正在读给你听") && strstr(snapshot, "合成语音回答"), "stop retains the text");
    delay_ms(3000); quiet(); on_ui(read_snapshot);
    CHECK(!strstr(snapshot, "正在读给你听"), "late speech frames do not reopen playback");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0); delay_ms(200);
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(sim_metric_value("action.user.shell.ai_key") + sim_metric_value("action.ai.shell.ai_key") >= 4, 5000), "physical and tap speech share the key capability");
    CHECK(WAIT_FOR(sim_metric_value("ai.presses") >= 2, 5000), "held AI key starts another real turn");
    CHECK(WAIT_FOR(speaking(), 20000), "the held-key turn reaches actual speech playback");
    double answered = sim_metric_value("ai.answers");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(ink_audio_capture_busy() && !speaking(), 5000), "pressing AI while it speaks interrupts playback and starts capture");
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(sim_metric_value("action.user.shell.ai_key") >= 4, 5000), "both held-key turns use the common capability");
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answered, 20000), "the interrupted turn's replacement finishes downloading");
    CHECK(speaking(), "completed answer still has real speech queued");
    CHECK(AI("shell.ai_stop", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same stop capability for queued speech");
    CHECK(WAIT_FOR(!speaking(), 5000), "stop also aborts playback after the server has finished sending");
    quiet(); on_ui(read_snapshot);
    CHECK(ink_member_switch("kid") == 0, "switch member with conversation active");
    quiet(); on_ui(check_cleared);
    CHECK(AI("app.open", "{\"id\":\"ai\"}") && g_rc.status == INK_RC_DONE, "child opens their own AI page");
    quiet(); on_ui(check_cleared);
    CHECK(AI("shell.ai_ask", NULL) && g_rc.status == INK_RC_DONE, "child opens empty question field");
    quiet(); CHECK(!ink_text_value()[0], "question draft does not cross members");
    CHECK(AI("text.insert", "{\"text\":\"合成当前版本问题\"}") && g_rc.status == INK_RC_DONE, "prepare a current-version AI send");
    answered = sim_metric_value("ai.answers"); on_ui(current_send);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answered, 20000), "the accepted current-version send reaches the AI service");
    CHECK(AI("shell.ai_home", NULL) && g_rc.status == INK_RC_DONE, "return from the question page to conversation records");
    quiet(); on_ui(read_snapshot);
    CHECK(!strcmp(ink_ui_page_name(), "AiHome") && !ink_text_focused() && strstr(snapshot, "还没有对话记录"), "records action reaches its entry page rather than resuming the question field");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ai-chat-test", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start scenario driver");
}
