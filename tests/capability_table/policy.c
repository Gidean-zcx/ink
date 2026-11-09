/* Real calls and the member gate, independent of the catalogue's explanations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../../system/policy/cap.c"
#include "../integration/whole_product.h"

#define REQUIRE(cond) do { if (!(cond)) osal_panic("%s", "capability policy: " #cond); } while (0)

typedef struct {
    const char *name;
    ink_rc_t status;
    int64_t before, after;
    bool whitelisted;
} observation_t;
static observation_t observations[16];
static int nobservations;

static observation_t *observe(const char *name) {
    REQUIRE(nobservations < (int)(sizeof observations / sizeof observations[0]));
    observation_t *o = &observations[nobservations++];
    o->name = name;
    o->status = g_rc.status;
    return o;
}

static void no_effect(ink_call_t *c) { (void)c; }
static const ink_cap_t EXTRA[] = {
    {.id = "reader.catalogue_extra", .title = "测试白名单额外放行", .effect = INK_FX_READ,
     .exec = INK_EXEC_DIRECT, .run = no_effect},
    {.id = "catalogue.denied", .title = "测试没有白名单", .effect = INK_FX_READ,
     .exec = INK_EXEC_DIRECT, .run = no_effect},
    {.id = "reader.catalogue_system", .title = "测试白名单不能放行系统操作", .effect = INK_FX_SYSTEM,
     .exec = INK_EXEC_DIRECT, .run = no_effect},
};

static struct {
    const char *id;
    ink_source_t source;
    ink_rc_t status;
} system_gates[8];
static int ngates;

static void check_system_gate(const char *id, ink_source_t source) {
    REQUIRE(ngates < (int)(sizeof system_gates / sizeof system_gates[0]));
    ink_receipt_t rc = {0};
    system_gates[ngates].id = id;
    system_gates[ngates].source = source;
    /* Do not execute shutdown/reboot: exercise the real gate without the hardware action. */
    system_gates[ngates++].status = gate(ink_cap_find(id), source, 0, false, &rc);
}

static void save_on_host(void *unused) {
    (void)unused;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/capability-policy.json", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *file = fopen(path, "wb");
    if (!file) abort();
    if (fputs("{\"calls\":[", file) == EOF) abort();
    for (int i = 0; i < nobservations; i++) {
        const observation_t *o = &observations[i];
        if (fprintf(file, "%s{\"case\":\"%s\",\"status\":%d,\"before\":%lld,\"after\":%lld,\"whitelisted\":%s}",
                    i ? "," : "", o->name, (int)o->status, (long long)o->before, (long long)o->after,
                    o->whitelisted ? "true" : "false") < 0) abort();
    }
    if (fputs("],\"system_gates\":[", file) == EOF) abort();
    for (int i = 0; i < ngates; i++) {
        if (fprintf(file, "%s{\"id\":\"%s\",\"source\":%d,\"status\":%d}", i ? "," : "",
                    system_gates[i].id, (int)system_gates[i].source, (int)system_gates[i].status) < 0) abort();
    }
    if (fputs("]}\n", file) == EOF || fclose(file)) abort();
}

static void run(void *unused) {
    (void)unused;
    on_ui(read_screen);
    REQUIRE(!strcmp(g_top, "home"));
    REQUIRE(ink_cap_register(EXTRA, (int)(sizeof EXTRA / sizeof EXTRA[0])) == 0);
    REQUIRE(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE);
    REQUIRE(ink_member_current()->role == INK_ROLE_CHILD);

    REQUIRE(AI("settings.set", "{\"id\":\"audio.volume\",\"value\":35}"));
    observation_t *o = observe("child_volume");
    o->after = ink_setting_get("audio.volume");
    o->whitelisted = ink_member_allows("settings.set", false);
    REQUIRE(AI("settings.get", "{\"id\":\"audio.volume\"}"));
    observe("child_get");
    REQUIRE(AI("notify.list", NULL));
    o = observe("child_notify");
    o->whitelisted = ink_member_allows("notify.list", false);

    int64_t before = ink_setting_get("audio.child_max");
    REQUIRE(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":50}"));
    o = observe("child_restricted_setting");
    o->before = before;
    o->after = ink_setting_get("audio.child_max");
    REQUIRE(AI("reader.catalogue_extra", NULL));
    o = observe("child_whitelist_extra");
    o->whitelisted = ink_member_allows(EXTRA[0].id, false);
    REQUIRE(AI("catalogue.denied", NULL));
    observe("child_no_whitelist");

    check_system_gate("system.shutdown", INK_SRC_TOUCH);
    check_system_gate("system.shutdown", INK_SRC_AI);
    check_system_gate("system.reboot", INK_SRC_KEY);
    check_system_gate("system.reboot", INK_SRC_AI);
    check_system_gate("system.factory_reset", INK_SRC_TOUCH);
    check_system_gate("system.factory_reset", INK_SRC_AI);
    REQUIRE(ink_member_allows(EXTRA[2].id, false));
    check_system_gate(EXTRA[2].id, INK_SRC_TOUCH);
    check_system_gate(EXTRA[2].id, INK_SRC_AI);

    REQUIRE(BY_HAND("system.switch_member", "{\"id\":\"owner\"}") && g_rc.status == INK_RC_DONE);
    REQUIRE(AI("settings.set", "{\"id\":\"audio.volume\",\"value\":45}") && g_rc.status == INK_RC_DONE);
    REQUIRE(AI("system.undo", "{\"object\":\"setting:audio.volume\"}"));
    o = observe("ordinary_setting_undo");
    o->after = ink_setting_get("audio.volume");

    REQUIRE(AI("alarm.set", "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_CATALOGUE\"}") &&
            g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0);
    char undo[160];
    int n = snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    REQUIRE(n > 0 && (size_t)n < sizeof undo);
    REQUIRE(AI("system.undo", undo));
    observe("ordinary_operation_undo");

    REQUIRE(setting_by_hand("ai.read_clipboard", 1));
    REQUIRE(setting_by_hand("ai.read_clipboard", 0));
    REQUIRE(AI("system.undo", "{\"object\":\"setting:ai.read_clipboard\"}"));
    o = observe("permission_undo");
    o->after = ink_setting_get("ai.read_clipboard");
    unsigned token = 0;
    REQUIRE(sscanf(g_rc.object, "confirm:%u", &token) == 1);
    REQUIRE(ink_confirm_withdraw(token, INK_SRC_TOUCH) == 0);
    vtk_host_call(save_on_host, NULL);
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "catalogue-policy", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    REQUIRE(osal_task_create(&cfg, NULL) == OSAL_OK);
}
