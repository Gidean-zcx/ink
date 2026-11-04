/* Inside the file service. */
#ifndef INK_FILE_INTERNAL_H
#define INK_FILE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

void fs_refresh_cb(void *arg);
int fs_resolve(const char *path, bool write, char *phys, size_t cap);
int ink_file_caps_init(void);

#endif
