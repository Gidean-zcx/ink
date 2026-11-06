#include "ink/osal.h"
#include "ink/sys/state.h"

static bool pause_publish, publish_paused;
static osal_sem_t *publish_release;
static void pause_before_state(void) {
    if (!__atomic_exchange_n(&pause_publish, false, __ATOMIC_ACQ_REL)) return;
    __atomic_store_n(&publish_paused, true, __ATOMIC_RELEASE);
    osal_sem_take(publish_release, OSAL_WAIT_FOREVER);
}
static void fixture_member_str(int h, const char *s) {
    if (h == ink_state_find("member.current")) pause_before_state();
    ink_state_set_str(h, s);
}
#define ink_state_set_str fixture_member_str
#ifndef INK_TEST_MEMBER_SOURCE
#define INK_TEST_MEMBER_SOURCE "../../system/policy/members.c"
#endif
#include INK_TEST_MEMBER_SOURCE
#undef ink_state_set_str

void test_member_pause(void) {
    publish_release = osal_sem_create(0, 1);
    __atomic_store_n(&publish_paused, false, __ATOMIC_RELEASE);
    __atomic_store_n(&pause_publish, true, __ATOMIC_RELEASE);
}
bool test_member_paused(void) { return __atomic_load_n(&publish_paused, __ATOMIC_ACQUIRE); }
void test_member_release(void) { osal_sem_give(publish_release); }
void test_member_pause_done(void) { osal_sem_delete(publish_release); }

static ink_exec_job_t member_job;
static bool member_job_done;
static int member_job_result;
static void change_member(void *arg) {
    const char *id = arg;
    if (id) member_job_result = ink_member_switch(id);
    else { ink_member_test_reset(); member_job_result = 0; }
    __atomic_store_n(&member_job_done, true, __ATOMIC_RELEASE);
}
void test_member_change(const char *id) {
    __atomic_store_n(&member_job_done, false, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_BG, &member_job, change_member, (void *)id);
}
bool test_member_change_done(void) {
    return __atomic_load_n(&member_job_done, __ATOMIC_ACQUIRE) && member_job_result == 0;
}
