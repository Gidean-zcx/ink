/* ui-scale-runtime: dimension cell_h,columns,capacity,w,x,y Theme-sized cells and bounded layout counts from callers. */
#include "ink/sys/ui_list.h"
#include "ink/sys/json.h"
#include "ink/osal.h"
#include "ink/ui_theme.h"
#include <stdio.h>
#include <string.h>

struct ink_ui_list {
    lv_obj_t *parent, *slots[16], *bar, *empty;
    ink_ui_page_spec_t page;
    ink_ui_field_spec_t fields[16];
    ink_ui_action_spec_t actions[19];
    ink_ui_list_position_t *position;
    ink_ui_list_bind_fn bind;
    void *ctx;
    size_t total, pages, visible, fixed;
    unsigned capacity, columns;
    int cell_h;
};

ink_rc_t ink_ui_list_action(ink_ui_list_t *l, const char *id, const ink_ui_action_target_t *target,
                            ink_source_t source, ink_receipt_t *rc) {
    for (size_t i = 0; i < l->visible; i++) {
        const ink_ui_action_spec_t *a = &l->actions[i + l->fixed];
        if (id && !strcmp(id, a->id) && target && ink_ui_page_current() &&
            target->page_version == ink_ui_content_version() && a->object.id && target->object.id &&
            !strcmp(a->object.id, target->object.id) && a->object.revision == target->object.revision && a->enabled)
            snprintf(l->position->selected, sizeof l->position->selected, "%s", a->object.id);
    }
    return ink_ui_semantics_action(&l->page, id, target, source, rc);
}
static void invoke(ink_ui_list_t *l, const ink_ui_action_spec_t *a, ink_source_t source) {
    ink_ui_action_target_t target = {ink_ui_content_version(), a->object.id ? a->object : l->page.object};
    ink_receipt_t rc;
    ink_ui_list_action(l, a->id, &target, source, &rc);
}
static void clicked(lv_event_t *e) {
    ink_ui_list_t *l = lv_event_get_user_data(e);
    for (size_t i = 0; i < l->visible; i++)
        if (l->slots[i] == lv_event_get_target_obj(e)) { invoke(l, &l->actions[i + l->fixed], INK_SRC_TOUCH); return; }
}
static void page_clicked(lv_event_t *e) {
    lv_obj_t *bar = lv_obj_get_parent(lv_event_get_target_obj(e));
    ink_ui_list_t *l = lv_obj_get_user_data(bar);
    invoke(l, &l->actions[lv_event_get_user_data(e) == (void *)(intptr_t)1 ? 1 : 0], INK_SRC_TOUCH);
}
static void deleted(lv_event_t *e) { osal_free(lv_event_get_user_data(e)); }
static lv_obj_t *new_slot(lv_obj_t *parent, unsigned i, unsigned columns, int cell_h) {
    int w = (INK_CANVAS_W - 2 * INK_MARGIN - (columns - 1) * INK_GAP_M) / columns;
    int x = INK_MARGIN + (i % columns) * (w + INK_GAP_M), y = (i / columns) * (cell_h + INK_GAP_M);
    return ink_ui_card(parent, x, y, w, cell_h, INK_SURFACE_WHITE);
}
static void draw_bar(ink_ui_list_t *l, unsigned capacity, unsigned columns, int cell_h) {
    int y = ((capacity + columns - 1) / columns) * (cell_h + INK_GAP_M) + INK_GAP;
    l->bar = ink_ui_pagebar(l->parent, (int)l->position->page + 1, (int)l->pages, y, page_clicked);
    lv_obj_set_user_data(l->bar, l);
}
ink_ui_list_t *ink_ui_list_create(lv_obj_t *parent, const ink_ui_page_spec_t *page,
    ink_ui_list_position_t *position, unsigned capacity, unsigned columns, int cell_h,
    const char *empty, ink_ui_list_bind_fn bind, void *ctx) {
    if (!capacity || capacity > 16 || !columns || columns > capacity || page->action_count < 2 || page->action_count > 3) return NULL;
    ink_ui_list_t *l = osal_calloc(1, sizeof *l, OSAL_MEM_EXTERNAL);
    if (!l) return NULL;
    l->parent = parent; l->page = *page; l->position = position; l->capacity = capacity;
    l->columns = columns; l->cell_h = cell_h; l->bind = bind; l->ctx = ctx;
    l->fixed = page->action_count; memcpy(l->actions, page->actions, l->fixed * sizeof *l->actions);
    l->page.fields = l->fields; l->page.actions = l->actions;
    l->empty = ink_ui_notice(parent, empty, INK_GAP_L);
    lv_obj_add_event_cb(parent, deleted, LV_EVENT_DELETE, l);
    return l;
}
void ink_ui_list_refresh(ink_ui_list_t *l, size_t total) {
    l->total = total; l->pages = total ? 1 + (total - 1) / l->capacity : 1;
    if (!total) l->position->selected[0] = 0;
    if (l->position->page >= l->pages) l->position->page = l->pages - 1;
    size_t offset = l->position->page * l->capacity;
    l->visible = total - offset < l->capacity ? total - offset : l->capacity;
    for (unsigned i = 0; i < l->capacity; i++) {
        if (i >= l->visible) { if (l->slots[i]) lv_obj_delete(l->slots[i]); l->slots[i] = NULL; continue; }
        if (!l->slots[i]) {
            l->slots[i] = new_slot(l->parent, i, l->columns, l->cell_h);
            lv_obj_add_flag(l->slots[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(l->slots[i], clicked, LV_EVENT_CLICKED, l);
        }
        l->fields[i] = (ink_ui_field_spec_t){0}; l->actions[i + l->fixed] = (ink_ui_action_spec_t){0};
        l->bind(l->slots[i], offset + i, &l->fields[i], &l->actions[i + l->fixed], l->ctx);
        lv_obj_set_style_border_width(l->slots[i], l->fields[i].object.id &&
            !strcmp(l->fields[i].object.id, l->position->selected) ? INK_STROKE : 0, 0);
    }
    l->page.field_count = l->visible; l->page.action_count = l->visible + l->fixed;
    l->actions[0].enabled = l->position->page > 0;
    l->actions[1].enabled = l->position->page + 1 < l->pages;
    if (total) lv_obj_add_flag(l->empty, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(l->empty, LV_OBJ_FLAG_HIDDEN);
    if (l->bar) lv_obj_delete(l->bar);
    draw_bar(l, l->capacity, l->columns, l->cell_h);
    ink_ui_content_changed();
    ink_ui_hint_obj(l->parent, INK_REFRESH_UI);
}
bool ink_ui_list_turn(ink_ui_list_t *l, int delta) {
    if ((delta != -1 && delta != 1) || (delta < 0 && !l->position->page) ||
        (delta > 0 && l->position->page + 1 >= l->pages)) return false;
    if (delta < 0) l->position->page--; else l->position->page++;
    ink_ui_list_refresh(l, l->total);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return true;
}
bool ink_ui_list_key(ink_ui_list_t *l, ink_key_t key, bool long_press) {
    if (long_press || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    invoke(l, &l->actions[key == INK_KEY_NEXT ? 1 : 0], INK_SRC_KEY);
    return true;
}
int ink_ui_list_snapshot(ink_ui_list_t *l, char *out, size_t cap) {
    int n = ink_ui_semantics_snapshot(&l->page, out, cap);
    if (n < 0) return -1;
    size_t offset = l->position->page * l->capacity;
    int more = snprintf(out + n - 1, cap - (size_t)n + 1,
        ",\"pagination\":{\"page\":%zu,\"pages\":%zu,\"total\":%zu,\"offset\":%zu,\"visible\":%zu,\"truncated\":%s,"
        "\"note\":\"仅含当前页，其他项请翻页读取\",\"selected\":", l->position->page + 1, l->pages, l->total, offset,
        l->visible, l->visible < l->total ? "true" : "false");
    if (more < 0 || (size_t)more >= cap - (size_t)n + 1) { out[0] = 0; return -1; }
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)(n - 1 + more)};
    ink_jw_str(&w, l->position->selected); ink_jw_raw(&w, "}}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
