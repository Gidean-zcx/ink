/* Lock styles an app draws (lock-styles-1): the client table, the two canvases and what changed between them. */
/* ui-scale-runtime: dimension x0,y0,num,den,x,y,x1,y1,x2,y2 Canvas pixels the client drew, mapped to the panel's own pixels. */
/* ui-scale-runtime: dimension pct The system text size in percent (90..120), as the reader takes it. */
#include <stdio.h>
#include <string.h>
#include "faces.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/exec.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"

enum { RECTS = 8, MERGE_ROWS = 16, BYTES = INK_LOCK_CANVAS_W * INK_LOCK_CANVAS_H };
_Static_assert(INK_LOCK_FACE_COUNT - INK_LOCK_DRAWN <= 16, "drawn styles fit one client table row each");
static struct {
    const ink_lock_client_t *clients[INK_LOCK_CLIENTS];
    uint8_t *front, *back;            /* front is what the image shows; back is the background executor's */
    lv_image_dsc_t dsc;
    lv_obj_t *image;
    ink_lock_request_t shown, want;   /* what front was drawn for; what back is being drawn for */
    ink_lock_drawn_t drawn, next;
    uint32_t shown_revision, want_revision;
    int select, select_style;
    bool front_ok, busy, again;
    char why[INK_LOCK_WHY_MAX], time[24], date[96], privacy[48];
    char pending_cap[32], pending_args[INK_LOCK_PATH_MAX + 16], pending_owner[16];
    ink_exec_job_t job, done, after;
    void (*changed)(int how);
} D INK_PSRAM_BSS;

static const ink_lock_client_t *client(int style) {
    for (int i = 0; i < INK_LOCK_CLIENTS; i++)
        if (D.clients[i] && style >= D.clients[i]->first && style < D.clients[i]->first + D.clients[i]->count) return D.clients[i];
    return NULL;
}
int ink_lock_face_register(const ink_lock_client_t *c) {
    if (!c || !c->name || strlen(c->name) >= INK_LOCK_NAME_MAX || !c->draw || c->first < INK_LOCK_DRAWN ||
        c->count < 1 || c->first + c->count > INK_LOCK_FACE_COUNT) return -1;
    for (int i = 0; i < INK_LOCK_CLIENTS; i++)
        if (!D.clients[i]) { D.clients[i] = c; return 0; }
    return -1;
}
void ink_lock_face_hook(void (*changed)(int how)) { D.changed = changed; }
static void tell(int how) { if (D.changed) D.changed(how); }
/* Books and covers are a member's own: those styles depend on whose space is open; clocks and calendars do not. */
static bool personal(int style) { return style == INK_LOCK_TICKET || style == INK_LOCK_COLLAGE || style == INK_LOCK_LIBRARY; }
static int text_px(int pct) { return ink_ui_px(pct); }
static void request(int style, ink_lock_request_t *r) {
    memset(r, 0, sizeof *r);
    r->style = style;
    r->option = style == INK_LOCK_TICKET ? (int)ink_setting_get("display.lock_ticket_bg") :
                style == INK_LOCK_LIBRARY ? (int)ink_setting_get("display.lock_library") : 0;
    ink_member_identity(&r->owner);
    r->child = ink_member_current()->role == INK_ROLE_CHILD;
    r->open = ink_member_space_open();
    static const int percent[] = {90, 100, 110, 120};
    int level = r->child ? 2 : (int)ink_setting_get("display.font_size");
    r->px = text_px(percent[level < 0 ? 0 : level > 3 ? 3 : level]); /* as the reader sets it (apps/mogo/mogo.c) */
    if (D.select_style != style) { D.select = 0; D.select_style = style; }
    r->select = D.select;
    r->clock = ink_time_valid();
    r->zone = (int)ink_setting_get("time.zone");
    ink_time_to_local(ink_time_now_s(), &r->now);
}
static uint32_t revision(int style) { const ink_lock_client_t *c = client(style); return c && c->revision ? c->revision(style) : 0; }
static bool same(const ink_lock_request_t *a, const ink_lock_request_t *b, bool minute) {
    if (a->style != b->style || a->option != b->option || a->select != b->select || a->px != b->px || a->zone != b->zone || a->child != b->child ||
        strcmp(a->owner.member, b->owner.member) || a->clock != b->clock || a->now.year != b->now.year ||
        a->now.month != b->now.month || a->now.day != b->now.day) return false;
    if (personal(a->style) && (a->open != b->open || a->owner.epoch != b->owner.epoch)) return false;
    return !minute || (a->now.hour == b->now.hour && a->now.min == b->now.min);
}
static bool has_minute(const ink_lock_drawn_t *d) { return d->minute.x2 >= d->minute.x1 && d->minute.y2 >= d->minute.y1; }

static void rendered(void *arg);
static void render(void *arg) { /* BG */
    (void)arg;
    const ink_lock_client_t *c = client(D.want.style);
    memset(&D.next, 0, sizeof D.next);
    D.next.minute = (lv_area_t){1, 1, 0, 0};
    bool ok = c && c->draw(&D.want, D.back, &D.next);
    if (!ok && !D.next.why[0]) snprintf(D.next.why, sizeof D.next.why, "%s", c ? TR("这个锁屏样式没画出来，先显示默认锁屏") : TR("这个锁屏样式在这台设备上用不了"));
    if (ok) D.next.why[0] = 0;
    ink_exec_post(INK_EXEC_UI, &D.done, rendered, (void *)(uintptr_t)ok);
}
static void start(int style) {
    if (D.busy) { D.again = true; return; }
    if (!D.front) D.front = osal_malloc(BYTES, OSAL_MEM_EXTERNAL);
    if (!D.back) D.back = osal_malloc(BYTES, OSAL_MEM_EXTERNAL);
    if (!D.front || !D.back) { snprintf(D.why, sizeof D.why, "%s", TR("内存不足")); tell(2); return; }
    request(style, &D.want);
    D.want_revision = revision(style);
    memset(D.back, 255, BYTES);
    if (style == INK_LOCK_TICKET && D.want.option == 1) ink_wallpaper_fill(D.back, INK_LOCK_CANVAS_W, INK_LOCK_CANVAS_H);
    D.busy = true; D.again = false;
    if (ink_exec_post(INK_EXEC_BG, &D.job, render, NULL)) D.busy = false;
}
static int locked_style(void) {
    int style = (int)ink_setting_get("display.lock_style");
    return ink_power_state() == INK_PWR_LOCKED && style >= INK_LOCK_DRAWN ? style : -1;
}
/* Rows that differ, as at most RECTS rectangles: what the panel has to change, and only that. */
static int differ(lv_area_t *out) {
    int n = 0;
    for (int y = 0; y < INK_LOCK_CANVAS_H; y++) {
        const uint8_t *a = D.front + (size_t)y * INK_LOCK_CANVAS_W, *b = D.back + (size_t)y * INK_LOCK_CANVAS_W;
        if (!memcmp(a, b, INK_LOCK_CANVAS_W)) continue;
        int x1 = 0, x2 = INK_LOCK_CANVAS_W - 1;
        while (a[x1] == b[x1]) x1++;
        while (a[x2] == b[x2]) x2--;
        if (n && y - out[n - 1].y2 <= MERGE_ROWS) {
            lv_area_t *r = &out[n - 1];
            r->y2 = y; if (x1 < r->x1) r->x1 = x1; if (x2 > r->x2) r->x2 = x2;
        } else if (n < RECTS) out[n++] = (lv_area_t){x1, y, x2, y};
        else {
            lv_area_t *r = &out[n - 1];
            r->y2 = y; if (x1 < r->x1) r->x1 = x1; if (x2 > r->x2) r->x2 = x2;
        }
    }
    return n;
}
/* The canvas is Mogo's whole page; here it sits under the lock's status bar, scaled down to fit (the bar carries the
 * emergency call and the camera, which Mogo's lock has not). Canvas rectangles map to the panel through this. */
static void place(int *x0, int *y0, int *num, int *den) {
    *y0 = ink_ui_px(INK_STATUS_BAR); *den = INK_LOCK_CANVAS_H; *num = ink_ui_height() - *y0;
    if (INK_LOCK_CANVAS_W * *num / *den > ink_ui_width()) { *num = ink_ui_width(); *den = INK_LOCK_CANVAS_W; }
    *x0 = (ink_ui_width() - INK_LOCK_CANVAS_W * *num / *den) / 2;
}
static lv_area_t on_panel(const lv_area_t *c) {
    int x0, y0, num, den; place(&x0, &y0, &num, &den);
    lv_area_t a = {x0 + c->x1 * num / den - 2, y0 + c->y1 * num / den - 2, x0 + (c->x2 + 1) * num / den + 2, y0 + (c->y2 + 1) * num / den + 2};
    if (a.x1 < 0) a.x1 = 0;
    if (a.y1 < y0) a.y1 = y0;
    if (a.x2 >= ink_ui_width()) a.x2 = ink_ui_width() - 1;
    if (a.y2 >= ink_ui_height()) a.y2 = ink_ui_height() - 1;
    return a;
}
static void bind_image(void) {
    D.dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8, .w = INK_LOCK_CANVAS_W,
        .h = INK_LOCK_CANVAS_H, .stride = INK_LOCK_CANVAS_W}, .data_size = BYTES, .data = D.front};
}
static void shown_job(void *arg) { /* BG */
    const ink_lock_client_t *c = client((int)(intptr_t)arg);
    if (c && c->shown) c->shown((int)(intptr_t)arg, D.front);
}
static void rendered(void *arg) { /* UI */
    bool ok = (uintptr_t)arg != 0;
    ink_probe_count("lock.drawn.renders");
    D.busy = false;
    int style = locked_style();
    ink_lock_request_t now; if (style >= 0) request(style, &now);
    bool current = style == D.want.style && same(&now, &D.want, true);
    if (!ok) {
        snprintf(D.why, sizeof D.why, "%s", D.next.why);
        if (current || style == D.want.style) {
            bool was = D.front_ok && D.image;
            D.front_ok = false;
            tell(was ? 1 : 2);
        }
    } else if (D.image && D.front_ok && D.shown.style == D.want.style) { /* on the panel: change what differs */
        lv_area_t rects[RECTS]; int n = differ(rects);
        for (int i = 0; i < n; i++) {
            for (int y = rects[i].y1; y <= rects[i].y2; y++)
                memcpy(D.front + (size_t)y * INK_LOCK_CANVAS_W + rects[i].x1, D.back + (size_t)y * INK_LOCK_CANVAS_W + rects[i].x1,
                       (size_t)(rects[i].x2 - rects[i].x1 + 1));
            lv_area_t panel = on_panel(&rects[i]);
            lv_obj_invalidate_area(D.image, &panel);
            ink_ui_hint_area(&panel, INK_REFRESH_TEXT);
            ink_probe_count("lock.drawn.areas"); /* what reached the panel, for the refresh budget and its test */
            ink_probe_value("lock.drawn.x1", rects[i].x1); ink_probe_value("lock.drawn.y1", rects[i].y1);
            ink_probe_value("lock.drawn.x2", rects[i].x2); ink_probe_value("lock.drawn.y2", rects[i].y2);
        }
        ink_probe_count(n ? "lock.drawn.changed" : "lock.drawn.same");
        D.shown = D.want; D.shown_revision = D.want_revision; D.drawn = D.next; D.why[0] = 0;
        if (n) tell(0);
    } else { /* not on the panel yet: the default lock gives way to it in one refresh */
        uint8_t *t = D.front; D.front = D.back; D.back = t; bind_image();
        D.shown = D.want; D.shown_revision = D.want_revision; D.drawn = D.next; D.front_ok = true; D.why[0] = 0;
        if (style == D.want.style) { ink_probe_count("lock.drawn.whole"); tell(1); }
    }
    if (ok && D.image) ink_exec_post(INK_EXEC_BG, &D.after, shown_job, (void *)(intptr_t)D.shown.style);
    if (style >= 0 && (!current || D.again)) start(style);
}
bool ink_lock_face_ready(int style) {
    if (style < INK_LOCK_DRAWN) return true;
    ink_lock_request_t now; request(style, &now);
    if (D.front_ok && D.shown.style == style && same(&now, &D.shown, false) && D.shown_revision == revision(style)) {
        if (same(&now, &D.shown, true) || !has_minute(&D.drawn)) return true;
        /* the minute moved since: the style shows at once without the old time, and the time follows in its own area */
        for (int y = D.drawn.minute.y1; y <= D.drawn.minute.y2; y++)
            memset(D.front + (size_t)y * INK_LOCK_CANVAS_W + D.drawn.minute.x1, 255, (size_t)(D.drawn.minute.x2 - D.drawn.minute.x1 + 1));
        start(style);
        return true;
    }
    if (!D.busy || D.want.style != style) start(style);
    return false;
}
const char *ink_lock_face_why(void) {
    return ink_setting_get("display.lock_style") >= INK_LOCK_DRAWN && D.why[0] ? D.why : NULL;
}
static void tapped(lv_event_t *e) {
    lv_event_stop_processing(e);
    const ink_lock_client_t *c = client(D.shown.style);
    lv_indev_t *in = lv_indev_active();
    if (!c || !c->tap || !in || strcmp(D.shown.owner.member, ink_member_current()->id)) return;
    lv_point_t p; lv_indev_get_point(in, &p);
    int x0, y0, num, den; place(&x0, &y0, &num, &den);
    p.x = (p.x - x0) * den / num; p.y = (p.y - y0) * den / num; /* back to the canvas Mogo's hit test knows */
    int index = 0;
    char cap[sizeof D.pending_cap] = "", args[sizeof D.pending_args] = "";
    ink_lock_tap_t t = c->tap(D.shown.style, p.x, p.y, &index, cap, sizeof cap, args, sizeof args);
    if (t == INK_LOCK_TAP_SELECT && index != D.select) { D.select = index; start(D.shown.style); }
    else if (t == INK_LOCK_TAP_OPEN && cap[0]) { /* the same path as the AI after an unlock: the lock rules first (D-95) */
        snprintf(D.pending_cap, sizeof D.pending_cap, "%s", cap); snprintf(D.pending_args, sizeof D.pending_args, "%s", args);
        snprintf(D.pending_owner, sizeof D.pending_owner, "%s", D.shown.owner.member);
        ink_power_unlock(ink_now_us());
    }
}
void ink_lock_drawn_draw(lv_obj_t *parent) {
    int x0, y0, num, den; place(&x0, &y0, &num, &den);
    D.image = lv_image_create(parent);
    lv_image_set_src(D.image, &D.dsc);
    lv_obj_set_pos(D.image, 0, y0);
    lv_obj_set_size(D.image, ink_ui_width(), ink_ui_height() - y0);
    lv_image_set_inner_align(D.image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_antialias(D.image, true);
    lv_obj_add_flag(D.image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(D.image, tapped, LV_EVENT_SHORT_CLICKED, NULL);
}
void ink_lock_drawn_clear(void) {
    if (D.image) lv_obj_delete(D.image);
    D.image = NULL;
}
bool ink_lock_drawn_update(int style) {
    if (style < INK_LOCK_DRAWN) return false;
    ink_lock_request_t now; request(style, &now);
    if (D.busy || (D.front_ok && same(&now, &D.shown, has_minute(&D.drawn)) && D.shown_revision == revision(style))) return false;
    start(style); /* a new minute, day or zone: drawn again, and only what differs reaches the panel */
    return false;
}
void ink_lock_drawn_resume(void) {
    if (!D.pending_cap[0]) return;
    if (strcmp(D.pending_owner, ink_member_current()->id)) { D.pending_cap[0] = 0; return; }
    if (ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open()) return;
    char cap[sizeof D.pending_cap]; snprintf(cap, sizeof cap, "%s", D.pending_cap); D.pending_cap[0] = 0;
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .args_json = D.pending_args};
    ink_cap_call(cap, NULL, &opts);
}
size_t ink_lock_drawn_fields(ink_ui_field_spec_t *fields) {
    if (!D.image || !D.front_ok) return 0;
    size_t n = 0;
    ink_shell_time_strings(D.time, sizeof D.time, D.date, sizeof D.date);
    fields[n++] = (ink_ui_field_spec_t){.id = "time", .role = "text", .label = TR("时间"), .value = D.time};
    fields[n++] = (ink_ui_field_spec_t){.id = "date", .role = "text", .label = TR("日期"), .value = D.date};
    fields[n++] = (ink_ui_field_spec_t){.id = "shows", .role = "text", .label = TR("画面上写着"), .value = D.drawn.says};
    if (personal(D.shown.style) && !D.shown.open) {
        snprintf(D.privacy, sizeof D.privacy, "%s", TR("解锁后显示"));
        fields[n++] = (ink_ui_field_spec_t){.id = "privacy", .role = "text", .label = TR("读书内容"), .value = D.privacy};
    }
    return n;
}
