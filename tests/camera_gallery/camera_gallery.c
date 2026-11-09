/* Real pages and bus, synthetic camera frames and member data only. */
#include <stdlib.h>
#include "../../system/apps/camera/camera.c"
#include "../../system/apps/gallery/gallery.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "image_fixtures.h"

static char phase[16], scene[16], snapshot[4096], saved_path[INK_FS_PATH_MAX];
static uint32_t saved_id;
static const char *wanted_cap;
static lv_point_t point;
static bool found;
static uint8_t last_gray;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy() && !__atomic_load_n(&taking, __ATOMIC_ACQUIRE); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "page and camera settle"); }
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page has a full AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "snapshot fits the actual AI context buffer");
    CHECK(!strcmp(snapshot, g_snap), "AI and UI share the full snapshot");
}
static void locate_bound(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    const char *bound = ink_shell_bound_cap(root);
    if (bound && !strcmp(bound, wanted_cap)) {
        lv_area_t a; lv_obj_get_coords(root, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate_bound(lv_obj_get_child(root, (int32_t)i));
}
static void locate_button(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate_bound(lv_screen_active()); }
static void press_cap(const char *cap) {
    wanted_cap = cap; on_ui(locate_button); CHECK(found, "visible button calls %s", cap);
    if (found) tap(point.x, point.y);
    quiet();
}
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s succeeds (%s: %s)", cap, ink_rc_name(g_rc.status), g_rc.detail); quiet();
}
static unsigned count_images(void) {
    uint32_t cursor = 0; ink_fs_entry_t e; int r; unsigned n = 0;
    while ((r = ink_fs_index("/me", INK_FS_IMAGE, &cursor, &e)) == 1) n++;
    CHECK(r == 0, "image index query ends normally"); return n;
}
static void get_saved(void) {
    uint32_t cursor = 0; ink_fs_entry_t e;
    CHECK(ink_fs_index("/me/照片", INK_FS_IMAGE, &cursor, &e) == 1, "photo has a stable media index entry");
    saved_id = e.id; snprintf(saved_path, sizeof saved_path, "%s", e.path);
    ink_fs_file_t *f = NULL; CHECK(ink_fs_open(e.path, INK_FS_READ, &f) == 0, "saved photo opens from storage card");
    uint8_t signature[8];
    CHECK(f && ink_fs_read(f, signature, sizeof signature) == sizeof signature && !memcmp(signature, "\x89PNG\r\n\x1a\n", 8), "card contains a real PNG");
    if (f) CHECK(ink_fs_close(f) == 0, "photo file closes");
}
static bool pictures_ready(void) {
    if (!G.list || G.count != 10) return false;
    size_t visible = G.position.page ? 1 : 9;
    for (size_t i = 0; i < visible; i++) if (!G.slots[i] || !lv_obj_get_child_count(G.slots[i])) return false;
    return true;
}
static bool ready;
static void inspect_grid(void) { ready = pictures_ready(); }
static void grid_ready(void) {
    for (unsigned i = 0; i < 500; i++) { on_ui(inspect_grid); if (ready) break; delay_ms(20); }
    CHECK(ready, "all visible F4 thumbnail slots loaded: page=%s top=%s count=%zu position=%zu", ink_ui_page_name(), ink_ui_top_name(), G.count, G.position.page); quiet();
}
static void gray_pixels(const ink_image_t *im) {
    CHECK(im->pixels && im->width && im->height, "image has owned pixels");
    bool gray4 = im->pixels != NULL;
    for (size_t i = 0; im->pixels && i < (size_t)im->width * im->height; i++) if (im->pixels[i] % 17) gray4 = false;
    CHECK(gray4, "every displayed pixel belongs to the screen's 16 gray levels");
}
static void inspect_preview(void) {
    CHECK(view && lv_obj_get_child_count(view) == 1, "real Camera page presents a frame");
    if (view && lv_obj_get_child_count(view)) {
        lv_obj_t *obj = lv_obj_get_child(view, 0); const lv_image_dsc_t *d = lv_image_get_src(obj);
        CHECK(d && d->header.w == 640 && d->header.h == 480, "viewfinder uses the actual VGA camera dimensions");
        if (d) { ink_image_t im = {d->header.w, d->header.h, (uint8_t *)d->data}; gray_pixels(&im); }
    }
    read_snapshot(); CHECK(strstr(snapshot, "camera.photo") && strstr(snapshot, "camera.preview"), "Camera exposes the shutter and viewfinder capabilities");
}
static void inspect_last_thumb(void) {
    CHECK(G.position.page == 1, "last F4 page selected");
    lv_obj_t *obj = G.slots[0] ? lv_obj_get_child(G.slots[0], 0) : NULL;
    const lv_image_dsc_t *d = obj ? lv_image_get_src(obj) : NULL;
    CHECK(d && d->header.w == 128 && d->header.h == 128, "file.thumb dimensions reach the actual grid");
    bool correct = d != NULL;
    if (d) for (unsigned i = 0; i < d->data_size; i++) if (d->data[i] != last_gray) correct = false;
    CHECK(correct, "last fixture has every thumbnail pixel correct for its actual indexed file");
    read_snapshot(); CHECK(strstr(snapshot, "\"page\":2") && strstr(snapshot, "\"visible\":1") && !strstr(snapshot, "gallery.next"), "last snapshot has exact pagination and no next action");
}
static bool photo_ready;
static void inspect_photo_ready(void) { photo_ready = G.box && lv_obj_get_child_count(G.box) && G.path[0]; }
static void wait_photo(void) {
    for (unsigned i = 0; i < 500; i++) { on_ui(inspect_photo_ready); if (photo_ready) break; delay_ms(20); }
    CHECK(photo_ready, "photo viewer loads a real indexed file"); quiet();
}
static void inspect_photo(void) {
    const lv_image_dsc_t *d = G.box && lv_obj_get_child_count(G.box) ? lv_image_get_src(lv_obj_get_child(G.box, 0)) : NULL;
    CHECK(d && d->header.w == 640 && d->header.h == 480, "Photo displays the original VGA dimensions");
    if (d) { ink_image_t im = {d->header.w, d->header.h, (uint8_t *)d->data}; gray_pixels(&im); }
    read_snapshot(); CHECK(strstr(snapshot, saved_path) && strstr(snapshot, "file.delete") && strstr(snapshot, "shell.ai_key"), "Photo snapshot exposes the displayed file and the same buttons");
}
static void first_slot(void) {
    lv_obj_t *slot = G.slots[0]; lv_area_t a; lv_obj_get_coords(slot, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}
static void take_scene(void *arg) {
    (void)arg; CHECK(settled(), "comparison screenshot is settled");
    ink_probe_count("test.snapshot.settled");
    char key_name[48]; snprintf(key_name, sizeof key_name, "test.snapshot.%s", scene); ink_probe_count(key_name);
    sim_display_snapshot("scene");
}
static void capture(const char *name) {
    if (strcmp(scene, name)) return;
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), take_scene, NULL); delay_ms(5);
}
static void fixtures(void) {
    CHECK(ink_fs_mkdir("/me/SYNTH") == 0, "create synthetic image folder");
    for (unsigned i = 0; i < 8; i++) {
        char path[64]; snprintf(path, sizeof path, "/me/SYNTH/%u.png", i);
        const uint8_t *data = i == 7 ? white_png : black_png; size_t bytes = i == 7 ? sizeof white_png : sizeof black_png;
        CHECK(ink_fs_write_atomic(path, data, bytes) == 0, "write unambiguous black/white fixture");
    }
    CHECK(count_images() == 10, "two saved photos plus eight synthetic fixtures");
}
static void member_isolation(void) {
    CHECK(ink_member_switch("kid") == 0, "enter the synthetic child's space"); quiet();
    ok("app.open", "{\"id\":\"gallery\"}"); delay_ms(500); quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, "\"total\":0") && !strstr(snapshot, saved_path), "child gallery contains no adult media or names");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", saved_id);
    CHECK(AI("photo.open", args) && g_rc.status == INK_RC_REJECTED, "child cannot open an adult photo by known id");
    CHECK(!strcmp(ink_ui_page_name(), "Gallery"), "rejected cross-member photo never opens a viewer");
    CHECK(AI("file.thumb", args) && g_rc.status == INK_RC_REJECTED, "file.thumb also enforces child isolation");
    ok("app.open", "{\"id\":\"camera\"}"); ok("camera.photo", NULL);
    CHECK(count_images() == 1, "child capture is saved only in the child's own media index");
    ok("app.open", "{\"id\":\"gallery\"}"); delay_ms(500); quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, "\"total\":1"), "child can see their own captured photo");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult"); quiet();
    CHECK(count_images() == 10, "adult media remains independent of the child's capture");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "restart")) {
        CHECK(count_images() == 9, "nine adult photos remain across process restart after one deletion"); get_saved();
        ok("app.open", "{\"id\":\"gallery\"}"); delay_ms(500); quiet(); on_ui(read_snapshot);
        CHECK(strstr(snapshot, "\"total\":9"), "restarted Gallery rebuilds from persisted files and index");
    } else {
        ok("app.open", "{\"id\":\"camera\"}"); quiet();
        CHECK(WAIT_FOR(!__atomic_load_n(&taking, __ATOMIC_ACQUIRE), 5000), "initial viewfinder finishes"); on_ui(inspect_preview);
        ok("camera.preview", NULL); on_ui(inspect_preview); capture("Camera");
        press_cap("camera.photo"); CHECK(count_images() == 1, "touch shutter saves exactly one photo");
        ok("camera.photo", NULL); CHECK(count_images() == 2, "AI shutter uses the same durable save path"); get_saved();
        fixtures(); ok("app.open", "{\"id\":\"gallery\"}"); grid_ready(); on_ui(read_snapshot);
        CHECK(strstr(snapshot, "\"total\":10") && strstr(snapshot, "\"visible\":9") && strstr(snapshot, "photo.open"), "F4 exposes nine visible items, real IDs and the next page"); capture("Gallery");
        key(INK_KEY_NEXT); grid_ready();
        ink_fs_entry_t last; CHECK(ink_fs_lookup(G.ids[9], &last) == 0, "resolve the actual final fixture id");
        last_gray = strstr(last.path, "/7.png") ? 255 : 0; on_ui(inspect_last_thumb);
        ok("gallery.prev", NULL); grid_ready(); on_ui(first_slot); tap(point.x, point.y); wait_photo(); on_ui(inspect_photo); capture("Photo");
        press_cap("shell.back"); grid_ready();
        char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", saved_id); ok("photo.open", args); wait_photo(); on_ui(inspect_photo);
        CHECK(AI("file.delete", G.delete_args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI deletion uses the existing irreversible-delete confirmation");
        unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
        CHECK(ink_confirm_decide(token, true, INK_SRC_AI) != 0, "AI cannot approve its own photo deletion");
        CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "person can cancel deletion"); quiet();
        ink_fs_stat_t st; CHECK(ink_fs_stat(saved_path, &st) == 0, "cancel keeps the actual photo on card");
        press_cap("shell.back"); grid_ready(); member_isolation();
        ok("app.open", "{\"id\":\"gallery\"}"); grid_ready(); ok("photo.open", args); wait_photo();
        press_cap("file.delete");
        ink_confirm_view_t card; CHECK(ink_confirm_list(&card, 1) == 1, "touch deletion also opens a confirmation card");
        CHECK(ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "person approves deletion");
        CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "Gallery"), 5000), "confirmed deletion returns to Gallery"); quiet();
        CHECK(ink_fs_stat(saved_path, &st) == INK_FS_ERR_NOT_FOUND && count_images() == 9, "confirmed photo is absent from card and index");
    }
    ok("system.home", NULL); ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "persist index and member data");
    ink_probe_count("test.finished");
}
static void host_options(void *arg) {
    (void)arg; const char *p = getenv("INK_CAMERA_PHASE"), *s = getenv("INK_CAMERA_SCENE");
    snprintf(phase, sizeof phase, "%s", p ? p : "write"); snprintf(scene, sizeof scene, "%s", s ? s : "");
}
void ink_main(void) {
    vtk_host_call(host_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "camera-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test-only driver");
}
