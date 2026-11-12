#include "graft.h"
#include "ink/osal.h"
#include "ink/ui_shell.h"
#include "ink/ui_theme.h"
#include <string.h>

static struct {
    const ink_graft_client_t *client;
    ink_member_identity_t member;
    lv_obj_t *image, *status;
    lv_image_dsc_t descriptor;
    uint8_t *pixels;
    int width, height, top;
    bool paused;
} G;

bool ink_graft_current(void) {
    return G.client && G.member.epoch == ink_member_epoch() && ink_member_space_open();
}

static bool admit(const ink_graft_input_t *event) {
    ink_graft_settle();
    return ink_graft_current() && !G.paused && G.client->input && G.client->input(G.client->ctx, event);
}

static void touched(lv_event_t *event) {
    lv_indev_t *input = lv_indev_active();
    if (!input) return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    ink_graft_input_t e = {.kind = lv_event_get_code(event) == LV_EVENT_LONG_PRESSED ? INK_GRAFT_HOLD : INK_GRAFT_TAP,
        .x = point.x, .y = point.y - G.top, .source = INK_SRC_TOUCH};
    if (e.x >= 0 && e.x < G.width && e.y >= 0 && e.y < G.height) (void)admit(&e);
}

void ink_graft_detach(void) {
    const ink_graft_client_t *client = G.client;
    G.client = NULL;
    if (client && client->close) client->close(client->ctx);
    if (G.image) {
        lv_obj_delete(G.image);
    }
    if (G.status) lv_obj_delete(G.status);
    osal_free(G.pixels);
    memset(&G, 0, sizeof G);
}

int ink_graft_attach(lv_obj_t *screen, const ink_graft_client_t *client) {
    if (!screen || !client || !client->app || !client->open || !client->close || !client->snapshot) return -1;
    ink_graft_detach();
    ink_member_identity(&G.member);
    if (!ink_member_space_open()) return -1;
    G.width = ink_ui_width();
    G.top = ink_ui_px(INK_STATUS_BAR);
    G.height = ink_ui_height() - G.top;
    if (G.width <= 0 || G.height <= 0) return -1;
    size_t bytes = (size_t)G.width * (size_t)G.height;
    G.pixels = osal_malloc(bytes, OSAL_MEM_EXTERNAL);
    if (!G.pixels) return -1;
    memset(G.pixels, 255, bytes);
    G.client = client;
    if (!client->open(client->ctx, &G.member, G.width, G.height) || !ink_graft_current()) {
        ink_graft_detach();
        return -1;
    }
    G.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    G.descriptor.header.cf = LV_COLOR_FORMAT_L8;
    G.descriptor.header.w = (uint32_t)G.width;
    G.descriptor.header.h = (uint32_t)G.height;
    G.descriptor.header.stride = (uint32_t)G.width;
    G.descriptor.data_size = (uint32_t)bytes;
    G.descriptor.data = G.pixels;
    G.image = lv_image_create(screen);
    lv_image_set_src(G.image, &G.descriptor);
    lv_obj_set_pos(G.image, 0, G.top);
    lv_obj_add_flag(G.image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(G.image, touched, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(G.image, touched, LV_EVENT_LONG_PRESSED, NULL);
    G.status = ink_shell_statusbar(screen, true);
    return 0;
}

void ink_graft_settle(void) {
    if (!G.client) return;
    if (!ink_graft_current()) {
        ink_graft_detach();
        return;
    }
    bool paused = ink_ui_overlay_up() || (G.image && lv_obj_get_screen(G.image) != lv_screen_active());
    if (paused == G.paused) return;
    G.paused = paused;
    if (paused && G.client->pause) G.client->pause(G.client->ctx);
    if (!paused && G.client->resume) G.client->resume(G.client->ctx);
}

int ink_graft_present(const uint8_t *pixels, int stride, const lv_area_t *dirty, ink_graft_refresh_t refresh) {
    if (!pixels || !dirty || !ink_graft_current() || !G.image || stride < G.width ||
        (unsigned)refresh > INK_GRAFT_PART) return -1;
    lv_area_t area = *dirty;
    if (area.x1 < 0) area.x1 = 0;
    if (area.y1 < 0) area.y1 = 0;
    if (area.x2 >= G.width) area.x2 = G.width - 1;
    if (area.y2 >= G.height) area.y2 = G.height - 1;
    if (area.x1 > area.x2 || area.y1 > area.y2) return -1;
    for (int y = area.y1; y <= area.y2; y++)
        memcpy(G.pixels + (size_t)y * (size_t)G.width + area.x1,
               pixels + (size_t)y * (size_t)stride + area.x1, (size_t)(area.x2 - area.x1 + 1));
    area.y1 += G.top;
    area.y2 += G.top;
    /* Whole is a drawing intent; only display_service decides when GC16 is due. */
    ink_ui_hint_area(&area, refresh == INK_GRAFT_PAGE ? INK_REFRESH_TEXT : INK_REFRESH_UI);
    lv_obj_invalidate_area(G.image, &area);
    ink_ui_content_changed();
    return 0;
}

bool ink_graft_key(ink_key_t key, bool held) {
    if (key == INK_KEY_AI || key == INK_KEY_POWER || key == INK_KEY_HOME) return false;
    ink_graft_input_t event = {.kind = INK_GRAFT_KEY, .value = key, .held = held, .source = INK_SRC_KEY};
    return admit(&event);
}

bool ink_graft_swipe(ink_swipe_dir_t direction) {
    ink_graft_input_t event = {.kind = INK_GRAFT_SWIPE, .value = direction, .source = INK_SRC_TOUCH};
    return admit(&event);
}

bool ink_graft_shake(int direction) {
    if (direction != -1 && direction != 1) return false;
    ink_graft_input_t event = {.kind = INK_GRAFT_SHAKE, .value = direction, .source = INK_SRC_KEY};
    return admit(&event);
}

int ink_graft_snapshot(char *out, size_t cap) {
    if (!out || !cap) return -1;
    out[0] = 0;
    if (!ink_graft_current()) return -1;
    return G.client->snapshot(G.client->ctx, out, cap);
}

static bool owner_current(const ink_member_identity_t *owner) {
    ink_member_identity_t now;
    ink_member_identity(&now);
    return owner && owner->epoch == now.epoch && !strcmp(owner->member, now.member) && ink_member_space_open();
}

int ink_graft_file_open(const ink_member_identity_t *owner, const char *path, int mode, ink_fs_file_t **out) {
    if (!out) return INK_FS_ERR_PATH;
    *out = NULL;
    if (!owner_current(owner)) return INK_FS_ERR_DENIED;
    if (!path || strncmp(path, "/me/", 4)) return INK_FS_ERR_PATH;
    return ink_fs_open_for(owner->member, path, mode, out);
}

ink_db_t *ink_graft_preferences_open(const ink_member_identity_t *owner, const char *app, const ink_db_def_t *def) {
    if (!owner_current(owner) || !app || !def || !def->name || def->scope != INK_DB_MEMBER) return NULL;
    size_t n = strlen(app);
    if (!n || strncmp(def->name, app, n) || def->name[n] != '.') return NULL;
    ink_db_t *db = ink_db_open(def, owner->member);
    if (db && !owner_current(owner)) {
        ink_db_close(db);
        return NULL;
    }
    return db;
}
