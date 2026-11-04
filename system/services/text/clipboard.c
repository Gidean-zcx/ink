/*
 * The clipboard (see ink/sys/textinput.h): one text in external RAM, never stored. Who reads it is the policy's: the AI
 * only if the member allowed it (default off), and never what was marked sensitive.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/textinput.h"
#include "text_internal.h"

#define SENSITIVE_US 60000000ull

static struct {
    char text[INK_CLIP_MAX + 1];
    bool sensitive;
    ink_svc_deadline_t expiry;
} CL INK_PSRAM_BSS;

void ink_clip_clear(void) {
    ink_svc_lock();
    CL.text[0] = 0;
    CL.sensitive = false;
    ink_svc_unlock();
    ink_svc_cancel(&CL.expiry);
}

static void expired(void *ctx) { /* the timer task */
    (void)ctx;
    ink_svc_lock();
    bool was = CL.sensitive;
    if (was) {
        CL.text[0] = 0;
        CL.sensitive = false;
    }
    ink_svc_unlock();
    if (was) ink_probe_count("clipboard.sensitive_cleared");
}

int ink_clip_set(const char *text, bool sensitive, uint8_t src) {
    (void)src;
    if (!text || strlen(text) > INK_CLIP_MAX) return -1;
    ink_svc_lock();
    snprintf(CL.text, sizeof CL.text, "%s", text);
    CL.sensitive = sensitive;
    ink_svc_unlock();
    if (sensitive) ink_svc_at(&CL.expiry, osal_now_us() + SENSITIVE_US, expired, NULL);
    else ink_svc_cancel(&CL.expiry);
    return 0;
}

int ink_clip_get(char *out, size_t cap, uint8_t src, bool *sensitive) {
    bool remote = src == INK_SRC_AI || src == INK_SRC_REMOTE;
    bool allowed = !remote || ink_setting_get("ai.read_clipboard");
    ink_svc_lock();
    bool s = CL.sensitive;
    int n = -1;
    if (allowed && !(remote && s)) {
        n = snprintf(out, cap, "%s", CL.text);
        if (n >= (int)cap) n = (int)cap - 1;
    }
    ink_svc_unlock();
    if (sensitive) *sensitive = s;
    if (n < 0) ink_probe_count("clipboard.refused");
    return n;
}

static void on_member(void *ctx, int h, const char *id) { /* DIRECT: at the moment it changes hands */
    (void)ctx;
    (void)h;
    (void)id;
    ink_clip_clear();
}

void clip_init(void) { ink_state_watch("member.current", on_member, NULL, INK_EXEC_DIRECT); }
