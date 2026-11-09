/* F9: system file selection. UI task only; the caller's page stays intact. */
#ifndef INK_SYS_FILE_PICKER_H
#define INK_SYS_FILE_PICKER_H
#include "ink/sys/file.h"
/* id == 0 means cancelled. Resolve a selected id with ink_fs_lookup on BG. */
typedef void (*ink_file_picker_done_fn)(void *ctx, uint32_t id);
/* Member/authentication changes close the picker and discard the callback. */
/* Lists readable family/member files of type. -1: not opened; no callback. */
int ink_file_picker_open(ink_fs_type_t type, ink_file_picker_done_fn done, void *ctx);
int ink_file_picker_init(void);
#endif
