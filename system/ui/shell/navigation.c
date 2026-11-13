#include "shell_internal.h"

struct ink_shell_state N INK_PSRAM_BSS;

/* The home, control centre and the first settings pages. */
typedef struct {
    char cap[48], args[192];
    uint32_t epoch, member_epoch, version, generation;
    bool versioned;
} binding_t;

static void refused(void *ctx, const ink_receipt_t *rc) { (void)ctx; ink_shell_refused(rc); } /* never silent */

static void airplane_done(void *ctx, const ink_receipt_t *rc) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    if (rc->status == INK_RC_DONE && !ink_setting_get("net.airplane")) {
        if (!strcmp(ink_ui_top_name(), "AiOffline")) ink_ai_dismiss();
        ink_shell_close();
    }
    ink_shell_refused(rc);
}

static void binding_event(lv_event_t *e) {
    binding_t *b = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        osal_free(b);
        return;
    }
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        b->version = ink_ui_content_version(); b->generation = ink_ui_generation();
        return;
    }
    if (b->versioned && b->version && (!ink_ui_touch_current(b->version) || b->generation != ink_ui_generation())) return;
    const ink_cap_t *cap = ink_cap_find(b->cap);
    bool locked_action = ink_ui_locked_activity() ? ink_ui_locked_allows(b->cap) : cap && cap->while_locked;
    if (!current_session(b->epoch) && (b->member_epoch != ink_member_epoch() || !locked_action)) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .page_gen = b->versioned ? ink_ui_content_version() : 0,
                         .args_json = b->args[0] ? b->args : NULL,
                         .reply_exec = INK_EXEC_UI,
                         .member = ink_member_current()->id, .done = refused};
    if (!strcmp(b->cap, "wifi.scan")) {
        if (N.scan_busy) return;
        N.scan_busy = true;
        o.done = wifi_receipt;
        o.ctx = (void *) (((uintptr_t) b->epoch << 1) | 1);
        ink_ui_busy_control(lv_event_get_target_obj(e), true);
    }
    if (!strcmp(b->cap, "text.voice_start")) ink_ui_cause(osal_now_us(), "text.voice_tap_to_panel_ms");
    if (!strcmp(b->cap, "net.set_airplane")) { o.done = airplane_done; o.ctx = (void *)(uintptr_t)b->epoch; }
    ink_cap_call(b->cap, NULL, &o);
}

void ink_shell_bind(lv_obj_t *obj, const char *cap, const char *json) {
    binding_t *b = osal_calloc(1, sizeof *b, OSAL_MEM_EXTERNAL);
    if (!b) return;
    snprintf(b->cap, sizeof b->cap, "%s", cap);
    snprintf(b->args, sizeof b->args, "%s", json ? json : "");
    b->epoch = N.epoch;
    b->member_epoch = ink_member_epoch();
    const char *page = ink_ui_top_name();
    b->versioned = !strcmp(page, "home") || !strcmp(page, "Ai") || !strcmp(page, "BatteryLow") ||
                   !strcmp(page, "KidLimit") || !strcmp(page, "KidTimeUp") || !strncmp(cap, "text.", 5);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    if (lv_obj_get_style_radius(obj, 0) == LV_RADIUS_CIRCLE &&
        lv_obj_get_style_width(obj, 0) == lv_obj_get_style_height(obj, 0) &&
        ink_ui_surface_kind(obj) == INK_SURFACE_WHITE)
        lv_obj_set_style_border_width(obj, ink_ui_px(INK_STROKE), 0);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_CLICKED, b);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_PRESSED, b);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_DELETE, b);
}

const char *ink_shell_bound_cap(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(obj); i++) {
        lv_event_dsc_t *d = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(d) == icon_event) return "shell.launch";
        if (lv_event_dsc_get_cb(d) == binding_event) return ((binding_t *) lv_event_dsc_get_user_data(d))->cap;
    }
    return NULL;
}

static void notify_open(ink_call_t *c);
const char *ink_shell_board(void) {
    if (N.control) return child() ? "KidControl" : "Control";
    if (N.home && !strcmp(ink_ui_page_name(), "home")) return child() ? "KidHome" : "Main";
    if (child() && !strcmp(ink_ui_top_name(), "Apps")) return "KidApps";
    return ink_ui_top_name();
}

static void cap_nav(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.control"))
        ink_shell_control();
    else if (!strcmp(c->cap->id, "shell.lock")) {
        ink_shell_close();
        ink_power_lock();
    } else if (!strcmp(c->cap->id, "shell.close"))
        ink_shell_close();
    else if (!strcmp(c->cap->id, "shell.back")) {
        const ink_activity_t *a = ink_ui_locked_activity();
        if (!(a && a->on_key && a->on_key(INK_KEY_BACK, false)) && !ink_shell_close()) ink_ui_pop();
    } else if (!strcmp(c->cap->id, "shell.members"))
        ink_shell_members_open(false);
    else if (!strcmp(c->cap->id, "shell.more")) {
        N.more = !N.more;
        if (N.control) draw_control();
    } else if (!strcmp(c->cap->id, "shell.settings"))
        ink_shell_open_page("Settings");
    else if (!strcmp(c->cap->id, "shell.display"))
        ink_shell_open_page("SetDisplay");
    else if (!strcmp(c->cap->id, "shell.sound"))
        ink_shell_open_page("SetSound");
    else if (!strcmp(c->cap->id, "shell.network"))
        ink_shell_open_page("SetNetwork");
    else if (!strcmp(c->cap->id, "shell.known_networks"))
        ink_shell_open_page("KnownNetworks");
    else if (!strcmp(c->cap->id, "shell.notifications"))
        ink_shell_open_page("Notifications");
    else if (!strcmp(c->cap->id, "shell.notify_settings"))
        ink_shell_open_page("SetNotifications");
    else if (!strcmp(c->cap->id, "shell.page_next"))
        turn_body(1);
    else if (!strcmp(c->cap->id, "shell.page_prev"))
        turn_body(-1);
    else if (!strcmp(c->cap->id, "shell.notify_next") || !strcmp(c->cap->id, "shell.notify_prev")) {
        if (N.notify_list) ink_ui_list_turn(N.notify_list, !strcmp(c->cap->id, "shell.notify_next") ? 1 : -1);
    } else if (!strcmp(c->cap->id, "shell.wifi_next") || !strcmp(c->cap->id, "shell.wifi_prev")) {
        if (N.page_body && N.page_id == PAGE_NETWORK && !N.picked_ssid[0]) {
            if (!strcmp(c->cap->id, "shell.wifi_next") && (N.wifi_page + 1) * 2 < N.wifi_count)
                N.wifi_page++;
            else if (!strcmp(c->cap->id, "shell.wifi_prev") && N.wifi_page)
                N.wifi_page--;
            ink_ui_content_changed();
            ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
            draw_page_body();
        }
    } else if (!strcmp(c->cap->id, "shell.wifi_pick")) {
        int64_t index = -1;
        ink_args_get_int(&c->args, "index", &index);
        if (!N.page_body || N.page_id != PAGE_NETWORK || index < 0 || index >= N.wifi_count)
            c->rc.status = INK_RC_REJECTED;
        else {
            N.result[0] = 0;
            snprintf(N.picked_ssid, sizeof N.picked_ssid, "%s", N.wifi_ssid[index]);
            bool saved = false;
            for (int i = 0; i < N.known_count; i++) saved |= !strcmp(N.known_ssid[i], N.picked_ssid);
            if (!N.wifi_info[index].secured || saved) wifi_password((void *)(uintptr_t)N.epoch, NULL);
            else { N.body_page = 0; draw_page_body(); }
        }
    }
    snprintf(c->rc.object, sizeof c->rc.object, "screen:%s", ink_shell_board());
}

static void cap_launch(ink_call_t *c) {
    if (ink_boot_flow_launch(c)) return;
    char id[32];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    if (!strcmp(id, "settings")) {
        if (child())
            c->rc.status = INK_RC_REJECTED;
        else
            ink_shell_open_page("Settings");
        return;
    }
    if (!strcmp(id, "ai")) {
        ink_cap_invoke("shell.ai_home", NULL, c->src, 0, &c->rc);
        return;
    }
    if (child() && !ink_member_app_mode(NULL, id)) { c->rc.status = INK_RC_REJECTED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", TR("家长没有打开这个应用")); return; }
    if (c->src == INK_SRC_TOUCH) ink_probe_count("home.taps");
    for (int i = 0; i < ink_app_count(); i++)
        if (!strcmp(ink_app_at(i)->id, id) && ink_app_at(i)->entry) {
            uint64_t cause = osal_now_us();
            if (ink_app_open(id, NULL, NULL) != 0)
                c->rc.status = INK_RC_REJECTED;
            else
                ink_ui_cause(cause, "ui.open_to_stable_ms");
            return;
        }
    c->rc.status = INK_RC_REJECTED;
    home_note(TR("这个应用还没有可打开的内容"));
    snprintf(c->rc.say, sizeof c->rc.say, "%s", N.result);
}
static void cap_resume(ink_call_t *c) {
    ink_card_t card;
    if (bookmark(&card) && card.action[0]) {
        ink_call_opts_t opts = {.src = c->src, .reply_exec = INK_EXEC_UI};
        if (ink_cap_call(card.action, NULL, &opts) == 0) return;
    }
    if (ink_app_open("reader", NULL, NULL) != 0) c->rc.status = INK_RC_REJECTED;
    ink_probe_count("home.taps");
}

#define NAV(id_, title_, child_)                                                                                       \
    {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = child_, .run = cap_nav}
static const ink_cap_t CAPS[] = {
    NAV("shell.control", "打开控制中心", true),
    NAV("shell.close", "收起面板", true),
    NAV("shell.back", "返回上一层", true),
    NAV("shell.lock", "锁屏", true),
    NAV("shell.members", "选择家人", true),
    NAV("shell.more", "更多控制", false),
    NAV("shell.settings", "打开设置", false),
    NAV("shell.display", "显示和亮度", false),
    NAV("shell.sound", "声音", false),
    NAV("shell.network", "网络和 SIM 卡", false),
    NAV("shell.known_networks", "已存的网络", false),
    NAV("shell.notifications", "查看通知", true),
    NAV("shell.notify_settings", "通知渠道设置", false),
    NAV("shell.page_next", "设置页的下一页", true),
    NAV("shell.page_prev", "设置页的上一页", true),
    NAV("shell.notify_next", "通知下一页", true),
    NAV("shell.notify_prev", "通知上一页", true),
    NAV("shell.wifi_next", "附近网络下一组", false),
    NAV("shell.wifi_prev", "附近网络上一组", false),
    {.id = "shell.wifi_pick",
     .title = "选附近网络（密码由人在设备上填写）",
     .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":["
              "\"index\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = false,
     .run = cap_nav},
    {.id = "shell.launch",
     .title = "打开主页上的应用",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_launch},
    {.id = "shell.resume", .title = "接着读", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_resume}};

static void refresh(void *unused) {
    (void) unused;
    if (ink_power_state() == INK_PWR_LOCKED) return;
    if (!current_session(N.epoch)) return;
    if (child() && N.home) kid_time_read();
    bool body = __atomic_exchange_n(&N.need_body, false, __ATOMIC_ACQ_REL);
    if (body && N.control && ink_member_space_open()) control_redraw_later();
    if (body && N.page_body && ink_member_space_open() && !N.picked_ssid[0]) draw_page_body();
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++)
        if (N.status[i]) {
            lv_obj_t *parent = lv_obj_get_parent(N.status[i]);
            bool show_time = N.status_time[i];
            lv_obj_delete(N.status[i]);
            lv_obj_t *bar = ink_shell_statusbar(parent, show_time);
            ink_ui_hint_obj(bar, INK_REFRESH_FAST);
        }
    home_refresh();
}
static void changed(void *ctx, int h, const char *id) {
    (void) h;
    if (ctx && !strcmp(id, "power.state")) return; /* No status-bar field changes. */
    if (ctx && !strcmp(id, "app.cards_rev")) {
        ink_ui_kick();
        return;
    }
    if (!ctx && (!strcmp(id, "display.font_size") || !strcmp(id, "system.language"))) {
        ink_ui_redraw();
        return;
    }
    if (ctx && (!strcmp(id, "member.current") || (!strcmp(id, "member.authed") && !ink_member_space_open()))) {
        __atomic_add_fetch(&N.session, 1, __ATOMIC_ACQ_REL);
        return;
    }
    /* A new signal reading redraws the status bar only when its bar count changes: every redraw is a refresh on the
     * panel, and the user wants as few as possible (D-127); a reading that moved a few dBm changes nothing seen. */
    bool cell_status = !strcmp(id, "cell.registered") || !strcmp(id, "cell.sim_missing") ||
        (!strcmp(id, "cell.signal") && integer("cell.registered") &&
         cell_bars(integer("cell.signal")) != __atomic_load_n(&N.cell_bars_shown, __ATOMIC_ACQUIRE));
    bool cell_config = !strcmp(id, "cell.config_readonly") && integer("cell.config_readonly") &&
        N.page_body && N.page_id == PAGE_NETWORK;
    if (cell_config) __atomic_store_n(&N.need_body, true, __ATOMIC_RELEASE);
    if (ctx && strncmp(id, "time.", 5) && strcmp(id, "notify.unread") && !cell_status && !cell_config &&
        strcmp(id, "audio.mic") && strcmp(id, "net.type") && strncmp(id, "wifi.", 5) && strcmp(id, "bt.connected"))
        return;
    if (!ctx || !strcmp(id, "notify.unread") || !strncmp(id, "wifi.", 5) || !strcmp(id, "bt.connected"))
        __atomic_store_n(&N.need_body, true, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_UI, &N.refresh, refresh, NULL);
}
int ink_shell_ai_init(void);
static void notify_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)args;
    if (current_session(N.epoch) && N.page_body &&
        ((N.page_id == PAGE_NOTIFICATIONS && !strcmp(id, "notify.changed")) ||
         (N.page_id == PAGE_CHANNELS && !strcmp(id, "notify.channels_changed"))))
        draw_page_body();
}

int ink_shell_init(void) {
    if (ink_cap_subscribe("wifi.changed", known_load, NULL, INK_EXEC_UI)) return -1;
    if (ink_activity_card_init() != 0) return -1;
    if (today_init() != 0 || home_init() != 0) return -1;
    static const ink_cap_t notify = {.id = "shell.notify_open", .title = "打开通知并标为已读", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = notify_open};
    if (ink_search_init() != 0 || ink_cap_register(&notify, 1) != 0) return -1;
    if (ink_settings_pages_init() != 0 || ink_bluetooth_page_init() != 0 || ink_time_settings_init() != 0 || ink_mobile_network_init() != 0) return -1;
    if (ink_boot_flow_init() != 0) return -1;
    if (ink_shell_ai_init() != 0 || ink_kid_pages_init() != 0) return -1;
    int ink_shell_bar_init(void); if (ink_shell_bar_init() != 0) return -1; /* control-1: the status bars and the control centre follow their states */
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_settings_define(SETTINGS, sizeof SETTINGS / sizeof *SETTINGS))
        return -1;
    if (ink_settings_watch("", changed, NULL, INK_EXEC_UI)) return -1;
    if (ink_cap_subscribe("notify.changed", notify_changed, NULL, INK_EXEC_UI)) return -1;
    if (ink_cap_subscribe("notify.channels_changed", notify_changed, NULL, INK_EXEC_UI)) return -1;
    return ink_state_watch("", changed, (void *) 1, INK_EXEC_DIRECT);
}

void ink_shell_activity_refresh(void) {
    if (!current_session(N.epoch)) return;
    if (N.home) home_redraw_later();
    if (N.control) control_redraw_later();
}

static void notify_open(ink_call_t *c) {
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    ink_db_cursor_t cursor = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cursor, &n) == 1) if (n.id == (uint32_t)id) {
        ink_notify_mark_read(n.id);
        if (n.action[0]) {
            ink_shell_close();
            ink_call_opts_t opts = {.src = c->src, .args_json = n.action_args, .reply_exec = INK_EXEC_UI};
            if (ink_cap_call(n.action, NULL, &opts) != 0) c->rc.status = INK_RC_FAILED;
        }
        return;
    }
    c->rc.status = INK_RC_STALE;
}
