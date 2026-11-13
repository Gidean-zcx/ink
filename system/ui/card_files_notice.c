/* What an app page says while a file it needs from the card cannot be used (D-138: never a blank page): one paragraph,
 * no button (plugging in a computer is noticed by itself). Only that text changes with the state (progress in tenths);
 * once the files are in place the app opens it again on the same screen, and takes the notice down when it has. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/card_files.h"
#include "ink/sys/i18n.h"
#include "ink/sys/state.h"
#include "ink/sys/ui.h"
#include "ink/ui_theme.h"

static struct {
    lv_obj_t *box, *label;
    const char *app, *what;
    void (*reopen)(void);
    int watch[2];
    bool from_ready; /* shown while the list said ready (or reopened): one recheck, no reopening loop */
    unsigned asked;  /* ink_card_files_checks() when the recheck was asked for: a later conclusion answers it */
    char text[192];
} N INK_PSRAM_BSS;

static const char *app_title(const char *id) {
    for (int i = 0; id && i < ink_app_count(); i++)
        if (!strcmp(ink_app_at(i)->id, id)) return TR(ink_app_at(i)->title);
    return id ? id : "";
}

static int compose(bool *lent) { /* the words for now, into N.text; the app's state */
    char mode[8] = "";
    int pct = 0;
    ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
    ink_card_state_t s = ink_card_files_state(N.app, &pct);
    const char *name = app_title(N.app);
    *lent = !strcmp(mode, "usb");
    if (*lent) snprintf(N.text, sizeof N.text, TR("存储卡正给电脑用，在电脑上弹出后%s就能打开"), name);
    else if (s == INK_CARD_COPYING) snprintf(N.text, sizeof N.text, TR("正在从电脑放%s，%d%%"), N.what, pct / 10 * 10);
    else if (s == INK_CARD_FAILED) snprintf(N.text, sizeof N.text, TR("%s没放成，下次连上电脑会再放"), N.what);
    else if (s == INK_CARD_NO_CARD) snprintf(N.text, sizeof N.text, TR("没有存储卡。%s要用的%s放在卡上，插上卡后连电脑会自动放进去"), name, N.what);
    else if (s == INK_CARD_MISSING) snprintf(N.text, sizeof N.text, TR("%s要用的%s还没放到存储卡上，连上电脑后会自动放进去"), name, N.what);
    else if (N.from_ready && ink_card_files_checks() != N.asked) /* checked again and right: not a card file problem */
        snprintf(N.text, sizeof N.text, TR("%s要用的%s检查过是好的，但现在打不开。请返回后再打开试试"), name, N.what);
    else snprintf(N.text, sizeof N.text, TR("%s要用的%s现在打不开，正在重新检查"), name, N.what);
    return s;
}

static void changed(void *ctx, int h, const char *id) { /* UI */
    (void)ctx; (void)h; (void)id;
    bool lent;
    if (!N.label) return;
    int s = compose(&lent);
    if (lent || (s != INK_CARD_READY && s != INK_CARD_CHECKING)) N.from_ready = false;
    if (s == INK_CARD_READY && !lent && !N.from_ready && lv_obj_get_screen(N.box) == lv_screen_active()) {
        N.from_ready = true; /* the text stays until the page is drawn under it: no blank moment, no whole-screen flash */
        if (N.reopen) N.reopen();
        else ink_ui_redraw();
        return;
    }
    if (strcmp(lv_label_get_text(N.label), N.text)) lv_label_set_text(N.label, N.text); /* only this text is redrawn */
}

static void gone(lv_event_t *e) { /* the watches go with the text (the table has 96 places) */
    (void)e;
    for (int i = 0; i < 2; i++) if (N.watch[i] > 0) ink_state_unwatch(N.watch[i]);
    N.watch[0] = N.watch[1] = 0, N.box = N.label = NULL;
}

lv_obj_t *ink_card_files_notice(lv_obj_t *screen, const char *app, const char *what, void (*reopen)(void)) {
    bool lent;
    if (N.box) lv_obj_delete(N.box);
    N.app = app, N.what = what, N.reopen = reopen, N.from_ready = false;
    N.from_ready = compose(&lent) == INK_CARD_READY && !lent;
    N.asked = ink_card_files_checks();
    N.box = ink_ui_empty(screen, N.text, NULL, NULL, NULL);
    N.label = lv_obj_get_child(N.box, 0);
    lv_label_set_long_mode(N.label, LV_LABEL_LONG_WRAP); /* a sentence, at any text size */
    lv_obj_set_height(N.label, LV_SIZE_CONTENT);
    lv_obj_set_height(N.box, LV_SIZE_CONTENT);
    lv_obj_align(N.box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(N.box, gone, LV_EVENT_DELETE, NULL);
    N.watch[0] = ink_state_watch_handle("card.files", changed, NULL, INK_EXEC_UI); /* and card.files_progress */
    N.watch[1] = ink_state_watch_handle("storage.mode", changed, NULL, INK_EXEC_UI);
    if (N.from_ready) ink_cap_call("card.files_check", NULL, &(ink_call_opts_t){.src = INK_SRC_SYSTEM});
    return N.box;
}

int ink_card_files_notice_snapshot(const char *app, char *out, size_t cap) { /* out may be NULL with cap 0 */
    char state[16] = "";
    if (!N.box || strcmp(N.app, app)) return snprintf(out, cap, "%s", "");
    ink_state_get_str(ink_state_find("card.files"), state, sizeof state);
    return snprintf(out, cap, "%s：%.180s（card.files=%s）", app_title(N.app), N.text, state);
}
