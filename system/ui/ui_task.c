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
#include "ink/sys/ui.h"

#define UI_STACK 10240
#define UI_PRIO 5
#define UI_QUEUE_LEN 24
#define STRIP_ROWS 96
#define MAX_HINTS 8
#define MAX_DIRTY 24
#define MAX_ACTIVITIES 6
#define PFIFO 16
#define INTERACTIVE_IDLE_MS 2500

typedef enum { UM_INPUT = 1, UM_CALL } um_type_t;

typedef struct {
    uint8_t type;
    uint32_t generation;
    ink_input_event_t ev;
    void (*fn)(void *);
    void *arg;
} umsg_t;

typedef struct {
    const ink_activity_t *a;
    void *arg;
} frame_t;

static struct {
    osal_queue_t *q;
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
    /* activities */
    frame_t stack[MAX_ACTIVITIES];
    int depth;
    lv_obj_t *screen;
    volatile uint32_t generation;
    bool interactive;
    uint64_t last_input_us;
    /* styles */
    lv_style_t st_screen, st_btn, st_btn_pr;
} UI;

/* ---------------------------------------------------------------- helpers */

int ink_dp(int dp) { return (dp * UI.dpi + 80) / 160; }
uint32_t ink_ui_generation(void) { return UI.generation; }

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

static void refr_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_START) {
        osal_cost_units("lvgl.refr", 1);
    } else if (code == LV_EVENT_REFR_READY) {
        if (UI.ndirty) {
            ink_display_commit(UI.dirty, UI.ndirty, UI.cause_us);
            UI.ndirty = 0;
            UI.cause_us = 0;
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
    lv_obj_get_coords(obj, &a);
    ink_ui_hint_area(&a, intent);
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
    lv_obj_t *old = UI.screen;
    UI.screen = lv_obj_create(NULL);
    ink_ui_style_screen(UI.screen);
    UI.generation++;
    /* a new screen replaces everything: flashing clean refresh */
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
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

void ink_ui_pop(void) {
    if (UI.depth <= 1) return;
    if (UI.stack[UI.depth - 1].a->destroy) UI.stack[UI.depth - 1].a->destroy();
    UI.depth--;
    show_top(); /* activities rebuild from their own state: only the top one holds a screen */
}

void ink_ui_post(void (*fn)(void *), void *arg) {
    umsg_t m = {.type = UM_CALL, .fn = fn, .arg = arg};
    osal_queue_send(UI.q, &m, OSAL_WAIT_FOREVER);
}

/* ---------------------------------------------------------------- input */

static void input_handler(const ink_input_event_t *e, void *ctx) {
    (void)ctx;
    umsg_t m = {.type = UM_INPUT, .generation = UI.generation, .ev = *e};
    /* moves may be dropped under load; presses, releases and keys never */
    bool droppable = e->kind == INK_EV_POINTER && e->phase == INK_PTR_MOVE;
    if (osal_queue_send(UI.q, &m, droppable ? 0 : OSAL_WAIT_FOREVER) != OSAL_OK) ink_probe_count("ui.dropped_moves");
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
        if (m->generation != UI.generation && e->phase != INK_PTR_RELEASE) {
            ink_probe_count("ui.stale_input_dropped");
            return;
        }
        if (e->phase == INK_PTR_PRESS) set_interactive(true);
        UI.cause_us = e->t_us;
        pointer_push(e->x, e->y, e->phase != INK_PTR_RELEASE);
        break;
    case INK_EV_SWIPE:
        if (m->generation != UI.generation) return;
        UI.cause_us = e->t_us;
        if (top && top->on_swipe) top->on_swipe((ink_swipe_dir_t)e->dir);
        break;
    case INK_EV_KEY:
    case INK_EV_KEY_LONG: {
        bool lng = e->kind == INK_EV_KEY_LONG;
        if (e->kind == INK_EV_KEY && (!e->pressed || (e->flags & INK_EVF_AFTER_LONG))) {
            /* act on press; releases only matter to push-to-talk (handled by the AI service later) */
            if (!e->pressed) return;
        }
        UI.cause_us = e->t_us;
        if (top && top->on_key && top->on_key((hal_key_t)e->key, lng)) return;
        if (e->key == HAL_KEY_BACK && !lng) ink_ui_pop();
        else if (e->key == HAL_KEY_HOME)
            while (UI.depth > 1) ink_ui_pop();
        break;
    }
    default: break;
    }
}

/* ----------------------------------------------------------------- task */

static const ink_activity_t *g_home;

static void ui_task(void *arg) {
    (void)arg;
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
        if (osal_queue_recv(UI.q, &m, timeout) != OSAL_OK) continue;
        if (m.type == UM_INPUT) handle_input(&m);
        else if (m.type == UM_CALL && m.fn) m.fn(m.arg);
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
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
}
