/* Shipping candidates, real touch hit testing and the AI's visible context. */
#include "ink/ui_shell.h"
#include "ink/sys/textinput.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static const char *query;
static int session, x, y;
static bool expanded;
static lv_obj_t *find_label(lv_obj_t *obj, const char *text) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(obj, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static bool fits(lv_obj_t *obj) {
    if (!obj || !lv_obj_is_visible(obj)) return false;
    lv_area_t a, parent;
    lv_obj_get_coords(obj, &a);
    if (a.x1 < 0 || a.y1 < 0 || a.x2 >= ink_ui_width() || a.y2 >= ink_ui_height()) return false;
    for (lv_obj_t *p = lv_obj_get_parent(obj); p; p = lv_obj_get_parent(p)) {
        lv_obj_get_coords(p, &parent);
        if (a.x1 < parent.x1 || a.x2 > parent.x2 || a.y1 < parent.y1 || a.y2 > parent.y2) return false;
    }
    return true;
}
static void focus(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .hint = "合成输入"};
    session = ink_text_focus(&cfg, NULL, NULL);
    CHECK(session > 0, "focus plain input");
    for (const char *p = query; *p; p++) ink_text_key(*p);
}
static void blur(void) { ink_text_blur(session); }
static void inspect(void) {
    lv_obj_update_layout(lv_layer_sys());
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "keyboard AI context fits");
    CHECK(strstr(g_snap, expanded ? "展开=1" : "展开=0") != NULL, "touch controls the actual expansion state");
    lv_obj_t *label = find_label(lv_layer_sys(), "更多");
    CHECK(label != NULL, "more label exists");
    if (!label) return;
    lv_obj_t *more = lv_obj_get_parent(label), *row = lv_obj_get_parent(more);
    CHECK(fits(more) && fits(label), "more is fully inside the screen and its parents: %s", query);
    CHECK(strstr(g_snap, "更多候选=1") != NULL, "snapshot reports the visible more button");
    lv_area_t a;
    lv_obj_get_coords(more, &a);
    x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    lv_point_t edge = {a.x2 - 1, y};
    CHECK(lv_indev_search_obj(lv_layer_sys(), &edge) == more, "right edge of more is touchable");
    lv_obj_t *mic = lv_obj_get_child(row, -1);
    CHECK(ink_text_voice_possible() && fits(mic), "microphone keeps a fully visible position");
    if (!expanded) {
        lv_area_t mic_area, row_area;
        lv_obj_get_coords(mic, &mic_area); lv_obj_get_coords(row, &row_area);
        CHECK(mic_area.x2 == row_area.x2 && a.x2 + ink_ui_px(INK_GAP_XS) + 1 == mic_area.x1,
              "more and microphone keep their reserved positions at the right edge");
    }
    CHECK(strstr(g_snap, "text.voice_start") != NULL, "snapshot exposes the visible microphone");
    ink_text_cand_t choices[INK_TEXT_PAGE];
    int n = ink_text_candidates(0, choices, INK_TEXT_PAGE), visible = 0;
    for (int i = 0; i < n; i++) {
        lv_obj_t *button = lv_obj_get_child(row, i), *word = lv_obj_get_child(button, 0);
        bool shown = !lv_obj_has_flag(button, LV_OBJ_FLAG_HIDDEN);
        char expected[sizeof choices[i].word + 32]; /* the whole word: a cut one would match a different snapshot */
        snprintf(expected, sizeof expected, "候选[%d]=%s；", i, choices[i].word);
        CHECK((strstr(g_snap, expected) != NULL) == shown, "snapshot lists exactly the displayed candidate %d", i);
        if (!shown) continue;
        visible++;
        CHECK(fits(button) && fits(word), "candidate is fully readable: %.39s", choices[i].word);
    }
    CHECK(visible > 0 && (expanded ? visible == n : visible < n), "overflow candidates remain in the expanded page");
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void frame(const char *name) {
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "keyboard display settles");
    on_ui(inspect);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    delay_ms(20);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    py_want_dict(NULL);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded") == 1, 5000), "shipping dictionary loads");
    for (int i = 0; i < 3; i++) {
        CHECK(AI("settings.set", i == 2 ? "{\"id\":\"display.font_size\",\"value\":3}" :
              "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "set supported text size");
        query = i ? "nh" : "zhrmghg"; expanded = false;
        on_ui(focus);
        frame(i == 0 ? "long-folded" : i == 1 ? "nh-folded" : "nh-large-folded");
        tap(x, y); expanded = true;
        frame(i == 0 ? "long-expanded" : i == 1 ? "nh-expanded" : "nh-large-expanded");
        on_ui(blur);
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ime-layout-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start keyboard boundary test");
}
