/* Original datastore/test helpers, with one explicitly failed load at the storage API boundary. */
#include "ink/sys/storage.h"
static int audit_load(const char *, void **, size_t *);
#define ink_storage_load audit_load
#define ink_main original_data_main
#include "../../../../tests/integration/data.c"
#undef ink_main
#undef ink_storage_load
#include "cJSON.h"

static bool fail_load;
static int audit_load(const char *path, void **out, size_t *len) {
    if (fail_load && strstr(path, "audit.load.0.log")) {
        fail_load = false;
        *out = NULL;
        *len = 0;
        return -1; /* same return as allocation/read failure in ink_storage_load */
    }
    return ink_storage_load(path, out, len);
}
static const ink_db_def_t LOAD = {.name="audit.load", .scope=INK_DB_DEVICE, .schema=1};
static const ink_db_def_t LARGE = {.name="audit.large", .scope=INK_DB_DEVICE, .schema=1, .json=true, .ai_read=true};
static volatile int audit_done;
static void audit_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    cJSON *j = rc->result ? cJSON_Parse(rc->result) : NULL;
    printf("AUDIT data.get status=%d result_bytes=%zu valid_json=%d\n", rc->status, rc->result ? strlen(rc->result) : 0, j != NULL);
    CHECK(rc->status == INK_RC_DONE && rc->result && !j, "reproduce invalid JSON success receipt");
    cJSON_Delete(j);
    audit_done = 1;
}
static void audit_task(void *arg) {
    (void)arg;
    if (ink_display_init() || ink_storage_init() || ink_cap_init() || ink_exec_init() || ink_res_init() || ink_state_init() ||
        ink_settings_init() || ink_svc_init() || ink_data_init() || ink_power_init()) return;
    ink_members_init();
    ink_db_t *db = ink_db_open(&LOAD, NULL);
    put_text(db, 1, 1, "old");
    drain(db);
    ink_db_close(db);
    forget(&LOAD, "");
    fail_load = true;
    db = ink_db_open(&LOAD, NULL);
    printf("AUDIT failed-load open_success=%d count=%d\n", db != NULL, ink_db_count(db));
    CHECK(db && ink_db_count(db) == 0, "failed existing log loads as empty");
    put_text(db, 1, 2, "new");
    drain(db);
    printf("AUDIT new-write durable_ver=%u\n", ink_db_durable_ver(db));
    ink_db_close(db);
    forget(&LOAD, "");
    db = ink_db_open(&LOAD, NULL);
    char value[16] = "";
    ink_db_get(db, 1, NULL, value, sizeof value);
    printf("AUDIT reload value=%s (expected new)\n", value);
    CHECK(!strcmp(value, "old"), "reproduce lost acknowledged new version");
    ink_db_close(db);

    static char large[3001];
    memset(large, 'x', 3000);
    memcpy(large, "{\"text\":\"", 9);
    memcpy(large + 2998, "\"}", 2);
    ink_db_register(&LARGE);
    db = ink_db_open(&LARGE, NULL);
    put_text(db, 1, 1, large);
    ink_call_opts_t o = {.src=INK_SRC_AI, .args_json="{\"collection\":\"audit.large\",\"id\":1}", .reply_exec=INK_EXEC_DIRECT, .done=audit_reply};
    ink_cap_call("data.get", NULL, &o);
    for (int i=0; i<1000 && !audit_done; i++) osal_task_delay_us(10000);
    CHECK(audit_done, "received data.get reply");
    ink_db_close(db);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name="audit-data", .fn=audit_task, .stack_bytes=8192, .priority=OSAL_PRIO_NORMAL, .core=-1};
    osal_task_create(&cfg, NULL);
}
