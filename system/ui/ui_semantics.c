#include "ink/sys/ui_semantics.h"
#include "ink/sys/json.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static void object_json(ink_jw_t *w, const ink_ui_object_ref_t *object) {
    ink_jw_raw(w, "{\"id\":"); ink_jw_str(w, object->id ? object->id : "");
    ink_jw_raw(w, ",\"revision\":"); ink_jw_int(w, object->revision);
    ink_jw_raw(w, "}");
}

static const ink_ui_object_ref_t *action_object(const ink_ui_page_spec_t *page, const ink_ui_action_spec_t *action) {
    return action->object.id ? &action->object : &page->object;
}

static bool current_page(const ink_ui_page_spec_t *page) {
    return page && page->id && ink_ui_is_current() && ink_ui_page_current() && !strcmp(page->id, ink_ui_page_name());
}

int ink_ui_semantics_snapshot(const ink_ui_page_spec_t *page, char *out, size_t cap) {
    if (!out || !cap) return -1;
    out[0] = 0;
    if (!current_page(page)) return -1;
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{\"page\":"); ink_jw_str(&w, page->id);
    ink_jw_raw(&w, ",\"title\":"); ink_jw_str(&w, page->title ? page->title : page->id);
    ink_jw_raw(&w, ",\"page_version\":"); ink_jw_int(&w, ink_ui_content_version());
    ink_jw_raw(&w, ",\"object\":"); object_json(&w, &page->object);
    ink_jw_raw(&w, ",\"fields\":[");
    for (size_t i = 0; i < page->field_count; i++) {
        const ink_ui_field_spec_t *f = &page->fields[i];
        ink_jw_raw(&w, i ? ",{\"id\":" : "{\"id\":"); ink_jw_str(&w, f->id);
        ink_jw_raw(&w, ",\"role\":"); ink_jw_str(&w, f->role);
        ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, f->label);
        ink_jw_raw(&w, ",\"value\":");
        ink_jw_str(&w, f->secret ? (f->value && f->value[0] ? "有值" : "无值") : (f->value ? f->value : ""));
        ink_jw_raw(&w, f->secret ? ",\"secret\":true" : ",\"secret\":false");
        if (!f->secret && f->object.id) {
            ink_jw_raw(&w, ",\"object\":"); object_json(&w, &f->object);
        }
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "],\"actions\":[");
    bool first = true;
    for (size_t i = 0; i < page->action_count; i++) {
        const ink_ui_action_spec_t *a = &page->actions[i];
        if (!a->enabled || !ink_cap_available(a->capability)) continue;
        ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":"); ink_jw_str(&w, a->id);
        first = false;
        ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, a->label);
        ink_jw_raw(&w, ",\"capability\":"); ink_jw_str(&w, a->capability);
        ink_jw_raw(&w, ",\"args_json\":"); ink_jw_str(&w, a->args_json ? a->args_json : "{}");
        ink_jw_raw(&w, ",\"object\":"); object_json(&w, action_object(page, a));
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}

static ink_rc_t refuse(ink_receipt_t *rc, ink_rc_t status, const char *detail) {
    rc->status = status;
    snprintf(rc->detail, sizeof rc->detail, "%s", detail);
    return status;
}

ink_rc_t ink_ui_semantics_action(const ink_ui_page_spec_t *page, const char *action_id,
                                 const ink_ui_action_target_t *target, ink_source_t source, ink_receipt_t *rc) {
    if (!rc) return INK_RC_INVALID;
    memset(rc, 0, sizeof *rc);
    rc->page_gen = ink_ui_content_version();
    if (!action_id || !target) return refuse(rc, INK_RC_INVALID, "missing action target");
    if (!current_page(page) || strcmp(page->id, ink_ui_action_page()) || target->page_version != rc->page_gen)
        return refuse(rc, INK_RC_STALE, "the page changed since the request was made");
    for (size_t i = 0; i < page->action_count; i++) {
        const ink_ui_action_spec_t *a = &page->actions[i];
        if (strcmp(a->id, action_id)) continue;
        if (!a->enabled) return refuse(rc, INK_RC_INVALID, "action disabled");
        const ink_ui_object_ref_t *object = action_object(page, a);
        if (!target->object.id || !object->id || strcmp(target->object.id, object->id) || target->object.revision != object->revision)
            return refuse(rc, INK_RC_STALE, "the object changed since the request was made");
        return ink_cap_invoke_json(a->capability, a->args_json, source, target->page_version, rc);
    }
    return refuse(rc, INK_RC_INVALID, "unknown page action");
}
