/*
 * Home (launcher). First demo screen for the simulator: a counter button
 * (touch feedback latency), a "page" of text that turns with keys/swipes
 * (text refresh latency, ghost cleanup) and a system info line.
 */
#include <stdio.h>

#include "ink/sys/app.h"
#include "ink/probe.h"
#include "ink/sys/font.h"
#include "ink/sys/ui.h"

extern const ink_activity_t ink_app_reader;

static struct {
    lv_obj_t *count_label;
    lv_obj_t *page;
    lv_obj_t *info;
    int count;
    int page_no;
} H;

static const char *PAGES[] = {
    "Page 1. The quick brown fox jumps over the lazy dog. E-ink panels keep their image without power, "
    "but every change costs a waveform: hundreds of milliseconds and a full-frame scan.",
    "Page 2. A refresh scheduler merges changes, puts touch feedback first and cleans ghosting "
    "every few pages with a flashing update.",
    "Page 3. The simulator charges every task for its CPU time, every object for its RAM and every "
    "flash write for the stall it causes, so slow designs show up before hardware arrives.",
};
#define NPAGES (int)(sizeof PAGES / sizeof PAGES[0])

static void update_info(void) {
    size_t in, in_min, ex;
    ink_app_mem_free(&in, &in_min, &ex);
    lv_label_set_text_fmt(H.info, "internal free %u KiB (min %u)  psram free %u KiB", (unsigned)(in / 1024), (unsigned)(in_min / 1024),
                          (unsigned)(ex / 1024));
}

static void count_cb(lv_event_t *e) {
    (void)e;
    H.count++;
    lv_label_set_text_fmt(H.count_label, "Tapped %d", H.count);
    ink_probe_count("home.taps");
}

static void reader_cb(lv_event_t *e) { ink_ui_open(lv_event_get_target_obj(e), &ink_app_reader, NULL); }

static void turn(int delta) {
    H.page_no = (H.page_no + delta + NPAGES) % NPAGES;
    ink_ui_hint_obj(H.page, INK_REFRESH_TEXT);
    lv_label_set_text(H.page, PAGES[H.page_no]);
    update_info();
    ink_probe_count("home.page_turns");
}

static bool on_key(ink_key_t key, bool long_press) {
    if (long_press) return false;
    if (key == INK_KEY_NEXT || key == INK_KEY_VOL_DOWN) turn(+1);
    else if (key == INK_KEY_PREV || key == INK_KEY_VOL_UP) turn(-1);
    else return false;
    return true;
}

static bool on_swipe(ink_swipe_dir_t dir) {
    if (dir == INK_SWIPE_LEFT) turn(+1);
    else if (dir == INK_SWIPE_RIGHT) turn(-1);
    else return false;
    return true;
}

static void create(lv_obj_t *scr, void *arg) {
    (void)arg;
    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, ink_ui_font(1), 0);
    lv_label_set_text(title, "ink");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(12));

    lv_obj_t *btn = lv_button_create(scr);
    ink_ui_style_button(btn);
    lv_obj_set_size(btn, ink_dp(200), ink_dp(56));
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(64));
    lv_obj_add_event_cb(btn, count_cb, LV_EVENT_CLICKED, NULL);
    H.count_label = lv_label_create(btn);
    lv_label_set_text_fmt(H.count_label, "Tapped %d", H.count);
    lv_obj_center(H.count_label);

    lv_obj_t *rb = lv_button_create(scr);
    ink_ui_style_button(rb);
    lv_obj_set_size(rb, ink_dp(200), ink_dp(56));
    lv_obj_align(rb, LV_ALIGN_TOP_LEFT, ink_dp(16), ink_dp(132));
    lv_obj_add_event_cb(rb, reader_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(rb);
    ink_font_t *cjk = ink_font_for_pt(9, ink_panel_dpi());
    if (cjk) lv_obj_set_style_text_font(rl, ink_font_lvgl(cjk), 0);
    lv_label_set_text(rl, cjk ? "阅读" : "Reader");
    lv_obj_center(rl);

    H.page = lv_label_create(scr);
    lv_label_set_long_mode(H.page, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(H.page, lv_pct(90));
    lv_obj_align(H.page, LV_ALIGN_TOP_MID, 0, ink_dp(204));
    lv_label_set_text(H.page, PAGES[H.page_no]);

    H.info = lv_label_create(scr);
    lv_obj_set_style_text_font(H.info, ink_ui_font(2), 0);
    lv_obj_set_width(H.info, lv_pct(94));
    lv_obj_align(H.info, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(8));
    update_info();
}

const ink_activity_t ink_app_home = {
    .name = "home",
    .create = create,
    .on_key = on_key,
    .on_swipe = on_swipe,
};
