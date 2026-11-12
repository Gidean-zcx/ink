/* A borrowed grayscale surface in the existing UI activity, never a second UI loop. */
#ifndef INK_GRAFT_H
#define INK_GRAFT_H

#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_GRAFT_PAGE, INK_GRAFT_WHOLE, INK_GRAFT_PART } ink_graft_refresh_t;
typedef enum { INK_GRAFT_TAP, INK_GRAFT_HOLD, INK_GRAFT_KEY, INK_GRAFT_SWIPE, INK_GRAFT_SHAKE } ink_graft_input_kind_t;
typedef struct {
    ink_graft_input_kind_t kind;
    int x, y, value;
    bool held;
    ink_source_t source;
} ink_graft_input_t;

typedef struct {
    const char *app;
    /* Start a clean member session; return false if its resources cannot be acquired. */
    bool (*open)(void *ctx, const ink_member_identity_t *member, int width, int height);
    /* Cancel background work, close the book and discard all member globals before returning. */
    void (*close)(void *ctx);
    /* Stop input and optional prepainting without changing the current page. */
    void (*pause)(void *ctx);
    /* Resume the same member session after the shell interruption ends. */
    void (*resume)(void *ctx);
    /* Resolve native hit regions into calls to the same registered capabilities used by AI. */
    bool (*input)(void *ctx, const ink_graft_input_t *event);
    /* Describe book, chapter, stable source position, visible text and panel; never credentials. */
    int (*snapshot)(void *ctx, char *out, size_t cap);
    void *ctx;
} ink_graft_client_t;

/* UI only: mount one client under the shell status bar, closing the previous client first. */
int ink_graft_attach(lv_obj_t *screen, const ink_graft_client_t *client);
/* UI only: close the client, release its surface and erase the snapshot. */
void ink_graft_detach(void);
/* UI only: copy finished L8 pixels, invalidate the clipped dirty region, and return on acceptance. */
int ink_graft_present(const uint8_t *pixels, int stride, const lv_area_t *dirty, ink_graft_refresh_t refresh);
/* UI only: update pause/resume state after shell or foreground changes. */
void ink_graft_settle(void);
/* UI only: forward an application key; AI and power keys remain owned by the shell. */
bool ink_graft_key(ink_key_t key, bool held);
/* UI only: forward a swipe already admitted by the shell. */
bool ink_graft_swipe(ink_swipe_dir_t direction);
/* UI only: forward an admitted shake as a semantic previous/next request. */
bool ink_graft_shake(int direction);
/* UI only: return the active member's page context, or an empty string when stale. */
int ink_graft_snapshot(char *out, size_t cap);
/* UI only: check that the mounted session still owns the current member incarnation. */
bool ink_graft_current(void);
/* BG only: open a virtual member file for the captured session; stale sessions fail closed. */
int ink_graft_file_open(const ink_member_identity_t *owner, const char *path, int mode, ink_fs_file_t **out);
/* BG only: open the client's member-scoped preference collection; binary records may hold native NVS values. */
ink_db_t *ink_graft_preferences_open(const ink_member_identity_t *owner, const char *app, const ink_db_def_t *def);

#ifdef __cplusplus
}
#endif

#endif
