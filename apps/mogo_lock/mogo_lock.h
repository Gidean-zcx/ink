/* Glue between Mogo's lock styles and the lock screen's client table (system/ui/lock_faces/faces.h). */
#pragma once
#include <lock_faces/faces.h>
extern const ink_lock_request_t *ink_mogo_lock_request; /* set for the length of one draw */
int ink_mogo_lock_init(void);
void ink_mogo_dimensions_get(int *w, int *h, int *px);
int ink_mogo_lock_stat(const char *path, struct stat *st);
