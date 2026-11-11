#ifndef INK_LOCK_FACES_H
#define INK_LOCK_FACES_H
#include "ink/ui_shell.h"
#include "ink/sys/ui_semantics.h"
enum { INK_LOCK_SIMPLE = 3, INK_LOCK_STANDBY, INK_LOCK_CALENDAR, INK_LOCK_FACE_COUNT };
enum { INK_LOCK_FACE_FIELDS = 6 };
extern const char *const ink_lock_face_choices[];
void ink_lock_face_draw(lv_obj_t *parent, int style);
bool ink_lock_face_update(void);
void ink_lock_face_clear(void);
void ink_lock_face_resume(void);
size_t ink_lock_face_fields(ink_ui_field_spec_t *fields);
void ink_lock_face_settings(lv_obj_t *parent);
#endif
