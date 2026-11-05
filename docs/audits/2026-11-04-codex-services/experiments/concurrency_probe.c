#include <stdio.h>
#include <string.h>
#include "ink/sys/svc.h"
#include "ink/sys/interrupt.h"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
#include "../../../../tests/integration/whole_product.h"

extern void audit_notify_member(char *, size_t);
extern int audit_interrupt_top_id(void);
static osal_sem_t *at_boundary, *peer_done, *bg_entered, *bg_release;
static osal_task_t *audit_thread;
static int mode;
static bool notify_armed, ring_armed, actual_ring;

void audit_notify_unlock(void) {
    ink_svc_unlock();
    if (notify_armed && osal_task_self() == audit_thread) {
        notify_armed = false;
        osal_sem_give(at_boundary);
        CHECK(osal_sem_take(peer_done, 2000000) == OSAL_OK, "member peer completed");
    }
}
void audit_audio_ring(bool on) {
    if (ring_armed && on && osal_task_self() == audit_thread) {
        ring_armed = false;
        osal_sem_give(at_boundary);
        CHECK(osal_sem_take(peer_done, 2000000) == OSAL_OK, "interrupt peer completed");
    }
    ink_audio_ring(on);
    actual_ring = on;
}
static void peer(void *arg) {
    (void)arg;
    for (;;) {
        osal_sem_take(at_boundary, OSAL_WAIT_FOREVER);
        if (mode == 1) {
            CHECK(ink_member_switch("kid") == 0, "switch member");
            char member[16] = "";
            for (int i = 0; i < 300; i++) {
                audit_notify_member(member, sizeof member);
                if (!strcmp(member, "kid")) break;
                delay_ms(5);
            }
            CHECK(!strcmp(member, "kid"), "notification service switched");
        } else if (mode == 2) {
            ink_interrupt_end(audit_interrupt_top_id());
        }
        osal_sem_give(peer_done);
    }
}
static bool title_present(const char *title) {
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t v;
    while (ink_notify_next(&cursor, &v) == 1)
        if (!strcmp(v.title, title)) return true;
    return false;
}
static void block_bg(void *arg) {
    (void)arg;
    osal_sem_give(bg_entered);
    osal_sem_take(bg_release, OSAL_WAIT_FOREVER);
}
static void run_audit(void *arg) {
    (void)arg;
    audit_thread = osal_task_self();
    delay_ms(1500);
    mode = 1;
    notify_armed = true;
    ink_notify_t n = {.member = "owner", .title = "owner-private-audit", .prio = INK_NOTIFY_LOW};
    int result = ink_notify_post(&n, NULL);
    bool kid_has = title_present(n.title);
    ink_member_test_reset();
    delay_ms(500);
    bool owner_has = title_present(n.title);
    printf("AUDIT notify.post result=%d recipient=owner owner_has=%d kid_has=%d\n", result, owner_has, kid_has);
    CHECK(result == 0 && !owner_has && kid_has, "reproduce wrong inbox");

    mode = 2;
    ring_armed = true;
    ink_interrupt_t it = {.kind = INK_INT_ALARM, .ringing = true};
    snprintf(it.title, sizeof it.title, "audit ring race");
    int iid = ink_interrupt_raise(&it);
    printf("AUDIT interrupt id=%d active=%d actual_ring=%d published=%s\n", iid, ink_interrupt_active(), actual_ring, active_state());
    CHECK(!ink_interrupt_active() && actual_ring, "reproduce ring left on");
    ink_audio_ring(false);

    CHECK(ink_fs_write_atomic("/me/audit-confirm.txt", "owner", 5) == 0, "write owner");
    CHECK(ink_fs_write_atomic("/members/kid/audit-confirm.txt", "kid", 3) == 0, "write child");
    CHECK(AI("file.delete", "{\"path\":\"/me/audit-confirm.txt\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI asks deletion");
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    static ink_exec_job_t blocked;
    CHECK(ink_exec_post(INK_EXEC_BG, &blocked, block_bg, NULL) == 0, "queue blocker");
    CHECK(osal_sem_take(bg_entered, 2000000) == OSAL_OK, "executor occupied");
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "owner confirms");
    CHECK(ink_member_switch("kid") == 0, "change before provider starts");
    osal_sem_give(bg_release);
    delay_ms(1000);
    ink_fs_stat_t stat;
    int kid_stat = ink_fs_stat("/me/audit-confirm.txt", &stat);
    ink_member_test_reset();
    delay_ms(500);
    int owner_stat = ink_fs_stat("/me/audit-confirm.txt", &stat);
    printf("AUDIT confirmed owner deletion owner_stat=%d kid_stat=%d\n", owner_stat, kid_stat);
    CHECK(owner_stat == 0 && kid_stat == INK_FS_ERR_NOT_FOUND, "reproduce wrong member deletion");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    at_boundary = osal_sem_create(0, 1);
    peer_done = osal_sem_create(0, 1);
    bg_entered = osal_sem_create(0, 1);
    bg_release = osal_sem_create(0, 1);
    osal_task_cfg_t other = {.name = "audit-peer", .fn = peer, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&other, NULL) == OSAL_OK, "create peer");
    osal_task_cfg_t cfg = {.name = "audit-races", .fn = run_audit, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create audit");
}
