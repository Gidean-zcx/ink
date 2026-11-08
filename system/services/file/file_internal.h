/* Inside the file service. */
#ifndef INK_FILE_INTERNAL_H
#define INK_FILE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include "ink/sys/cap.h"
#include "ink/sys/file.h"
#include "ink/sys/layout.h"

typedef struct {
    char member[INK_MEMBER_ID_MAX];
    bool adult;
} file_context_t;

bool fs_context_for_call(file_context_t *context, ink_call_t *call);
int fs_open_for(const file_context_t *c, const char *path, int mode, ink_fs_file_t **out);
int fs_stat_for(const file_context_t *c, const char *path, ink_fs_stat_t *out);
int fs_dir_open_for(const file_context_t *c, const char *path, ink_fs_dir_t **out);
int fs_mkdir_for(const file_context_t *c, const char *path);
int fs_remove_for(const file_context_t *c, const char *path, uint32_t revision);
int fs_move_for(const file_context_t *c, const char *from, const char *to, uint32_t revision, uint32_t *written);
int fs_job_begin_for(const file_context_t *c, int op, const char *from, const char *to);
int fs_index_for(const file_context_t *c, const char *path, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *out);

void fs_refresh_cb(void *arg);
int fs_resolve(const char *path, bool write, char *phys, size_t cap);
int ink_file_caps_init(void);

#endif
