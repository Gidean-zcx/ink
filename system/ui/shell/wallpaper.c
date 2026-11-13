/* Decode on the media executor; keep only dithered pixels in PSRAM. */
/* ui-scale-runtime: dimension x,y,w,h Theme-sized preview rows and widget controls. */
/* ui-scale-runtime: dimension room,used Page room in canvas pixels, from the box the page gave the list. */
#include "ink/sys/i18n.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/image.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "../lock_faces/faces.h"

/* The style list (lock-styles-1): shared list rows in pages, one line each saying what the style shows, the chosen one
 * ticked; a style's own setting appears under the list only when that style is chosen. No pretend previews. */
enum { OPTION_ROWS = 2, TOGGLES = 3, FACE_SETTINGS_MAX = INK_LOCK_FACE_COUNT + OPTION_ROWS + TOGGLES + 1, FACE_ITEMS = FACE_SETTINGS_MAX };
static const char *const TICKET_BG[] = {TR_NOOP("封面"), TR_NOOP("壁纸"), NULL};
static const char *const LIBRARY_MODE[] = {TR_NOOP("最近"), TR_NOOP("随机"), NULL};
static const char *const TOGGLE_IDS[TOGGLES] = {"display.lock_todo", "display.lock_reading", "display.lock_background"};
static const char *const TOGGLE_TITLES[TOGGLES] = {TR_NOOP("锁屏显示待办"), TR_NOOP("锁屏继续读"), TR_NOOP("表盘叠在壁纸上")};
static const char *const FACE_ICONS[] = {"bell", "clock", "gallery", "clock", "moon", "calendar", "bookmark", "grid", "clock", "news", "calendar", "book"};
_Static_assert(sizeof FACE_ICONS / sizeof *FACE_ICONS == INK_LOCK_FACE_COUNT, "one icon a style");
static struct {
    ink_ui_field_spec_t fields[FACE_SETTINGS_MAX];
    ink_ui_action_spec_t actions[FACE_SETTINGS_MAX];
    char args[FACE_SETTINGS_MAX][96], option_args[2][96], page_text[32], page_args[2][48];
    ink_ui_page_spec_t model;
    int page, pages;
} P INK_PSRAM_BSS;

static const char *const *option_choices(int i) { return i == 0 ? TICKET_BG : LIBRARY_MODE; }
static const char *option_id(int i) { return i == 0 ? "display.lock_ticket_bg" : "display.lock_library"; }
static const char *option_title(int i) { return i == 0 ? TR("票根背景") : TR("迷你书库显示"); }
static bool option_shown(int i) { return ink_setting_get("display.lock_style") == (i == 0 ? INK_LOCK_TICKET : INK_LOCK_LIBRARY); }
/* What the list holds, in order: the styles, the chosen style's own setting, then the switches of the older faces. */
static int items(int *kind, int *index) {
    int n = 0;
    for (int i = 0; i < INK_LOCK_FACE_COUNT; i++) { kind[n] = 0; index[n++] = i; }
    for (int i = 0; i < OPTION_ROWS; i++) if (option_shown(i)) { kind[n] = 1; index[n++] = i; }
    for (int i = 0; i < TOGGLES; i++) { kind[n] = 2; index[n++] = i; }
    return n;
}
static int item_h(int kind) {
    return kind == 0 ? ink_ui_row_double() : kind == 1 ? ink_ui_line_height(INK_FONT_LIST) + INK_SEGMENT + INK_GAP : INK_CONTROL;
}
static void page_clicked(lv_event_t *e) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = P.page_args[lv_event_get_user_data(e) == (void *)(intptr_t)1]};
    ink_cap_call("shell.lock_faces", NULL, &o);
}
void ink_lock_face_settings(lv_obj_t *parent) {
    lv_obj_update_layout(parent);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    int room = lv_obj_get_height(parent) * INK_CANVAS_W / LV_MAX(ink_ui_width(), 1) - INK_CONTROL - INK_GAP;
    int kind[FACE_ITEMS], index[FACE_ITEMS], n = items(kind, index), first[FACE_ITEMS + 1], pages = 0;
    for (int i = 0, used = room; i < n; used += item_h(kind[i]), i++)
        if (used + item_h(kind[i]) > room) { first[pages++] = i; used = 0; }
    first[pages] = n;
    P.pages = pages;
    if (P.page >= pages) P.page = pages - 1;
    if (P.page < 0) P.page = 0;
    size_t f = 0;
    int y = 0;
    for (int i = 0; i < n; i++, f++) {
        bool here = i >= first[P.page] && i < first[P.page + 1];
        if (kind[i] == 0) {
            int s = index[i];
            const char *title = TR(ink_lock_face_choices[s]);
            bool selected = ink_setting_get("display.lock_style") == s;
            snprintf(P.args[f], sizeof P.args[f], "{\"id\":\"display.lock_style\",\"value\":%d}", s);
            if (here) {
                lv_obj_t *row = ink_ui_list_row(parent, FACE_ICONS[s], title, TR(ink_lock_face_notes[s]), INK_MARGIN, y, w, true, NULL, NULL);
                if (selected) ink_ui_icon(row, "check", INK_ICON_SMALL, INK_ICON_LINE, w - INK_READING_MARGIN, (ink_ui_row_double() - INK_ICON_SMALL) / 2, false);
                ink_shell_bind(row, "settings.set", P.args[f]);
            }
            P.fields[f] = (ink_ui_field_spec_t){.id = ink_lock_face_choices[s], .role = "option", .label = title,
                .value = selected ? TR("已选择") : TR(ink_lock_face_notes[s])};
            P.actions[f] = (ink_ui_action_spec_t){.id = ink_lock_face_choices[s], .label = title, .capability = "settings.set", .args_json = P.args[f], .enabled = true};
        } else if (kind[i] == 1) {
            int o = index[i], value = ink_setting_get(option_id(o)) ? 1 : 0;
            const char *labels[2] = {TR(option_choices(o)[0]), TR(option_choices(o)[1])};
            snprintf(P.args[f], sizeof P.args[f], "{\"id\":\"%s\",\"value\":%d}", option_id(o), !value);
            if (here) {
                ink_ui_line(parent, option_title(o), INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
                lv_obj_t *seg = ink_ui_segments(parent, labels, 2, value, INK_MARGIN, y + ink_ui_line_height(INK_FONT_LIST), w, NULL, NULL);
                for (int k = 0; seg && k < 2; k++) {
                    snprintf(P.option_args[k], sizeof P.option_args[k], "{\"id\":\"%s\",\"value\":%d}", option_id(o), k);
                    if (k != value) ink_shell_bind(lv_obj_get_child(seg, k), "settings.set", P.option_args[k]);
                }
            }
            P.fields[f] = (ink_ui_field_spec_t){.id = option_id(o), .role = "setting", .label = option_title(o), .value = labels[value]};
            P.actions[f] = (ink_ui_action_spec_t){.id = option_id(o), .label = option_title(o), .capability = "settings.set", .args_json = P.args[f], .enabled = true};
        } else {
            int t = index[i];
            bool on = ink_setting_get(TOGGLE_IDS[t]) != 0;
            snprintf(P.args[f], sizeof P.args[f], "{\"id\":\"%s\",\"value\":%d}", TOGGLE_IDS[t], !on);
            if (here) ink_shell_bind(ink_ui_toggle(parent, TR(TOGGLE_TITLES[t]), on, INK_MARGIN, y, w, NULL, NULL), "settings.set", P.args[f]);
            P.fields[f] = (ink_ui_field_spec_t){.id = TOGGLE_IDS[t], .role = "setting", .label = TR(TOGGLE_TITLES[t]), .value = on ? TR("开") : TR("关")};
            P.actions[f] = (ink_ui_action_spec_t){.id = TOGGLE_IDS[t], .label = TR(TOGGLE_TITLES[t]), .capability = "settings.set", .args_json = P.args[f], .enabled = true};
        }
        if (here) y += item_h(kind[i]);
    }
    snprintf(P.page_args[0], sizeof P.page_args[0], "{\"page\":%d}", P.page);
    snprintf(P.page_args[1], sizeof P.page_args[1], "{\"page\":%d}", P.page + 2);
    snprintf(P.page_text, sizeof P.page_text, TR("第 %d/%d 页"), P.page + 1, P.pages);
    size_t actions = f;
    P.fields[f++] = (ink_ui_field_spec_t){.id = "page", .role = "text", .label = TR("页"), .value = P.page_text};
    if (P.pages > 1) ink_ui_pagebar(parent, P.page + 1, P.pages, room + INK_GAP, page_clicked);
    P.model = (ink_ui_page_spec_t){.id = "LockFaces", .title = TR("锁屏画面"), .object = {"display.lock_style", 0}, .fields = P.fields,
        .field_count = f, .actions = P.actions, .action_count = actions};
}
static void face_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("锁屏画面"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    ink_lock_face_settings(ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false));
}
static int face_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
static void face_turn(int page) {
    if (page < 0 || page >= P.pages || page == P.page) return;
    P.page = page; ink_ui_hint_frame(INK_REFRESH_UI); ink_ui_redraw();
}
static bool face_key(ink_key_t key, bool long_press) { /* the cover's page keys turn the list */
    if (long_press || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    face_turn(P.page + (key == INK_KEY_NEXT ? 1 : -1));
    return true;
}
static const ink_activity_t FACE_PAGE = {.name = "LockFaces", .create = face_create, .snapshot = face_snapshot, .on_key = face_key};
static void face_open(ink_call_t *c) { /* {"page":n} turns the open list to page n (from 1), as the page buttons do */
    int64_t page = 0;
    bool turn = ink_args_get_int(&c->args, "page", &page);
    if (ink_ui_page_current() && !strcmp(ink_ui_page_name(), FACE_PAGE.name)) {
        if (turn) face_turn((int)page - 1);
        return;
    }
    P.page = turn && page >= 1 ? (int)page - 1 : 0;
    ink_shell_close(); ink_ui_push(&FACE_PAGE, NULL);
}
static void face_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (ink_ui_page_current() && !strcmp(ink_ui_page_name(), FACE_PAGE.name)) ink_ui_redraw();
}

typedef struct { ink_exec_job_t job; ink_image_t image; uint32_t epoch; int64_t id; } loaded_t;
static struct { ink_image_t image; lv_image_dsc_t dsc; ink_exec_job_t load; uint32_t epoch; } W INK_PSRAM_BSS;
bool ink_wallpaper_ready(void) { return W.image.pixels && W.epoch == ink_member_epoch(); }
void ink_wallpaper_draw(lv_obj_t *parent) {
    if (!ink_wallpaper_ready()) return;
    lv_obj_t *im = lv_image_create(parent); lv_image_set_src(im, &W.dsc);
    lv_obj_set_size(im, ink_ui_px(INK_CANVAS_W), ink_ui_px(INK_CANVAS_H));
    lv_image_set_inner_align(im, LV_IMAGE_ALIGN_COVER); lv_obj_move_background(im);
}
bool ink_wallpaper_fill(uint8_t *canvas, int w, int h) {
    if (!ink_wallpaper_ready() || !canvas || w <= 0 || h <= 0) return false;
    unsigned sw = W.image.width, sh = W.image.height, cw = sw, ch = sh, cx = 0, cy = 0; /* cover, as the image object does */
    if ((uint64_t)sw * (unsigned)h > (uint64_t)sh * (unsigned)w) { cw = (unsigned)((uint64_t)sh * (unsigned)w / (unsigned)h); cx = (sw - cw) / 2; }
    else { ch = (unsigned)((uint64_t)sw * (unsigned)h / (unsigned)w); cy = (sh - ch) / 2; }
    for (int y = 0; y < h; y++) {
        const uint8_t *row = W.image.pixels + (size_t)(cy + (unsigned)y * ch / (unsigned)h) * sw + cx;
        for (int x = 0; x < w; x++) canvas[(size_t)y * (unsigned)w + (unsigned)x] = row[(unsigned)x * cw / (unsigned)w];
    }
    return true;
}
static void install(void *arg) {
    loaded_t *result = arg;
    if (result->epoch != ink_member_epoch() || result->id != ink_setting_get("display.wallpaper_file")) {
        ink_image_free(&result->image); osal_free(result); return;
    }
    ink_image_free(&W.image); W.image = result->image; W.epoch = result->epoch; osal_free(result);
    W.dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8,
        .w = W.image.width, .h = W.image.height, .stride = W.image.width},
        .data_size = W.image.width * W.image.height, .data = W.image.pixels};
    ink_ui_redraw();
}
static void load(void *arg) {
    (void)arg;
    uint32_t epoch = ink_member_epoch(); int64_t id = ink_setting_get("display.wallpaper_file");
    ink_fs_entry_t e; ink_fs_file_t *f = NULL; void *bytes = NULL; ink_image_t image = {0};
    if (id && !ink_fs_lookup((uint32_t)id, &e) && e.type == INK_FS_IMAGE && e.size > 0 && e.size <= INK_IMAGE_MAX_INPUT &&
        (bytes = osal_malloc((size_t)e.size, OSAL_MEM_EXTERNAL)) && !ink_fs_open(e.path, INK_FS_READ, &f)) {
        if (ink_fs_read(f, bytes, (size_t)e.size) == e.size && !ink_image_decode(bytes, (size_t)e.size, &image)) ink_image_dither(&image);
        ink_fs_close(f);
    }
    osal_free(bytes);
    if (epoch != ink_member_epoch()) { ink_image_free(&image); return; }
    loaded_t *result = osal_calloc(1, sizeof *result, OSAL_MEM_EXTERNAL);
    if (!result) { ink_image_free(&image); return; }
    result->image = image; result->epoch = epoch; result->id = id;
    if (ink_exec_post(INK_EXEC_UI, &result->job, install, result)) { ink_image_free(&result->image); osal_free(result); }
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_setting_get("display.wallpaper_file") && !W.image.pixels) return;
    ink_exec_post(INK_EXEC_MEDIA, &W.load, load, NULL);
}
static void selected(void *ctx, uint32_t id) {
    if (!id) return;
    ink_args_t args; ink_args_init(&args); ink_args_str(&args, "id", "display.wallpaper_file"); ink_args_int(&args, "value", id);
    ink_call_opts_t o = {.src = (ink_source_t)(uintptr_t)ctx}; ink_cap_call("settings.set", &args, &o);
}
static void choose(ink_call_t *c) {
    if (ink_file_picker_open(INK_FS_IMAGE, selected, (void *)(uintptr_t)c->src)) c->rc.status = INK_RC_FAILED;
}
int ink_wallpaper_init(void) {
    static const ink_setting_def_t widgets[] = {
        {.id = "display.lock_todo", .title = TR_NOOP("锁屏显示待办"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true},
        {.id = "display.lock_reading", .title = TR_NOOP("锁屏继续读"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true},
        {.id = "display.lock_background", .title = TR_NOOP("表盘叠在壁纸上"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true},
        {.id = "display.lock_ticket_bg", .title = TR_NOOP("票根背景"), .type = INK_SET_ENUM, .choices = TICKET_BG, .per_member = true, .child_may_change = true},
        {.id = "display.lock_library", .title = TR_NOOP("迷你书库显示"), .type = INK_SET_ENUM, .choices = LIBRARY_MODE, .per_member = true, .child_may_change = true}};
    if (ink_settings_define(widgets, sizeof widgets / sizeof *widgets)) return -1;
    static const ink_setting_def_t setting = {.id = "display.wallpaper_file", .title = TR_NOOP("壁纸图片编号"), .type = INK_SET_INT, .max = UINT32_MAX};
    static const ink_cap_t cap = {.id = "wallpaper.choose", .title = "选择锁屏壁纸", .effect = INK_FX_NAVIGATE, .run = choose};
    static const ink_cap_t faces = {.id = "shell.lock_faces", .title = "选择锁屏表盘和小组件", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = face_open};
    if (ink_settings_define(&setting, 1) || ink_cap_register(&cap, 1) ||
        ink_cap_register(&faces, 1) || ink_settings_watch("display.lock_", face_changed, NULL, INK_EXEC_UI) ||
        ink_settings_watch("display.wallpaper_file", changed, NULL, INK_EXEC_UI) || ink_state_watch("member.", changed, NULL, INK_EXEC_UI)) return -1;
    changed(NULL, 0, NULL); return 0;
}
