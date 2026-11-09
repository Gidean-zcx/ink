/* A declared one-hour workload and a separate controlled contention probe. */
#include "../services_finish/product.h"
#include "measure.h"
#include <unistd.h>

unsigned ink_test_index_count(void);
bool ink_test_index_idle(void);
static osal_sem_t *probe_sem;
static bool probe_enabled, probe_requested, probe_control, probe_finished, short_run, host_ok;
static unsigned expected_files;
static const ink_db_def_t PROBE = {.name = "SYNTH.index-cost", .scope = INK_DB_DEVICE};
typedef struct {
    const char *path;
    int kind, result;
    size_t len;
    uint64_t queued, begin, end;
} timing_t;
static timing_t current;
static char current_path[200];
static uint64_t flash_before[3];

static void begin_host(void *arg) {
    current = *(timing_t *)arg;
    sim_copy_name(current_path, sizeof current_path, current.path);
    current.path = current_path;
    ink_cost_flash_counts(flash_before);
}
void ink_cost_begin(const char *path, int kind, size_t len, uint64_t queued, uint64_t begin) {
    timing_t t = {.path = path, .kind = kind, .len = len, .queued = queued, .begin = begin};
    vtk_host_call(begin_host, &t);
    if (probe_enabled && kind == 0 && !strcmp(path, "/user/sys/file.index.snap")) {
        probe_enabled = false;
        osal_sem_give(probe_sem);
    }
}
static void end_host(void *arg) {
    timing_t *end = arg;
    uint64_t flash[3];
    ink_cost_flash_counts(flash);
    printf("COST {\"type\":\"storage\",\"path\":\"%s\",\"job\":%d,\"bytes\":%zu,"
           "\"queued_us\":%llu,\"begin_us\":%llu,\"end_us\":%llu,\"result\":%d,"
           "\"erases\":%llu,\"programs\":%llu,\"program_bytes\":%llu}\n",
           current.path, current.kind, current.len, (unsigned long long)current.queued,
           (unsigned long long)current.begin, (unsigned long long)end->end, end->result,
           (unsigned long long)(flash[0] - flash_before[0]), (unsigned long long)(flash[1] - flash_before[1]),
           (unsigned long long)(flash[2] - flash_before[2]));
}
void ink_cost_end(int result) {
    timing_t t = {.end = osal_now_us(), .result = result};
    vtk_host_call(end_host, &t);
}
static void wait_host(void *arg) {
    timing_t *t = arg;
    printf("COST {\"type\":\"wait\",\"name\":\"%s\",\"begin_us\":%llu,\"end_us\":%llu,\"result\":%d}\n",
           t->path, (unsigned long long)t->begin, (unsigned long long)t->end, t->result);
}
void ink_cost_wait(const char *name, uint64_t begin, uint64_t end, int result) {
    timing_t t = {.path = name, .begin = begin, .end = end, .result = result};
    vtk_host_call(wait_host, &t);
}
static void marker_host(void *arg) {
    printf("COST {\"type\":\"phase\",\"name\":\"%s\",\"begin_us\":%llu}\n",
           (const char *)arg, (unsigned long long)(vtk_now() / 1000));
}
static void phase(const char *name) { vtk_host_call(marker_host, (void *)name); }
static bool ready(void) {
    char s[32];
    return ink_state_get_str(ink_state_find("file.index_state"), s, sizeof s) && !strcmp(s, "ready");
}
static bool completed(void) {
    return ready() && ink_test_index_idle() && ink_db_idle() && !ink_storage_pending() &&
           !ink_ui_busy() && !ink_display_busy() && hal_fs_size("/content/.file-index-dirty") < 0;
}
static void settle(void) { CHECK(WAIT_FOR(completed(), 40000), "index and accepted writes settle"); }
static void until(unsigned seconds) {
    uint64_t target = (uint64_t)seconds * 1000000ull, now = osal_now_us();
    CHECK(now < target, "workload keeps its declared schedule at %u s", seconds);
    while ((now = osal_now_us()) < target) {
        ink_power_activity(now);
        osal_task_delay_us(target - now > 10000000 ? 10000000 : (int64_t)(target - now));
    }
}
static void probe(void *arg) {
    (void)arg;
    ink_db_t *db = ink_db_open(&PROBE, NULL);
    CHECK(db != NULL, "open foreground collection before contention");
    osal_sem_take(probe_sem, OSAL_WAIT_FOREVER);
    phase("probe-request");
    ink_db_write_t w = {.data = "SYNTH", .len = 5, .flags = INK_DB_LAZY};
    CHECK(db && !ink_db_put(db, &w, NULL), "accept synthetic foreground write");
    CHECK(db && !ink_db_wait(db, ink_db_last_ver(db), 15000000), "foreground probe becomes durable");
    ink_db_close(db);
    probe_finished = true;
}
static void command(const char *id, const char *args) {
    ink_power_activity(osal_now_us());
    CHECK(BY_HAND(id, args) && g_rc.status == INK_RC_DONE, "%s: %s %.100s", id, ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 15000), "capability UI completes before next input");
}
static void photo(void) {
    command("app.open", "{\"id\":\"camera\"}");
    command("camera.photo", NULL);
    command("system.home", NULL);
    settle();
}
static void note(unsigned id) {
    char args[32];
    command("app.open", "{\"id\":\"memo\"}");
    if (id) {
        snprintf(args, sizeof args, "{\"id\":%u}", id);
        command("memo.edit", args);
    } else command("memo.edit", NULL);
    CHECK(WAIT_FOR(ink_text_focused(), 10000), "memo editor is focused before typing");
    ink_db_t *db = ink_db_open_member(ink_db_find_def("memo.notes"), ink_member_current()->id);
    CHECK(db != NULL, "memo collection is loaded");
    uint32_t before = ink_db_last_ver(db);
    command("text.insert", "{\"text\":\"SYNTHETIC NOTE\"}");
    CHECK(WAIT_FOR(ink_text_draft_saved() == 1 && ink_db_last_ver(db) > before &&
          ink_db_durable_ver(db) == ink_db_last_ver(db), 15000), "changed memo and draft become durable");
    ink_db_close(db);
    command("text.submit", NULL);
    command("system.home", NULL);
    settle();
}
/* The simulated computer edits only its synthetic, currently lent card. */
static void computer_edit(void *arg) {
    (void)arg;
    char path[700], other[700];
    host_ok = true;
    for (unsigned i = 100; i < 150; i++) {
        snprintf(path, sizeof path, "%s/content/SYNTH_%04u.txt", sim_sd_root(), i);
        FILE *f = fopen(path, "wb");
        if (!f) { host_ok = false; continue; }
        if (fwrite("SYNTHETIC COMPUTER EDIT", 1, 23, f) != 23) host_ok = false;
        if (fclose(f)) host_ok = false;
    }
    for (unsigned i = 0; i < 6; i++) {
        snprintf(path, sizeof path, "%s/content/SYNTH_%04u.txt", sim_sd_root(), 200 + i);
        if (unlink(path)) host_ok = false;
        snprintf(path, sizeof path, "%s/content/SYNTH_COMPUTER_%u.txt", sim_sd_root(), i);
        FILE *f = fopen(path, "wb");
        if (!f) { host_ok = false; continue; }
        if (fwrite("SYNTH", 1, 5, f) != 5) host_ok = false;
        if (fclose(f)) host_ok = false;
    }
    for (unsigned i = 300; i < 304; i++) {
        snprintf(path, sizeof path, "%s/content/SYNTH_%04u.txt", sim_sd_root(), i);
        snprintf(other, sizeof other, "%s/content/SYNTH_RENAMED_%04u.txt", sim_sd_root(), i);
        if (rename(path, other)) host_ok = false;
    }
}
static void workload(void *arg) {
    (void)arg;
    phase("boot");
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    CHECK(WAIT_FOR(ready(), 40000), "automatic index is ready");
    test_start_home();
    settle();
    CHECK(ink_test_index_count() == expected_files, "initial card has %u indexed files", expected_files);
    if (short_run) {
        if (probe_control) osal_sem_give(probe_sem);
        if (probe_control || probe_requested) {
            CHECK(WAIT_FOR(probe_finished, 20000), "contention probe finishes");
        }
        until(59);
    } else {
        until(60); phase("photos-1");
        for (unsigned i = 0; i < 6; i++) {
            if (i) until(60 + 60 * i);
            photo();
        }
        until(480); phase("notes");
        for (unsigned i = 0; i < 6; i++) {
            if (i) until(480 + 60 * i);
            note(0);
            note(i + 1);
        }
        until(1020); phase("delete");
        for (unsigned i = 0; i < 12; i++) {
            if (i) until(1020 + 10 * i);
            char path[64]; snprintf(path, sizeof path, "/family/SYNTH_%04u.txt", i);
            CHECK(!ink_fs_remove(path), "delete synthetic indexed file");
            settle();
        }
        until(1500); phase("reinsert");
        sim_sd_set_present(false);
        until(1510); sim_sd_set_present(true); settle();
        until(2100); phase("computer");
        hw(HW_USB_ON, 0, 0);
        delay_ms(1000);
        CHECK(!ink_usb_disk_start(), "lend synthetic card to simulated computer");
        CHECK(ink_usb_disk_active(), "computer owns card before edits");
        until(2170); vtk_host_call(computer_edit, NULL); CHECK(host_ok, "all declared computer edits succeeded");
        until(2220); CHECK(!ink_usb_disk_stop(), "computer returns card"); hw(HW_USB_OFF, 0, 0); settle();
        until(2700); phase("photos-2");
        for (unsigned i = 0; i < 6; i++) {
            if (i) until(2700 + 60 * i);
            photo();
        }
        until(3599); settle();
        ink_probe_value("test.cost.final_index_records", ink_test_index_count());
    }
    phase("finished");
    ink_probe_count("test.finished");
}
static void options(void *arg) {
    (void)arg;
    const char *mode = getenv("INK_COST_MODE");
    short_run = mode && strcmp(mode, "hour");
    probe_enabled = mode && !strcmp(mode, "probe");
    probe_requested = probe_enabled;
    probe_control = mode && !strcmp(mode, "control");
    expected_files = probe_control ? 0 : 1000;
}
void ink_main(void) {
    product_main();
    vtk_host_call(options, NULL);
    probe_sem = osal_sem_create(0, 1);
    if (probe_enabled || probe_control) {
        osal_task_cfg_t p = {.name = "cost-probe", .fn = probe, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL,
            .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        CHECK(probe_sem && osal_task_create(&p, NULL) == OSAL_OK, "probe task starts");
    }
    osal_task_cfg_t w = {.name = "index-cost", .fn = workload, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&w, NULL) == OSAL_OK, "workload task starts");
}
