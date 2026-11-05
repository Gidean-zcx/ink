/* While the system is being installed nothing else can be used (placeholder look, D-64): the card says how far it is. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/exec.h"
#include "ink/sys/state.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    ink_exec_job_t job;
    int shown;
} C;

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "正在安装新系统，%d%%。", C.shown);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "update", .level = 4, .snapshot = snapshot};

static void sync_card(void *arg) { /* UI task */
    (void)arg;
    char st[16] = "";
    int64_t pct = 0;
    ink_state_get_str(ink_state_find("update.state"), st, sizeof st);
    ink_state_get_int(ink_state_find("update.progress"), &pct);
    bool want = !strcmp(st, "installing") || !strcmp(st, "rebooting");
    if (!want) {
        if (C.card.root) {
            shell_card_close(&C.card);
            ink_ui_overlay_remove(&OVERLAY);
        }
        return;
    }
    if (!C.card.root) {
        shell_card_open(&C.card);
        ink_ui_overlay_add(&OVERLAY, C.card.root);
        C.shown = -1;
    }
    /* every ten percent: a refresh costs the installation time too */
    if (pct / 10 != C.shown / 10 || C.shown < 0) {
        char body[48];
        C.shown = (int)pct;
        snprintf(body, sizeof body, "%d%%，别关机", C.shown);
        shell_card_text(&C.card, "正在安装新系统", body);
    }
}

static void on_change(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_UI, &C.job, sync_card, NULL);
}

int ink_shell_update_card_init(void) {
    if (ink_state_watch("update.", on_change, NULL, INK_EXEC_DIRECT) != 0) return -1;
    return 0;
}
