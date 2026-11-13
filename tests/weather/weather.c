/* Weather on the real pages with the simulated hub's fake forecast: the city in settings, the Today card, the weather
 * page, weather.get for the AI, the last forecast while offline and the empty states. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "../keyboard_matrix.h"
#include "ink/sys/hub.h"
const char *ink_weather_city(void);

static char snapshot[3072];
static bool big_font;
static const char *wanted_text;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real page settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page has an AI snapshot"); }
static void expect(const char *text) { on_ui(read_snapshot); CHECK(strstr(snapshot, text), "snapshot contains %s: %.200s", text, snapshot); }
static void expect_day3(void) { /* the third day's row is named by its weekday, not by the update line after it */
    on_ui(read_snapshot); const char *d = strstr(snapshot, "{\"id\":\"day3\""), *l = d ? strstr(d, ",\"label\":\"") : NULL;
    CHECK(l && !strncmp(l + 10, "周", strlen("周")), "day3 is labelled with its weekday: %.200s", d ? d : snapshot);
}
static void find_in(lv_obj_t *o) {
    if (found || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted_text)) { found = o; return; }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_in(lv_obj_get_child(o, (int32_t)i));
}
static void find_job(void) { found = NULL; lv_obj_update_layout(lv_screen_active()); find_in(lv_screen_active()); }
static lv_obj_t *find(const char *text) { wanted_text = text; on_ui(find_job); return found; }
static void find_job_text(const char *text) { wanted_text = text; find_job(); }
static void shows(const char *text) { CHECK(find(text), "the screen shows %s", text); }
static void weather_layout(void) {
    find_job_text("刷新");
    CHECK(found && ink_ui_surface_kind(lv_obj_get_parent(found)) == INK_SURFACE_WHITE, "UX20 W01 refresh is a secondary action");
    find_job_text("12°"); lv_obj_t *temperature = found;
    find_job_text("数据来自 Open-Meteo.com");
    CHECK(temperature && found && lv_obj_get_style_text_font(temperature, 0)->line_height > lv_obj_get_style_text_font(found, 0)->line_height,
          "UX20 W01 the current temperature is larger than the update note");
    if (found) {
        lv_area_t a; lv_obj_get_coords(found, &a);
        CHECK(a.y2 + ink_ui_px(INK_GAP_L) < ink_ui_px(INK_CANVAS_H - INK_MARGIN - INK_PRIMARY), "UX20 W02 update note stays above the footer");
    }
}
static void tap_obj(lv_obj_t *o) {
    lv_area_t a; lv_obj_get_coords(o, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); quiet();
}
static void capture_event(void *arg) { CHECK(settled(), "the captured frame is presented"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5); }
static void go_home(void) { CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "back home"); quiet(); }
static void today(void) { go_home(); quiet(); } /* D-135: the weather is on the one page's date line */
/* The real keyboard, as people press it (same way as tests/ui_input_host.c). */
static const char *key_label;
static lv_point_t key_point;
static bool key_seen;
static lv_obj_t *label_in(lv_obj_t *o, const char *text) {
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text) && lv_obj_is_visible(o)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) { lv_obj_t *f = label_in(lv_obj_get_child(o, (int32_t)i), text); if (f) return f; }
    return NULL;
}
static void locate_key(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_area_t a;
    key_seen = ink_test_matrix_key(lv_layer_sys(), key_label, &a);
    if (!key_seen) {
        lv_obj_t *label = label_in(lv_layer_sys(), key_label);
        if (!label) return;
        key_seen = true; lv_obj_get_coords(lv_obj_get_parent(label), &a);
    }
    key_point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}
static void press(const char *label) {
    key_label = label; on_ui(locate_key);
    CHECK(key_seen, "the keyboard shows %s", label);
    if (key_seen) tap(key_point.x, key_point.y);
}
/* The Today card's icon is the current weather's: the same picture ink_ui_icon gives for that name. */
static void ai_weather(bool fresh) {
    CHECK(AI("weather.get", NULL) && g_rc.status == INK_RC_DONE, "AI weather.get (%s): %.100s", ink_rc_name(g_rc.status), g_rc.say);
    const char *want[] = {"\"city\":\"示例市\"", "\"t\":12", "\"w\":\"小雨\"", "{\"day\":\"今天\",\"w\":\"小雨\",\"hi\":15,\"lo\":3,\"rain\":80}",
                          "{\"day\":\"明天\",\"w\":\"多云\",\"hi\":16,\"lo\":5,\"rain\":20}", "\"updated\":\"", fresh ? "\"fresh\":true" : "\"fresh\":false"};
    for (unsigned i = 0; i < sizeof want / sizeof *want; i++) CHECK(strstr(g_result, want[i]), "AI gets %s: %.200s", want[i], g_result);
    CHECK(strstr(g_rc.say, "示例市现在12°") && (fresh == !strstr(g_rc.say, "没联网")), "AI can say it: %.150s", g_rc.say);
}

static char phase[16];
static void restart(void) {
    /* A second boot on the same flash, no network: the city, the last forecast and the Today card are still there. */
    CHECK(!strcmp(ink_weather_city(), "示例市"), "the city survives a restart (%s)", ink_weather_city());
    today();
    CHECK(WAIT_FOR((on_ui(read_snapshot), strstr(snapshot, " · 12° 小雨；")), 5000), "the date line keeps the last weather: %.200s", snapshot);
    CHECK(BY_HAND("weather.open", NULL), "open weather"); quiet();
    shows("12°"); shows("明天 · 多云"); shows("没联网，"); expect("更新\\n数据来自"); capture("WeatherRestart");
    ai_weather(false);
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    CHECK(AI("settings.set", big_font ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "choose weather font size"); quiet();
    if (!strcmp(phase, "restart")) { restart(); ink_probe_count("test.finished"); return; }
    /* Never set: the AI and the page say so; there is no Today card yet. */
    CHECK(AI("weather.get", NULL) && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, "还没有设天气城市"), "AI is told no city: %.100s", g_rc.say);
    CHECK(BY_HAND("weather.open", NULL) && !strcmp(ink_ui_page_name(), "Weather"), "the weather page opens"); quiet();
    shows("还没有设天气城市"); expect("还没有设天气城市"); expect("weather.city"); capture("WeatherEmpty");
    CHECK(AI("shell.today_card", "{\"card\":\"weather:now\"}") && g_rc.status == INK_RC_REJECTED, "no weather card for Today before a city is set");
    today(); expect("页面=Main"); CHECK((on_ui(read_snapshot), !strstr(snapshot, "°")), "the home has no weather before a city is set");

    /* 选城市 → type with the keyboard → search → pick by touch, all in the weather app. */
    CHECK(BY_HAND("weather.open", NULL), "open weather"); quiet();
    lv_obj_t *choose = find("选城市");
    CHECK(choose, "the empty page offers 选城市");
    if (choose) tap_obj(choose);
    CHECK(!strcmp(ink_ui_page_name(), "WeatherCity"), "选城市 opens the city page (%s)", ink_ui_page_name());
    shows("现在：还没有设"); expect("weather.focus"); capture("WeatherCityEmpty");
    CHECK(AI("weather.set_city", "{\"id\":101}") && g_rc.status == INK_RC_INVALID, "a city must come from a search first");
    lv_obj_t *field = find("输入城市名");
    CHECK(field, "the city input is on the page");
    if (field) tap_obj(lv_obj_get_parent(field));
    CHECK(WAIT_FOR(ink_text_focused(), 3000), "a tap on the input brings up the keyboard");
    const char *const letters[] = {"b", "e", "i", "j", "i", "n", "g"};
    for (unsigned i = 0; i < 7; i++) press(letters[i]);
    quiet(); capture("WeatherCityTyping");
    press("北京"); quiet();
    CHECK(!strcmp(ink_text_value(), "北京"), "the typed name is in the input (%s)", ink_text_value());
    press("搜索"); /* the keyboard's own search key */
    CHECK(WAIT_FOR(find("另一省 · 中国") != NULL, 5000), "the hub's cities are listed");
    quiet(); expect("weather.set_city"); expect("{\\\"id\\\":101}"); capture("WeatherCity");
    lv_obj_t *pick = find("示例省 · 中国");
    CHECK(pick, "the first city is listed");
    if (pick) tap_obj(lv_obj_get_parent(pick));
    CHECK(WAIT_FOR(strstr(ink_weather_city(), "示例市") != NULL, 5000), "the city is set by touch");
    CHECK(!strcmp(ink_ui_page_name(), "Weather"), "back to the weather page (%s)", ink_ui_page_name());

    /* The page, the Today card (put there by the first city) and the AI read the same fake forecast. */
    CHECK(WAIT_FOR(find("小雨 · 最高 15° 最低 3°") != NULL, 5000), "the page fills in");
    quiet(); shows("12°"); shows("明天 · 多云"); shows("16° / 5°"); shows("今天降水概率"); shows("80%");
    shows("数据来自 Open-Meteo.com"); expect("后天 · 晴"); expect_day3(); expect("更新"); capture("WeatherPage");
    on_ui(weather_layout);
    CHECK(!find("没联网"), "online: no offline note");
    shows("换城市");
    today();
    CHECK(WAIT_FOR((on_ui(read_snapshot), strstr(snapshot, " · 12° 小雨；")), 5000), "the first city puts the weather on the date line: %.200s", snapshot);
    CHECK(!find("12° 小雨 15°/3°"), "no separate weather card: it is on the date line");
    capture("WeatherToday");
    ai_weather(true);

    /* Offline: the page keeps the last forecast and says when it was updated; the AI gets the same, marked not fresh. */
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "go offline");
    CHECK(WAIT_FOR(!ink_hub_online(), 5000), "the hub link is down");
    CHECK(BY_HAND("weather.open", NULL), "open weather"); quiet();
    lv_obj_t *refresh = find("刷新");
    CHECK(refresh, "refresh is on the page");
    if (refresh) tap_obj(refresh);
    CHECK(WAIT_FOR(find("没联网，") != NULL, 10000), "the page says it is offline");
    shows("12°"); shows("明天 · 多云"); expect("没联网，"); expect("更新\\n数据来自"); capture("WeatherOffline");
    shows("显示上次天气"); expect("显示上次天气"); on_ui(weather_layout);
    ai_weather(false);
    today(); expect(" · 12° 小雨；");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "back online");
    int64_t up = 0;
    CHECK(WAIT_FOR(ink_state_get_int(ink_state_find("net.online"), &up) && up, 20000), "the network is back");

    /* The weather is no card (D-135): a later city shows on the same date line. */
    CHECK(AI("shell.today_card", "{\"card\":\"weather:now\"}") && g_rc.status == INK_RC_REJECTED, "the weather is not one of the home's cards");
    CHECK(AI("weather.find", "{\"name\":\"示例\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"id\":102"), "AI finds cities (%s): %.100s %.100s", ink_rc_name(g_rc.status), g_rc.say, g_result);
    CHECK(AI("weather.set_city", "{\"id\":102}") && g_rc.status == INK_RC_DONE, "AI changes the city: %.100s", g_rc.say);
    today(); CHECK(WAIT_FOR((on_ui(read_snapshot), strstr(snapshot, " · 12° 小雨；")), 5000), "the later city's weather is on the date line: %.200s", snapshot);
    delay_ms(70000); /* past the store's batching, so the second boot reads it from flash */
    ink_probe_count("test.finished");
}
static void host_phase(void *arg) {
    (void)arg; const char *p = getenv("INK_WEATHER_PHASE"), *font = getenv("INK_UI_FIX_FONT");
    snprintf(phase, sizeof phase, "%s", p ? p : ""); big_font = font && !strcmp(font, "120");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "weather-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test-only driver");
}
