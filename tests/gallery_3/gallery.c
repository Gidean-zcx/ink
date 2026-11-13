/* gallery-3 on the real product: synthetic photos whose file times run.py sets on the host card folder.
 * INK_G3_PHASE: walk (by touch, with pictures and refresh marks), ai (the same walk through capabilities), kid,
 * more (what cannot be shown or read, long paths, cards, the lock), purge (30 days), empty. INK_G3_SIZE: 1 or 3.
 * Refresh areas and counts are judged by run.py from the display trace between g3.<step>.before/after marks. */
#include <stdlib.h>
#include "ink/rotation.h"
#include "ink/sys/time.h"
#include "ink/sys/confirm.h"
#include "src/core/lv_obj_draw_private.h" /* lv_obj_get_ext_draw_size: what an object may draw past its box */
bool g3_time_valid(void);
#define ink_time_valid g3_time_valid /* the clock can be marked untrusted while the wall clock keeps running */
#include "../../system/apps/gallery/gallery.c"
#undef ink_time_valid
#include "../../system/services/media/image.c" /* compiled here so run.py --reverse can change it */
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool untrusted;
bool g3_time_valid(void) { return !untrusted && ink_time_valid(); }
static char phase[16], size_text[4], snap[8192], found_text[160];
static const char *wanted_cap, *wanted_text;
static lv_point_t point;
static lv_area_t box;
static bool found;
static volatile bool captured;
static int64_t T0; /* 2026-11-20 12:00 local */

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "the panel settles (%s)", ink_ui_page_name()); }
static void read_snap(void) { CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "%s has a snapshot", ink_ui_page_name()); }
static void look(void) { quiet(); on_ui(read_snap); }
static bool has(const char *s) { return strstr(snap, s) != NULL; }
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void shot(const char *name) {
    static char file[64]; quiet();
    snprintf(file, sizeof file, "%s-%s", name, size_text);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture, file);
    CHECK(WAIT_FOR(captured, 2000), "picture %s", file);
}
static void walk_find(lv_obj_t *o) {
    if (found || !lv_obj_is_visible(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    const char *cap = ink_shell_bound_cap(o);
    bool hit = wanted_cap ? cap && !strcmp(cap, wanted_cap) : lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted_text);
    if (hit) {
        lv_obj_get_coords(o, &box); point = (lv_point_t){(box.x1 + box.x2) / 2, (box.y1 + box.y2) / 2}; found = true;
        if (!wanted_cap) snprintf(found_text, sizeof found_text, "%s", lv_label_get_text(o));
        return;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk_find(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { found = false; lv_obj_update_layout(lv_screen_active()); walk_find(lv_layer_sys()); walk_find(lv_screen_active()); }
static bool find_cap(const char *cap) { wanted_cap = cap; wanted_text = NULL; on_ui(locate); return found; }
static bool find_text(const char *text) { wanted_cap = NULL; wanted_text = text; on_ui(locate); return found; }
static void press(const char *cap) { quiet(); CHECK(find_cap(cap), "a visible button calls %s", cap); if (found) tap(point.x, point.y); }
static void touch(const char *text) { quiet(); CHECK(find_text(text), "\"%s\" is on the screen", text); if (found) tap(point.x, point.y); }
static void drag(int x0, int y0, int x1, int y1, const char *mark) {
    char m[48]; hw(HW_PRESS, x0, y0); delay_ms(100); hw(HW_PRESS, (x0 + x1) / 2, (y0 + y1) / 2); delay_ms(100); hw(HW_PRESS, x1, y1); delay_ms(400);
    snprintf(m, sizeof m, "g3.%s.held", mark); ink_probe_mark(m); /* nothing may be drawn while the finger is down */
    hw(HW_RELEASE, x1, y1); delay_ms(300);
}
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s %s: %s %.90s", cap, args ? args : "", ink_rc_name(g_rc.status), g_rc.detail);
}
static double metric(const char *name) { return sim_metric_value(name); }
static void page_ready(double before) { CHECK(WAIT_FOR(metric("gallery.page_ready") > before, 15000), "the gallery page is drawn"); quiet(); }
static void photo_drawn(double before) { CHECK(WAIT_FOR(metric("gallery.photo_drawn") > before, 15000), "the photo is drawn"); quiet(); }

/* Where a step's refreshes must stay, in the panel's native pixels, and the marks around the step. */
static lv_area_t bounds;
static void drawn_area(lv_obj_t *o, lv_area_t *a) { /* what LVGL redraws for it: its box and what its children may draw past it */
    lv_obj_update_layout(lv_screen_active()); lv_obj_get_coords(o, a); int32_t ext = lv_obj_get_ext_draw_size(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) ext = LV_MAX(ext, lv_obj_get_ext_draw_size(lv_obj_get_child(o, (int32_t)i)));
    a->x1 -= ext; a->y1 -= ext; a->x2 += ext; a->y2 += ext;
}
static void obj_area(lv_obj_t *o) { drawn_area(o, &bounds); }
static void grow(lv_obj_t *o) {
    lv_area_t a; drawn_area(o, &a);
    bounds.x1 = LV_MIN(bounds.x1, a.x1); bounds.y1 = LV_MIN(bounds.y1, a.y1); bounds.x2 = LV_MAX(bounds.x2, a.x2); bounds.y2 = LV_MAX(bounds.y2, a.y2);
}
static void publish(const char *what) {
    int w, h, x0, y0, x1, y1; hal_panel_info_t info; char key[64];
    hal_display_get_info(&info); ink_display_native_size(&w, &h);
    ink_logical_to_native(w, h, info.rotation_deg, bounds.x1, bounds.y1, &x0, &y0);
    ink_logical_to_native(w, h, info.rotation_deg, bounds.x2, bounds.y2, &x1, &y1);
    snprintf(key, sizeof key, "test.%s.x1", what); ink_probe_value(key, x0 < x1 ? x0 : x1);
    snprintf(key, sizeof key, "test.%s.y1", what); ink_probe_value(key, y0 < y1 ? y0 : y1);
    snprintf(key, sizeof key, "test.%s.x2", what); ink_probe_value(key, x0 < x1 ? x1 : x0);
    snprintf(key, sizeof key, "test.%s.y2", what); ink_probe_value(key, y0 < y1 ? y1 : y0);
}
static void mark(const char *step, const char *when) { char m[48]; snprintf(m, sizeof m, "g3.%s.%s", step, when); ink_probe_mark(m); }
static void area_screen(void) { bounds = (lv_area_t){0, 0, lv_display_get_horizontal_resolution(NULL) - 1, lv_display_get_vertical_resolution(NULL) - 1}; }
static void area_content(void) { obj_area(G.content); }
static void area_switch(void) { obj_area(G.content); grow(G.segments); }
static void area_photo(void) { obj_area(G.box); }
static void area_body(void) { obj_area(G.body); }
static void area_next(void) { obj_area(G.body); grow(G.name); }
static void area_cell(void) { /* with the outline it draws when pressed */
    int o = ink_ui_px(INK_STROKE_FOCUS) + 1; obj_area(G.slots[0]); bounds.x1 -= o; bounds.y1 -= o; bounds.x2 += o; bounds.y2 += o;
}
static void area_empty(void) { bounds = (lv_area_t){0, 0, 0, 0}; } /* nothing at all may be drawn */

/* Screen point of a picture point (upright, turned picture pixels) in the photo area. */
static void picture_point(float px, float py) {
    lv_area_t a; lv_obj_update_layout(lv_screen_active()); lv_obj_get_coords(G.box, &a);
    float s = fit(G.dw, G.dh, G.v.w, G.v.h) * (float)G.v.zoom;
    point = (lv_point_t){(int)(a.x1 + G.v.w / 2.f + (px - G.v.cx) * s), (int)(a.y1 + G.v.h / 2.f + (py - G.v.cy) * s)};
}
static bool text_visible; /* the small words of 字条.png are at 440..580 x 330..350 of 600 x 400 */
static void check_text(void) {
    float s = fit(G.dw, G.dh, G.v.w, G.v.h) * (float)G.v.zoom, vw = G.v.w / s, vh = G.v.h / s;
    text_visible = G.v.cx - vw / 2 <= 440 && G.v.cx + vw / 2 >= 580 && G.v.cy - vh / 2 <= 330 && G.v.cy + vh / 2 >= 350;
}
static void tap_text(void) { picture_point(510, 340); }
static void box_centre(void) { lv_area_t a; lv_obj_get_coords(G.box, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; }

static uint32_t id_of(const char *path) {
    uint32_t cursor = 0; ink_fs_entry_t e; char dir[INK_FS_PATH_MAX]; snprintf(dir, sizeof dir, "%.*s", (int)(strrchr(path, '/') - path), path);
    int r, seen = 0;
    while ((r = ink_fs_index(dir, INK_FS_IMAGE, &cursor, &e)) == 1) { seen++; if (!strcmp(e.path, path)) return e.id; }
    CHECK(false, "%s is in the index (%d entries in %s, then %d)", path, seen, dir, r);
    return 0;
}
static bool exists(const char *path) { ink_fs_stat_t st; return ink_fs_stat(path, &st) == 0; }
static char trashed[INK_FS_PATH_MAX];
static void read_undo(void) {
    const char *p = strstr(G.undo_args, "\"path\":\""), *e = p ? strchr(p + 8, '"') : NULL; trashed[0] = 0;
    if (p && e) snprintf(trashed, sizeof trashed, "%.*s", (int)(e - p - 8), p + 8);
}
static bool index_ready(void) {
    char s[24] = ""; ink_state_get_str(ink_state_find("file.index_state"), s, sizeof s); return !strcmp(s, "ready");
}
static void start(void) {
    CHECK(WAIT_FOR(ink_member_space_open() && ink_ui_page_current(), 12000), "member and UI ready");
    test_start_home(); quiet();
    ink_tm_t t = {.year = 2026, .month = 11, .day = 20, .hour = 12}; T0 = ink_time_from_local(&t); ink_time_set(T0, false);
    ok("settings.set", !strcmp(size_text, "3") ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}");
    CHECK(WAIT_FOR(index_ready(), 30000), "the card's index is ready"); quiet();
}
static void open_gallery(void) { double n = metric("gallery.page_ready"); ok("app.open", "{\"id\":\"gallery\"}"); page_ready(n); }
static const char *TEXT_PNG = "/me/旅行/字条.png", *TALL_JPG = "/me/照片/竖拍.jpg", *BIG_JPG = "/me/照片/大图.jpg", *WEBP = "/me/照片/动图.webp";

/* ---------------------------------------------------------------------------------------------- by touch */
static void walk(void) {
    start();
    mark("open", "before"); area_screen(); publish("open"); open_gallery(); mark("open", "after");
    look(); CHECK(G.count == 28 && G.view == VIEW_ALL && has("2026年11月"), "全部: every photo, newest first, its month above (%zu)", G.count);
    CHECK(item_of(G.ids[0]) && item_of(G.ids[0])->modified >= item_of(G.ids[1])->modified, "sorted by file time");
    shot("all");
    on_ui(area_content); publish("turn"); mark("turn", "before");
    double n = metric("gallery.page_ready"); key(INK_KEY_NEXT); page_ready(n); mark("turn", "after");
    CHECK(G.position.page == 1, "the cover key turns the grid page");
    key(INK_KEY_PREV); quiet();
    on_ui(area_switch); publish("months"); mark("months", "before"); touch("日期"); quiet(); mark("months", "after");
    look(); CHECK(G.view == VIEW_MONTHS && has("2026年9月") && has("9 张"), "日期: one row a month with its count"); shot("months");
    on_ui(area_switch); publish("albums"); mark("albums", "before"); touch("相簿"); quiet(); mark("albums", "after");
    look(); CHECK(G.view == VIEW_ALBUMS && has("旅行") && has("照片") && !has("最近删除"), "相簿: a row a folder, no 最近删除 row while it is empty"); shot("albums");
    touch("日期"); quiet(); n = metric("gallery.page_ready"); touch("2026年9月"); page_ready(n);
    look(); CHECK(G.view == VIEW_ALL && G.filter == FILTER_MONTH && G.count == 9 && has("2026年9月 · 9 张"), "a month row filters the grid to its 9 photos"); shot("filtered");
    /* the newest of September: the photo with small words */
    on_ui(area_cell); publish("open_photo_press"); /* the pressed cell's outline (D-31) is the only other thing drawn */
    mark("open_photo", "before"); area_screen(); publish("open_photo");
    n = metric("gallery.photo_drawn");
    CHECK(G.slots[0], "the first cell"); if (G.slots[0]) { lv_area_t a; lv_obj_get_coords(G.slots[0], &a); tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); }
    photo_drawn(n); mark("open_photo", "after");
    look(); CHECK(!strcmp(G.path, TEXT_PNG) && has("第1/9张"), "it opens the newest of the month as 1 of 9"); shot("photo_fit");
    /* double tap on the words: the first tap draws nothing, the second zooms there */
    on_ui(area_photo); publish("tap1"); on_ui(tap_text); mark("tap1", "before"); n = metric("gallery.photo_drawn");
    hw(HW_PRESS, point.x, point.y); delay_ms(80); hw(HW_RELEASE, point.x, point.y); delay_ms(150); mark("tap1", "after");
    CHECK(metric("gallery.photo_drawn") == n && G.v.zoom == 1, "a single tap changes nothing");
    on_ui(area_body); publish("zoom2"); mark("zoom2", "before");
    hw(HW_PRESS, point.x, point.y); delay_ms(80); hw(HW_RELEASE, point.x, point.y); photo_drawn(n); mark("zoom2", "after");
    on_ui(check_text); CHECK(G.v.zoom == 2 && text_visible, "double tap: 2x around the words, which are on the screen");
    shot("photo_zoom2");
    on_ui(area_photo); publish("drag"); on_ui(box_centre); mark("drag", "before"); n = metric("gallery.photo_drawn");
    drag(point.x, point.y, point.x - 60, point.y - 40, "drag"); photo_drawn(n); mark("drag", "after");
    on_ui(check_text); CHECK(G.v.zoom == 2 && text_visible, "dragging pans; the words stay in sight");
    on_ui(area_photo); publish("pan"); mark("pan", "before"); n = metric("gallery.photo_drawn"); float cx = G.v.cx;
    touch("左移"); photo_drawn(n); mark("pan", "after");
    CHECK(G.v.cx < cx, "左移 moves half a screen");
    on_ui(area_photo); publish("rotate"); mark("rotate", "before"); n = metric("gallery.photo_drawn");
    press("photo.rotate"); photo_drawn(n); mark("rotate", "after");
    look(); CHECK(G.v.rot == 1 && G.v.zoom == 2 && has("转了 90 度"), "旋转 turns the view a quarter, the zoom stays");
    ok("photo.rotate", "{\"dir\":\"left\"}"); quiet();
    on_ui(area_body); publish("fit"); mark("fit", "before"); n = metric("gallery.photo_drawn");
    ok("photo.zoom", "{\"step\":\"fit\"}"); photo_drawn(n); mark("fit", "after");
    CHECK(G.v.zoom == 1, "back to fit");
    /* swipe to the next one */
    on_ui(area_next); publish("next"); on_ui(box_centre); mark("next", "before"); n = metric("gallery.photo_drawn");
    drag(point.x + 150, point.y, point.x - 150, point.y, "next"); photo_drawn(n); mark("next", "after");
    look(); CHECK(has("第2/9张"), "a left swipe shows 2 of 9");
    uint32_t second = G.photo;
    mark("info", "before"); area_screen(); publish("info"); press("photo.info"); quiet(); mark("info", "after");
    look(); CHECK(!strcmp(ink_ui_page_name(), "PhotoInfo") && has("文件时间") && has("120×90") && has("旅行") && has("PNG"), "信息: file time named as such, size, album, format");
    shot("info_png");
    mark("info_back", "before"); press("shell.back"); quiet(); mark("info_back", "after");
    CHECK(!strcmp(ink_ui_page_name(), "Photo") && G.photo == second && G.image, "back from 信息 shows the same photo as it was");
    n = metric("gallery.page_ready"); press("shell.back"); page_ready(n);
    look(); CHECK(G.position.page == index_of(second) / PHOTO_SLOTS, "back: the page of the last photo seen");
    char outlined[32]; snprintf(outlined, sizeof outlined, "file:%u", (unsigned)second);
    CHECK(!strcmp(G.position.selected, outlined) && G.filter == FILTER_MONTH, "return_position: that cell outlined, the filter kept");
    /* delete one: no card, the next one, 撤销 */
    n = metric("gallery.photo_drawn"); on_ui(locate);
    if (G.slots[2]) { lv_area_t a; lv_obj_get_coords(G.slots[2], &a); tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); }
    photo_drawn(n); char was[INK_FS_PATH_MAX]; snprintf(was, sizeof was, "%s", G.path);
    on_ui(area_next); publish("trash"); mark("trash", "before"); n = metric("gallery.photo_drawn");
    ink_confirm_view_t card;
    press("photo.trash"); photo_drawn(n); mark("trash", "after");
    look(); on_ui(read_undo);
    CHECK(ink_confirm_list(&card, 1) == 0 && !exists(was) && trashed[0] && exists(trashed) && has("已移到最近删除") && has("第3/8张"),
          "trash_no_confirm: no card, it is in 最近删除, the next one shows with 撤销");
    shot("photo_after_trash");
    on_ui(area_next); publish("undo"); mark("undo", "before"); n = metric("gallery.photo_drawn");
    press("photo.restore"); photo_drawn(n); quiet(); mark("undo", "after");
    look(); CHECK(exists(was) && !exists(trashed) && !strcmp(G.path, was) && !has("已移到最近删除"), "撤销 puts it back and shows it");
    n = metric("gallery.photo_drawn"); press("photo.trash"); photo_drawn(n); on_ui(read_undo);
    n = metric("gallery.page_ready"); press("shell.back"); page_ready(n);
    touch("相簿"); quiet(); look(); CHECK(has("最近删除") && has("1 张"), "相簿 ends with 最近删除 · 1 张");
    touch("最近删除"); quiet(); CHECK(WAIT_FOR(G.data && G.data->ntrash == 1 && G.list, 5000), "最近删除 lists it"); quiet();
    look(); CHECK(has("还剩 30 天") && has("11月20日删除"), "each row: the day it was deleted and the days left"); shot("trash");
    mark("restore", "before"); area_screen(); publish("restore"); press("photo.restore");
    CHECK(WAIT_FOR(exists(was) && !exists(trashed), 5000), "restore_original"); quiet();
    CHECK(WAIT_FOR(G.data && !G.data->ntrash, 5000), "最近删除 is empty again"); quiet(); mark("restore", "after"); shot("trash_empty");
    uint32_t back = id_of(was); const ink_gallery_item_t *it = item_of(back);
    CHECK(back && it && !strcmp(it->month, "2026-09") && !strcmp(G.data->albums[it->album].path, "/me/旅行"), "restore_same_month: the same album and month (the file time did not change)");
    press("shell.back"); quiet();
}

/* ---------------------------------------------------------------------------------------------- by the AI */
static void ai_walk(void) {
    start(); open_gallery();
    ok("gallery.show", "{\"view\":\"months\"}"); quiet(); look(); CHECK(has("2026年9月"), "AI: the months");
    double n = metric("gallery.page_ready"); ok("gallery.find", "{\"month\":\"2026-09\"}"); page_ready(n);
    look(); CHECK(G.count == 9 && has("2026年9月 · 9 张"), "AI: the September filter, the same snapshot");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)G.ids[0]);
    n = metric("gallery.photo_drawn"); ok("photo.open", args); photo_drawn(n);
    look(); CHECK(!strcmp(G.path, TEXT_PNG) && has("第1/9张") && has("适合屏幕") && has("photo.zoom"), "AI: the same photo and actions");
    n = metric("gallery.photo_drawn"); ok("photo.zoom", "{\"step\":\"in\"}"); photo_drawn(n);
    for (int i = 0; i < 4; i++) { n = metric("gallery.photo_drawn"); if (AI("photo.pan", "{\"dir\":\"right\"}") && g_rc.status == INK_RC_DONE) photo_drawn(n); }
    for (int i = 0; i < 4; i++) { n = metric("gallery.photo_drawn"); if (AI("photo.pan", "{\"dir\":\"down\"}") && g_rc.status == INK_RC_DONE) photo_drawn(n); }
    on_ui(check_text); look(); CHECK(G.v.zoom == 2 && text_visible && has("2 倍"), "AI: 2x and panned to the words (bottom right)");
    CHECK(AI("photo.pan", "{\"dir\":\"right\"}") && g_rc.status == INK_RC_INVALID, "AI: at the edge a pan says it cannot move");
    n = metric("gallery.photo_drawn"); ok("photo.zoom", "{\"step\":\"fit\"}"); photo_drawn(n);
    n = metric("gallery.photo_drawn"); ok("photo.next", NULL); photo_drawn(n); look(); CHECK(has("第2/9张"), "AI: next is 2 of 9");
    uint32_t second = G.photo;
    ok("photo.info", NULL); quiet(); look(); CHECK(has("文件时间"), "AI: details"); ok("shell.back", NULL); quiet();
    n = metric("gallery.page_ready"); ok("shell.back", NULL); page_ready(n);
    char outlined[32]; snprintf(outlined, sizeof outlined, "file:%u", (unsigned)second);
    look(); CHECK(!strcmp(G.position.selected, outlined) && has(outlined), "AI: back on the same page, the cell outlined");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)G.ids[2]);
    n = metric("gallery.photo_drawn"); ok("photo.open", args); photo_drawn(n);
    char was[INK_FS_PATH_MAX]; snprintf(was, sizeof was, "%s", G.path);
    n = metric("gallery.photo_drawn"); ok("photo.trash", NULL); photo_drawn(n); on_ui(read_undo);
    ink_confirm_view_t card; look();
    CHECK(ink_confirm_list(&card, 1) == 0 && !exists(was) && exists(trashed) && has("已移到最近删除"), "AI: trash without a card");
    ok("shell.back", NULL); quiet(); ok("gallery.show", "{\"view\":\"trash\"}"); CHECK(WAIT_FOR(G.data && G.data->ntrash == 1 && G.list, 5000), "AI: 最近删除"); quiet();
    look(); CHECK(has("photo.restore") && has(trashed), "AI: the trash snapshot names the photo and how to restore it");
    char path_args[INK_FS_PATH_MAX + 16]; snprintf(path_args, sizeof path_args, "{\"path\":\"%s\"}", trashed);
    ok("photo.restore", path_args); CHECK(exists(was) && !exists(trashed), "AI: restored where it was");
    uint32_t back = id_of(was); CHECK(WAIT_FOR(item_of(back) != NULL, 5000), "AI: listed again");
    const ink_gallery_item_t *it = item_of(back);
    CHECK(it && !strcmp(it->month, "2026-09") && !strcmp(G.data->albums[it->album].path, "/me/旅行"), "AI: same album and month");
}

/* ---------------------------------------------------------------------------------------------- the child */
static void kid(void) {
    start();
    char adult[48]; snprintf(adult, sizeof adult, "{\"id\":%u}", (unsigned)id_of(TEXT_PNG));
    CHECK(ink_member_switch("kid") == 0, "the child's space"); quiet(); strcpy(size_text, "kid");
    open_gallery(); look(); CHECK(G.count == 1 && !has("旅行") && !has("字条"), "kid_isolation: only the child's photo"); shot("all");
    CHECK(AI("photo.open", adult) && g_rc.status == INK_RC_REJECTED, "the child cannot open an adult photo by its number");
    touch("相簿"); quiet(); look(); CHECK(has("照片") && !has("旅行"), "only the child's albums"); shot("albums");
    touch("日期"); quiet(); shot("months");
    touch("全部"); double n = metric("gallery.photo_drawn"); quiet();
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)G.ids[0]); ok("photo.open", args); photo_drawn(n); shot("photo_fit");
    ok("photo.info", NULL); quiet(); shot("info_png"); ok("shell.back", NULL); quiet();
    n = metric("gallery.page_ready"); ok("photo.trash", NULL); quiet(); on_ui(read_undo);
    CHECK(exists("/me/最近删除/2026-11-20/照片/kid-1.png") && !exists("/me/照片/kid-1.png"), "the child's own 最近删除");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "Gallery"), 5000), "the last photo gone: back to the list"); quiet();
    ok("gallery.show", "{\"view\":\"trash\"}"); CHECK(WAIT_FOR(G.data && G.data->ntrash == 1, 5000), "the child sees only its own deletion"); quiet(); shot("trash");
    CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet(); strcpy(size_text, "1");
    open_gallery(); ok("gallery.show", "{\"view\":\"albums\"}"); quiet(); look(); CHECK(!has("最近删除"), "the adult does not see the child's 最近删除");
}

/* ---------------------------------------------------------------------------------------------- the rest */
static void open_photo(const char *path) { char a[48]; snprintf(a, sizeof a, "{\"id\":%u}", (unsigned)id_of(path)); double n = metric("gallery.photo_drawn"); ok("photo.open", a); photo_drawn(n); }
static void cell_of(const char *path) { /* the grid cell that shows path */
    found = false; uint32_t id = id_of(path);
    for (unsigned i = 0; i < PHOTO_SLOTS && G.slots[i]; i++)
        if (G.ids[G.position.page * PHOTO_SLOTS + i] == id) { lv_obj_get_coords(G.slots[i], &box); found = true; }
}
static bool cell_says(void) { /* the cell holds the "看不了预览" label */
    for (unsigned i = 0; i < PHOTO_SLOTS && G.slots[i]; i++) {
        lv_area_t a; lv_obj_get_coords(G.slots[i], &a);
        if (a.x1 == box.x1 && a.y1 == box.y1) for (uint32_t k = 0; k < lv_obj_get_child_count(G.slots[i]); k++) {
            lv_obj_t *c = lv_obj_get_child(G.slots[i], (int32_t)k);
            if (lv_obj_check_type(c, &lv_label_class) && strstr(lv_label_get_text(c), "看不了预览")) return true;
        }
    }
    return false;
}
static bool unviewable;
static void inspect_cell(void) { cell_of(BIG_JPG); unviewable = found && cell_says(); }
static void index_state(void *arg) { ink_state_set_str(ink_state_find("file.index_state"), arg); }
static void no_card(void *arg) { (void)arg; sim_sd_set_present(false); }
static void card_back(void *arg) { (void)arg; sim_sd_set_present(true); }
static const char *LONG_PNG = "/me/长路径/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.png";
static const char *const NEW_CAPS[] = {"gallery.show", "gallery.find", "photo.next", "photo.prev", "photo.info", "photo.zoom", "photo.pan",
                                        "photo.rotate", "photo.trash", "photo.restore", "photo.delete_forever", "gallery.empty_trash"};
static void more(void) {
    start(); open_gallery();
    on_ui(inspect_cell); CHECK(unviewable, "cell_unviewable: the 1600 x 1200 photo's cell says 看不了预览"); shot("cell_unviewable");
    open_photo(BIG_JPG); look(); CHECK(has("太大（1600×1200）"), "too_big_message (%.80s)", G.caption); shot("photo_too_big");
    open_photo(WEBP); look(); CHECK(find_text("这种格式（WebP）还看不了"), "webp_message"); ok("shell.back", NULL); quiet();
    open_photo(TALL_JPG); look();
    CHECK(G.info.width == 200 && G.info.height == 300 && G.dw == 200 && G.dh == 300, "exif_orientation: the tall photo stands upright (%u x %u)", G.info.width, G.info.height);
    ok("photo.info", NULL); quiet(); look();
    CHECK(has("2026年11月2日 10:20") && !has("文件时间") && has("200×300") && has("照片") && has("JPEG") && has("KB"), "info_exif_date: EXIF time, upright size, size, album");
    shot("info_jpeg"); ok("shell.back", NULL); quiet(); ok("shell.back", NULL); quiet();
    open_photo("/me/照片/坏的.png"); ok("photo.info", NULL); quiet(); look();
    CHECK(!has("尺寸") && has("文件时间") && has("名字") && has("大小"), "info_probe_fail: what is known, no size row, no empty line"); shot("info_missing");
    ok("shell.back", NULL); quiet(); ok("shell.back", NULL); quiet();
    /* long path: only delete forever, and that asks */
    open_photo(LONG_PNG); CHECK(AI("photo.trash", NULL) && g_rc.status == INK_RC_FAILED, "a path too long for 最近删除 is refused"); quiet(); look();
    CHECK(exists(LONG_PNG) && has("路径太长") && has("photo.delete_forever"), "trash_long_path: the reason, and only 永久删除");
    shot("photo_long_path");
    ink_confirm_view_t card;
    press("photo.delete_forever"); CHECK(ink_confirm_list(&card, 1) == 1 && strstr(card.detail, "不能恢复"), "delete_forever_card says it cannot be undone");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "cancel"); quiet(); CHECK(exists(LONG_PNG), "cancelled: the photo stays");
    ok("shell.back", NULL); quiet();
    /* a book arrives: nothing is redrawn */
    ok("system.home", NULL); quiet(); open_gallery(); on_ui(area_empty); publish("book"); mark("book", "before");
    CHECK(ink_fs_mkdir("/me/书") == 0 && ink_fs_write_atomic("/me/书/合成.txt", "synthetic", 9) == 0, "a book arrives"); delay_ms(1500); quiet(); mark("book", "after");
    /* trash two, empty asks, cancel keeps */
    open_photo("/me/照片/nov-01.png"); ok("photo.trash", NULL); quiet(); ok("shell.back", NULL); quiet();
    ok("gallery.show", "{\"view\":\"trash\"}"); CHECK(WAIT_FOR(G.data && G.data->ntrash == 1, 5000), "one in 最近删除"); quiet();
    press("gallery.empty_trash"); CHECK(ink_confirm_list(&card, 1) == 1 && strstr(card.detail, "不能恢复"), "empty_trash_card");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "cancel emptying"); quiet(); CHECK(G.data->ntrash == 1, "cancelled: still there");
    press("gallery.empty_trash"); CHECK(ink_confirm_list(&card, 1) == 1 && ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "empty it");
    CHECK(WAIT_FOR(G.data && !G.data->ntrash && !G.list, 8000), "emptied"); quiet();
    ok("shell.back", NULL); quiet();
    /* the card goes and comes back */
    vtk_event_at(vtk_now() + SIM_MS(1), no_card, NULL);
    CHECK(WAIT_FOR(G.data && G.data->err == INK_FS_ERR_NO_CARD && !G.list, 8000), "no card: said, not an empty album"); quiet();
    CHECK(find_text("没插存储卡") && find_cap("gallery.reload"), "no_card: one sentence and 重读照片"); shot("no_card");
    double n = metric("gallery.page_ready"); vtk_event_at(vtk_now() + SIM_MS(1), card_back, NULL);
    CHECK(WAIT_FOR(G.data && !G.data->err && G.list, 10000), "card_back_auto: back without a button"); page_ready(n);
    /* lent to a computer */
    hw(HW_USB_ON, 0, 0); delay_ms(300);
    CHECK(ink_usb_disk_start() == 0, "lend the card");
    CHECK(WAIT_FOR(G.data && G.data->err == INK_FS_ERR_USB, 8000), "usb_lent: said"); quiet(); CHECK(find_text("拔线或在电脑上推出"), "usb_lent sentence"); shot("usb_lent");
    CHECK(ink_usb_disk_stop() == 0, "take it back"); CHECK(WAIT_FOR(G.data && !G.data->err && G.list, 10000), "back by itself"); quiet();
    hw(HW_USB_OFF, 0, 0); delay_ms(300); quiet();
    CHECK(!strcmp(read_error(INK_FS_ERR_INDEX_FULL), TR("文件太多，有些照片没列出来")), "index full has its sentence");
    /* locked: every new capability waits for the owner */
    CHECK(BY_HAND("shell.lock", NULL) && g_rc.status == INK_RC_DONE, "lock"); quiet();
    for (unsigned i = 0; i < sizeof NEW_CAPS / sizeof *NEW_CAPS; i++)
        for (ink_source_t s = INK_SRC_AI; s <= INK_SRC_REMOTE; s++) if (s != INK_SRC_SYSTEM)
            CHECK(ink_cap_find(NEW_CAPS[i])->locked_owner_only && call_as(NEW_CAPS[i], NULL, s) &&
                  (g_rc.status == INK_RC_REJECTED || (ink_cap_find(NEW_CAPS[i])->activity && g_rc.status == INK_RC_INVALID)),
                  "locked_caps: %s from source %d is refused (%s; a page-bound one is not even there under the lock)", NEW_CAPS[i], (int)s, ink_rc_name(g_rc.status));
}
static void empty(void) {
    start(); open_gallery(); look(); CHECK(G.count == 0 && find_text("还没有照片"), "empty: how to take one"); shot("empty");
    ok("system.home", NULL); quiet();
    index_state("organizing"); delay_ms(50); /* as the file service says it while it builds the index */
    ok("app.open", "{\"id\":\"gallery\"}"); CHECK(WAIT_FOR(find_text("正在整理照片"), 8000), "index_not_ready: said, no spinner"); quiet(); shot("index_not_ready");
    index_state("ready");
    CHECK(WAIT_FOR(find_text("还没有照片"), 8000), "ready: read again by itself");
}

/* ---------------------------------------------------------------------------------------------- 30 days */
static void purge(void) {
    start(); open_gallery();
    open_photo("/me/照片/nov-01.png"); ok("photo.trash", NULL); quiet(); on_ui(read_undo); char old[INK_FS_PATH_MAX]; snprintf(old, sizeof old, "%s", trashed);
    ok("shell.back", NULL); quiet(); ok("system.home", NULL); quiet();
    ink_time_set(T0 + 2 * 86400, false);
    open_gallery(); open_photo("/me/照片/nov-02.png"); ok("photo.trash", NULL); quiet(); on_ui(read_undo); char young[INK_FS_PATH_MAX]; snprintf(young, sizeof young, "%s", trashed);
    untrusted = true;
    open_photo("/me/照片/nov-03.png"); ok("photo.trash", NULL); quiet(); on_ui(read_undo); char undated[INK_FS_PATH_MAX]; snprintf(undated, sizeof undated, "%s", trashed);
    CHECK(strstr(undated, "/最近删除/日期不详/") != NULL, "deleted while the clock is untrusted: 日期不详 (%s)", undated);
    ok("shell.back", NULL); quiet(); ok("system.home", NULL); quiet();
    ink_time_set(T0 + 31 * 86400, false); untrusted = true;
    open_gallery(); delay_ms(3000); quiet();
    CHECK(exists(old) && exists(young) && exists(undated), "purge_time_invalid: nothing goes while the clock is untrusted");
    ok("system.home", NULL); quiet(); untrusted = false;
    open_gallery(); CHECK(WAIT_FOR(!exists(old), 8000), "purge_31_29: deleted 31 days ago goes"); delay_ms(2000); quiet();
    CHECK(exists(young) && exists(undated), "deleted 29 days ago stays, 日期不详 never goes");
}

static void run(void *arg) {
    (void)arg;
    if (!strcmp(phase, "walk")) walk();
    else if (!strcmp(phase, "ai")) ai_walk();
    else if (!strcmp(phase, "kid")) kid();
    else if (!strcmp(phase, "more")) more();
    else if (!strcmp(phase, "purge")) purge();
    else empty();
    ok("system.home", NULL); ink_db_flush_all();
    ink_probe_count("test.finished");
}
static void host_options(void *arg) {
    (void)arg; const char *p = getenv("INK_G3_PHASE"), *s = getenv("INK_G3_SIZE"), *b = getenv("INK_G3_BATCH_US");
    snprintf(phase, sizeof phase, "%s", p ? p : "walk"); snprintf(size_text, sizeof size_text, "%s", s ? s : "1");
    if (b) gallery_batch_us = strtoull(b, NULL, 10);
}
void ink_main(void) {
    vtk_host_call(host_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "gallery-3", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the gallery-3 driver");
}
