/*
 * What stays on the panel once the power has gone (placeholder look, D-64; system UI spec §7.12). Electronic paper keeps it, so it
 * has to say the device is off; and it leaves where the reading was, like a bookmark, unless that is behind a closed space.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "shell_card.h"

#define PANEL_WAIT_US 6000000ull

static struct {
    shell_card_t card;
    ink_exec_job_t job;
    const char *why;
    char body[192];
    bool drawn;
} O;

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "已关机。%s", O.body);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "off", .level = 5, .snapshot = snapshot};

static void add_bookmark(void) { /* the first card that is one */
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *d = ink_app_at(i);
        ink_card_t c = {0};
        for (int k = 0; d->card && d->card(k, &c) == 1; k++)
            if (c.bookmark) {
                size_t n = strlen(O.body);
                snprintf(O.body + n, sizeof O.body - n, "\n%s：%s", c.title, c.text);
                return;
            }
    }
}

static void draw(void *arg) { /* UI task */
    (void)arg;
    snprintf(O.body, sizeof O.body, "%s", !strcmp(O.why, "battery_unsaved") ? "没电了，有些内容没保存。充上电再开机" : !strcmp(O.why, "battery") ? "没电了，东西都存好了。充上电再开机" : "按电源键开机");
    if (ink_member_space_open()) add_bookmark();
    shell_card_open(&O.card);
    ink_ui_overlay_add(&OVERLAY, O.card.root);
    shell_card_text(&O.card, "已关机", O.body);
    __atomic_store_n(&O.drawn, true, __ATOMIC_RELEASE);
}

static void off_picture(const char *why) { /* the task that turns it off */
    O.why = why;
    ink_exec_post(INK_EXEC_UI, &O.job, draw, NULL);
    uint64_t end = osal_now_us() + PANEL_WAIT_US;
    while ((!__atomic_load_n(&O.drawn, __ATOMIC_ACQUIRE) || ink_ui_busy() || ink_display_busy() > 0) && osal_now_us() < end) osal_task_delay_us(50000);
    ink_probe_count(osal_now_us() < end ? "ui.off_picture_on_panel" : "ui.off_picture_late");
}

int ink_shell_off_picture_init(void) {
    ink_power_set_off_picture(off_picture);
    return 0;
}
