/* Backup manifest wire format v1. All integer fields are little endian; names are UTF-8. */
#ifndef INK_SYS_BACKUP_FORMAT_H
#define INK_SYS_BACKUP_FORMAT_H
#include <stdint.h>
#define INK_BACKUP_ITEMS 128
#define INK_BACKUP_MAGIC 0x314d4249u

typedef struct {
    char name[128];
    uint32_t kind, size, revision, records;
    uint8_t hash[32], before[32];
} ink_backup_item_t;

typedef struct {
    uint32_t magic, format, count;
    char member[16];
    uint32_t reserved;
    int64_t created_s;
    ink_backup_item_t item[INK_BACKUP_ITEMS];
} ink_backup_manifest_t;
_Static_assert(sizeof(ink_backup_item_t) == 208, "backup entry wire size");
_Static_assert(sizeof(ink_backup_manifest_t) == 40 + 208 * INK_BACKUP_ITEMS, "backup manifest wire size");
#endif
