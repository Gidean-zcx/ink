/* write-defer-1: typing on the keyboard while the writes it causes (the text draft per key, the memo, the log) wait
 * out the seconds someone is typing. A real finger (touch controller model, input service, LVGL keyboard) types into
 * the memo editor, every erase at the datasheet's worst (--flash-timing max, 240 ms: a tap that falls whole into one
 * is never read). INK_WD_PHASE:
 *   type         20 keys at 3 per second, then 20 at 1 per second: every key arrives once, in order; the draft is on
 *                the flash after the typing; log lines made during the typing reach the flash only after it;
 *   cut-pause    10 keys at 1 per second, then the hands stop; the power fails PAUSE_CUT_MS after the last lift;
 *   after-pause  the boot after it (same flash): all 10 keys are in the draft, and the memo the editor saved is there;
 *   cut-run      40 keys at 1 per second without a pause; the power fails 0.5 s before the last press;
 *   after-run    the draft holds what was typed before the storage service's typing limit (MAX_DEFER_US, 30 s) ran out. */
#include <stdlib.h>
#include "ink/hal_log.h"
#include "ink/sys/data.h"
#include "ink/sys/input.h"
#include "ink/sys/log.h"
#include "ink/sys/storage.h"
#include "ink/sys/textfield.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"
#define key ink_test_keyboard_key
#include "../../system/ui/shell/keyboard.c"
#undef key

#define MAX_KEYS 40
/* After the last lift: the 2 s the storage service waits (TYPING_US), then the draft's batch, about 3.5 s for 10 keys
 * at the worst erase time (each key's draft is a whole 4 KiB record). */
#define PAUSE_CUT_MS 6500
static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmn";
_Static_assert(sizeof LETTERS - 1 >= MAX_KEYS, "letters");

static char g_phase[16];
static void host_phase(void *unused) { (void)unused; const char *p = getenv("INK_WD_PHASE"); snprintf(g_phase, sizeof g_phase, "%s", p ? p : "type"); }
static bool settled(void) { return !ink_input_touch_held() && !ink_ui_busy() && !ink_display_busy(); }

static int g_kx, g_ky, g_kcode;
static void key_target(void) {
    int i = -1;
    for (int k = 0; k < K.buttons; k++) if (K.codes[k] == g_kcode) i = k;
    if (i < 0) { g_kx = -1; return; }
    lv_obj_t *board = K.footer && i >= 9 ? K.footer : K.board;
    lv_obj_update_layout(K.root);
    lv_area_t bounds; lv_obj_get_coords(board, &bounds);
    lv_area_t a = ((lv_buttonmatrix_t *)board)->button_areas[K.footer && i >= 9 ? i - 9 : i];
    g_kx = bounds.x1 + (a.x1 + a.x2) / 2; g_ky = bounds.y1 + (a.y1 + a.y2) / 2;
}
static void english(void) { if (!ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); }
static char g_value[256];
static void read_value(void) { snprintf(g_value, sizeof g_value, "%s", ink_text_value()); }
static int g_saved;
static void read_saved(void) { g_saved = ink_text_draft_saved(); }

/* The taps are timed events, as a finger is: the test task itself stops in every erase. */
typedef struct { int x, y; bool down; } touch_t;
static touch_t g_touch[2 * MAX_KEYS];
static void touch_cb(void *arg) { touch_t *t = arg; sim_input_touch(0, t->down, t->x, t->y); }
static void cut_cb(void *unused) { (void)unused; sim_storage_power_cut(); }
static uint64_t g_first_press, g_last_release;
static void type(const char *letters, int keys, int interval_ms) {
    for (int i = 0; i < keys && i < MAX_KEYS; i++) {
        g_kcode = letters[i]; on_ui(key_target);
        CHECK(g_kx >= 0, "key %c on the keyboard", letters[i]);
        g_touch[2 * i] = (touch_t){g_kx, g_ky, true};
        g_touch[2 * i + 1] = (touch_t){g_kx, g_ky, false};
    }
    vtk_lock();
    g_first_press = vtk_now() + SIM_MS(5);
    for (int i = 0; i < keys && i < MAX_KEYS; i++) {
        vtk_event_at(g_first_press + SIM_MS(i * interval_ms), touch_cb, &g_touch[2 * i]);
        vtk_event_at(g_first_press + SIM_MS(i * interval_ms + 80), touch_cb, &g_touch[2 * i + 1]);
    }
    g_last_release = g_first_press + SIM_MS((keys - 1) * interval_ms + 80);
    vtk_unlock();
}
static void cut_at(uint64_t t) {
    ink_probe_count("test.finished"); /* the run ends with the power */
    vtk_lock();
    vtk_event_at(t, cut_cb, NULL);
    vtk_unlock();
    delay_ms(60000);
    CHECK(false, "the power was cut");
}

static void open_editor(void) {
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    CHECK(BY_HAND("app.open", "{\"id\":\"memo\"}") && g_rc.status == INK_RC_DONE, "open memo");
    CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open the memo editor");
    on_ui(english);
    CHECK(WAIT_FOR(settled() && !ink_storage_pending(), 20000), "the editor is up, nothing being written");
}

/* Log lines (the flush prepares its next sector: an erase) made during the typing: on the flash only after it. */
static bool log_has(const char *marker) {
    static uint8_t sector[INK_LOG_SECTOR];
    size_t n = strlen(marker);
    for (uint32_t off = 0; off < INK_LOG_BYTES; off += INK_LOG_SECTOR)
        if (hal_log_read(false, off, sector, sizeof sector) && ink_log_valid(sector)) {
            const ink_log_record_t *r = (const void *)sector;
            for (size_t i = sizeof *r; i + n <= sizeof *r + r->len; i++)
                if (!memcmp(sector + i, marker, n)) return true;
        }
    return false;
}

static void burst(const char *what, const char *letters, int interval_ms, const char *expect) {
    double delivered = sim_metric_value("input.touch_delivery_ms");
    type(letters, 20, interval_ms);
    delay_ms(300); /* typing has begun: these lines make the log due, its flush waits for quiet */
    char marker[40]; snprintf(marker, sizeof marker, "write-defer marker %d/s", 1000 / interval_ms);
    for (int i = 0; i < 90; i++) ink_log(INK_LOG_INFO, "test", "%s line %d: making the log due (8 KiB) .........................................", marker, i);
    uint64_t logged = 0;
    while (vtk_now() < g_last_release + SIM_MS(15000) && !logged) {
        delay_ms(100);
        if (log_has(marker)) logged = vtk_now();
    }
    CHECK(WAIT_FOR(settled(), 15000), "%s: the panel settles", what);
    on_ui(read_value);
    CHECK(!strcmp(g_value, expect), "%s: every key arrives once, in order (%.60s)", what, g_value);
    CHECK(sim_metric_value("input.touch_delivery_ms") == delivered + 40, "%s: 20 presses and 20 lifts reach the input service (%.0f)",
          what, sim_metric_value("input.touch_delivery_ms") - delivered);
    CHECK(logged && logged >= g_last_release, "%s: the log written during the typing reaches the flash only after it (%.1f s after the last lift)",
          what, logged ? ((double)logged - (double)g_last_release) / 1e9 : -1.0);
    CHECK(WAIT_FOR((on_ui(read_saved), g_saved == 1), 15000), "%s: the draft is on the flash after the typing", what);
}

/* The boot after a cut: the longest memo editor draft, and how many memos there are. */
static size_t kept(char *text, size_t cap, int *memos) {
    const char *me = ink_member_current()->id;
    const ink_db_def_t *def = ink_db_find_def("text.drafts");
    ink_db_t *db = def ? ink_db_open_member(def, me) : NULL;
    ink_db_cursor_t cur = {0}; ink_rec_t rec;
    text[0] = 0;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) {
        char v[256];
        if (ink_text_draft_read(me, "MemoEdit", rec.id, v, sizeof v) > (int)strlen(text)) snprintf(text, cap, "%s", v);
    }
    ink_db_close(db);
    def = ink_db_find_def("memo.notes");
    db = def ? ink_db_open_member(def, me) : NULL;
    *memos = db ? ink_db_count(db) : -1;
    ink_db_close(db);
    ink_probe_value("test.write_defer.keys_kept", (double)strlen(text));
    return strlen(text);
}

static void run(void *arg) {
    (void)arg;
    vtk_host_call(host_phase, NULL);
    if (!strncmp(g_phase, "after", 5)) {
        CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot after the cut settles");
        char text[256]; int memos = 0;
        size_t n = kept(text, sizeof text, &memos);
        CHECK(!strncmp(text, LETTERS, n), "the draft is what was typed, from the first key (%.40s)", text);
        CHECK(memos >= 1, "the memo the editor saved before the typing is there (%d)", memos);
        if (!strcmp(g_phase, "after-pause"))
            CHECK(n == 10, "the hands stopped %.1f s before the cut: all 10 keys are in the draft (%u)", PAUSE_CUT_MS / 1000.0, (unsigned)n);
        else /* the first batch (the first key's draft) waited the 30 s limit; the next one waits its own (each key's draft is whole) */
            CHECK(n >= 1, "typing without a pause: the batch that waited 30 s is on the flash (%u of 39 keys kept)", (unsigned)n);
        ink_probe_count("test.finished");
        return;
    }
    open_editor();
    if (!strcmp(g_phase, "cut-pause")) {
        type(LETTERS, 10, 1000);
        cut_at(g_last_release + SIM_MS(PAUSE_CUT_MS));
        return;
    }
    if (!strcmp(g_phase, "cut-run")) {
        type(LETTERS, 40, 1000);
        cut_at(g_first_press + SIM_MS(38500));
        return;
    }
    burst("3 keys a second", "thequickbrownfoxjump", 333, "thequickbrownfoxjump");
    burst("1 key a second", "abcdefghijklmnopqrst", 1000, "thequickbrownfoxjumpabcdefghijklmnopqrst");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "write-defer", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
