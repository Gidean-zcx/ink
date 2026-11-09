#include "../services_finish/product.h"
#include <sys/stat.h>

static unsigned sequence;
static const char *case_name;
static bool acknowledged;
static void announce(void *arg) {
    (void)arg;
    char path[768]; snprintf(path, sizeof path, "%s/session.json", g_sim.out_dir);
    FILE *f = fopen(path, "wb");
    if (!f || fprintf(f, "{\"sequence\":%u,\"case\":\"%s\",\"session\":%s}\n", sequence, case_name, g_result) < 0 || fclose(f)) abort();
}
static void check_ack(void *arg) {
    (void)arg;
    char path[768]; struct stat st;
    snprintf(path, sizeof path, "%s/ack-%u", g_sim.out_dir, sequence);
    acknowledged = stat(path, &st) == 0;
}
static void exchange_for(const char *name, uint64_t timeout_us) {
    sequence++; case_name = name; acknowledged = false;
    vtk_host_call(announce, NULL);
    uint64_t end = osal_now_us() + timeout_us;
    while (!acknowledged && osal_now_us() < end) { delay_ms(20); vtk_host_call(check_ack, NULL); }
    CHECK(acknowledged, "external HTTP client acknowledged %s", name);
}
static void exchange(const char *name) { exchange_for(name, 60000000); }
static void start_transfer(void) {
    CHECK(BY_HAND("transfer.start", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"active\":true"), "authorize transfer: %.80s / %.150s", g_rc.detail, g_result);
}
static bool inactive(const char *reason) {
    return BY_HAND("transfer.status", NULL) && strstr(g_result, "\"active\":false") && strstr(g_result, reason) && strstr(g_result, "\"code\":\"000000\"");
}
static void no_file(const char *name) {
    char path[160]; ink_fs_stat_t st;
    snprintf(path, sizeof path, "/me/收件箱/%s", name);
    CHECK(ink_fs_stat(path, &st) == INK_FS_ERR_NOT_FOUND, "no incomplete target %s", name);
    snprintf(path, sizeof path, "/me/收件箱/%s.part", name);
    CHECK(ink_fs_stat(path, &st) == INK_FS_ERR_NOT_FOUND, "no abandoned part %s", name);
}
static uint32_t verify_file(const char *name, size_t repeats) {
    char path[160]; snprintf(path, sizeof path, "/me/收件箱/%s", name);
    ink_fs_file_t *f = NULL;
    CHECK(ink_fs_open(path, INK_FS_READ, &f) == 0, "open uploaded file in member inbox");
    static const char pattern[] = "SYNTHETIC\0UPLOAD\n";
    char buf[256]; size_t offset = 0; bool same = true; int64_t n = 0;
    while (f && (n = ink_fs_read(f, buf, sizeof buf)) > 0) {
        for (int64_t i = 0; i < n; i++) if (buf[i] != pattern[(offset + (size_t)i) % (sizeof pattern - 1)]) same = false;
        offset += (size_t)n;
    }
    CHECK(f && n == 0 && same && offset == repeats * (sizeof pattern - 1), "exact binary content from external client (%zu bytes)", offset);
    if (f) ink_fs_close(f);
    uint32_t cursor = 0, id = 0; ink_fs_entry_t e;
    while (ink_fs_index("/me/收件箱", INK_FS_ANY, &cursor, &e) == 1) if (!strcmp(e.path, path)) id = e.id;
    CHECK(id && ink_fs_lookup(id, &e) == 0 && !strcmp(e.path, path), "file has a stable lookup id");
    return id;
}
static void run(void *arg) {
    (void)arg; delay_ms(4000);
    start_transfer(); exchange("success");
    CHECK(inactive("文件已收到"), "successful upload consumes authorization");
    uint32_t owner_file = verify_file("合成.txt", 4096);
    const char *bad[] = {"dotdot", "absolute", "nested", "control", "nul", "backslash", "utf8", "invalid_large"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        start_transfer(); exchange(bad[i]);
        CHECK(WAIT_FOR(inactive("不合法"), 30000), "invalid name fails honestly after request cleanup: %s", bad[i]);
    }
    start_transfer(); exchange("wrong_code");
    CHECK(BY_HAND("transfer.status", NULL) && strstr(g_result, "\"active\":true"), "wrong code does not grant write access");
    no_file("wrong.txt");
    CHECK(BY_HAND("transfer.stop", NULL) && inactive("关闭"), "explicit stop closes authorization");
    start_transfer(); exchange("three_wrong");
    CHECK(WAIT_FOR(inactive("次数过多"), 30000), "three wrong codes close the authorization");
    no_file("guess.txt");
    start_transfer();
    uint64_t began = osal_now_us();
    exchange_for("large", 300000000);
    CHECK(osal_now_us() - began >= 120000000 && inactive("文件已收到"), "progressing upload completes after authorization deadline");
    verify_file("large.bin", 1048576);
    start_transfer(); exchange("reject_idle"); delay_ms(11000);
    CHECK(inactive("超时"), "rejected request also uses the receive idle timeout"); no_file("reject-idle.txt");
    start_transfer(); exchange("raw_nul");
    no_file("bad");
    CHECK(BY_HAND("transfer.stop", NULL) && inactive("关闭"), "malformed header did not create a file");
    start_transfer(); exchange("disconnect");
    CHECK(WAIT_FOR(inactive("中断"), 5000), "disconnect is reported"); no_file("disconnect.txt");
    sim_sd_set_free(3);
    start_transfer(); exchange("full");
    CHECK(WAIT_FOR(inactive("空间不足"), 5000), "preflight insufficient space"); no_file("full.txt");
    sim_sd_set_free(-1);
    start_transfer(); exchange("midfull");
    ink_fs_stat_t part_stat;
    CHECK(WAIT_FOR(ink_fs_stat("/me/收件箱/midfull.txt.part", &part_stat) == 0 && part_stat.size == 9, 5000), "partial bytes reached storage before injecting full card");
    sim_sd_set_free(0); exchange("midfull_finish");
    CHECK(WAIT_FOR(inactive("空间不足"), 5000), "mid-transfer short write is reported after client disconnects"); no_file("midfull.txt");
    sim_sd_set_free(-1);
    start_transfer(); exchange("idle"); delay_ms(11000);
    CHECK(inactive("超时"), "stalled receiver times out"); no_file("idle.txt");
    start_transfer(); delay_ms(121000); exchange("expired");
    CHECK(inactive("超时"), "authorization expires without an upload");
    hw(HW_USB_ON, 0, 0);
    CHECK(ink_usb_disk_start() == 0, "enter USB mode");
    CHECK(BY_HAND("transfer.start", NULL) && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "U 盘"), "USB mode refuses wireless with its reason");
    CHECK(ink_usb_disk_stop() == 0, "leave USB mode");
    start_transfer();
    CHECK(ink_usb_disk_start() == INK_FS_ERR_BUSY && !ink_usb_disk_active(), "wireless authorization refuses USB before any file opens");
    CHECK(BY_HAND("transfer.stop", NULL), "stop transfer releases lease");
    CHECK(ink_usb_disk_start() == 0 && ink_usb_disk_stop() == 0, "USB works again after transfer closes");
    CHECK(BY_HAND("member.add", "{\"id\":\"synthetic-child\",\"role\":\"child\"}"), "add synthetic child");
    if (g_rc.status == INK_RC_PENDING_CONFIRMATION) {
        unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "confirm child fixture");
    }
    CHECK(WAIT_FOR(ink_member_exists("synthetic-child"), 5000), "child fixture exists");
    start_transfer(); CHECK(ink_member_switch("synthetic-child") == 0, "switch member while authorized");
    CHECK(inactive("成员"), "new member cannot read previous authorization"); exchange("switched");
    ink_fs_entry_t e;
    CHECK(ink_fs_lookup(owner_file, &e) == INK_FS_ERR_DENIED, "child cannot resolve parent's uploaded file");
    CHECK(BY_HAND("transfer.start", NULL) && g_rc.status == INK_RC_REJECTED, "child touch cannot start wireless transfer");
    CHECK(AI("transfer.start", NULL) && g_rc.status == INK_RC_REJECTED, "child AI cannot start wireless transfer");
    CHECK(inactive("成员"), "child can query status but no authorization was opened"); exchange("child"); no_file("child.txt");
    CHECK(AI("transfer.stop", NULL) && inactive("关闭"), "child can still close wireless transfer");
    CHECK(ink_member_switch("owner") == 0, "return to owner"); no_file("child.txt");
    CHECK(AI("transfer.start", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same transfer capability");
    CHECK(AI("transfer.stop", NULL) && inactive("关闭"), "AI closes through the same service");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "transfer-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
