#pragma once

#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/camera.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/search.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "activity_card.h"

typedef enum { PAGE_SETTINGS, PAGE_DISPLAY, PAGE_SOUND, PAGE_NETWORK, PAGE_NOTIFICATIONS, PAGE_CHANNELS, PAGE_KNOWN } page_t;
#define KNOWN_NETWORKS_MAX 8
#define WIFI_SCAN_MAX 6

struct ink_shell_state {
    lv_obj_t *home, *content, *dots, *clock, *date, *message;
    lv_obj_t *control, *page, *page_body;
    lv_obj_t *status[3];
    bool status_time[3];
    int home_page, apps_page, apps_pages, wifi_page, body_page, body_pages, wifi_forget_page;
    page_t page_id;
    bool more, initialized, need_body;
    int cell_bars_shown; /* the signal bars the status bar last drew (atomic): only a different count redraws it */
    char member[16];
    uint32_t session, epoch, icon_version, icon_generation;
    char result[160], search[80];
    char wifi_ssid[WIFI_SCAN_MAX][40], picked_ssid[40];
    struct { int rssi; bool secured; } wifi_info[WIFI_SCAN_MAX];
    int wifi_count;
    char known_ssid[KNOWN_NETWORKS_MAX][33];
    int known_count;
    uint32_t known_epoch;
    bool scan_busy;
    ink_notify_view_t control_items[2], notify_items[6];
    int control_count, notify_capacity;
    int channel_first, channel_end;
    ink_ui_list_t *notify_list;
    ink_ui_list_position_t notify_position;
    char notify_ids[6][32], notify_args[6][64];
    ink_exec_job_t refresh;
    ink_card_t cards[5];
    ink_ui_field_spec_t card_fields[5];
    ink_ui_action_spec_t card_actions[5];
    char card_objects[5][48];
    ink_ui_page_spec_t today;
    lv_timer_t *cards_timer;
    lv_timer_t *note_timer; /* clears a passing note on the home page */
    int64_t cards_rev;
    uint64_t cards_at;
    bool cards_ok;
    char kid_time[80];
    lv_obj_t *kid_time_label;
};
extern struct ink_shell_state N;
static inline bool current_session(uint32_t epoch) {
    return epoch == __atomic_load_n(&N.session, __ATOMIC_ACQUIRE) && !strcmp(N.member, ink_member_current()->id) &&
           ink_member_space_open();
}

static inline int width(void) {
    return INK_CANVAS_W - 2 * INK_MARGIN;
}
static inline bool child(void) {
    return ink_member_current()->role == INK_ROLE_CHILD;
}
static inline int64_t integer(const char *id) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find(id), &v);
    return v;
}
static inline void state_text(const char *id, char *buf, size_t cap) {
    if (ink_state_get_str(ink_state_find(id), buf, cap) < 0 && cap) buf[0] = 0;
}
/* Signal bars from the modem's reading in dBm (0: none). */
static inline int cell_bars(int64_t dbm) { return dbm == 0 ? 0 : dbm >= -75 ? 4 : dbm >= -85 ? 3 : dbm >= -95 ? 2 : 1; }

extern const ink_setting_def_t SETTINGS[2];

void known_load(void *ctx, const char *id, const ink_args_t *args);
void wifi_receipt(void *ctx, const ink_receipt_t *rc);
void kid_time_read(void);
const char *wifi_line(char *ssid, size_t cap);
bool bookmark(ink_card_t *out);
void home_note(const char *text);
void draw_home_content(void);
void slider_changed(lv_event_t *e);
void bind_toggle(lv_obj_t *o, const char *id);
void draw_control(void);
void wifi_password(void *ctx, const char *password);
void draw_page_body(void);
void turn_body(int delta);
void today_load(void);
int today_slots(void);
bool today_match(const ink_json_t *j, int slot);
const char *today_icon(int slot);
void today_model(void);
bool icon_close(void);
void icon_event(lv_event_t *e);
void icon_bind(lv_obj_t *obj, const char *id);
int today_init(void);
/* The one-page home (D-135): favourites, labels and the minute's date line. */
int home_init(void);
int home_apps(const char *ids[6]);
bool home_usable(const char *id);
const char *home_icon(const char *id);
const char *home_label(const char *id);
void home_refresh(void);
