/* Measure the product's real launcher tiles, with an already configured device. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

typedef struct { const char *mark, *tile, *page, *content; bool apps; } entry_t;
static const entry_t ENTRIES[] = {
    {"open.settings", "设置", "Settings", "显示和亮度", false},
    {"open.gallery", "相册", "Gallery", "还没有照片，点右上角拍一张", false},
    {"open.files", "文件", "Files", "我的", true},
    {"open.sms", "短信", "Sms", "还没有短信", true},
    {"open.ai", "对话记录", "AiHome", "还没有对话记录", true},
};
static const entry_t *entry;
static int x, y;
static bool readable;

static lv_obj_t *label(lv_obj_t *o, const char *text) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text) && lv_obj_is_visible(o)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *found = label(lv_obj_get_child(o, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}

static void rendered(lv_event_t *event) {
    (void)event;
    const entry_t *e = __atomic_load_n(&entry, __ATOMIC_ACQUIRE);
    if (!e || __atomic_load_n(&readable, __ATOMIC_ACQUIRE) || strcmp(ink_ui_page_name(), e->page)) return;
    lv_obj_t *root = lv_screen_active();
    if (!label(root, e->content) || (!strcmp(e->page, "Gallery") && label(root, "正在读取照片"))) return;
    ink_probe_mark("open.readable_frame");
    __atomic_store_n(&readable, true, __ATOMIC_RELEASE);
}

static void setup(void) {
    lv_display_add_event_cb(lv_display_get_default(), rendered, LV_EVENT_REFR_READY, NULL);
}

static void locate(void) {
    x = -1;
    lv_obj_t *text = label(lv_screen_active(), __atomic_load_n(&entry, __ATOMIC_ACQUIRE)->tile);
    if (!text) return;
    int32_t i = (int32_t)lv_obj_get_index(text);
    lv_obj_t *tile = i > 0 ? lv_obj_get_child(lv_obj_get_parent(text), i - 1) : NULL;
    const char *cap = tile ? ink_shell_bound_cap(tile) : NULL;
    if (!cap || strcmp(cap, "shell.launch")) return;
    lv_area_t a;
    lv_obj_get_coords(tile, &a);
    x = (a.x1 + a.x2) / 2;
    y = (a.y1 + a.y2) / 2;
}

static bool idle(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool ready(void) { return __atomic_load_n(&readable, __ATOMIC_ACQUIRE) && idle(); }
static bool navigate(const char *cap, const char *args) {
    return BY_HAND(cap, args) && g_rc.status == INK_RC_DONE && WAIT_FOR(idle(), 12000);
}

static void frame_hash(void *out) {
    int w, h;
    ink_display_native_size(&w, &h);
    const uint8_t *fb = ink_display_fb();
    uint32_t hash = UINT32_C(2166136261);
    for (size_t i = 0; i < (size_t)w * h; i++) hash = (hash ^ fb[i]) * UINT32_C(16777619);
    *(uint32_t *)out = hash;
}

static void run(void *unused) {
    (void)unused;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens");
    on_ui(setup);
    CHECK(WAIT_FOR(idle(), 12000), "home settles");
    if (!getenv("INK_OPEN_SEED")) for (size_t i = 0; i < sizeof ENTRIES / sizeof *ENTRIES; i++) {
        const entry_t *e = &ENTRIES[i];
        if (i) CHECK(navigate("system.home", NULL), "return to home");
        if (e->apps) CHECK(navigate("shell.search", "{\"query\":\"\"}"), "show unfiltered Apps");
        __atomic_store_n(&readable, false, __ATOMIC_RELEASE);
        __atomic_store_n(&entry, e, __ATOMIC_RELEASE);
        on_ui(locate);
        for (int p = 0; x < 0 && e->apps && p < 6; p++) {
            if (!navigate("home.next", NULL)) break;
            on_ui(locate);
        }
        CHECK(x >= 0, "visible launch tile: %s", e->tile);
        if (x < 0) break;
        CHECK(WAIT_FOR(idle(), 12000), "launcher is idle");
        uint32_t failures = ink_display_failures(), phases = sim_display_phase_seq();
        ink_probe_mark(e->mark);
        tap(x, y);
        CHECK(WAIT_FOR(ready(), 12000), "%s content rendered and display completed", e->page);
        CHECK(ink_display_failures() == failures, "%s refresh succeeds", e->page);
        ink_probe_value("open.phases", sim_display_phase_seq() - phases);
        uint32_t hash;
        ink_display_lock();
        vtk_host_call(frame_hash, &hash);
        ink_display_unlock();
        ink_probe_value("open.frame_hash", hash);
        ink_probe_mark("open.end");
        __atomic_store_n(&entry, NULL, __ATOMIC_RELEASE);
    }
    ink_probe_count("test.finished");
    vtk_stop();
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "test.open-app", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start app measurement");
}
