/* Existing home/reader scenarios describe a device whose setup is already finished. */
#include "ink/osal.h"
#include "ink/sys/data.h"
#include "ink/sys/ui.h"

static int configured_ui_start(const ink_activity_t *home) {
    static const ink_db_def_t progress = {.name = "boot.progress", .title = "开机设置进度", .scope = INK_DB_DEVICE,
        .schema = 1, .json = true, .max_records = 1};
    ink_db_t *db = ink_db_open(&progress, NULL);
    ink_rec_t old;
    if (!db) osal_panic("configured fixture: cannot open progress");
    if (ink_db_get(db, 1, &old, NULL, 0) != INK_DB_OK || old.sort != -1) {
        ink_db_write_t record = {.id = 1, .sort = -1, .data = "{}", .len = 2};
        if (ink_db_put(db, &record, NULL) != INK_DB_OK) osal_panic("configured fixture: cannot finish setup");
        ink_db_flush(db);
        if (ink_db_wait(db, ink_db_last_ver(db), 5000000) != 0) osal_panic("configured fixture: progress not durable");
    }
    ink_db_close(db);
    return ink_ui_start(home);
}

#define ink_ui_start configured_ui_start
#include "../system/boot/main.c"
#undef ink_ui_start
