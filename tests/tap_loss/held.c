/* tap-loss-1: changes the person did not make, while a finger is down, do not lose the tap.
 *
 * A real finger (the touch controller model, input service, LVGL) presses and holds; while it is down a reply, a
 * timer or a watched state changes the page: a page redraw, the home cards (app.cards_rev), the call banner's
 * refresh, an overlay that comes and goes, content changing elsewhere (ink_ui_content_changed), a suggestion for the
 * keyboard (text_presenter_kick). Each tap still acts exactly once, nothing it pressed is deleted under it
 * (ui.press_lost_to_redraw), and the held-back change is drawn once the finger lifts. A candidate or the space acts on
 * the word the finger came down on although the list changed meanwhile (tests/tap_loss/held_text.c), and a key does not
 * go into another input focused during the press. INK_TAP_HELD_REVERSE=redraw
 * runs the same with the rule switched off by the test (expected to fail). */
#include <stdlib.h>
#include "ink/sys/hub.h"
#include "ink/sys/input.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"
#define key ink_test_keyboard_key
#include "../../system/ui/shell/keyboard.c"
#undef key
#include "../../system/ui/shell/shell_internal.h"

static bool settled(void) { return !ink_input_touch_held() && !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the panel settles"); }

/* --------------------------------------------------------------- a page of our own: one button */
static int g_clicks, g_creates;
static int g_bx, g_by;
static void clicked(lv_event_t *e) { (void)e; g_clicks++; }
static void tp_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    g_creates++;
    lv_obj_t *b = lv_button_create(screen);
    lv_obj_set_size(b, ink_ui_px(200), ink_ui_px(120));
    lv_obj_center(b);
    lv_obj_add_event_cb(b, clicked, LV_EVENT_CLICKED, NULL);
}
static const ink_activity_t TAP_PAGE = {.name = "TapHeld", .app = "tap-held", .create = tp_create};
static void tp_open(void) { ink_ui_push(&TAP_PAGE, NULL); }
static void tp_where(void) {
    lv_obj_t *b = lv_obj_get_child(lv_screen_active(), 0);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a; lv_obj_get_coords(b, &a);
    g_bx = (a.x1 + a.x2) / 2; g_by = (a.y1 + a.y2) / 2;
}

static bool g_pressing;
static void read_pressing(void) { g_pressing = ink_ui_pressing(); }
static bool finger_down_on_ui(void) { on_ui(read_pressing); return g_pressing; }
static void press(int x, int y) {
    hw(HW_PRESS, x, y);
    CHECK(WAIT_FOR(finger_down_on_ui(), 1000), "the finger is down on the page");
}
static void release(int x, int y) { hw(HW_RELEASE, x, y); quiet(); }

static void do_redraw(void) { ink_ui_redraw(); }
static const ink_ui_overlay_t TEST_OVERLAY = {.name = "test-overlay", .level = 1};
static lv_obj_t *g_overlay;
static void overlay_up(void) {
    g_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_overlay, ink_ui_px(100), ink_ui_px(60));
    ink_ui_overlay_add(&TEST_OVERLAY, g_overlay);
}
static void overlay_down(void) { ink_ui_overlay_remove(&TEST_OVERLAY); lv_obj_delete(g_overlay); g_overlay = NULL; }

static void own_page(void) {
    on_ui(tp_open); quiet(); on_ui(tp_where);
    double lost = sim_metric_value("ui.press_lost_to_redraw");
    /* 1. the page redraws (a status reply) while the finger is down */
    int clicks = g_clicks, creates = g_creates;
    press(g_bx, g_by);
    on_ui(do_redraw);
    CHECK(g_creates == creates, "the redraw waits while the finger is down (%d builds)", g_creates - creates);
    release(g_bx, g_by);
    CHECK(g_clicks == clicks + 1, "the tap acts once although the page redrew during it (%d)", g_clicks - clicks);
    CHECK(g_creates == creates + 1, "and the page is rebuilt once after it (%d)", g_creates - creates);
    CHECK(sim_metric_value("ui.press_lost_to_redraw") == lost, "nothing under the finger was deleted");
    /* 2. an overlay comes and goes during the press, after a redraw was asked for: the tap is dropped as for any
     * overlay (what was aimed at the old content), the redraw still comes */
    on_ui(tp_where);
    clicks = g_clicks; creates = g_creates;
    press(g_bx, g_by);
    on_ui(do_redraw); on_ui(overlay_up); on_ui(overlay_down);
    release(g_bx, g_by);
    CHECK(g_clicks == clicks, "a tap under an overlay that came and went does not act (%d)", g_clicks - clicks);
    CHECK(g_creates == creates + 1, "the redraw asked for during the press still comes once (%d)", g_creates - creates);
    /* 3. a finger left on the screen holds a redraw back about 2 s, not for ever */
    on_ui(tp_where);
    creates = g_creates;
    double forced = sim_metric_value("ui.passive_forced");
    press(g_bx, g_by);
    on_ui(do_redraw);
    delay_ms(1500);
    CHECK(g_creates == creates, "held 1.5 s: the redraw still waits");
    delay_ms(1000);
    CHECK(g_creates == creates + 1 && sim_metric_value("ui.passive_forced") == forced + 1, "held 2.5 s: it is done anyway (%d builds)",
          g_creates - creates);
    CHECK(sim_metric_value("ui.press_lost_to_redraw") == lost + 1, "and the probe counts the press it deleted");
    release(g_bx, g_by);
    CHECK(BY_HAND("system.home", NULL), "back home");
    quiet();
}

/* --------------------------------------------------------------- home: an app icon */
static int g_ix, g_iy;
static char g_icon_cap[48];
static void find_icon(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !g_icon_cap[0]; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        const char *cap = ink_shell_bound_cap(c);
        if (cap && !strcmp(cap, "shell.launch") && lv_obj_is_visible(c)) {
            lv_area_t a; lv_obj_get_coords(c, &a);
            g_ix = (a.x1 + a.x2) / 2; g_iy = (a.y1 + a.y2) / 2;
            snprintf(g_icon_cap, sizeof g_icon_cap, "%s", cap);
            return;
        }
        find_icon(c);
    }
}
static void find_home_icon(void) { g_icon_cap[0] = 0; lv_obj_update_layout(lv_screen_active()); find_icon(lv_screen_active()); }
static void bump_cards(void) {
    int h = ink_state_find("app.cards_rev");
    int64_t v = 0; ink_state_get_int(h, &v);
    ink_state_set_int(h, v + 1);
}
static void banner_refresh(void) { ink_shell_activity_refresh(); }
static void elsewhere_changed(void) { ink_ui_content_changed(); }
static char g_top[32];
static void read_top(void) { snprintf(g_top, sizeof g_top, "%s", ink_ui_top_name()); }

static void home_case(const char *what, void (*during)(void)) {
    CHECK(BY_HAND("system.home", NULL), "home for %s", what);
    quiet();
    delay_ms(1200); /* the home cards redraw at most once a second */
    on_ui(find_home_icon);
    CHECK(g_icon_cap[0], "an app icon on home");
    double lost = sim_metric_value("ui.press_lost_to_redraw");
    press(g_ix, g_iy);
    on_ui(during);
    on_ui(during); /* a second time: the same change waits once */
    release(g_ix, g_iy);
    on_ui(read_top);
    CHECK(strcmp(g_top, "home") != 0, "%s during the press: the icon still opens its app (top %s)", what, g_top);
    CHECK(sim_metric_value("ui.press_lost_to_redraw") == lost, "%s: nothing under the finger was deleted", what);
}

/* --------------------------------------------------------------- the keyboard */
static int g_kx, g_ky, g_kcode;
static void key_target(void) {
    int i = -1;
    for (int k = 0; k < K.buttons; k++) if (K.codes[k] == g_kcode) i = k;
    CHECK(i >= 0, "key %d on the keyboard", g_kcode);
    if (i < 0) return;
    lv_obj_t *board = K.footer && i >= 9 ? K.footer : K.board;
    lv_obj_update_layout(K.root);
    lv_area_t bounds; lv_obj_get_coords(board, &bounds);
    lv_area_t a = ((lv_buttonmatrix_t *)board)->button_areas[K.footer && i >= 9 ? i - 9 : i];
    g_kx = bounds.x1 + (a.x1 + a.x2) / 2; g_ky = bounds.y1 + (a.y1 + a.y2) / 2;
}
static void english(void) { if (!ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); }
static void kick(void) { text_presenter_kick(); }
static size_t g_len;
static void read_len(void) { g_len = strlen(ink_text_value()); }

static void keyboard_case(const char *what, void (*during)(void)) {
    on_ui(read_len);
    size_t before = g_len;
    g_kcode = 'a'; on_ui(key_target);
    press(g_kx, g_ky);
    delay_ms(40);
    on_ui(during);
    delay_ms(40);
    release(g_kx, g_ky);
    on_ui(read_len);
    CHECK(g_len == before + 1, "%s during a key press: the key still types (%u -> %u)", what, (unsigned)before, (unsigned)g_len);
}

/* A candidate pressed while the list changes under the finger: the word on the button goes in, once. */
void tap_held_suggest(const char *const *words, int n);
void tap_held_reorder(void);
static const char *const LATER[] = {"今天下雨", "早点睡觉", "晚安"};
static void suggest_later(void) { tap_held_suggest(LATER, 3); }
static int g_count;
static bool g_ai;
static void read_cands(void) {
    ink_text_cand_t w[INK_TEXT_PAGE];
    g_count = ink_text_candidates(0, w, INK_TEXT_PAGE);
    g_ai = g_count > 0 && w[0].ai;
}
static char g_word[40], g_value[256];
static int g_cand;
static void cand_target(void) {
    lv_obj_update_layout(K.root);
    if (g_cand < 0) /* the last one that fits on the row */
        for (int i = 0; i < K.shown && i < INK_TEXT_PAGE; i++) if (lv_obj_is_visible(K.cand_btn[i])) g_cand = i;
    if (g_cand < 0) g_cand = 0;
    lv_obj_t *b = K.cand_btn[g_cand];
    CHECK(lv_obj_is_visible(b), "candidate %d is on the keyboard", g_cand);
    lv_area_t a; lv_obj_get_coords(b, &a);
    g_kx = (a.x1 + a.x2) / 2; g_ky = (a.y1 + a.y2) / 2;
    snprintf(g_word, sizeof g_word, "%s", lv_label_get_text(K.cand_label[g_cand]));
}
static void read_value(void) { snprintf(g_value, sizeof g_value, "%s", ink_text_value()); }
static int occurrences(const char *in, const char *word) {
    int n = 0;
    for (const char *p = strstr(in, word); p; p = strstr(p + strlen(word), word)) n++;
    return n;
}
static void press_during(void (*during)(void)) {
    press(g_kx, g_ky);
    delay_ms(40);
    on_ui(during);
    delay_ms(40);
    release(g_kx, g_ky);
}

static void candidate_case(void) {
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 10000), "the hub connects");
    CHECK(AI("settings.set", "{\"id\":\"text.assist\",\"value\":true}") && g_rc.status == INK_RC_DONE, "continuations on");
    CHECK(WAIT_FOR((on_ui(read_cands), g_count >= 2 && g_ai), 5000), "the hub's continuations are on the keyboard");
    quiet();
    g_cand = -1; on_ui(cand_target);
    on_ui(read_value);
    int had = occurrences(g_value, g_word);
    press_during(suggest_later);
    on_ui(read_value);
    CHECK(occurrences(g_value, g_word) == had + 1, "a new continuation during the press: the word pressed (%s) goes in once (%.80s)", g_word, g_value);
    CHECK(!strstr(g_value, LATER[g_cand]), "and not the one now in its place (%s)", LATER[g_cand]);
    CHECK(AI("settings.set", "{\"id\":\"text.assist\",\"value\":false}") && g_rc.status == INK_RC_DONE, "continuations off");
}

static void chinese(void) { if (ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); ink_text_key('n'); ink_text_key('i'); }
static void space_case(void) {
    on_ui(chinese);
    CHECK(WAIT_FOR((on_ui(read_cands), g_count >= 2), 5000), "pinyin candidates for ni");
    quiet();
    g_cand = 0; on_ui(cand_target);
    on_ui(read_value);
    int had = occurrences(g_value, g_word);
    g_kcode = ' '; on_ui(key_target);
    press_during(tap_held_reorder);
    on_ui(read_value);
    CHECK(occurrences(g_value, g_word) == had + 1, "the list reordered during a space press: the first word shown (%s) goes in (%.80s)", g_word, g_value);
    /* the second candidate pressed while the first two swap: the second as shown, not the first now in its place */
    on_ui(chinese);
    CHECK(WAIT_FOR((on_ui(read_cands), g_count >= 2), 5000), "pinyin candidates for ni again");
    quiet();
    g_cand = 1; on_ui(cand_target);
    on_ui(read_value);
    had = occurrences(g_value, g_word);
    size_t len = strlen(g_value);
    press_during(tap_held_reorder);
    on_ui(read_value);
    CHECK(occurrences(g_value, g_word) == had + 1 && strlen(g_value) == len + strlen(g_word),
          "the list reordered during a candidate press: the second word shown (%s) goes in, alone (%.80s)", g_word, g_value);
}

/* Another input is focused while a key is down: the key was for the one the keyboard showed, not this one. */
static int g_other;
static void focus_other(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN};
    g_other = ink_text_focus(&cfg, NULL, NULL);
}
static void blur_other(void) { ink_text_blur(g_other); }
static void other_case(void) {
    on_ui(english); quiet();
    g_kcode = 'a'; on_ui(key_target);
    press_during(focus_other);
    on_ui(read_value);
    CHECK(g_other > 0 && !g_value[0], "another input focused during a key press: the key does not go into it (%.80s)", g_value);
    on_ui(key_target);
    press(g_kx, g_ky); release(g_kx, g_ky);
    on_ui(read_value);
    CHECK(!strcmp(g_value, "a"), "the next key goes into the input now shown (%.80s)", g_value);
    on_ui(blur_other);
}


static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    quiet();
    own_page();
    home_case("the home cards changing", bump_cards);
    home_case("the call banner's refresh", banner_refresh);
    home_case("content changing elsewhere", elsewhere_changed);
    CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open the memo editor");
    on_ui(english); quiet();
    keyboard_case("content changing elsewhere", elsewhere_changed);
    keyboard_case("a suggestion arriving", kick);
    candidate_case();
    space_case();
    other_case();
    CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "leave the editor");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "tap-held", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
