#define ink_main original_file_main
#include "../../../../tests/integration/files.c"
#undef ink_main
#include "cJSON.h"
static volatile int audit_done;
static void audit_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    cJSON *j = rc->result ? cJSON_Parse(rc->result) : NULL;
    cJSON *next = j ? cJSON_GetObjectItem(j, "next_offset") : NULL;
    cJSON *end = j ? cJSON_GetObjectItem(j, "end") : NULL;
    printf("AUDIT file.read status=%d next_offset=%.0f end=%d source_bytes=3000\n", rc->status, next ? next->valuedouble : -1, cJSON_IsTrue(end));
    CHECK(rc->status == INK_RC_DONE && next && next->valuedouble == 2046 && cJSON_IsTrue(end), "reproduce false UTF-8 EOF");
    cJSON_Delete(j);
    audit_done = 1;
}
static void audit_task(void *arg) {
    (void)arg;
    if (ink_display_init() || ink_storage_init() || ink_cap_init() || ink_exec_init() || ink_res_init() || ink_state_init() ||
        ink_settings_init() || ink_svc_init() || ink_data_init() || ink_power_init() || ink_file_service_init()) return;
    ink_members_init();
    static char chinese[3001];
    for (int i=0; i<1000; i++) memcpy(chinese + 3*i, "中", 3);
    write_text("/me/chinese.txt", chinese);
    ink_call_opts_t o = {.src=INK_SRC_AI, .args_json="{\"path\":\"/me/chinese.txt\"}", .reply_exec=INK_EXEC_DIRECT, .done=audit_reply};
    ink_cap_call("file.read", NULL, &o);
    for (int i=0; i<1000 && !audit_done; i++) osal_task_delay_us(10000);
    CHECK(audit_done, "received file.read reply");
    write_text("/members/kid/private.txt", "kid private");
    write_text("/family/public.txt", "family public");
    char buf[32];
    printf("AUDIT before family delete private_read=%d hidden_in_list=%d\n", read_text("/members/kid/private.txt", buf, sizeof buf), !listed("/family", ".m"));
    CHECK(read_text("/members/kid/private.txt", buf, sizeof buf) == 0 && !listed("/family", ".m"), "private file exists outside listed family scope");
    int id = ink_fs_delete_start("/family");
    ink_fs_job_status_t st = {0};
    for (int i=0; i<1500; i++) {
        ink_fs_job_status(id, &st);
        if (st.state != INK_FS_JOB_RUNNING) break;
        osal_task_delay_us(10000);
    }
    printf("AUDIT after family delete job_state=%d private_read=%d (expected preserved)\n", st.state, read_text("/members/kid/private.txt", buf, sizeof buf));
    CHECK(st.state == INK_FS_JOB_DONE && read_text("/members/kid/private.txt", buf, sizeof buf) == INK_FS_ERR_NOT_FOUND, "reproduce deletion outside family scope");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name="audit-file", .fn=audit_task, .stack_bytes=8192, .priority=OSAL_PRIO_NORMAL, .core=-1};
    osal_task_create(&cfg, NULL);
}
