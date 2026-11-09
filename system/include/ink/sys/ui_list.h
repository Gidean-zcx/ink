/* F4: bounded visible items, with F1 semantics. UI task only. */
#ifndef INK_SYS_UI_LIST_H
#define INK_SYS_UI_LIST_H
#include "ink/sys/ui.h"
#include "ink/sys/ui_semantics.h"

typedef struct { size_t page; char selected[64]; } ink_ui_list_position_t;
typedef struct ink_ui_list ink_ui_list_t;
/* Bind one visible slot. Create its children only once, then update them.
 * Strings belong to the caller and stay valid until the next refresh. Object
 * ids are stable service ids (file ids, for example), never row numbers/names. */
typedef void (*ink_ui_list_bind_fn)(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field,
                                    ink_ui_action_spec_t *action, void *ctx);
/* page has two or three actions: prev, next, then an optional page action.
 * Bind prev/next to registered capabilities whose handlers call turn().
 * Other actions come from bind(). capacity <= 16.
 * columns = 1 for a list; otherwise a grid. cell_h uses theme logical pixels.
 * parent is a dedicated container; deleting it releases the view. Keep position
 * with the member/page navigation state, and clear it when that owner changes. */
ink_ui_list_t *ink_ui_list_create(lv_obj_t *parent, const ink_ui_page_spec_t *page,
    ink_ui_list_position_t *position, unsigned capacity, unsigned columns, int cell_h,
    const char *empty, ink_ui_list_bind_fn bind, void *ctx);
void ink_ui_list_refresh(ink_ui_list_t *list, size_t total);
bool ink_ui_list_turn(ink_ui_list_t *list, int delta);
bool ink_ui_list_key(ink_ui_list_t *list, ink_key_t key, bool long_press);
int ink_ui_list_snapshot(ink_ui_list_t *list, char *out, size_t cap);
ink_rc_t ink_ui_list_action(ink_ui_list_t *list, const char *id, const ink_ui_action_target_t *target,
                            ink_source_t source, ink_receipt_t *rc);
#endif
