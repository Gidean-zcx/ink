/*
 * Where data lives (system services design §2, R-057): every path of the data
 * areas is built here, nowhere else.
 *
 *   /user/sys/<name>            device data: settings, members, alarms ...
 *   /user/m/<member>/<name>     a member's small, important data; names start with their owner
 *                               (a service's domain or an app id): "notify.inbox", "reader.pos-x"
 *   /content/.m/<member>/...    a member's files on the card (apps: under apps/<app>/)
 *   /content/...                family content on the card
 *   /content/.update/           downloaded system updates
 *   /cache/sys/, /cache/<app>/  rebuildable caches
 *
 * LittleFS spends at least two erase blocks (8 KiB) per directory and /user
 * is 1.4 MiB, so directories are few and names carry the owner.
 */
#ifndef INK_SYS_LAYOUT_H
#define INK_SYS_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INK_LAYOUT_PATH_MAX 96 /* the longest /user path built here, NUL included */
#define INK_MEMBER_ID_MAX 16   /* NUL included */

/** A member id: 1..15 of [a-z0-9_-]. */
bool ink_layout_member_ok(const char *id);
/** A file name in a data area: 1..63 bytes, no slash, not starting with a dot, no control characters. */
bool ink_layout_name_ok(const char *name);
/** /user/sys/<name>. Length, or -1 if it does not fit or the name is not valid. */
int ink_layout_sys(char *out, size_t cap, const char *name);
/** /user/m/<member>/<name>. Length, or -1. */
int ink_layout_member(char *out, size_t cap, const char *member, const char *name);
/** /content/.m/<member> (the member's root on the card). Length, or -1. */
int ink_layout_member_files(char *out, size_t cap, const char *member);

#ifdef __cplusplus
}
#endif
#endif
