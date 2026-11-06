/*
 * UI runtime: the one task that owns LVGL.
 *
 * Rendering path: LVGL renders logical (UI-orientation) 8-bit strips into a
 * PSRAM draw buffer -> flush rotates them into the display service's native
 * framebuffer and records dirty native rectangles with their refresh intent
 * -> at the end of the LVGL refresh the frame's regions are committed to the
 * refresh scheduler. flush_ready only means "draw buffer reusable", never
 * "visible" (foundation §4.2).
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/rotation.h"
#include "ink/sys/ai.h"
#include "ink/sys/exec.h"
#include "ink/sys/ui.h"
#include "ink/sys/textinput.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/svc.h"

#define UI_STACK 10240
#define UI_PRIO 5
#define UI_QUEUE_LEN 24
#define INPUT_BATCH_MAX UI_QUEUE_LEN /* queued input handled before the next frame is drawn */
#define STRIP_ROWS 96
#define MAX_HINTS 8
#define MAX_DIRTY 24
#define MAX_ACTIVITIES 6
#define MAX_OVERLAYS 4
#define PFIFO 16
#define INTERACTIVE_IDLE_MS 2500

typedef enum { UM_INPUT = 1, UM_CALL } um_type_t;

typedef struct {
    uint8_t type;
    uint32_t generation;
    ink_input_event_t ev;
} umsg_t;

typedef struct {
    const ink_activity_t *a;
    void *arg;
} frame_t;

static struct {
    osal_queue_t *q;
    osal_task_t *task;
    uint32_t content_version; /* ink_ui_content_version (atomic) */
    lv_display_t *disp;
    lv_indev_t *indev;
    int lw, lh, nw, nh, rot, dpi;
    uint8_t *drawbuf;
    /* pointer events waiting for LVGL's indev */
    struct {
        int16_t x, y;
        bool pressed;
    } pfifo[PFIFO];
    int ph, pn;
    lv_point_t last_pt;
    bool last_pressed;
    /* refresh intents for the frame being built */
    struct {
        lv_area_t a;
        ink_refresh_t intent;
    } hints[MAX_HINTS];
    int nhints;
    ink_refresh_t frame_intent;
    ink_region_t dirty[MAX_DIRTY];
    int ndirty;
    bool fb_locked;
    uint64_t cause_us;
    const char *cause_tag;
    /* activities */
    frame_t stack[MAX_ACTIVITIES];
    int depth;
    void (*top_hook)(const char *app);
    lv_obj_t *screen;
    ink_member_identity_t page_owner;
    const ink_ui_overlay_t *overlays[MAX_OVERLAYS];
    lv_obj_t *overlay_root[MAX_OVERLAYS];
    ink_member_identity_t overlay_owner[MAX_OVERLAYS];
    int noverlays;
    bool skip_touch; /* a finger went down before the screen changed under it: nothing until it lifts */
    volatile uint32_t generation;
    bool interactive;
    uint64_t last_input_us;
    /* busy signal for the power service (atomics): messages posted and not yet handled, and
     * whether LVGL has a refresh requested that it has not rendered yet */
    int posts;
    int kick_lost; /* a job was posted without a message (ink_ui_kick) */
    bool render_pending;
    /* a posted call taken from the queue while collecting queued input: handled first next round */
    umsg_t held;
    bool has_held;
    /* styles */
    lv_style_t st_screen, st_btn, st_btn_pr;
} UI INK_PSRAM_BSS;

/* ---------------------------------------------------------------- helpers */

int ink_dp(int dp) { return (dp * UI.dpi + 80) / 160; }
uint32_t ink_ui_generation(void) { return UI.generation; }

uint32_t ink_ui_content_version(void) { return __atomic_load_n(&UI.content_version, __ATOMIC_ACQUIRE) + 1; } /* never 0: "not page-bound" */

void ink_ui_content_changed(void) { __atomic_add_fetch(&UI.content_version, 1, __ATOMIC_ACQ_REL); }

static uint32_t tick_cb(void) { return (uint32_t)(osal_now_us() / 1000ull); }

static void log_cb(lv_log_level_t level, const char *buf) {
    (void)level;
    char line[256];
    snprintf(line, sizeof line, "lvgl: %s", buf);
    size_t n = strlen(line);
    if (n && line[n - 1] == '\n') line[n - 1] = 0;
    hal_log_write(line);
}

static hal_rect_t native_rect(const lv_area_t *a) {
    int x0, y0, x1, y1;
    ink_logical_to_native(UI.nw, UI.nh, UI.rot, a->x1, a->y1, &x0, &y0);
    ink_logical_to_native(UI.nw, UI.nh, UI.rot, a->x2, a->y2, &x1, &y1);
    int nx0 = x0 < x1 ? x0 : x1, nx1 = x0 < x1 ? x1 : x0;
    int ny0 = y0 < y1 ? y0 : y1, ny1 = y0 < y1 ? y1 : y0;
    return (hal_rect_t){nx0, ny0, nx1 - nx0 + 1, ny1 - ny0 + 1};
}

static bool touches(hal_rect_t a, hal_rect_t b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}

static void add_dirty(hal_rect_t r, ink_refresh_t intent) {
    for (int i = 0; i < UI.ndirty; i++) {
        ink_region_t *d = &UI.dirty[i];
        if (d->intent != intent || !touches(d->rect, r)) continue;
        int x0 = d->rect.x < r.x ? d->rect.x : r.x, y0 = d->rect.y < r.y ? d->rect.y : r.y;
        int x1 = d->rect.x + d->rect.w > r.x + r.w ? d->rect.x + d->rect.w : r.x + r.w;
        int y1 = d->rect.y + d->rect.h > r.y + r.h ? d->rect.y + d->rect.h : r.y + r.h;
        d->rect = (hal_rect_t){x0, y0, x1 - x0, y1 - y0};
        return;
    }
    if (UI.ndirty < MAX_DIRTY) {
        UI.dirty[UI.ndirty++] = (ink_region_t){r, intent};
    } else { /* fold into the last region */
        ink_region_t *d = &UI.dirty[MAX_DIRTY - 1];
        int x0 = d->rect.x < r.x ? d->rect.x : r.x, y0 = d->rect.y < r.y ? d->rect.y : r.y;
        int x1 = d->rect.x + d->rect.w > r.x + r.w ? d->rect.x + d->rect.w : r.x + r.w;
        int y1 = d->rect.y + d->rect.h > r.y + r.h ? d->rect.y + d->rect.h : r.y + r.h;
        d->rect = (hal_rect_t){x0, y0, x1 - x0, y1 - y0};
        if (intent > d->intent) d->intent = intent;
    }
}

static bool area_overlap(const lv_area_t *a, const lv_area_t *b) {
    return a->x1 <= b->x2 && b->x1 <= a->x2 && a->y1 <= b->y2 && b->y1 <= a->y2;
}

static ink_refresh_t intent_for(const lv_area_t *a) {
    bool hit = false;
    ink_refresh_t best = INK_REFRESH_A2;
    for (int i = 0; i < UI.nhints; i++) {
        if (!area_overlap(a, &UI.hints[i].a)) continue;
        if (!hit || UI.hints[i].intent > best) best = UI.hints[i].intent;
        hit = true;
    }
    return hit ? best : UI.frame_intent;
}

/* ----------------------------------------------------------- LVGL glue */

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px) {
    if (!UI.fb_locked) {
        ink_display_lock();
        UI.fb_locked = true;
    }
    int w = lv_area_get_width(area), h = lv_area_get_height(area);
    uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)w, LV_COLOR_FORMAT_L8);
    uint8_t *fb = ink_display_fb();
    int fs = ink_display_stride();
    osal_cost_units("lvgl.px", (uint64_t)w * (uint64_t)h);       /* rendering, amortized per pixel */
    osal_cost_units("ui.rotate_px", (uint64_t)w * (uint64_t)h);  /* transpose into the native framebuffer */
    for (int y = 0; y < h; y++) {
        const uint8_t *row = px + (size_t)y * stride;
        int ly = area->y1 + y;
        for (int x = 0; x < w; x++) {
            int nx, ny;
            ink_logical_to_native(UI.nw, UI.nh, UI.rot, area->x1 + x, ly, &nx, &ny);
            fb[(size_t)ny * fs + nx] = row[x];
        }
        if ((y & 31) == 31) osal_preempt_point();
    }
    add_dirty(native_rect(area), intent_for(area));
    lv_display_flush_ready(d);
}

static uint32_t count_objs(lv_obj_t *o) {
    if (!o) return 0;
    uint32_t n = 1, c = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < c; i++) n += count_objs(lv_obj_get_child(o, (int32_t)i));
    return n;
}

static void refr_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_START) {
        /* fixed refresh overhead + per-object style/layout/draw-task work; objects created since
         * the last refresh also pay their creation (allocations, style init, events) */
        static uint32_t prev_objs;
        uint32_t objs = count_objs(lv_screen_active()) + count_objs(lv_layer_top()) + count_objs(lv_layer_sys());
        osal_cost_units("lvgl.refr", 1);
        osal_cost_units("lvgl.obj_refr", objs);
        if (objs > prev_objs) osal_cost_units("lvgl.obj_create", objs - prev_objs);
        prev_objs = objs;
    } else if (code == LV_EVENT_REFR_READY) {
        if (UI.ndirty) {
            ink_display_commit_tagged(UI.dirty, UI.ndirty, UI.cause_us, UI.cause_tag);
            UI.ndirty = 0;
            UI.cause_us = 0;
            UI.cause_tag = NULL;
        }
        UI.nhints = 0;
        UI.frame_intent = INK_REFRESH_UI;
        if (UI.fb_locked) {
            ink_display_unlock();
            UI.fb_locked = false;
        }
    }
}

static void indev_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    if (UI.pn) {
        UI.last_pt.x = UI.pfifo[UI.ph].x;
        UI.last_pt.y = UI.pfifo[UI.ph].y;
        UI.last_pressed = UI.pfifo[UI.ph].pressed;
        UI.ph = (UI.ph + 1) % PFIFO;
        UI.pn--;
        data->continue_reading = UI.pn > 0;
    }
    data->point = UI.last_pt;
    data->state = UI.last_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void pointer_push(int x, int y, bool pressed) {
    if (UI.pn == PFIFO) { /* drop the oldest move */
        UI.ph = (UI.ph + 1) % PFIFO;
        UI.pn--;
    }
    int i = (UI.ph + UI.pn) % PFIFO;
    UI.pfifo[i].x = (int16_t)x;
    UI.pfifo[i].y = (int16_t)y;
    UI.pfifo[i].pressed = pressed;
    UI.pn++;
    lv_indev_read(UI.indev);
}

/* -------------------------------------------------------------- hints */

void ink_ui_hint_area(const lv_area_t *a, ink_refresh_t intent) {
    if (UI.frame_intent >= INK_REFRESH_FLASH_UI) return; /* a screen transition is pending: one clean refresh */
    if (UI.nhints < MAX_HINTS) {
        UI.hints[UI.nhints].a = *a;
        UI.hints[UI.nhints].intent = intent;
        UI.nhints++;
    } else if (intent > UI.frame_intent) {
        UI.frame_intent = intent;
    }
}

void ink_ui_hint_obj(lv_obj_t *obj, ink_refresh_t intent) {
    lv_area_t a;
    lv_obj_update_layout(obj); /* a just-created object has no coordinates until layout runs */
    lv_obj_get_coords(obj, &a);
    ink_ui_hint_area(&a, intent);
}

void ink_ui_cause(uint64_t t_us, const char *tag) {
    UI.cause_us = t_us;
    UI.cause_tag = tag;
}

const char *ink_ui_page_name(void) {
    const ink_activity_t *top = UI.depth ? UI.stack[UI.depth - 1].a : NULL;
    return top && top->name ? top->name : "";
}

const char *ink_ui_top_name(void) { return UI.noverlays ? UI.overlays[UI.noverlays - 1]->name : ink_ui_page_name(); }

static int snapshot_content(char *buf, size_t cap, const char **activity_name) {
    if (UI.noverlays) {
        const ink_ui_overlay_t *o = UI.overlays[UI.noverlays - 1];
        if (activity_name) *activity_name = o->name;
        if (o->snapshot && cap) return o->snapshot(buf, cap);
        if (cap) buf[0] = 0;
        return 0;
    }
    const ink_activity_t *top = UI.depth ? UI.stack[UI.depth - 1].a : NULL;
    if (activity_name) *activity_name = top ? top->name : "";
    if (!cap || ((!top || !top->snapshot) && !ink_text_focused())) {
        if (cap) buf[0] = 0;
        return 0;
    }
    if (!ink_text_focused()) return top->snapshot(buf, cap);
    char *page = osal_malloc(cap, OSAL_MEM_EXTERNAL);
    char *text = osal_malloc(cap, OSAL_MEM_EXTERNAL);
    int pn = page ? (top && top->snapshot ? top->snapshot(page, cap) : snprintf(page, cap, "%s", top ? top->name : "")) : -1;
    int tn = text ? ink_text_snapshot(text, cap) : -1;
    ink_jw_t w;
    ink_jw_init(&w, buf, cap);
    if (pn >= 0 && tn >= 0) {
        ink_jw_raw(&w, "{\"page\":"); ink_jw_strn(&w, page, (size_t)pn);
        ink_jw_raw(&w, ",\"keyboard\":"); ink_jw_raw(&w, text); ink_jw_raw(&w, "}");
    } else w.overflow = true;
    osal_free(page); osal_free(text);
    if (w.overflow) { buf[0] = 0; return -1; }
    return (int)w.len;
}

static bool snapshot_owner_matches(const ink_member_identity_t *owner, const ink_member_identity_t *identity) {
    return owner->generation == identity->generation && !strcmp(owner->member, identity->member);
}

bool ink_ui_page_current(void) {
    ink_member_identity_t identity;
    ink_member_identity(&identity);
    ink_svc_lock();
    bool current = identity.authed && snapshot_owner_matches(&UI.page_owner, &identity);
    ink_svc_unlock();
    return current;
}

int ink_ui_snapshot(char *buf, size_t cap, const char **activity_name) {
    ink_member_identity_t identity, after;
    ink_member_identity(&identity);
    /* Reauthentication resumes this member's page; overlays provide their own sanitized snapshot. */
    bool current = UI.noverlays ? snapshot_owner_matches(&UI.overlay_owner[UI.noverlays - 1], &identity) :
                                 identity.authed && snapshot_owner_matches(&UI.page_owner, &identity);
    int n = current ? snapshot_content(buf, cap, activity_name) : -1;
    ink_member_identity(&after);
    if (!snapshot_owner_matches(&identity, &after) || identity.epoch != after.epoch || identity.authed != after.authed) n = -1;
    if (n < 0) {
        if (cap) buf[0] = 0;
        if (activity_name) *activity_name = "";
    }
    return n;
}

void ink_ui_hint_frame(ink_refresh_t intent) {
    if (intent > UI.frame_intent) UI.frame_intent = intent;
}

/* --------------------------------------------------------------- styles */

static void styles_init(void) {
    lv_style_init(&UI.st_screen);
    lv_style_set_bg_color(&UI.st_screen, lv_color_white());
    lv_style_set_bg_opa(&UI.st_screen, LV_OPA_COVER);
    lv_style_set_text_color(&UI.st_screen, lv_color_black());
    lv_style_set_text_font(&UI.st_screen, ink_ui_font(0));
    lv_style_set_pad_all(&UI.st_screen, 0);

    lv_style_init(&UI.st_btn);
    lv_style_set_bg_color(&UI.st_btn, lv_color_white());
    lv_style_set_bg_opa(&UI.st_btn, LV_OPA_COVER);
    lv_style_set_border_color(&UI.st_btn, lv_color_black());
    lv_style_set_border_width(&UI.st_btn, ink_dp(2));
    lv_style_set_radius(&UI.st_btn, ink_dp(6));
    lv_style_set_text_color(&UI.st_btn, lv_color_black());
    lv_style_set_shadow_width(&UI.st_btn, 0);
    lv_style_set_pad_all(&UI.st_btn, ink_dp(8));
    lv_style_set_anim_duration(&UI.st_btn, 0);

    lv_style_init(&UI.st_btn_pr);
    lv_style_set_bg_color(&UI.st_btn_pr, lv_color_black());
    lv_style_set_text_color(&UI.st_btn_pr, lv_color_white());
}

void ink_ui_style_screen(lv_obj_t *scr) {
    lv_obj_remove_style_all(scr);
    lv_obj_add_style(scr, &UI.st_screen, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

static void btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
        ink_ui_hint_obj(lv_event_get_target_obj(e), INK_REFRESH_FAST); /* press feedback: fast waveform */
}

/* No pressed-state inversion by default: on a panel without a fast A2 waveform,
 * "invert on press, restore on release" costs two DU refreshes (~320 ms each) and
 * pushes the real result back by a whole waveform (measured in the simulator).
 * The feedback is the action's own result, drawn with the FAST waveform. */
void ink_ui_style_button(lv_obj_t *btn) {
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &UI.st_btn, 0);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);
}

void ink_ui_style_button_pressable(lv_obj_t *btn) {
    ink_ui_style_button(btn);
    lv_obj_add_style(btn, &UI.st_btn_pr, LV_STATE_PRESSED);
}

const lv_font_t *ink_ui_font(int role) {
    switch (role) {
    case 1: return &lv_font_montserrat_40;
    case 2: return &lv_font_montserrat_20;
    default: return &lv_font_montserrat_28;
    }
}

/* ----------------------------------------------------------- activities */

static void show_top(void) {
    frame_t *f = &UI.stack[UI.depth - 1];
    ink_member_identity_t owner;
    ink_member_identity(&owner);
    ink_svc_lock(); UI.page_owner = owner; ink_svc_unlock();
    lv_obj_t *old = UI.screen;
    UI.screen = lv_obj_create(NULL);
    ink_ui_style_screen(UI.screen);
    UI.generation++;
    ink_ui_content_changed();
    /* a new screen replaces everything: one flashing clean refresh. Area hints from the old
     * screen (e.g. FAST for the button that was tapped) must not split the frame: they would
     * drive part of the NEW screen with a binary waveform first (seen in the simulator). */
    UI.nhints = 0;
    UI.frame_intent = INK_REFRESH_FLASH_UI;
    if (UI.top_hook) UI.top_hook(f->a->app);
    f->a->create(UI.screen, f->arg);
    lv_screen_load(UI.screen);
    if (old) lv_obj_delete(old);
    /* drop LVGL's pointer state from the previous screen */
    UI.pn = 0;
    UI.last_pressed = false;
}

void ink_ui_push(const ink_activity_t *a, void *arg) {
    if (UI.depth >= MAX_ACTIVITIES) return;
    if (UI.depth && UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    UI.stack[UI.depth++] = (frame_t){a, arg};
    show_top();
}

void ink_ui_home(void) {
    if (UI.depth <= 1) return;
    while (UI.depth > 1) {
        if (UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
        UI.depth--;
    }
    show_top(); /* one rebuild, not one per level */
}

void ink_ui_pop(void) {
    if (UI.depth <= 1) return;
    if (UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    UI.depth--;
    show_top(); /* activities rebuild from their own state: only the top one holds a screen */
}

void ink_ui_restart(void (*between)(void)) {
    if (UI.depth && UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    if (UI.depth) UI.depth = 1;
    if (between) between();
    if (UI.depth) show_top();
}

bool ink_ui_full(void) { return UI.depth >= MAX_ACTIVITIES; }

void ink_ui_set_top_hook(void (*fn)(const char *app)) { UI.top_hook = fn; }

static bool of_app(const frame_t *f, const char *app) { return f->a->app && !strcmp(f->a->app, app); }

bool ink_ui_bring_up(const char *app) {
    int at = -1;
    for (int i = UI.depth - 1; i >= 0 && at < 0; i--)
        if (of_app(&UI.stack[i], app)) at = i;
    if (at < 0) return false;
    if (at == UI.depth - 1) return true;
    if (UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    UI.depth = at + 1;
    show_top();
    return true;
}

void ink_ui_drop_app(const char *app) {
    bool was_top = UI.depth && of_app(&UI.stack[UI.depth - 1], app);
    if (was_top && UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    int kept = 0;
    for (int i = 0; i < UI.depth; i++)
        if (!of_app(&UI.stack[i], app)) UI.stack[kept++] = UI.stack[i];
    UI.depth = kept;
    if (was_top && UI.depth) show_top();
}

/* What was aimed at the old content is dropped, as for a new screen. A touch in progress too: the next move of that
 * finger would press what is under it now, and lifting it would be a tap (a finger down when a call came in, on the
 * spot where "hang up" appears). The content version stays: the page under it has not changed, and what is prepared
 * for it is still right when the overlay goes (while it is up, the gate refuses what belongs to the page). */
static void overlays_changed(void) {
    UI.generation++;
    lv_indev_reset(UI.indev, NULL);
    UI.pn = 0;
    if (UI.last_pressed) UI.skip_touch = true;
    UI.last_pressed = false;
}

void ink_ui_overlay_add(const ink_ui_overlay_t *o, lv_obj_t *root) {
    for (int i = 0; i < UI.noverlays; i++)
        if (UI.overlays[i] == o) return;
    if (UI.noverlays >= MAX_OVERLAYS) return;
    int at = UI.noverlays++;
    while (at > 0 && UI.overlays[at - 1]->level > o->level) {
        UI.overlays[at] = UI.overlays[at - 1];
        UI.overlay_root[at] = UI.overlay_root[at - 1];
        UI.overlay_owner[at] = UI.overlay_owner[at - 1];
        at--;
    }
    UI.overlays[at] = o;
    UI.overlay_root[at] = root;
    ink_member_identity(&UI.overlay_owner[at]);
    for (int i = at + 1; i < UI.noverlays; i++) lv_obj_move_foreground(UI.overlay_root[i]);
    overlays_changed();
}

bool ink_ui_overlay_up(void) { return UI.noverlays > 0; }

void ink_ui_overlay_remove(const ink_ui_overlay_t *o) {
    for (int i = 0; i < UI.noverlays; i++)
        if (UI.overlays[i] == o) {
            memmove(&UI.overlays[i], &UI.overlays[i + 1], sizeof UI.overlays[0] * (size_t)(UI.noverlays - i - 1));
            memmove(&UI.overlay_root[i], &UI.overlay_root[i + 1], sizeof UI.overlay_root[0] * (size_t)(UI.noverlays - i - 1));
            memmove(&UI.overlay_owner[i], &UI.overlay_owner[i + 1], sizeof UI.overlay_owner[0] * (size_t)(UI.noverlays - i - 1));
            UI.noverlays--;
            overlays_changed();
            return;
        }
}

/* Open with feedback: the tapped control inverts immediately (one FAST refresh, first
 * visible change ~160 ms) and the new activity is built only after that frame was
 * committed — heavy work (loading a book) no longer delays the first visible change. */
static struct {
    const ink_activity_t *a;
    void *arg;
    uint64_t cause_us; /* the tap that opened it: end-to-end "tap -> new screen stable" */
    ink_exec_job_t job;
} pending_open;

static void do_open(void *unused) {
    (void)unused;
    if (pending_open.a) {
        ink_ui_push(pending_open.a, pending_open.arg);
        if (pending_open.cause_us) ink_ui_cause(pending_open.cause_us, "ui.open_to_stable_ms");
    }
    pending_open.a = NULL;
}

void ink_ui_open(lv_obj_t *from, const ink_activity_t *a, void *arg) {
    if (from) {
        lv_obj_add_style(from, &UI.st_btn_pr, LV_STATE_USER_1);
        lv_obj_add_state(from, LV_STATE_USER_1);
        ink_ui_hint_obj(from, INK_REFRESH_FAST);
    }
    pending_open.a = a;
    pending_open.arg = arg;
    pending_open.cause_us = UI.cause_us;
    ink_exec_post(INK_EXEC_UI, &pending_open.job, do_open, NULL);
}

/* A job waits: its message brings it into a round, in turn with the input before it. With the queue
 * full (or before the task runs) there is no room for one: the task's loop then looks for itself. */
void ink_ui_kick(void) {
    umsg_t m = {.type = UM_CALL};
    __atomic_add_fetch(&UI.posts, 1, __ATOMIC_SEQ_CST);
    if (!UI.q || osal_queue_send(UI.q, &m, 0) != OSAL_OK) {
        __atomic_sub_fetch(&UI.posts, 1, __ATOMIC_SEQ_CST);
        __atomic_store_n(&UI.kick_lost, 1, __ATOMIC_SEQ_CST);
    }
}

/* Read the post count first: the UI task stores render_pending before it lowers the count, and
 * a rendered frame reaches the display service (ink_display_busy) before render_pending drops.
 * A job taken from the list runs inside a round, whose message is counted until that same point. */
bool ink_ui_busy(void) {
    if (ink_exec_waiting(INK_EXEC_UI)) return true;
    if (__atomic_load_n(&UI.posts, __ATOMIC_SEQ_CST) > 0) return true;
    return __atomic_load_n(&UI.render_pending, __ATOMIC_SEQ_CST);
}

bool ink_ui_queue_pending(void) { return ink_exec_waiting(INK_EXEC_UI) || __atomic_load_n(&UI.posts, __ATOMIC_SEQ_CST) > 0; }

static void note_render_pending(void) { /* UI task */
    lv_timer_t *t = lv_display_get_refr_timer(UI.disp);
    __atomic_store_n(&UI.render_pending, t && !lv_timer_get_paused(t), __ATOMIC_SEQ_CST);
}

/* ---------------------------------------------------------------- input */

static void input_handler(const ink_input_event_t *e, void *ctx) {
    (void)ctx;
    umsg_t m = {.type = UM_INPUT, .generation = UI.generation, .ev = *e};
    /* moves may be dropped under load; presses, releases and keys never */
    bool droppable = e->kind == INK_EV_POINTER && e->phase == INK_PTR_MOVE;
    __atomic_add_fetch(&UI.posts, 1, __ATOMIC_SEQ_CST);
    if (osal_queue_send(UI.q, &m, droppable ? 0 : OSAL_WAIT_FOREVER) != OSAL_OK) {
        __atomic_sub_fetch(&UI.posts, 1, __ATOMIC_SEQ_CST);
        ink_probe_count("ui.dropped_moves");
    }
}

static void set_interactive(bool on) {
    if (on == UI.interactive) return;
    UI.interactive = on;
    ink_display_interactive(on);
}

static void handle_input(const umsg_t *m) {
    const ink_input_event_t *e = &m->ev;
    const ink_activity_t *top = UI.depth ? UI.stack[UI.depth - 1].a : NULL;
    UI.last_input_us = osal_now_us();
    switch (e->kind) {
    case INK_EV_POINTER:
        if (UI.skip_touch) {
            if (e->phase == INK_PTR_RELEASE) UI.skip_touch = false;
            ink_probe_count("ui.touch_under_overlay_dropped");
            return;
        }
        if (m->generation != UI.generation && e->phase != INK_PTR_RELEASE) {
            ink_probe_count("ui.stale_input_dropped");
            return;
        }
        if (e->phase == INK_PTR_PRESS) set_interactive(true);
        UI.cause_us = e->t_us;
        UI.cause_tag = NULL;
        if (e->phase == INK_PTR_RELEASE && (e->flags & INK_EVF_SWIPE)) {
            /* a swipe is not a click: LVGL would send CLICKED on any release without scrolling
             * (found in the simulator: a swipe turned the page twice, once per gesture path) */
            lv_indev_reset(UI.indev, NULL);
            UI.pn = 0;
            UI.last_pressed = false;
            break;
        }
        pointer_push(e->x, e->y, e->phase != INK_PTR_RELEASE);
        break;
    case INK_EV_LONGPRESS: /* LVGL looks at the clock only when it reads: this is when it finds out, and no click follows */
        if (!UI.skip_touch && m->generation == UI.generation && UI.last_pressed) lv_indev_read(UI.indev);
        break;
    case INK_EV_SWIPE:
        if (m->generation != UI.generation) return;
        UI.cause_us = e->t_us;
        UI.cause_tag = NULL;
        if (!UI.noverlays && top && top->on_swipe) top->on_swipe((ink_swipe_dir_t)e->dir);
        break;
    case INK_EV_KEY:
    case INK_EV_KEY_LONG: {
        bool lng = e->kind == INK_EV_KEY_LONG;
        if (e->key == HAL_KEY_AI) { /* system-wide: push-to-talk, handled by the AI service; an overlay comes first, but one already talking may let go */
            if (e->kind == INK_EV_KEY && (!UI.noverlays || !e->pressed)) ink_ai_key(e->pressed, e->t_us);
            return;
        }
        if (e->kind == INK_EV_KEY && (!e->pressed || (e->flags & INK_EVF_AFTER_LONG))) {
            /* act on press; releases only matter to push-to-talk (handled by the AI service later) */
            if (!e->pressed) return;
        }
        UI.cause_us = e->t_us;
        UI.cause_tag = NULL;
        if (UI.noverlays) {
            const ink_ui_overlay_t *o = UI.overlays[UI.noverlays - 1];
            if (o->on_key) o->on_key((hal_key_t)e->key, lng);
            return;
        }
        if ((e->key == HAL_KEY_BACK || e->key == HAL_KEY_HOME) && ink_ai_dismiss()) return;
        if (top && top->on_key && top->on_key((hal_key_t)e->key, lng)) return;
        if (e->key == HAL_KEY_BACK && !lng) ink_ui_pop();
        else if (e->key == HAL_KEY_HOME)
            ink_ui_home();
        break;
    }
    default: break;
    }
}

/* ----------------------------------------------------------------- task */

static const ink_activity_t *g_home;

/* Handle one message; *first_cause keeps the first action of the round (a key press, swipe, tap
 * release or posted job that set a cause; pointer presses and moves are not actions by themselves,
 * as for a single tap, which counts from its release). */
static void handle(const umsg_t *m, uint64_t *first_cause, const char **first_tag) {
    uint64_t before = UI.cause_us;
    if (m->type == UM_INPUT) handle_input(m);
    else if (m->type == UM_CALL) ink_exec_run(INK_EXEC_UI);
    bool action = !(m->type == UM_INPUT && m->ev.kind == INK_EV_POINTER && m->ev.phase != INK_PTR_RELEASE);
    if (action && !*first_cause && UI.cause_us && UI.cause_us != before) {
        *first_cause = UI.cause_us;
        *first_tag = UI.cause_tag;
    }
}

/* One round of messages, drawn as one frame. Input that queued up while the task was busy (during a
 * waveform the feeders hold both cores, and drawing a page can take as long as the waveform) is
 * handled together before anything is drawn, so a burst of page turns shows its last page instead
 * of every page in turn, and the wait after the last input stays bounded (found by the 2026-11
 * audit, AUD-002: four turns a second queued up five seconds). A posted job ends the batch: it may
 * need the frame drawn first ("open with feedback"). The frame counts from the batch's first action. */
static void handle_round(const umsg_t *first) {
    uint64_t cause = 0;
    const char *tag = NULL;
    int handled = 1;
    handle(first, &cause, &tag);
    if (first->type == UM_INPUT) {
        umsg_t m;
        while (handled < INPUT_BATCH_MAX && osal_queue_recv(UI.q, &m, 0) == OSAL_OK) {
            if (m.type != UM_INPUT) {
                UI.held = m; /* still counted in posts until handled */
                UI.has_held = true;
                break;
            }
            handle(&m, &cause, &tag);
            handled++;
        }
    }
    const ink_activity_t *top = UI.depth ? UI.stack[UI.depth - 1].a : NULL;
    if (top && top->settle) top->settle();
    if (cause && handled > 1) { /* later input of the batch set newer causes: the frame counts from the first */
        UI.cause_us = cause;
        UI.cause_tag = tag;
    }
    note_render_pending(); /* before the count drops: see ink_ui_busy */
    __atomic_sub_fetch(&UI.posts, handled, __ATOMIC_SEQ_CST);
}

static void ui_task(void *arg) {
    (void)arg;
    UI.task = osal_task_self(); /* before anything can ask ink_ui_is_current() */
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_log_register_print_cb(log_cb);

    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    UI.nw = pi.width;
    UI.nh = pi.height;
    UI.rot = pi.rotation_deg;
    UI.dpi = pi.dpi;
    ink_logical_size(pi.width, pi.height, pi.rotation_deg, &UI.lw, &UI.lh);

    UI.disp = lv_display_create(UI.lw, UI.lh);
    lv_display_set_color_format(UI.disp, LV_COLOR_FORMAT_L8);
    lv_display_set_dpi(UI.disp, UI.dpi);
    uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)UI.lw, LV_COLOR_FORMAT_L8);
    size_t bufsz = (size_t)stride * STRIP_ROWS;
    const char *prev = osal_mem_tag("ui-drawbuf");
    UI.drawbuf = osal_malloc(bufsz, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!UI.drawbuf) osal_panic("ui: no memory for the draw buffer (%zu bytes)", bufsz);
    lv_display_set_buffers(UI.disp, UI.drawbuf, NULL, (uint32_t)bufsz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(UI.disp, flush_cb);
    lv_display_add_event_cb(UI.disp, refr_event_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(UI.disp, refr_event_cb, LV_EVENT_REFR_READY, NULL);

    UI.indev = lv_indev_create();
    lv_indev_set_type(UI.indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(UI.indev, indev_read);
    lv_indev_set_mode(UI.indev, LV_INDEV_MODE_EVENT);

    styles_init();
    UI.frame_intent = INK_REFRESH_UI;
    ink_ui_push(g_home, NULL);
    ink_input_init(input_handler, NULL);

    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        note_render_pending(); /* a frame just rendered is already queued at the display service */
        int64_t timeout = wait_ms == LV_NO_TIMER_READY ? OSAL_WAIT_FOREVER : (int64_t)wait_ms * 1000;
        if (UI.interactive) {
            int64_t left = (int64_t)(UI.last_input_us + INTERACTIVE_IDLE_MS * 1000ull) - (int64_t)osal_now_us();
            if (left <= 0) {
                set_interactive(false);
            } else if (timeout == OSAL_WAIT_FOREVER || timeout > left) {
                timeout = left;
            }
        }
        umsg_t m;
        if (UI.has_held) {
            m = UI.held;
            UI.has_held = false;
        } else if (__atomic_exchange_n(&UI.kick_lost, 0, __ATOMIC_SEQ_CST)) { /* a job whose message found no room */
            m = (umsg_t){.type = UM_CALL};
            __atomic_add_fetch(&UI.posts, 1, __ATOMIC_SEQ_CST);
        } else if (osal_queue_recv(UI.q, &m, timeout) != OSAL_OK) {
            continue;
        }
        handle_round(&m);
    }
}

int ink_ui_start(const ink_activity_t *home) {
    g_home = home;
    const char *prev = osal_mem_tag("ui");
    UI.q = osal_queue_create(sizeof(umsg_t), UI_QUEUE_LEN);
    osal_mem_tag(prev);
    if (!UI.q) return -1;
    /* Stack in PSRAM: the UI task works on PSRAM framebuffers anyway (it freezes during flash
     * writes regardless) and internal RAM is the scarcest resource (foundation §3.2). */
    osal_task_cfg_t cfg = {.name = "ui", .fn = ui_task, .stack_bytes = UI_STACK, .priority = UI_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, &UI.task) == OSAL_OK ? 0 : -1;
}

bool ink_ui_is_current(void) { return UI.task && osal_task_self() == UI.task; }
