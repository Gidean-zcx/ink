/* keyboard-2 checks (docs/tasks/keyboard-2/对照表.md, the kb_* rows): refresh areas, the settle redraw, presses that draw
 * nothing, the nine-key readings and rules, the symbol panel, letters (case, long press, swipe), number pads, held
 * delete, passwords and members, and the review pictures. One case per run (INK_KB_CASE); real touches through the
 * touch driver unless a row is about the AI's call. INK_KB_CASE=reverse-redraw / reverse-press switch a rule off in the
 * test (each must fail). */
#include <stdlib.h>
#include "ink/hal_display.h"
#include "ink/rotation.h"
#include "ink/sys/text.h"
enum { RECTS_MAX = 512 };
static struct { hal_rect_t r; hal_waveform_t wf; } g_rects[RECTS_MAX];
static int g_nrects;
static bool g_tracking;
static uint32_t kb_update(const hal_rect_t *r, const uint8_t *px, int stride, hal_waveform_t wf, uint32_t flags,
                          hal_display_done_fn done, void *ctx) {
    if (g_tracking && g_nrects < RECTS_MAX) { g_rects[g_nrects].r = *r; g_rects[g_nrects++].wf = wf; }
    return hal_display_update(r, px, stride, wf, flags, done, ctx);
}
#define hal_display_update kb_update
#include "../../system/services/display/display_service.c"
#undef hal_display_update
#include "ink/sys/textfield.h"
#include "ink/sys/storage.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"
#define key ink_test_keyboard_key
#include "../../system/ui/shell/keyboard.c"
#undef key
#include "../keyboard_matrix.h"
void tap_held_reorder(void);

static char g_case[32], g_snapshot[8192];
static void read_case(void *arg) { (void)arg; const char *c = getenv("INK_KB_CASE"); snprintf(g_case, sizeof g_case, "%s", c ? c : "refresh"); }
static bool settled(void) { return !ink_input_touch_held() && !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the panel settles"); }

/* ------------------------------------------------------------ a page of our own: one field at the top */
static lv_obj_t *g_field;
static ink_text_type_t g_type = INK_TEXT_PLAIN;
static void field_page(lv_obj_t *scr, void *arg) {
    (void)arg;
    ink_text_cfg_t cfg = {.type = g_type, .hint = "合成输入", .max = 200};
    g_field = ink_textfield_create(scr, &cfg, NULL, NULL, false);
    ink_textfield_style(g_field, INK_FIELD_SINGLE, "合成输入");
    lv_obj_set_pos(g_field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_MARGIN));
}
static const ink_activity_t FIELD_PAGE = {.name = "KeyboardTwo", .app = "keyboard-2", .create = field_page};
static void open_page(void) { ink_ui_push(&FIELD_PAGE, NULL); }
static void focus_field(void) { CHECK(ink_textfield_short_focus(g_field) == 0, "the field takes the keyboard"); }
static void chinese(void) { if (ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); }
static void english(void) { if (!ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); }
static void nine_on(void) { ink_receipt_t rc; ink_setting_set("text.nine_key", 1, INK_SRC_TOUCH, &rc); sync(); }
static void nine_off(void) { ink_text_clear_composing(); ink_receipt_t rc; ink_setting_set("text.nine_key", 0, INK_SRC_TOUCH, &rc); sync(); }
static void field(ink_text_type_t type) {
    g_type = type; on_ui(open_page); quiet(); on_ui(focus_field); quiet();
}
static void leave(void) { ink_text_blur(0); }
static void leave_page(void) { on_ui(leave); CHECK(BY_HAND("system.home", NULL), "home"); quiet(); }

/* ------------------------------------------------------------ real touches */
static int g_x, g_y, g_code;
static bool g_down;
static void read_down(void) {
    g_down = false;
    for (lv_indev_t *d = lv_indev_get_next(NULL); d; d = lv_indev_get_next(d))
        if (lv_indev_get_type(d) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(d) == LV_INDEV_STATE_PRESSED) g_down = true;
}
static bool down_is(bool down) { on_ui(read_down); return g_down == down; }
static void press_at(int x, int y) { hw(HW_PRESS, x, y); CHECK(WAIT_FOR(down_is(true), 1000), "LVGL has the press"); }
static void release_at(int x, int y) { hw(HW_RELEASE, x, y); CHECK(WAIT_FOR(down_is(false), 1000), "LVGL has the release"); }
static void tap_at(int x, int y) { press_at(x, y); delay_ms(40); release_at(x, y); quiet(); }
static void key_center(void) {
    int i = -1;
    for (int k = 0; k < K.buttons; k++) if (K.codes[k] == g_code) i = k;
    CHECK(i >= 0, "key %d is on the keyboard", g_code);
    if (i < 0) return;
    lv_obj_t *board = K.footer && i >= K.footer_offset ? K.footer : K.board;
    lv_obj_update_layout(K.root);
    lv_area_t b; lv_obj_get_coords(board, &b);
    lv_area_t a = ((lv_buttonmatrix_t *)board)->button_areas[K.footer && i >= K.footer_offset ? i - K.footer_offset : i];
    g_x = b.x1 + (a.x1 + a.x2) / 2; g_y = b.y1 + (a.y1 + a.y2) / 2;
}
static void tap_key(int code) { g_code = code; on_ui(key_center); tap_at(g_x, g_y); }
static const char *g_label;
static bool g_found;
static void label_center(void) {
    lv_area_t a; lv_obj_update_layout(lv_layer_sys());
    g_found = ink_test_matrix_key(lv_layer_sys(), g_label, &a);
    if (g_found) { g_x = (a.x1 + a.x2) / 2; g_y = (a.y1 + a.y2) / 2; }
}
static void fast_keys(const int *codes, int n) {
    int xs[8], ys[8];
    for (int i = 0; i < n && i < 8; i++) { g_code = codes[i]; on_ui(key_center); xs[i] = g_x; ys[i] = g_y; }
    for (int i = 0; i < n && i < 8; i++) { hw(HW_PRESS, xs[i], ys[i]); delay_ms(40); hw(HW_RELEASE, xs[i], ys[i]); delay_ms(90); }
}
static void tap_label(const char *label) {
    g_label = label; on_ui(label_center);
    CHECK(g_found, "the keyboard shows %.40s", label);
    if (g_found) tap_at(g_x, g_y);
}
static int g_step;
static void symbol_step(void) {
    lv_area_t a; g_step = ink_test_keyboard_symbol(g_label, &a);
    g_x = (a.x1 + a.x2) / 2; g_y = (a.y1 + a.y2) / 2;
}
static void tap_symbol(const char *label) { /* the way a person finds it: pages, then tabs */
    g_label = label;
    for (int i = 0; i < 40; i++) {
        on_ui(symbol_step);
        if (g_step < 0) break;
        tap_at(g_x, g_y);
        if (g_step == 1) return;
    }
    CHECK(false, "the symbol panel has %.40s", label);
}
static void capture(const char *name);
static void ai(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "AI %.40s %.40s (%d)", cap, args ? args : "", g_rc.status);
    quiet();
}
static void ai_key(int code) { char args[32]; snprintf(args, sizeof args, "{\"code\":%d}", code); ai("keyboard.key", args); }

/* ------------------------------------------------------------ what was drawn */
typedef struct { lv_area_t a[6]; int n; } areas_t;
/* The display merges what one frame changed into one box (refresh_policy.c rule 1, and disjoint regions in one
 * dispatch): an update counts as allowed inside one allowed area, or inside their box without touching the keys. */
static areas_t g_allowed, g_keys;
static void allow(lv_obj_t *o) {
    if (!o || !lv_obj_is_visible(o) || g_allowed.n == 6) return;
    lv_obj_update_layout(lv_layer_sys()); lv_obj_get_coords(o, &g_allowed.a[g_allowed.n++]);
}
static void allow_area(const lv_area_t *a) { if (g_allowed.n < 6) g_allowed.a[g_allowed.n++] = *a; }
static hal_rect_t native(const lv_area_t *a) {
    hal_panel_info_t p; hal_display_get_info(&p); int x0, y0, x1, y1;
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a->x1, a->y1, &x0, &y0);
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a->x2, a->y2, &x1, &y1);
    return (hal_rect_t){x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, abs(x1 - x0) + 1, abs(y1 - y0) + 1};
}
static bool inside(const hal_rect_t *r, const hal_rect_t *o) {
    return r->x >= o->x && r->y >= o->y && r->x + r->w <= o->x + o->w && r->y + r->h <= o->y + o->h;
}
static bool meets(const hal_rect_t *r, const hal_rect_t *o) {
    return r->x < o->x + o->w && o->x < r->x + r->w && r->y < o->y + o->h && o->y < r->y + r->h;
}
/* Every update inside one allowed area; none the whole panel. */
static int outside_allowed(void) {
    int bad = 0;
    for (int i = 0; i < g_nrects; i++) {
        bool ok = false; /* a whole-panel update is inside no allowed area */
        for (int k = 0; k < g_allowed.n && !ok; k++) { hal_rect_t a = native(&g_allowed.a[k]); ok = inside(&g_rects[i].r, &a); }
        if (!ok && g_keys.n) {
            lv_area_t box = g_allowed.a[0];
            for (int k = 1; k < g_allowed.n; k++) { const lv_area_t *o = &g_allowed.a[k]; box = (lv_area_t){LV_MIN(box.x1, o->x1), LV_MIN(box.y1, o->y1), LV_MAX(box.x2, o->x2), LV_MAX(box.y2, o->y2)}; }
            hal_rect_t b = native(&box);
            ok = inside(&g_rects[i].r, &b);
            for (int k = 0; k < g_keys.n && ok; k++) { hal_rect_t a = native(&g_keys.a[k]); if (meets(&g_rects[i].r, &a)) ok = false; }
        }
        if (!ok) {
            bad++;
            if (bad <= 3) fprintf(stderr, "OUTSIDE %d,%d %dx%d wf=%d\n", g_rects[i].r.x, g_rects[i].r.y, g_rects[i].r.w, g_rects[i].r.h, (int)g_rects[i].wf);
        }
    }
    for (int k = 0; bad && k < g_allowed.n; k++) { hal_rect_t a = native(&g_allowed.a[k]); fprintf(stderr, "  ALLOWED %d,%d %dx%d\n", a.x, a.y, a.w, a.h); }
    return bad;
}
static void track(void) { g_nrects = 0; g_tracking = true; }
/* What LVGL was asked to draw again, before the display merges a frame's areas into one box. */
static lv_area_t g_inv[RECTS_MAX];
static int g_ninv;
/* LVGL also sends this event to probe its draw buffer rounding: 0,0..0,h (lv_refr.c, get_max_row); those are no draws. */
static void invalidated(lv_event_t *e) {
    const lv_area_t *a = lv_event_get_param(e);
    if (g_tracking && a && g_ninv < RECTS_MAX && !(a->x1 == 0 && a->x2 == 0 && a->y1 == 0)) g_inv[g_ninv++] = *a;
}
static void watch_invalidation(void) { lv_display_add_event_cb(lv_display_get_default(), invalidated, LV_EVENT_INVALIDATE_AREA, NULL); }
static int invalidated_outside(void) {
    int bad = 0;
    for (int i = 0; i < g_ninv; i++) {
        bool ok = false;
        lv_area_t box = g_allowed.a[0];
        for (int k = 1; k < g_allowed.n; k++) { const lv_area_t *o = &g_allowed.a[k]; box = (lv_area_t){LV_MIN(box.x1, o->x1), LV_MIN(box.y1, o->y1), LV_MAX(box.x2, o->x2), LV_MAX(box.y2, o->y2)}; }
        const lv_area_t *v = &g_inv[i];
        if (v->x1 >= box.x1 && v->y1 >= box.y1 && v->x2 <= box.x2 && v->y2 <= box.y2) { /* in the box, off the keys */
            ok = true;
            for (int k = 0; k < g_keys.n; k++) if (v->x1 <= g_keys.a[k].x2 && g_keys.a[k].x1 <= v->x2 && v->y1 <= g_keys.a[k].y2 && g_keys.a[k].y1 <= v->y2) ok = false;
        }
        for (int k = 0; k < g_allowed.n && !ok; k++) { const lv_area_t *o = &g_allowed.a[k]; ok = v->x1 >= o->x1 && v->y1 >= o->y1 && v->x2 <= o->x2 && v->y2 <= o->y2; }
        if (!ok && ++bad <= 3) fprintf(stderr, "INVALID %d,%d..%d,%d\n", (int)g_inv[i].x1, (int)g_inv[i].y1, (int)g_inv[i].x2, (int)g_inv[i].y2);
    }
    return bad;
}
static void untrack(void) { quiet(); g_tracking = false; }
static int count_meeting(lv_obj_t *o, bool text_waveform) {
    lv_area_t a; lv_obj_get_coords(o, &a); hal_rect_t n = native(&a);
    int count = 0;
    for (int i = 0; i < g_nrects; i++)
        if (meets(&g_rects[i].r, &n) && (!text_waveform || g_rects[i].wf == HAL_WF_GL16 || g_rects[i].wf == HAL_WF_GC16) /* rule 3 makes every Nth a GC16 */) count++;
    return count;
}

/* ------------------------------------------------------------ reverse controls (switched on in the test only) */
static void redraw_all(lv_event_t *e) { /* outside the press's own drawing switch, as a keyboard that redrew itself would */
    (void)e; bool on = lv_display_is_invalidation_enabled(NULL);
    lv_display_enable_invalidation(NULL, true); lv_obj_invalidate(K.keys); lv_display_enable_invalidation(NULL, on);
}
static void reverse(void) {
    if (!strcmp(g_case, "reverse-redraw")) lv_obj_add_event_cb(K.board, redraw_all, LV_EVENT_VALUE_CHANGED, NULL);
    if (!strcmp(g_case, "reverse-press")) while (lv_obj_remove_event_cb(K.board, before_feedback)) {}
}

/* ============================================================ refresh (C1-C5) */
static void allow_typing(void) {
    g_allowed.n = 0; g_keys.n = 0; allow(K.composing); allow(K.cands); allow(g_field);
    lv_obj_update_layout(K.root); lv_obj_get_coords(K.board, &g_keys.a[g_keys.n++]); /* the key rows: never drawn by typing */
    if (K.readings) allow(K.readings);
    if (K.space_key >= 0 && K.footer) {
        lv_area_t key = ((lv_buttonmatrix_t *)K.footer)->button_areas[K.space_key - K.footer_offset], board;
        lv_obj_get_coords(K.footer, &board); lv_area_move(&key, board.x1, board.y1); allow_area(&key);
        g_keys.a[g_keys.n] = board; g_keys.a[g_keys.n++].x2 = key.x1 - 1; /* the footer keys beside the space */
        g_keys.a[g_keys.n] = board; g_keys.a[g_keys.n++].x1 = key.x2 + 1;
    }
}
static void allow_root(void) { g_allowed.n = g_keys.n = 0; allow(K.root); }
static void allow_picker(void) { g_allowed.n = g_keys.n = 0; allow(K.picker); }
static void allow_popup(void) { g_allowed.n = g_keys.n = 0; allow(K.popup); }
/* How many panel updates each main action costs, for the report (printed, not judged). */
static void count(const char *what) { untrack(); printf("REFRESHES %s %d\n", what, g_nrects); }
static void counts(void) {
    field(INK_TEXT_PLAIN); on_ui(chinese); quiet(); delay_ms(600);
    track(); tap_key('n'); count("一个字母");
    tap_key('i'); delay_ms(600); quiet();
    track(); tap_label(K.cand[0].word); count("选词");
    for (const char *p = "shi"; *p; p++) tap_key(*p);
    delay_ms(600); quiet();
    track(); tap_label("▾"); count("展开候选");
    track(); tap_label("▶"); count("候选翻页");
    tap_label(TR("键盘")); on_ui(leave); quiet(); field(INK_TEXT_PLAIN); on_ui(chinese); quiet();
    track(); tap_key(KEY_SWITCH); count("切到符号面板");
    track(); tap_label(TR("数学")); count("切分类");
    track(); tap_label("▶"); count("符号翻页");
    tap_label("26"); on_ui(english); quiet(); delay_ms(600);
    g_code = 'u'; on_ui(key_center); track(); press_at(g_x, g_y); delay_ms(700); release_at(g_x, g_y); count("长按弹出");
    track(); tap_label("ǔ"); count("选备选");
    on_ui(chinese); on_ui(nine_on); quiet();
    for (const char *p = "94664"; *p; p++) tap_key(*p);
    delay_ms(600); quiet();
    track(); tap_label("zhong"); count("选读音");
    on_ui(nine_off); quiet();
    leave_page();
}
static void refresh_case(void) {
    field(INK_TEXT_PLAIN); on_ui(english); quiet(); on_ui(reverse);
    /* C3: a press alone draws nothing (D-31): no update while a key is held */
    g_code = 'g'; on_ui(key_center); track(); press_at(g_x, g_y); delay_ms(300); g_tracking = false;
    CHECK(g_nrects == 0, "a press alone draws nothing (%d updates)", g_nrects);
    release_at(g_x, g_y); quiet();
    on_ui(chinese); quiet(); on_ui(allow_typing);
    track();
    for (const char *p = "nihaozhong"; *p; p++) tap_key(*p);
    untrack();
    CHECK(g_nrects > 0 && !outside_allowed(), "C1: ten letters refresh only the composition, the candidates and the field (%d of %d outside)", outside_allowed(), g_nrects);
    /* C2: a layout change draws the keyboard area; tabs and pages draw inside the panel */
    on_ui(allow_root); track(); tap_key(KEY_SWITCH); untrack();
    CHECK(g_nrects > 0 && !outside_allowed(), "C2: the symbol panel draws within the keyboard (%d outside)", outside_allowed());
    on_ui(allow_picker); track(); tap_label("数学"); tap_label("▶"); untrack();
    CHECK(g_nrects > 0 && !outside_allowed(), "C2: a tab and a page draw within the panel (%d outside)", outside_allowed());
    tap_label("26"); on_ui(english); quiet();
    /* A4: the long-press list opens and closes in its own area */
    delay_ms(600); g_code = 'e'; on_ui(key_center); track(); press_at(g_x, g_y); delay_ms(700); release_at(g_x, g_y); quiet(); g_tracking = false;
    on_ui(allow_popup);
    CHECK(K.popup && !outside_allowed(), "A4: the long-press list draws only its own area (%d outside)", outside_allowed());
    track(); tap_label(TR("收起")); untrack();
    CHECK(!K.popup && g_nrects > 0 && !outside_allowed(), "A4: 收起 draws only that area again (%d outside)", outside_allowed());
    /* C4: three changes, then 400 ms after the last key one clean redraw of the strip; none while typing on or held.
     * Typing on is a person's pace (150 ms a key), so the keys go straight to the touch driver here. */
    on_ui(chinese); quiet(); delay_ms(600);
    static const int burst[] = {'n', 'i', KEY_BACKSPACE, KEY_BACKSPACE, 'n', 'i'};
    track(); fast_keys(burst, 6); g_tracking = false;
    int while_typing = count_meeting(K.cands, true);
    CHECK(while_typing == 0, "C4: no clean redraw of the strip while typing on (%d)", while_typing);
    g_nrects = 0; g_tracking = true; delay_ms(700); quiet(); g_tracking = false;
    CHECK(count_meeting(K.cands, true) == 1, "C4: one clean redraw of the strip after the pause (%d)", count_meeting(K.cands, true));
    lv_area_t c; on_ui(allow_typing); c = g_allowed.a[0];
    int hx = (c.x1 + c.x2) / 2, hy = (c.y1 + c.y2) / 2; /* the composition line: nothing to press there */
    static const int again[] = {KEY_BACKSPACE, KEY_BACKSPACE, 'n', 'i'};
    track(); fast_keys(again, 4); press_at(hx, hy); delay_ms(1000); g_tracking = false;
    int held = count_meeting(K.cands, true);
    CHECK(held == 0, "C4: no settle redraw while a finger is held for a second (%d)", held);
    g_nrects = 0; g_tracking = true; release_at(hx, hy); delay_ms(700); quiet(); g_tracking = false;
    CHECK(count_meeting(K.cands, true) == 1, "C4: it comes once the finger lifts (%d)", count_meeting(K.cands, true));
    tap_key(KEY_BACKSPACE); tap_key(KEY_BACKSPACE);
    /* C1 nine-key: the reading column and, when the composition starts and ends, only the space key */
    /* Here the reading column at the left and the field at the top change in one frame, and the display sends their
     * box (it passes the group keys, whose pixels do not change: refresh_policy.c), so what is checked is what the
     * keyboard asks LVGL to draw, and that no update is the whole panel. */
    on_ui(nine_on); quiet(); on_ui(allow_typing); on_ui(watch_invalidation);
    g_ninv = 0; track(); for (const char *p = "94664"; *p; p++) tap_key(*p); tap_key(KEY_RETYPE); untrack();
    hal_panel_info_t panel; hal_display_get_info(&panel);
    bool whole = false;
    for (int i = 0; i < g_nrects; i++) if (g_rects[i].r.w * g_rects[i].r.h >= panel.width * panel.height) whole = true;
    CHECK(g_ninv > 0 && !invalidated_outside() && !whole, "C1: nine-key typing draws the composition, candidates, field, readings and the space key only (%d of %d outside)", invalidated_outside(), g_ninv);
    on_ui(nine_off); quiet();
    leave_page();
}

/* ============================================================ tap-loss with the new keys (F9, step 0.4) */
static bool g_invalidation;
static void read_invalidation(void) { g_invalidation = lv_display_is_invalidation_enabled(NULL); }
static char g_first[40];
static void read_first(void) { snprintf(g_first, sizeof g_first, "%s", lv_label_get_text(K.cand_label[0])); }
static ink_text_cand_t g_cand0;
static void read_service_first(void) { ink_text_candidates(0, &g_cand0, 1); }
static char g_second[40];
static void shi(void) { chinese(); ink_text_key('s'); ink_text_key('h'); ink_text_key('i'); }
static void read_second(void) { snprintf(g_second, sizeof g_second, "%s", lv_label_get_text(K.cand_label[1])); }
static void pending_case(void) {
    py_want_dict(NULL);
    CHECK(WAIT_FOR(py_dict_loaded(), 5000), "the dictionary is in (its arrival would reorder the list by itself)");
    field(INK_TEXT_PLAIN); on_ui(shi); quiet();
    on_ui(read_first); on_ui(read_second); char before[40]; snprintf(before, sizeof before, "%s", g_first);
    CHECK(g_second[0] && strcmp(g_first, g_second), "two different first words to swap (%.40s, %.40s)", g_first, g_second);
    g_code = 'a'; on_ui(key_center);
    press_at(g_x, g_y);
    on_ui(tap_held_reorder); /* the list changes under the finger */
    on_ui(read_first);
    CHECK(!strcmp(g_first, before), "kb_pending_redraw_after_release: nothing moves under the finger (%.40s)", g_first);
    lv_area_t c; lv_obj_get_coords(K.cands, &c); hal_rect_t strip = native(&c);
    hw(HW_PRESS, g_x, g_y + ink_ui_px(2 * key_height(false))); /* slide off, downwards (up would be a swipe): no key */
    track(); release_at(g_x, g_y + ink_ui_px(2 * key_height(false))); quiet(); g_tracking = false;
    on_ui(read_first); on_ui(read_service_first); on_ui(read_invalidation);
    bool drawn = false;
    for (int i = 0; i < g_nrects; i++) if (meets(&g_rects[i].r, &strip)) drawn = true;
    CHECK(!strcmp(g_first, g_second) && !strcmp(g_first, g_cand0.word) && drawn && g_invalidation,
          "kb_pending_redraw_after_release: the strip is drawn again after the lift (%.40s -> %.40s, drawn %d, second %.40s, service %.40s)", before, g_first, drawn, g_second, g_cand0.word);
    /* layout changes from a key (symbols, language, nine-key) leave screen invalidation on */
    static const int codes[] = {KEY_SWITCH, KEY_SWITCH, INK_TEXT_KEY_LANGUAGE, INK_TEXT_KEY_LANGUAGE, KEY_LAYOUT, KEY_LAYOUT};
    on_ui(leave); quiet(); field(INK_TEXT_PLAIN); on_ui(chinese); quiet();
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        tap_key(codes[i]); on_ui(read_invalidation);
        CHECK(g_invalidation, "kb_invalidation_restored: drawing is on after key %d", codes[i]);
    }
    leave_page();
}

/* ============================================================ nine-key (B1-B15) */
static char g_list[64][12];
static int g_nlist;
static void read_readings(void) { /* what the column shows now */
    for (int i = 0; i < K.readings_shown && g_nlist < 64; i++) snprintf(g_list[g_nlist++], sizeof g_list[0], "%s", lv_label_get_text(lv_obj_get_child(K.reading[i], 0)));
}
static int g_all;
static const char *g_mogo[64];
static void mogo_pages(void) { /* Mogo's own paging of syllables(), three at a time */
    size_t keep = K.reading_offset; g_all = 0;
    const char *out[4]; bool more = true;
    for (K.reading_offset = 0; more && g_all < 60; K.reading_offset += 3) {
        size_t n = syllables(out, &more);
        for (size_t i = 0; i < n; i++) g_mogo[g_all++] = out[i];
    }
    K.reading_offset = keep;
}
static bool g_more_cell;
static void read_more_cell(void) { g_more_cell = K.actions[ACT_READING + 2].cap && !strcmp(K.actions[ACT_READING + 2].cap, "keyboard.readings_more"); }
static char g_value[256], g_comp[64], g_line[400];
static void read_text(void) {
    snprintf(g_value, sizeof g_value, "%s", ink_text_value()); snprintf(g_comp, sizeof g_comp, "%s", ink_text_composing());
    snprintf(g_line, sizeof g_line, "%s", lv_label_get_text(K.composing));
}
static bool has_digit(const char *s) { return strpbrk(s, "0123456789") != NULL; }
static void insert_text(void) { ink_text_commit_utf8("好"); }
static void nine_case(void) {
    field(INK_TEXT_PLAIN); on_ui(chinese); on_ui(nine_on); quiet();
    for (const char *p = "74"; *p; p++) tap_key(*p); /* pi qi ri si */
    /* B2: longest first; 更多 walks every reading, none skipped, and comes back to the first group */
    on_ui(mogo_pages);
    g_nlist = 0; on_ui(read_readings); on_ui(read_more_cell);
    CHECK(g_all > 3 && g_more_cell && K.readings_shown == 2, "B2: more than three readings: two and 更多 (%d readings)", g_all);
    for (int i = 1; i < g_all; i++) CHECK(strlen(g_mogo[i]) <= strlen(g_mogo[i - 1]), "B2: longer readings first (%.40s, %.40s)", g_mogo[i - 1], g_mogo[i]);
    for (int turn = 0; turn < 30 && g_nlist < g_all; turn++) { tap_label(TR("更多")); on_ui(read_readings); }
    bool same = g_nlist == g_all;
    for (int i = 0; same && i < g_all; i++) same = !strcmp(g_list[i], g_mogo[i]);
    CHECK(same, "kb_readings_none_skipped: 更多 shows every reading once, in Mogo's order (%d of %d: %.40s %.40s %.40s %.40s / %.40s %.40s %.40s %.40s)", g_nlist, g_all, g_list[0], g_list[1], g_list[2], g_list[3], g_mogo[0], g_mogo[1], g_mogo[2], g_mogo[3]);
    tap_label(TR("更多"));
    g_nlist = 0; on_ui(read_readings);
    CHECK(K.reading_offset == 0 && g_nlist && !strcmp(g_list[0], g_mogo[0]), "kb_readings_more_wraps: past the last group 更多 starts again");
    /* B3/B4 by touch, then the same by the AI */
    tap_key(KEY_RETYPE);
    for (const char *p = "94664"; *p; p++) tap_key(*p);
    on_ui(read_text);
    CHECK(!has_digit(g_line) && !strcmp(g_line, "[wxyz] [ghi] [mno] [mno] [ghi]"), "kb_nine_no_digits_shown: %.40s", g_line);
    tap_label("zhong"); on_ui(read_text);
    bool china = false;
    for (int i = 0; i < K.shown && i < INK_TEXT_PAGE; i++) if (!strcmp(K.cand[i].word, "中国")) china = true;
    CHECK(china && !strcmp(g_line, "zhong") && !strcmp(ink_text_preferred_reading(), "zhong"), "kb_nine_prefer_zhong: 中国 on the first page (%.40s)", g_line);
    CHECK(lv_color_eq(lv_obj_get_style_bg_color(K.reading[0], 0), ink_ui_ink()) || lv_color_eq(lv_obj_get_style_bg_color(K.reading[1], 0), ink_ui_ink()), "B3: the chosen reading is inverted");
    tap_key('4'); on_ui(read_text);
    CHECK(!strcmp(ink_text_preferred_reading(), "zhong") && !strcmp(g_line, "zhong [ghi]"), "B3: more digits keep the reading (%.40s)", g_line);
    tap_key(KEY_BACKSPACE);
    CHECK(!ink_text_preferred_reading()[0], "B3: delete forgets the reading");
    ai("keyboard.reading", "{\"text\":\"zhong\"}");
    CHECK(!strcmp(ink_text_preferred_reading(), "zhong"), "B3: the AI chooses the same reading");
    CHECK(AI("keyboard.reading", "{\"text\":\"bang\"}") && g_rc.status == INK_RC_REJECTED, "B3: a reading the digits do not spell is refused");
    ai("keyboard.reading", "{\"text\":\"\"}");
    CHECK(!ink_text_preferred_reading()[0], "B3: an empty reading takes it back");
    /* B12: digits never become text, by any path */
    static const int paths[] = {KEY_ENTER, INK_TEXT_KEY_LANGUAGE, KEY_SWITCH, KEY_LAYOUT, ',', '.'};
    for (unsigned i = 0; i < sizeof paths / sizeof *paths; i++) {
        tap_key(KEY_RETYPE); tap_key('9'); tap_key('9'); tap_key('9'); tap_key('9');
        tap_key(paths[i]); on_ui(read_text);
        CHECK(!has_digit(g_value) && !strcmp(g_comp, "9999") && !strcmp(g_line, TR("先选词或按重输")) && ink_text_nine() && !K.symbols,
              "kb_nine_digits_never_committed: key %d asks for a word and keeps the digits (%.40s / %.40s)", paths[i], g_value, g_line);
    }
    delay_ms(3000); on_ui(read_text);
    CHECK(!strcmp(g_line, TR("先选词或按重输")), "kb_notice_stays_until_next_key: no timer takes it away");
    tap_key('9'); on_ui(read_text);
    CHECK(strcmp(g_line, TR("先选词或按重输")) && !has_digit(g_line), "kb_notice_stays_until_next_key: the next key does (%.40s)", g_line);
    on_ui(insert_text); on_ui(read_text);
    CHECK(!has_digit(g_value) && !g_comp[0] && strstr(g_value, "好"), "kb_nine_digits_never_committed: the AI's text.insert path drops the digits (%.40s)", g_value);
    tap_key('9'); tap_key('9');
    on_ui(leave); quiet(); field(INK_TEXT_PLAIN); on_ui(read_text);
    CHECK(!has_digit(g_value) && !g_comp[0], "kb_nine_digits_never_committed: another field drops them (%.40s)", g_value);
    /* B7: 重输 clears the composition only; B15: too long */
    on_ui(nine_on); quiet();
    tap_key('9'); tap_key('4'); tap_key(KEY_RETYPE); on_ui(read_text);
    CHECK(!g_comp[0], "kb_retype_clears_composing");
    for (int i = 0; i < 41; i++) ai_key('2');
    on_ui(read_text);
    CHECK(!strcmp(g_line, TR("拼音过长，先选词")) && strlen(g_comp) == 40, "kb_composing_too_long_notice (%zu letters, %.40s)", strlen(g_comp), g_line);
    tap_key(KEY_RETYPE); on_ui(nine_off); quiet();
    leave_page();
}

/* ============================================================ symbols (E1-E19) */
static bool g_ok;
static char g_bad[64];
static void glyphs(void) {
    g_ok = true; g_bad[0] = 0;
    lv_font_glyph_dsc_t d;
    for (int t = 0; t < CATEGORIES - 1; t++) for (int i = 0; i < TABLES[t].count; i++) {
        const char *s = TABLES[t].items[i]; size_t at = 0, n = strlen(s);
        while (at < n) { uint32_t cp = ink_utf8_next(s, n, &at); if (!lv_font_get_glyph_dsc(font(), &d, cp, 0)) { g_ok = false; snprintf(g_bad, sizeof g_bad, "%s", s); } }
    }
    for (int l = 0; l < 52; l++) { char alt[16];
        for (int k = 0; alternate(l, k, alt); k++) { size_t at = 0, n = strlen(alt);
            while (at < n) { uint32_t cp = ink_utf8_next(alt, n, &at); if (!lv_font_get_glyph_dsc(font(), &d, cp, 0)) { g_ok = false; snprintf(g_bad, sizeof g_bad, "%s", alt); } } } }
}
static void ids(void) { /* recent ids are (table << 7 | index) + 1: these must never move */
    g_ok = symbol_by_id(1) && !strcmp(symbol_by_id(1), "，") && symbol_by_id(2 * 128 + 1) && !strcmp(symbol_by_id(2 * 128 + 1), "≈") &&
           symbol_by_id(128 + 1) && !strcmp(symbol_by_id(128 + 1), "1") && symbol_by_id(5 * 128 + 1) && !strcmp(symbol_by_id(5 * 128 + 1), "①") &&
           symbol_by_id(11) && !strcmp(symbol_by_id(11), "……") && symbol_by_id(4 * 128 + 1) && !strcmp(symbol_by_id(4 * 128 + 1), "△") &&
           symbol_by_id(3 * 128 + 1) && !strcmp(symbol_by_id(3 * 128 + 1), "℃");
}
static void min108(lv_obj_t *row) {
    for (uint32_t i = 0; row && i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *o = lv_obj_get_child(row, (int32_t)i);
        if (lv_obj_is_visible(o) && lv_obj_check_type(o, &lv_button_class) && (lv_obj_get_width(o) < 108 || lv_obj_get_height(o) < 108)) g_ok = false;
    }
}
static void targets(void) { g_ok = true; lv_obj_update_layout(K.root); min108(K.tabs); min108(K.symbol_grid); min108(K.picker_nav); }
static int g_category, g_page;
static void read_panel(void) { g_category = K.category; g_page = K.symbol_page; read_text(); }
static int g_took;
static void take_snapshot(void) { g_took = ink_ui_context_snapshot(g_snapshot, sizeof g_snapshot, NULL); }
static void symbols_case(void) {
    on_ui(glyphs); CHECK(g_ok, "kb_symbol_glyphs: every symbol and alternate has a glyph (%.40s)", g_bad);
    on_ui(ids); CHECK(g_ok, "kb_recent_ids_stable: the stored ids keep their symbols");
    field(INK_TEXT_PLAIN); on_ui(chinese); quiet();
    tap_key(KEY_SWITCH); on_ui(read_panel);
    CHECK(K.symbols && g_category == 1, "kb_picker_opens_on_cn: 中文 first, not 最近 (%d)", g_category);
    on_ui(targets); CHECK(g_ok, "kb_tabs_min_108 / kb_symbol_cells_min_108");
    tap_symbol("……"); on_ui(read_text);
    CHECK(!strcmp(g_value, "……"), "kb_multi_char_symbol: …… goes in whole (%.40s)", g_value);
    tap_label(TR("英文")); tap_symbol("1"); on_ui(read_text);
    tap_label(TR("最近")); on_ui(read_panel);
    CHECK(symbol_at(RECENT, 0) && !strcmp(symbol_at(RECENT, 0), "……") && !symbol_at(RECENT, 1), "kb_recent_skips_digits: 1 is not remembered");
    tap_label(TR("数学")); key(INK_KEY_NEXT); quiet(); on_ui(read_panel);
    CHECK(g_category == 3 && g_page == 1, "kb_cover_keys_page_symbols: the page key turns the symbol page (%d)", g_page);
    key(INK_KEY_PREV); quiet(); on_ui(read_panel);
    CHECK(g_page == 0, "kb_symbol_pages: and back");
    tap_symbol("≈"); tap_label(TR("最近"));
    CHECK(symbol_at(RECENT, 0) && !strcmp(symbol_at(RECENT, 0), "≈"), "kb_recent_first_is_last_used");
    tap_label(TR("删除")); on_ui(read_text);
    CHECK(!strcmp(g_value, "……1"), "kb_picker_delete: the panel's delete (%.40s)", g_value);
    tap_label("26");
    on_ui(leave); quiet();
    int64_t mine = ink_setting_get(RECENT_SETTINGS[0]);
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "switch to the child");
    CHECK(!ink_setting_get(RECENT_SETTINGS[0]) && !ink_setting_get(RECENT_SETTINGS[1]), "kb_recent_per_member: the child has their own");
    CHECK(ink_member_switch("owner") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "back to the owner");
    CHECK(ink_setting_get(RECENT_SETTINGS[0]) == mine, "kb_recent_per_member: the owner's come back");
    /* E19 / H1: a password has no 最近, learns nothing, shows the AI nothing and takes nothing from it */
    field(INK_TEXT_PASSWORD);
    tap_key('s'); tap_key('e'); tap_key(KEY_SWITCH);
    g_label = TR("最近"); on_ui(label_center);
    CHECK(!g_found && K.symbols && K.category == 2, "kb_password_no_recent: no 最近 tab, ASCII only");
    tap_symbol("#");
    CHECK(ink_setting_get(RECENT_SETTINGS[0]) == mine, "kb_password_no_recent: nothing typed into a password is remembered");
    CHECK(AI("keyboard.symbol", "{\"text\":\"#\"}") && g_rc.status == INK_RC_REJECTED, "H1: the AI cannot type into a password");
    CHECK(AI("keyboard.key", "{\"code\":97}") && g_rc.status == INK_RC_REJECTED, "H1: not by keys either");
    on_ui(take_snapshot); int took = g_took;
    CHECK(took > 0 && !strstr(g_snapshot, "se#"), "H1: no text in the snapshot (%d)", took);
    CHECK(!lv_obj_is_visible(K.cands) || !K.cands, "H1: no candidates");
    leave_page();
}

/* ============================================================ letters, popup, swipe, number pads, grid, gaps */
static void read_case_state(void) { g_ok = K.shift; g_category = K.caps; read_text(); }
static int g_gap[2], g_key[2];
static void read_gap(void) { g_gap[g_page] = gap(); g_key[g_page] = key_height(false); }
static int g_total;
static void read_total(void) { g_total = candidate_total(); read_text(); }
static void accepted(void) {
    g_ok = true;
    for (int i = 0; i < K.buttons; i++) {
        char c = K.codes[i];
        if (!c || c == KEY_BACKSPACE || c == KEY_ENTER) continue;
        size_t before = strlen(ink_text_value());
        ink_text_key(c);
        if (strlen(ink_text_value()) != before + 1) { g_ok = false; g_bad[0] = c; g_bad[1] = 0; }
    }
}
static void letters_case(void) {
    field(INK_TEXT_PLAIN); on_ui(english); quiet();
    /* A6: once, locked, off */
    tap_key(KEY_SHIFT); tap_key('a'); tap_key('b'); on_ui(read_case_state);
    CHECK(!strcmp(g_value, "Ab") && !g_ok, "kb_caps_cycle: one tap capitalises one letter (%.40s)", g_value);
    tap_key(KEY_SHIFT); tap_key(KEY_SHIFT); tap_key('c'); tap_key('d'); on_ui(read_case_state);
    CHECK(!strcmp(g_value, "AbCD") && g_category, "kb_caps_cycle: two taps lock (%.40s)", g_value);
    tap_key(KEY_SHIFT); tap_key('e'); on_ui(read_case_state);
    CHECK(!strcmp(g_value, "AbCDe") && !g_category && !g_ok, "kb_caps_cycle: a third tap ends it (%.40s)", g_value);
    tap_key(KEY_SHIFT); tap_key(INK_TEXT_KEY_LANGUAGE); tap_key(INK_TEXT_KEY_LANGUAGE); on_ui(read_case_state);
    CHECK(!g_ok && !g_category, "kb_caps_reset: switching language resets the case");
    CHECK(ink_ui_secondary().red < 0x80, "kb_alt_label_contrast: the swipe hint is dark gray");
    /* A3: swipe up types the alternate; sliding to a neighbour types nothing */
    on_ui(english); quiet();
    on_ui(leave); quiet(); field(INK_TEXT_PLAIN); on_ui(english); quiet();
    g_code = 'q'; on_ui(key_center);
    press_at(g_x, g_y); hw(HW_PRESS, g_x, g_y - ink_ui_px(key_height(false)) * 3 / 4); release_at(g_x, g_y - ink_ui_px(key_height(false)) * 3 / 4); quiet();
    on_ui(read_text); CHECK(!strcmp(g_value, "1"), "kb_swipe_up_q: 1 (%.40s)", g_value);
    int ax, ay; g_code = 'a'; on_ui(key_center); ax = g_x; ay = g_y; g_code = 's'; on_ui(key_center);
    press_at(ax, ay); hw(HW_PRESS, g_x, g_y); release_at(g_x, g_y); quiet(); on_ui(read_text);
    CHECK(!strcmp(g_value, "1"), "kb_swipe_no_neighbor: a slide from a to s types neither (%.40s)", g_value);
    /* A4: hold, then tap a cell */
    g_code = 'u'; on_ui(key_center); press_at(g_x, g_y); delay_ms(700); release_at(g_x, g_y); quiet();
    CHECK(K.popup, "kb_popup_pick: holding u opens its list");
    on_ui(read_text); CHECK(!strcmp(g_value, "1"), "A4: the release after the hold types nothing (%.40s)", g_value);
    tap_label("ǔ"); on_ui(read_text);
    CHECK(!K.popup && !strcmp(g_value, "1ǔ"), "kb_popup_pick: ǔ (%.40s)", g_value);
    /* F3-F5, B13: the grid pages by buttons and the page keys, to the service's last candidate */
    on_ui(chinese); quiet(); tap_key('s'); tap_key('h'); tap_key('i');
    ai("keyboard.candidates", "{}");
    on_ui(read_total);
    lv_obj_t *cell = K.cand_btn[0];
    CHECK(K.expanded && g_total > 6 && lv_obj_get_width(cell) >= 108 && lv_obj_get_height(cell) >= 108, "kb_grid_cells_min_108 (%d candidates)", g_total);
    int pages = (g_total + 5) / 6;
    key(INK_KEY_NEXT); quiet();
    CHECK(K.first == 6, "kb_cover_keys_page_grid: the page key turns the grid");
    for (int i = 0; i < pages + 2; i++) tap_label("▶");
    char want[32]; snprintf(want, sizeof want, "%d/%d", pages, pages);
    CHECK(K.first == (pages - 1) * 6 && !strcmp(lv_label_get_text(K.cand_page), want) && g_total <= 60, "kb_grid_pages_to_60: the last page says %.40s (%.40s)", want, lv_label_get_text(K.cand_page));
    tap_label(TR("键盘")); ai_key(KEY_RETYPE);
    on_ui(leave); quiet();
    /* D2/D3: the number and phone fields only show keys the field takes; digits have no long press */
    field(INK_TEXT_NUMBER); on_ui(accepted); CHECK(g_ok && K.built_for == 3, "kb_no_dead_keys: number field (%.40s)", g_bad);
    g_code = '5'; on_ui(key_center); press_at(g_x, g_y); delay_ms(700); release_at(g_x, g_y); quiet();
    CHECK(!K.popup, "kb_number_no_popup");
    on_ui(leave); quiet();
    field(INK_TEXT_PHONE); on_ui(accepted); CHECK(g_ok && K.built_for == 3, "kb_no_dead_keys: phone field (%.40s)", g_bad);
    leave_page();
    /* H3: the gaps grow with the keys, by one factor */
    for (int large = 0; large < 2; large++) {
        CHECK(BY_HAND("settings.set", large ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "text size");
        g_page = large; on_ui(read_gap);
    }
    CHECK(g_key[1] > g_key[0], "H3: the keys grow at the largest size (%d -> %d)", g_key[0], g_key[1]);
    for (int i = 0; i < 2; i++)
        CHECK(abs(g_gap[i] * INK_KB_KEY - g_key[i] * INK_KB_GAP) < INK_KB_KEY, "kb_gap_scales_with_text: gap %d for key %d keeps 5:96 within a pixel", g_gap[i], g_key[i]);
    CHECK(BY_HAND("settings.set", "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "text size back");
}

/* ============================================================ held delete (A11, D6, D7) */
static size_t g_len;
static void read_len(void) { g_len = strlen(ink_text_value()); }
static void fill(void) { ink_text_commit_utf8("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"); }
static int g_x2, g_y2;
static void second_finger(void *arg) { sim_input_touch(1, arg != NULL, g_x2, g_y2); }
static void delete_case(void) {
    field(INK_TEXT_PLAIN); on_ui(english); on_ui(fill); quiet();
    g_code = KEY_BACKSPACE; on_ui(key_center); on_ui(read_len); size_t before = g_len;
    tap_at(g_x, g_y); on_ui(read_len);
    CHECK(g_len + 1 == before, "kb_delete_hold: one tap deletes one");
    press_at(g_x, g_y); delay_ms(1100); release_at(g_x, g_y); on_ui(read_len); size_t after = g_len;
    CHECK(after + 3 < before, "kb_delete_hold: held, it repeats (%zu -> %zu)", before, after);
    delay_ms(500); on_ui(read_len);
    CHECK(g_len == after, "kb_delete_stop_release: it stops when the finger lifts");
    press_at(g_x, g_y); delay_ms(700); hw(HW_PRESS, g_x, g_y - ink_ui_px(3 * key_height(false))); on_ui(read_len); after = g_len;
    delay_ms(500); on_ui(read_len);
    CHECK(g_len == after, "kb_delete_stop_move: it stops when the finger leaves the key");
    release_at(g_x, g_y - ink_ui_px(3 * key_height(false))); quiet();
    g_code = 'm'; on_ui(key_center); g_x2 = g_x; g_y2 = g_y - ink_ui_px(2 * key_height(false));
    g_code = KEY_BACKSPACE; on_ui(key_center);
    press_at(g_x, g_y); delay_ms(700);
    vtk_event_at(vtk_now() + SIM_MS(1), second_finger, (void *)1); delay_ms(30);
    on_ui(read_len); after = g_len;
    delay_ms(500); on_ui(read_len);
    CHECK(g_len == after, "kb_delete_stop_second_finger: a second finger stops it (%zu -> %zu)", after, g_len);
    vtk_event_at(vtk_now() + SIM_MS(1), second_finger, NULL); delay_ms(30);
    release_at(g_x, g_y); quiet();
    leave_page();
}

/* ============================================================ review pictures */
static volatile bool g_captured;
static void frame(void *arg) { sim_display_snapshot(arg); g_captured = true; }
static void capture(const char *name) {
    quiet();
    g_captured = false; vtk_event_at(vtk_now() + SIM_US(1), frame, (void *)name);
    CHECK(WAIT_FOR(g_captured, 2000), "picture %.40s", name);
}
static void nihao(void) { chinese(); for (const char *p = "nihao"; *p; p++) ink_text_key(*p); }
static void shots_case(void) {
    static const char *const SIZES[] = {"default", "large", "kid"};
    for (int s = 0; s < 3; s++) {
        if (s == 2) CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "the child");
        else CHECK(BY_HAND("settings.set", s ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "text size");
        char name[64];
#define SHOT(what) do { snprintf(name, sizeof name, "KB-%s-%s", what, SIZES[s]); capture(name); } while (0)
        field(INK_TEXT_PLAIN); on_ui(nihao); quiet(); SHOT("full");
        if (s == 0) { /* the key centres tools/device/autotest/memo.script taps, in percent of the screen */
            static const int keys[] = {INK_TEXT_KEY_LANGUAGE, 'h', 'e', 'l', 'o', KEY_BACKSPACE};
            for (unsigned k = 0; k < sizeof keys / sizeof *keys; k++) {
                g_code = keys[k]; on_ui(key_center);
                printf("KEY_CENTRE %d %.1f%% %.1f%%\n", keys[k], 100.0 * g_x / ink_ui_width(), 100.0 * g_y / ink_ui_height());
            }
        }
        ai("keyboard.candidates", "{}"); SHOT("expanded"); ai("keyboard.candidates", "{\"fold\":true}");
        ai_key(KEY_RETYPE); on_ui(english); quiet();
        ai("keyboard.alternates", "{\"code\":101}"); SHOT("popup"); ai("keyboard.alternates", "{\"code\":0}");
        ai_key(KEY_SHIFT); ai_key(KEY_SHIFT); SHOT("caps-lock"); ai_key(KEY_SHIFT);
        ai_key(KEY_NUMBER); SHOT("number"); ai_key(KEY_NUMBER);
        on_ui(chinese); on_ui(nine_on); quiet();
        for (const char *p = "94664"; *p; p++) ai_key(*p);
        SHOT("nine");
        on_ui(nine_off); quiet();
        ai_key(KEY_SWITCH);
        for (int c = 0; c < CATEGORIES; c++) {
            char args[32]; snprintf(args, sizeof args, "{\"category\":%d}", c); ai("keyboard.category", args);
            snprintf(name, sizeof name, "KB-symbols-%d-%s", c, SIZES[s]); capture(name);
        }
        on_ui(leave); quiet();
        field(INK_TEXT_NUMBER); SHOT("number-field"); on_ui(leave); quiet();
        field(INK_TEXT_PHONE); SHOT("phone-field"); on_ui(leave); quiet();
        leave_page();
    }
    CHECK(ink_member_switch("owner") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "back to the owner");
    CHECK(BY_HAND("settings.set", "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "text size back");
}

static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE); quiet();
    if (!strcmp(g_case, "refresh") || !strncmp(g_case, "reverse", 7)) refresh_case();
    else if (!strcmp(g_case, "counts")) counts();
    else if (!strcmp(g_case, "pending")) pending_case();
    else if (!strcmp(g_case, "nine")) nine_case();
    else if (!strcmp(g_case, "symbols")) symbols_case();
    else if (!strcmp(g_case, "letters")) letters_case();
    else if (!strcmp(g_case, "delete")) delete_case();
    else if (!strcmp(g_case, "shots")) shots_case();
    else CHECK(false, "unknown case %.40s", g_case);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(read_case, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "keyboard-2", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the keyboard-2 driver");
}
