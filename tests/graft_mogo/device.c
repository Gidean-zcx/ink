/* Test the actual carrier with an explicitly synthetic client, not the Mogo application. */
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/appreg.h"
#include "ink/ui_theme.h"

static struct {
    ink_member_identity_t owner;
    uint8_t *pixels;
    int width, height, page, opened, closed, paused, resumed, inputs;
    int present_result;
} T;
static char snapshot[256];
static lv_obj_t *overlay;
static const ink_ui_overlay_t CARD = {.name = "GraftProbeOverlay", .level = 90};
static const ink_db_def_t PREFS = {.name = "graftprobe.settings", .title = "嫁接测试偏好",
    .scope = INK_DB_MEMBER, .schema = 1, .max_records = 1, .max_bytes = 64};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(),15000), "carrier settles"); }
static void draw(void) {
    memset(T.pixels, (T.page & 1) ? 0x99 : 0xdd, (size_t)T.width * (size_t)T.height);
    lv_area_t area = {0,0,T.width-1,T.height-1};
    T.present_result = ink_graft_present(T.pixels,T.width,&area,INK_GRAFT_PAGE);
}
static void next(ink_call_t *call) {
    if (!ink_graft_current()) { call->rc.status = INK_RC_STALE; return; }
    T.page++;
    draw();
    call->rc.revision = (uint32_t)T.page;
}
static const ink_cap_t NEXT = {.id = "graftprobe.next", .app = "graftprobe", .title = "测试翻页",
    .child_ok = true, .effect = INK_FX_NAVIGATE, .offline = INK_OFFLINE_LOCAL, .exec = INK_EXEC_UI, .run = next};
static bool opened(void *ctx, const ink_member_identity_t *owner, int w, int h) {
    (void)ctx;
    T.owner = *owner;
    T.width = w;
    T.height = h;
    T.page = 0;
    T.pixels = osal_malloc((size_t)w * (size_t)h,OSAL_MEM_EXTERNAL);
    T.opened++;
    return T.pixels != NULL;
}
static void closed(void *ctx) {
    (void)ctx;
    osal_free(T.pixels);
    T.pixels = NULL;
    memset(&T.owner,0,sizeof T.owner);
    T.page = 0;
    T.closed++;
}
static void paused(void *ctx) { (void)ctx; T.paused++; }
static void resumed(void *ctx) { (void)ctx; T.resumed++; }
static bool input(void *ctx, const ink_graft_input_t *event) {
    (void)ctx;
    T.inputs++;
    ink_call_opts_t opts = {.src = event->source};
    return ink_cap_call("graftprobe.next",NULL,&opts) == 0;
}
typedef struct { char *out; size_t cap; int length; } description_t;
static void describe_host(void *ctx) {
    description_t *d = ctx;
    d->length = snprintf(d->out,d->cap,"合成测试画面 member=%s page=%d",T.owner.member,T.page);
}
static int describe(void *ctx, char *out, size_t cap) {
    (void)ctx;
    description_t d = {.out = out,.cap = cap};
    vtk_host_call(describe_host,&d);
    return d.length;
}
static const ink_graft_client_t CLIENT = {.app = "graftprobe", .open = opened, .close = closed,
    .pause = paused, .resume = resumed, .input = input, .snapshot = describe};
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    CHECK(!ink_graft_attach(screen,&CLIENT),"mount synthetic carrier client");
    if (T.pixels) draw();
}
static const ink_activity_t ACTIVITY = {.name = "GraftProbe", .app = "graftprobe", .create = create,
    .destroy = ink_graft_detach, .on_key = ink_graft_key, .on_swipe = ink_graft_swipe,
    .snapshot = ink_graft_snapshot, .settle = ink_graft_settle};
static const ink_app_def_t APP = {.id = "graftprobe", .title = "嫁接测试", .entry = &ACTIVITY,
    .system_app = true, .child_ok = true};
static void start(void) {
    CHECK(!ink_app_register(&APP),"register probe app");
    CHECK(!ink_cap_register(&NEXT,1),"register probe action");
    CHECK(!ink_app_open("graftprobe",NULL,NULL),"open carrier activity");
}
static void inspect(void) {
    CHECK(ink_graft_snapshot(snapshot,sizeof snapshot) > 0,"snapshot available for current member");
}
static void keys(void) {
    int before = T.inputs;
    CHECK(!ink_graft_key(INK_KEY_AI,false) && !ink_graft_key(INK_KEY_POWER,false) &&
          !ink_graft_key(INK_KEY_HOME,false),"shell keys stay in shell");
    CHECK(T.inputs == before,"shell keys never enter native client");
    CHECK(ink_graft_key(INK_KEY_NEXT,false),"native key calls capability");
}
static void block(void) {
    overlay = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(overlay,200,200);
    ink_ui_overlay_add(&CARD,overlay);
    ink_graft_settle();
    int before = T.inputs;
    CHECK(!ink_graft_key(INK_KEY_NEXT,false) && !ink_graft_swipe(INK_SWIPE_LEFT) &&
          !ink_graft_shake(1),"overlay pauses all native input");
    CHECK(T.inputs == before && T.paused == 1,"one pause callback");
}
static void unblock(void) {
    ink_ui_overlay_remove(&CARD);
    lv_obj_delete(overlay);
    overlay = NULL;
    ink_graft_settle();
    CHECK(T.resumed == 1,"one resume callback");
}
static void dirty(void) {
    lv_area_t empty = {T.width+1,0,T.width+3,3};
    lv_area_t outside = {-10,-10,T.width+10,T.height+10};
    CHECK(ink_graft_present(T.pixels,T.width,&empty,INK_GRAFT_PART) < 0,"reject empty clipped dirty area");
    CHECK(!ink_graft_present(T.pixels,T.width,&outside,INK_GRAFT_WHOLE),"clip full drawing intent");
    CHECK(ink_graft_present(T.pixels,T.width-1,&outside,INK_GRAFT_PART) < 0,"reject short stride");
}
static void go_home(void) { ink_ui_home(); }
static void reopen(void) { CHECK(!ink_app_open("graftprobe",NULL,NULL),"reopen clean carrier session"); }
static void run(void *arg) {
    (void)arg;
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_hold(INK_NEED_NO_LOCK);
    quiet();
    on_ui(start); quiet();
    CHECK(T.width == ink_ui_width() && T.height == ink_ui_height()-ink_ui_px(INK_STATUS_BAR),"surface leaves system status bar");
    on_ui(keys); quiet();
    CHECK(T.page == 1 && !T.present_result,"key and surface presentation complete");
    CHECK(AI("graftprobe.next",NULL) && g_rc.status == INK_RC_DONE,"AI uses same action"); quiet();
    CHECK(T.page == 2 && g_rc.revision == 2,"AI and key share state");
    on_ui(block); quiet();
    on_ui(unblock); quiet();
    on_ui(inspect);
    CHECK(strstr(snapshot,"page=2"),"overlay preserves the underlying page");
    on_ui(dirty); quiet();
    ink_member_identity_t owner = T.owner;
    ink_db_t *db = ink_graft_preferences_open(&owner,"graftprobe",&PREFS);
    CHECK(db != NULL,"member preference collection opens");
    uint32_t id = 0;
    ink_db_write_t value = {.id = 1,.data = "fake-value",.len = 11,.src = INK_SRC_TOUCH};
    CHECK(db && !ink_db_put(db,&value,&id),"native preference persists through data service");
    if (db) {
        ink_db_flush(db);
        CHECK(!ink_db_wait(db,ink_db_last_ver(db),5000000),"preference write becomes durable");
        ink_db_close(db);
    }
    CHECK(!ink_graft_preferences_open(&owner,"other",&PREFS),"preference namespace belongs to app");
    ink_fs_file_t *file = NULL;
    CHECK(ink_graft_file_open(&owner,"/family/test",INK_FS_READ,&file) == INK_FS_ERR_PATH,"carrier restricts native paths to me");
    CHECK(ink_graft_file_open(&owner,"/family/fonts/x.ttf",INK_FS_WRITE|INK_FS_CREATE,&file) == INK_FS_ERR_PATH,"shared fonts are read-only to native apps");
    CHECK(ink_graft_file_open(&owner,"/family/fontsx/x.ttf",INK_FS_READ,&file) == INK_FS_ERR_PATH,"only the fonts folder itself is shared");
    CHECK(!ink_fs_mkdir("/me/graftprobe"),"prepare synthetic member folder");
    CHECK(!ink_graft_file_open(&owner,"/me/graftprobe/fake.txt",INK_FS_WRITE|INK_FS_CREATE,&file),"native member file opens");
    if (file) {
        CHECK(ink_fs_write(file,"fake",4) == 4,"member file written");
        CHECK(!ink_fs_close(file),"member file closes");
    }
    on_ui(go_home); quiet();
    CHECK(T.closed == 1 && !T.pixels,"close releases client resources");
    on_ui(reopen); quiet();
    CHECK(T.opened == 2 && T.page == 0,"reopen starts with cleared native state");
    CHECK(!ink_member_switch("kid"),"switch to synthetic child"); quiet();
    CHECK(WAIT_FOR(T.closed == 2,15000) && !T.pixels,"member switch closes the active client before showing another member");
    CHECK(!ink_graft_preferences_open(&owner,"graftprobe",&PREFS),"old member settings access denied");
    CHECK(ink_graft_file_open(&owner,"/me/graftprobe/fake.txt",INK_FS_READ,&file) == INK_FS_ERR_DENIED,"old member file access denied");
    ink_member_identity(&owner);
    db = ink_graft_preferences_open(&owner,"graftprobe",&PREFS);
    CHECK(db && ink_db_count(db) == 0,"child cannot see adult native preferences");
    if (db) ink_db_close(db);
    CHECK(ink_graft_file_open(&owner,"/me/graftprobe/fake.txt",INK_FS_READ,&file) == INK_FS_ERR_NOT_FOUND,"child cannot see adult book path");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t task = {.name = "test.graft",.fn = run,.stack_bytes = 16384,.priority = OSAL_PRIO_NORMAL,
        .core = -1,.flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&task,NULL) == OSAL_OK,"start carrier checks");
}
