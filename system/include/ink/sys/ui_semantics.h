/* F1: shared declarations for C controls, AI snapshots and capability actions. */
#ifndef INK_SYS_UI_SEMANTICS_H
#define INK_SYS_UI_SEMANTICS_H

#include "ink/sys/cap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    uint32_t revision;
} ink_ui_object_ref_t;

typedef struct {
    const char *id, *role, *label, *value;
    ink_ui_object_ref_t object;
    bool secret; /* Passwords, verification codes and identity documents. */
} ink_ui_field_spec_t;

typedef struct {
    const char *id, *label, *capability, *args_json;
    ink_ui_object_ref_t object; /* NULL id inherits the page's object. */
    bool enabled;
} ink_ui_action_spec_t;

typedef struct {
    const char *id, *title; /* id must equal the activity name. */
    ink_ui_object_ref_t object;
    const ink_ui_field_spec_t *fields;
    size_t field_count;
    const ink_ui_action_spec_t *actions;
    size_t action_count;
} ink_ui_page_spec_t;

typedef struct {
    uint32_t page_version;
    ink_ui_object_ref_t object;
} ink_ui_action_target_t;

/* UI task only. Declarations and strings remain owned by the page. Stable ids are
 * unique within each array. Labels, object ids and action arguments contain no
 * credentials; only secret field values may contain them. No input session is opened.
 * Call ink_ui_content_changed() whenever values, objects or available actions change. */

/* Use as the activity's snapshot callback, inside the runtime's member guard.
 * Returns bytes, or -1 with an empty output if unavailable or too small. Secret
 * fields expose only presence, never values, lengths or object references. */
int ink_ui_semantics_snapshot(const ink_ui_page_spec_t *page, char *out, size_t cap);

/* Touch and AI use the same binding and bus. Target comes from the snapshot;
 * source comes from the trusted caller. Providers still validate object revisions
 * in args_json. This bridge checks the page and target before invoking them. */
ink_rc_t ink_ui_semantics_action(const ink_ui_page_spec_t *page, const char *action_id,
                                 const ink_ui_action_target_t *target, ink_source_t source, ink_receipt_t *rc);

#ifdef __cplusplus
}
#endif
#endif
