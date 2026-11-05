/* Original product with the synthetic child required by the proto1 hub fixture. */
#include <stdio.h>
#include <string.h>
#include "sim.h"

#define ink_main ink_product_main
#include "../../system/boot/main.c"
#undef ink_main

static osal_sem_t *fixture_done;
static ink_receipt_t fixture_receipt;
static bool fixture_confirmed;

static void receipt(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    fixture_receipt = *rc;
    fixture_receipt.result = NULL;
    osal_sem_give(fixture_done);
}

static void confirmation(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx;
    (void)id;
    if (!fixture_done) return;
    char status[24] = "";
    ink_args_get_str(args, "status", status, sizeof status);
    fixture_confirmed = !strcmp(status, "done");
    osal_sem_give(fixture_done);
}

void ink_main(void) {
    ink_product_main();
    fixture_done = osal_sem_create(0, 1);
    if (!fixture_done || ink_cap_subscribe("confirm.result", confirmation, NULL, INK_EXEC_DIRECT) != 0)
        osal_panic("cannot prepare hub member fixture");
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_DIRECT, .done = receipt,
        .args_json = "{\"id\":\"default\",\"name\":\"合成儿童\",\"role\":\"child\"}"};
    unsigned token = 0;
    if (ink_cap_call("member.add", NULL, &opts) != 0 || osal_sem_take(fixture_done, 3000000) != OSAL_OK ||
        fixture_receipt.status != INK_RC_PENDING_CONFIRMATION || sscanf(fixture_receipt.object, "confirm:%u", &token) != 1 ||
        ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0 || osal_sem_take(fixture_done, 3000000) != OSAL_OK || !fixture_confirmed ||
        ink_member_switch("default") != 0 || ink_member_current()->role != INK_ROLE_CHILD)
        osal_panic("hub member fixture was not confirmed");
    osal_sem_delete(fixture_done);
    fixture_done = NULL;
    ink_probe_count("test.hub_member_ready");
}
