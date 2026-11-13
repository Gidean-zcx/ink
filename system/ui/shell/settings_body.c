/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension F.y,heading_h,notice_h,home_room,below_heading,y Page flow: theme sizes added up, plus text measured at the current size. */
/* ui-scale-runtime: dimension lv_obj_get_y Password backing follows the native field's theme-derived position. */
/* ui-scale-runtime: dimension h,title_w,top,row_h,body_top Settings rows: theme sizes and measured text; the body starts under the search field. */
#include "shell_internal.h"
#include "settings_registry.h"

static const char *const CHANNEL_NOTE = TR_NOOP("关闭后不再收进新通知，已有通知保留。电话和闹钟照常。");
static void create_page(lv_obj_t *screen, void *arg);
static void destroy_page(void);
static int page_snapshot(char *buf, size_t cap);
static bool page_key(ink_key_t key, bool long_press);
static const ink_activity_t PAGES[] = {
    {.name = "Settings", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetDisplay",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "SetSound", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetNetwork",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "Notifications",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "SetNotifications", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "KnownNetworks", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key}};

static void known_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != N.known_epoch || !current_session(N.epoch) || !N.page_body || (N.page_id != PAGE_KNOWN && N.page_id != PAGE_NETWORK)) return;
    N.known_count = -2;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        N.known_count = 0;
        for (int i = 1; i < j->ntok && N.known_count < KNOWN_NETWORKS_MAX; i++)
            if (ink_json_str(j, i, N.known_ssid[N.known_count], sizeof N.known_ssid[0]) >= 0) N.known_count++;
    }
    osal_free(j);
    ink_ui_content_changed();
    if (N.page_id == PAGE_KNOWN) draw_page_body();
}
void known_load(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (!N.page_body || (N.page_id != PAGE_KNOWN && N.page_id != PAGE_NETWORK)) return;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = known_reply,
        .ctx = (void *)(uintptr_t)N.known_epoch};
    if (ink_cap_call("wifi.list", NULL, &o)) { N.known_count = -2; draw_page_body(); }
}

void wifi_receipt(void *ctx, const ink_receipt_t *rc) {
    uintptr_t request = (uintptr_t) ctx;
    if (!current_session((uint32_t) (request >> 1))) return;
    bool scan = (request & 1) != 0;
    N.scan_busy = false;
    if (!scan && rc->status == INK_RC_DONE) N.picked_ssid[0] = 0;
    snprintf(N.result, sizeof N.result, "%s",
             rc->say[0]                  ? rc->say
             : rc->status == INK_RC_DONE ? TR("已经找过附近的网络")
                                         : TR("没能完成，可以再试一次"));
    if (scan) { N.wifi_count = N.wifi_page = N.body_page = 0; }
    if (scan && rc->status == INK_RC_DONE && rc->result) {
        ink_json_t j;
        N.wifi_count = 0;
        N.wifi_page = 0;
        if (ink_json_parse(&j, rc->result, strlen(rc->result)) == 0)
            for (int i = 1; i < j.ntok && N.wifi_count < WIFI_SCAN_MAX; i++) {
                if (j.tok[i].type != 1) continue;
                int token = ink_json_find(&j, i, "ssid");
                if (ink_json_str(&j, token, N.wifi_ssid[N.wifi_count], sizeof N.wifi_ssid[0]) >= 0) {
                    int64_t rssi = 0; ink_json_int(&j, ink_json_find(&j, i, "rssi"), &rssi);
                    N.wifi_info[N.wifi_count].rssi = (int)rssi;
                    N.wifi_info[N.wifi_count++].secured = ink_json_eq(&j, ink_json_find(&j, i, "secured"), "true");
                }
                int end = j.tok[i].end;
                while (i + 1 < j.ntok && j.tok[i + 1].start < end) i++;
            }
        ink_ui_content_changed();
    }
    char connected[40]; ink_state_get_str(ink_state_find("wifi.ssid"), connected, sizeof connected);
    for (int i = 1; i < N.wifi_count; i++) if (!strcmp(connected, N.wifi_ssid[i])) {
        char ssid[40]; memcpy(ssid, N.wifi_ssid[i], sizeof ssid);
        int rssi = N.wifi_info[i].rssi; bool secured = N.wifi_info[i].secured;
        memmove(N.wifi_ssid[1], N.wifi_ssid[0], (size_t)i * sizeof N.wifi_ssid[0]);
        memmove(N.wifi_info + 1, N.wifi_info, (size_t)i * sizeof N.wifi_info[0]);
        memcpy(N.wifi_ssid[0], ssid, sizeof ssid); N.wifi_info[0].rssi = rssi; N.wifi_info[0].secured = secured;
        break;
    }
    if (N.page_body && N.page_id == PAGE_NETWORK) draw_page_body();
}

const char *wifi_line(char *ssid, size_t cap) { /* the Wi-Fi network in use, or why there is none */
    state_text("wifi.ssid", ssid, cap);
    return !ink_setting_get("wifi.enabled") ? TR("Wi-Fi 已关闭") : ssid[0] ? ssid : TR("还没有连接 Wi-Fi");
}

static void group_row(lv_obj_t *group, const char *icon, const char *title, const char *value, int row,
                      const char *action) {
    lv_obj_t *r = ink_ui_list_row(group, icon, title, value, 0, row * INK_CONTROL, width(), false, NULL, NULL);
    if (action) {
        ink_shell_bind(r, action, NULL);
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN,
                    (INK_CONTROL - INK_ICON_SMALL) / 2, false);
    }
}

void wifi_password(void *ctx, const char *password) {
    if (!current_session((uint32_t) (uintptr_t) ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args);
    ink_args_str(args, "ssid", N.picked_ssid);
    if (password) ink_args_str(args, "password", password);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH,
                         .reply_exec = INK_EXEC_UI,
                         .done = wifi_receipt,
                         .ctx = (void *) ((uintptr_t) N.epoch << 1),
                         .member = ink_member_current()->id};
    ink_cap_call("wifi.connect", args, &o);
    memset(args, 0, sizeof *args);
    osal_free(args);
    ink_text_blur(0);
}
static void wifi_picked(lv_event_t *e) {
    int index = (int) (intptr_t) lv_event_get_user_data(e);
    char args[40];
    snprintf(args, sizeof args, "{\"index\":%d}", index);
    ink_receipt_t rc;
    ink_cap_invoke_json("shell.wifi_pick", args, INK_SRC_TOUCH, 0, &rc);
}

/* Pages do not scroll (spec section 1): a block that does not fit goes to the next page. */
static struct {
    int y, page, room;
    bool draw;
} F;
static bool block(int h) {
    if (F.y && F.y + h > F.room) {
        F.page++;
        F.y = 0;
    }
    return F.draw && F.page == N.body_page;
}
/* A block of rows the registry declares (ids, space separated): the page settings.open asked for turns to it and
 * frames it, as AOSP highlights the row a search found. */
static int target_page = -1;
static bool at(const char *ids, int h) {
    bool here = block(h), target = false;
    size_t n = ink_sr_target ? strlen(ink_sr_target->id) : 0;
    for (const char *p = ids; n && (p = strstr(p, ink_sr_target->id)); p += n)
        target |= (p == ids || p[-1] == ' ') && (!p[n] || p[n] == ' ');
    if (target && !F.draw) target_page = F.page;
    if (target && here) {
        lv_obj_t *frame = ink_ui_box(N.page_body, INK_MARGIN - INK_GAP_S, F.y - INK_GAP_S, width() + 2 * INK_GAP_S,
                                     h + 2 * INK_GAP_S, INK_RADIUS_CARD, INK_SURFACE_WHITE, false);
        lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(frame, ink_ui_px(INK_STROKE_FOCUS), 0);
    }
    return here;
}
static bool for_me(const char *id) { /* a child does not meet what is for parents (spec section 0, rule 8) */
    const ink_sr_item_t *it = ink_sr_find(id);
    return !(it && it->adult && child());
}
static void gray_labels(lv_obj_t *row) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++)
        if (lv_obj_check_type(lv_obj_get_child(row, (int32_t)i), &lv_label_class))
            lv_obj_set_style_text_color(lv_obj_get_child(row, (int32_t)i), ink_ui_secondary(), 0);
}

/* The home (settings-1): the registry's home rows in their groups (AOSP top_level_settings.xml:30-94, MuditaOS
 * SettingsMainWindow.cpp:14-39), or what a search found and where it is; every row says what it is now. The drawn
 * rows are kept, so a changed value is written into its own row and nothing else is redrawn (D-127). */
enum { HOME_ROWS = 8, FOUND_MAX = 10 };
static const char *const STATES[] = {"available", "unsearchable", "not_now", "unsupported", "parent", "dependent"};
static struct {
    char query[48], member[16]; /* whose search it is */
    const ink_sr_item_t *it[HOME_ROWS], *seen[HOME_ROWS];
    lv_obj_t *value[HOME_ROWS];
    bool gray[HOME_ROWS];
    int n, nseen, page;
} H INK_PSRAM_BSS;
/* What a row says: its value, or for a search its path and value, joined as AOSP's search_breadcrumb_connector does.
 * False: not shown (the font has no ›, so the path takes " > " as AOSP). */
static bool home_line(const ink_sr_item_t *it, char *out, size_t cap, bool *gray) {
    char value[96];
    ink_sr_state_t s = ink_sr_value(it, value, sizeof value);
    *gray = s == INK_SR_PARENT || s == INK_SR_DEPENDENT;
    if (s == INK_SR_NOT_NOW || s == INK_SR_UNSUPPORTED || (!H.query[0] && s == INK_SR_PARENT)) return false;
    if (H.query[0]) snprintf(out, cap, "%s > %s%s%s", ink_sr_group(it->group), TR(it->title), value[0] ? " · " : "", value);
    else snprintf(out, cap, "%s", value);
    return true;
}
static const char *row_cap(const ink_sr_item_t *it, bool home, char *args, size_t cap) { /* a tap and the AI: the same */
    snprintf(args, cap, "%s", home && it->cap ? (it->args ? it->args : "{}") : "");
    if (!args[0]) snprintf(args, cap, "{\"id\":\"%s\"}", it->id);
    return home && it->cap ? it->cap : "settings.open";
}
static void row_json(ink_jw_t *w, const ink_sr_item_t *it, bool comma, bool home) { /* a row as the AI is told of it */
    char value[96], args[64];
    ink_sr_state_t s = ink_sr_value(it, value, sizeof value);
    ink_jw_raw(w, comma ? ",{\"id\":" : "{\"id\":"); ink_jw_str(w, it->id);
    ink_jw_raw(w, ",\"group\":"); ink_jw_str(w, ink_sr_group(it->group));
    ink_jw_raw(w, ",\"title\":"); ink_jw_str(w, TR(it->title));
    ink_jw_raw(w, ",\"value\":"); ink_jw_str(w, value);
    ink_jw_raw(w, ",\"state\":"); ink_jw_str(w, STATES[s]);
    if (s != INK_SR_PARENT && s != INK_SR_DEPENDENT) {
        ink_jw_raw(w, ",\"capability\":"); ink_jw_str(w, row_cap(it, home, args, sizeof args));
        ink_jw_raw(w, ",\"args_json\":"); ink_jw_str(w, args);
    }
    ink_jw_raw(w, "}");
}
/* One line with the value on the right; a value that does not fit there goes under the name and wraps, never cut. */
static int row_h(const ink_sr_item_t *it, const char *text) {
    int left = INK_CIRCLE_SMALL + INK_GAP_XL, room = width() - left - INK_BLOCK - INK_GAP;
    if (!H.query[0] && ink_ui_text_width(TR(it->title), INK_FONT_LIST, INK_WEIGHT_REGULAR) + INK_GAP_M +
                       ink_ui_text_width(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR) <= room + INK_GAP) return INK_CONTROL;
    return LV_MAX(ink_ui_row_double(), ink_ui_line_height(INK_FONT_LIST) + 2 * INK_GAP_XS +
                  ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, room));
}
static void home_row(lv_obj_t *box, const ink_sr_item_t *it, const char *text, bool gray, int y, int h) {
    bool found = H.query[0] != 0;
    int left = INK_CIRCLE_SMALL + INK_GAP_XL, room = width() - left - INK_BLOCK - INK_GAP;
    lv_obj_t *r = ink_ui_list_row(box, it->icon ? it->icon : "gear", TR(it->title), text, 0, y, width(), h > INK_CONTROL, NULL, NULL);
    lv_obj_t *label = NULL, *value = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(r); i++)
        if (lv_obj_check_type(lv_obj_get_child(r, (int32_t)i), &lv_label_class)) {
            if (label) value = lv_obj_get_child(r, (int32_t)i);
            else label = lv_obj_get_child(r, (int32_t)i);
        }
    if (value && h > INK_CONTROL) { /* under the name, as many lines as it takes */
        lv_obj_set_height(r, ink_ui_px(h));
        lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
        lv_obj_set_height(value, LV_SIZE_CONTENT);
    } else if (value) { /* the value takes the room the title leaves, so a new value fits in place */
        const char *name = TR(it->title);
        int title_w = ink_ui_text_width(name, INK_FONT_LIST, INK_WEIGHT_REGULAR);
        lv_obj_set_width(label, ink_ui_px(title_w));
        lv_obj_set_x(value, ink_ui_px(left + title_w + INK_GAP_M));
        lv_obj_set_width(value, ink_ui_px(room + INK_GAP - title_w - INK_GAP_M));
    }
    if (gray) {
        gray_labels(r);
    } else {
        char args[64];
        const char *cap = row_cap(it, !found, args, sizeof args);
        ink_shell_bind(r, cap, args);
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
    }
    if (F.draw && H.n < HOME_ROWS) { H.it[H.n] = it; H.value[H.n] = value; H.gray[H.n++] = gray; } /* its labels are never cut */
}
static void home_blocks(void) {
    const ink_sr_item_t *found[FOUND_MAX];
    int count = H.query[0] ? ink_sr_search(H.query, found, FOUND_MAX) : 0, shown = -1;
    int top = 0;
    lv_obj_t *box = NULL;
    H.nseen = 0;
    if (F.draw) H.n = 0;
    if (H.query[0] && !count) {
        const char *none = TR("没有找到这项设置");
        if (block(ink_ui_line_height(INK_FONT_NOTE))) ink_ui_notice(N.page_body, none, F.y);
        F.y += ink_ui_line_height(INK_FONT_NOTE);
    }
    for (int i = 0;; i++) {
        const ink_sr_item_t *it = H.query[0] ? (i < count ? found[i] : NULL) : ink_sr_at(i);
        char text[192];
        bool gray;
        if (!it) break;
        if ((!H.query[0] && !(it->where & INK_SR_HOME)) || !home_line(it, text, sizeof text, &gray)) continue;
        int h = row_h(it, text), page = F.page;
        if (!H.query[0] && (it->group != shown || (F.y && F.y + h > F.room))) { /* a group's name, kept with its row */
            shown = it->group;
            if (block(INK_GROUP_TITLE + h)) {
                lv_obj_t *l = ink_ui_line(N.page_body, ink_sr_group(shown), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
                                          F.y + (INK_GROUP_TITLE - ink_ui_line_height(INK_FONT_NOTE)) / 2, width());
                lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
            }
            F.y += INK_GROUP_TITLE;
            box = NULL;
        }
        bool here = block(h);
        if (F.page != page) box = NULL;
        if (F.page == N.body_page && H.nseen < HOME_ROWS) H.seen[H.nseen++] = it;
        if (here) {
            if (!box) { top = F.y; box = ink_ui_box(N.page_body, INK_MARGIN, top, width(), h, 0, INK_SURFACE_STATIC, false); }
            lv_obj_set_height(box, ink_ui_px(F.y + h - top));
            home_row(box, it, text, gray, F.y - top, h);
        }
        F.y += h;
    }
}
/* Only what the rows say changed: each new value goes into its own label, nothing else is drawn again. */
static bool home_update(void) {
    char text[192];
    bool gray;
    if (N.page_id != PAGE_SETTINGS || !H.n || H.n != H.nseen || memcmp(H.it, H.seen, (size_t)H.n * sizeof *H.it)) return false;
    for (int k = 0; k < H.n; k++) /* a row turning gray, or a value needing another line, is a new layout */
        if (!H.value[k] || !home_line(H.it[k], text, sizeof text, &gray) || gray != H.gray[k] ||
            row_h(H.it[k], text) != row_h(H.it[k], lv_label_get_text(H.value[k]))) return false;
    for (int k = 0; k < H.n; k++)
        if (home_line(H.it[k], text, sizeof text, &gray) && strcmp(lv_label_get_text(H.value[k]), text)) lv_label_set_text(H.value[k], text);
    return true;
}
void ink_settings_home_sync(void) {
    if (N.page_body && N.page_id == PAGE_SETTINGS && current_session(N.epoch)) draw_page_body();
}
static int body_top(void) {
    return INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS +
           (N.page_id == PAGE_SETTINGS ? ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_XS : 0);
}
static lv_obj_t *group(int rows) {
    return ink_ui_card(N.page_body, INK_MARGIN, F.y, width(), rows * INK_CONTROL, INK_SURFACE_STATIC);
}
static int heading_h(void) {
    return ink_ui_line_height(INK_FONT_LIST) + INK_GAP_M;
}
static void heading(const char *text) {
    ink_ui_line(N.page_body, text, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, F.y, width());
}
static int notice_h(const char *text) {
    return ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, width());
}
static void bind_choices(lv_obj_t *row, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        char args[96];
        snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, i);
        ink_shell_bind(lv_obj_get_child(row, i), "settings.set", args);
    }
}

static void body_blocks(void) {
    if (N.page_id == PAGE_SETTINGS) {
        home_blocks();
    } else if (N.page_id == PAGE_CHANNELS) {
        const char *note = TR(CHANNEL_NOTE);
        int h = notice_h(note);
        if (block(h)) ink_ui_notice(N.page_body, note, F.y);
        F.y += h + INK_GAP_L;
        if (F.draw) N.channel_first = N.channel_end = -1;
        const ink_notify_channel_t *ch;
        for (int i = 0; (ch = ink_notify_channel_at(i)); i++) {
            if (block(INK_CONTROL)) {
                bool on = ink_notify_channel_on(ch->id);
                char args[96];
                snprintf(args, sizeof args, "{\"id\":\"%s\",\"enabled\":%s}", ch->id, on ? "false" : "true");
                ink_shell_bind(ink_ui_toggle(N.page_body, TR(ch->title), on, INK_MARGIN, F.y, width(), NULL, NULL),
                    "notify.set_channel", args);
                if (N.channel_first < 0) N.channel_first = i;
                N.channel_end = i + 1;
            }
            F.y += INK_CONTROL + INK_GAP_M;
        }
    } else if (N.page_id == PAGE_DISPLAY) {
        if (at("display.light", INK_SLIDER))
            ink_ui_slider(N.page_body, TR("亮度"), (int) ink_setting_get("display.light") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "display.light");
        F.y += INK_SLIDER + INK_GAP_M;
        if (at("display.warmth", INK_SLIDER))
            ink_ui_slider(N.page_body, TR("暖光"), (int) ink_setting_get("display.warmth") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "display.warmth");
        F.y += INK_SLIDER + INK_GAP_L;
        const char *const sample_text = TR("天黑以后，阿木背起邮包。");
        int sample_h = 2 * INK_GAP_L +
                       ink_ui_text_height(sample_text, INK_FONT_BODY, INK_WEIGHT_REGULAR, width() - 2 * INK_MARGIN);
        int h = heading_h() + INK_SEGMENT + INK_GAP + sample_h;
        if (for_me("display.font_size") && at("display.font_size", h)) {
            heading(TR("字号"));
            const char *const labels[] = {TR("小"), TR("中"), TR("大"), TR("特大")};
            lv_obj_t *seg = ink_ui_segments(N.page_body, labels, 4, (int) ink_setting_get("display.font_size"),
                                            INK_MARGIN, F.y + heading_h(), width(), NULL, NULL);
            bind_choices(seg, 4, "display.font_size");
            lv_obj_t *sample = ink_ui_card(N.page_body, INK_MARGIN, F.y + heading_h() + INK_SEGMENT + INK_GAP, width(),
                                           sample_h, INK_SURFACE_STATIC);
            ink_ui_label(sample, sample_text, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L,
                         width() - 2 * INK_MARGIN);
        }
        F.y += for_me("display.font_size") ? h + INK_GAP_L : 0;
        if (at("refresh", INK_CONTROL)) {
            lv_obj_t *refresh =
                ink_ui_pill(N.page_body, TR("刷新屏幕"), INK_MARGIN, F.y, width(), INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(refresh, "system.full_refresh", NULL);
        }
        F.y += INK_CONTROL + INK_GAP;
        if (for_me("power.lock_after_s") && at("power.lock_after_s", INK_CONTROL)) {
            static const int seconds[] = {60, 300, 600, 1800, 3600};
            int64_t current = ink_setting_get("power.lock_after_s");
            int next = seconds[0];
            for (unsigned i = 0; i < sizeof seconds / sizeof *seconds; i++)
                if (seconds[i] > current) {
                    next = seconds[i];
                    break;
                }
            char label[64], args[96];
            snprintf(label, sizeof label, TR("自动锁屏 %lld 分钟"), (long long) current / 60);
            snprintf(args, sizeof args, "{\"id\":\"power.lock_after_s\",\"value\":%d}", next);
            lv_obj_t *lock = ink_ui_pill(N.page_body, label, INK_MARGIN, F.y, width(), INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(lock, "settings.set", args);
        }
        F.y += for_me("power.lock_after_s") ? INK_CONTROL + INK_GAP : 0;
        if (at("today", INK_CONTROL))
            group_row(group(1), "grid", TR("今日屏放什么"), "", 0, "shell.today_edit");
        F.y += INK_CONTROL;
    } else if (N.page_id == PAGE_SOUND) {
        const char *const about = TR("听书、AI 和提示音用这个音量");
        int h = INK_SLIDER + INK_GAP_S + notice_h(about);
        if (at("audio.volume", h)) {
            ink_ui_slider(N.page_body, TR("音量"), (int) ink_setting_get("audio.volume") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "audio.volume");
            ink_ui_notice(N.page_body, about, F.y + INK_SLIDER + INK_GAP_S);
        }
        F.y += h + INK_GAP_L;
        h = child() ? 0 : heading_h() + 2 * INK_CONTROL; /* the headset is the parents' (SetBluetooth) */
        if (h && block(h)) {
            heading(TR("声音从哪里出来"));
            F.y += heading_h();
            char route[32];
            state_text("audio.route", route, sizeof route);
            lv_obj_t *g = group(2);
            group_row(g, "volume", TR("喇叭"), !strcmp(route, "speaker") ? TR("当前") : "", 0, "bt.disconnect");
            group_row(g, "headphones", TR("蓝牙耳机"), !strcmp(route, "bt_headset") ? TR("当前") : TR("没有连接"), 1, "shell.bluetooth");
            F.y -= heading_h();
        }
        F.y += h ? h + INK_GAP_L : 0;
        if (at("notify.dnd", INK_CONTROL)) {
            lv_obj_t *toggle =
                ink_ui_toggle(N.page_body, TR("勿扰"), ink_setting_get("notify.dnd"), INK_MARGIN, F.y, width(), NULL, NULL);
            bind_toggle(toggle, "notify.dnd");
        }
        F.y += INK_CONTROL + INK_GAP_L;
        h = heading_h() + 3 * INK_CONTROL;
        if (at("audio.key_tone audio.vibration audio.vibration_level", h)) {
            heading(TR("提示"));
            F.y += heading_h();
            lv_obj_t *g = group(3);
            const char *ids[] = {"audio.key_tone", "audio.vibration"}, *labels[] = {TR_NOOP("按键提示音"), TR_NOOP("振动")};
            for (int i = 0; i < 2; i++) {
                ink_ui_icon(g, i ? "wave" : "bell", INK_ICON_LIST, INK_ICON_LINE, INK_GAP_L, i * INK_CONTROL + (INK_CONTROL - INK_ICON_LIST) / 2, false);
                lv_obj_t *toggle = ink_ui_toggle(g, TR(labels[i]), ink_setting_get(ids[i]), INK_ROW_DOUBLE, i * INK_CONTROL, width() - INK_ROW_DOUBLE - INK_GAP_L, NULL, NULL);
                lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP, 0);
                bind_toggle(toggle, ids[i]);
            }
            char args[80], level[64]; /* grayed with the reason while vibration is off (the registry's dependency) */
            bool off = ink_sr_value(ink_sr_find("audio.vibration_level"), level, sizeof level) == INK_SR_DEPENDENT;
            snprintf(args, sizeof args, "{\"id\":\"audio.vibration_level\",\"value\":%d}", (int)(ink_setting_get("audio.vibration_level") + 1) % 3);
            lv_obj_t *row = ink_ui_list_row(g, "wave", TR("振动强度"), level, 0, 2 * INK_CONTROL, width(), false, NULL, NULL);
            if (off) gray_labels(row);
            else ink_shell_bind(row, "settings.set", args);
            F.y -= heading_h();
        }
        F.y += h + INK_GAP_L;
        h = heading_h() + 2 * INK_CONTROL;
        if (at("audio.call_ring audio.alarm_ring", h)) {
            heading(TR("铃声"));
            F.y += heading_h();
            lv_obj_t *g = group(2);
            const char *ids[] = {"audio.call_ring", "audio.alarm_ring"}, *labels[] = {TR_NOOP("来电"), TR_NOOP("闹钟")};
            const char *names[] = {TR_NOOP("清音"), TR_NOOP("双音"), TR_NOOP("柔音")};
            for (int i = 0; i < 2; i++) {
                int v = (int)ink_setting_get(ids[i]); char value[64], args[96];
                snprintf(value, sizeof value, TR("%s · 点按换音试听"), TR(names[v]));
                lv_obj_t *row = ink_ui_list_row(g, i ? "clock" : "phone", TR(labels[i]), value, 0, i * INK_CONTROL, width(), false, NULL, NULL);
                snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", ids[i], (v + 1) % 3);
                ink_shell_bind(row, "settings.set", args);
            }
            F.y -= heading_h();
        }
        F.y += h + INK_GAP_L;
        if (for_me("notify.channels") && at("notify.channels", INK_CONTROL)) group_row(group(1), "bell", TR("通知"), "", 0, "shell.notify_settings");
        F.y += for_me("notify.channels") ? INK_CONTROL : 0;
    } else if (N.page_id == PAGE_KNOWN) {
        char connected[33], state[24];
        state_text("wifi.ssid", connected, sizeof connected);
        state_text("wifi.state", state, sizeof state);
        if (N.known_count <= 0) {
            const char *text = TR(N.known_count == -1 ? TR_NOOP("正在读取网络") : N.known_count == -2 ? TR_NOOP("网络列表暂时读不出来") : TR_NOOP("还没有已存的网络"));
            if (block(notice_h(text))) ink_ui_notice(N.page_body, text, F.y);
            F.y += notice_h(text) + INK_GAP_M;
            if (block(INK_CONTROL)) group_row(group(1), "wifi", N.known_count == -2 ? TR("重试") : TR("找附近的 Wi-Fi"), "", 0,
                N.known_count == -2 ? "shell.known_networks" : "shell.network");
        }
        for (int i = 0; i < N.known_count; i++) {
            int row = ink_ui_row_double(), h = row;
            if (block(h)) {
                bool same = !strcmp(connected, N.known_ssid[i]);
                const char *status = same && !strcmp(state, "connected") ? TR("已连接") : same && !strcmp(state, "joining") ? TR("正在连接网络") : TR("已存");
                char args[224]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
                ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, N.known_ssid[i]); ink_jw_raw(&w, "}");
                lv_obj_t *join = ink_ui_list_row(N.page_body, "wifi", N.known_ssid[i], status, INK_MARGIN, F.y,
                    width() - INK_PRIMARY - INK_GAP, true, NULL, NULL);
                int text_y = (row - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
                lv_obj_t *name = lv_obj_get_child(join, 1), *value = lv_obj_get_child(join, 2);
                lv_obj_set_y(name, ink_ui_px(text_y));
                lv_obj_set_width(name, ink_ui_px(width() - INK_PRIMARY - INK_ROW_DOUBLE - 2 * INK_GAP));
                lv_obj_set_pos(value, ink_ui_px(INK_CIRCLE_SMALL + INK_GAP_XL), ink_ui_px(text_y + ink_ui_line_height(INK_FONT_LIST)));
                lv_obj_set_width(value, lv_obj_get_width(name)); lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_LEFT, 0);
                ink_shell_bind(join, "wifi.connect", args);
                if (!ink_setting_get("wifi.enabled") || (same && (!strcmp(state, "connected") || !strcmp(state, "joining")))) lv_obj_add_state(join, LV_STATE_DISABLED);
                ink_shell_bind(ink_ui_pill(N.page_body, TR("忘掉"), INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, F.y + (row - INK_CONTROL) / 2,
                    INK_PRIMARY, INK_CONTROL, false, NULL, NULL), "wifi.forget", args);
            }
            F.y += h + INK_GAP_L;
        }
    } else if (N.page_id == PAGE_NETWORK) {
        if (at("net.airplane wifi", 2 * INK_CONTROL)) {
            lv_obj_t *g = group(2);
            const char *labels[] = {TR("飞行模式"), "Wi-Fi"}, *icons[] = {"plane", "wifi"};
            bool plane = ink_setting_get("net.airplane");
            for (int i = 0; i < 2; i++) {
                ink_ui_icon(g, icons[i], INK_ICON_LIST, INK_ICON_LINE, INK_GAP_L, i * INK_CONTROL + (INK_CONTROL - INK_ICON_LIST) / 2, false);
                lv_obj_t *toggle = ink_ui_toggle(g, labels[i], i ? ink_setting_get("wifi.enabled") : plane, INK_ROW_DOUBLE, i * INK_CONTROL, width() - INK_ROW_DOUBLE - INK_GAP_L, NULL, NULL);
                lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP, 0);
                if (i) bind_toggle(toggle, "wifi.enabled");
                else ink_shell_bind(toggle, "net.set_airplane", plane ? "{\"enabled\":false}" : "{\"enabled\":true}");
            }
        }
        F.y += 2 * INK_CONTROL + INK_GAP_L;
        char ssid[64];
        wifi_line(ssid, sizeof ssid);
        int h;
        if (N.picked_ssid[0]) {
            if (block(ink_textfield_height(INK_FIELD_PASSWORD))) {
                ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD,
                                      .enter = INK_ENTER_DONE,
                                      .max = 64,
                                      .hint = TR("输入网络密码"),
                                      .ai_hidden = true};
                lv_obj_t *strip = ink_ui_box(N.page_body, 0, F.y, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
                lv_obj_t *password_field =
                    ink_textfield_create(N.page_body, &cfg, wifi_password, (void *) (uintptr_t) N.epoch, false);
                lv_obj_set_pos(password_field, ink_ui_px(INK_MARGIN), ink_ui_px(F.y));
                ink_textfield_style(password_field, INK_FIELD_PASSWORD, N.picked_ssid);
                ink_textfield_error(password_field, N.result);
                ink_textfield_short_focus(password_field);
                lv_obj_update_layout(password_field);
                lv_obj_set_y(strip, lv_obj_get_y(password_field) - ink_ui_px(INK_GAP_M));
                lv_obj_set_style_border_width(strip, ink_ui_px(INK_STROKE), 0);
                lv_obj_set_style_border_side(strip, LV_BORDER_SIDE_TOP, 0);
            }
            return;
        }
        if (block(INK_CONTROL)) {
            lv_obj_t *scan = ink_ui_pill(N.page_body, N.scan_busy ? TR("正在找附近的 Wi-Fi") : TR("找附近的 Wi-Fi"), INK_MARGIN,
                                         F.y, width(), INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(scan, "wifi.scan", NULL);
        }
        F.y += INK_CONTROL + INK_GAP_M;
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2; i++) {
            if (block(INK_CONTROL)) {
                lv_obj_t *row = ink_ui_list_row(N.page_body, !strcmp(ssid, N.wifi_ssid[i]) ? "check" : "wifi", N.wifi_ssid[i], "",
                    INK_MARGIN, F.y, width(), false, NULL, NULL);
                lv_obj_set_width(lv_obj_get_child(row, 1), ink_ui_px(width() - INK_ROW_DOUBLE - INK_BLOCK_L));
                lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(row, wifi_picked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
                if (N.wifi_info[i].secured) ink_ui_icon(row, "lock", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_BLOCK_L + INK_GAP_M, (INK_CONTROL - INK_ICON_SMALL) / 2, false);
                int bars = N.wifi_info[i].rssi >= -60 ? 3 : N.wifi_info[i].rssi >= -75 ? 2 : 1;
                for (int b = 0; b < bars; b++) ink_ui_box(row, width() - INK_ICON_SMALL - INK_GAP_M + b * INK_GAP_S,
                    (INK_CONTROL + INK_ICON_SMALL) / 2 - (b + 1) * INK_GAP_S, INK_GAP_XS, (b + 1) * INK_GAP_S, 0,
                    INK_SURFACE_BLACK, false);
            }
            F.y += INK_CONTROL + INK_GAP_S;
        }
        if (N.wifi_count > 2) {
            if (block(INK_CONTROL)) {
                lv_obj_t *prev = ink_ui_pill(N.page_body, TR("上一组"), INK_MARGIN, F.y, (width() - INK_GAP) / 2,
                                             INK_CONTROL, false, NULL, NULL);
                if (N.wifi_page) ink_shell_bind(prev, "shell.wifi_prev", NULL);
                lv_obj_t *next = ink_ui_pill(N.page_body, TR("下一组"), INK_MARGIN + (width() + INK_GAP) / 2, F.y,
                                             (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
                if ((N.wifi_page + 1) * 2 < N.wifi_count) ink_shell_bind(next, "shell.wifi_next", NULL);
            }
            F.y += INK_CONTROL + INK_GAP_M;
        }
        if (!N.wifi_count) {
            const char *const none = N.result[0] ? N.result : TR("还没有附近的网络列表");
            if (block(notice_h(none))) ink_ui_notice(N.page_body, none, F.y);
            F.y += notice_h(none) + INK_GAP_L;
        }
        const char *const about = TR("自动：有 Wi-Fi 用 Wi-Fi，没有就用 4G");
        h = heading_h() + INK_SEGMENT + INK_GAP_M + notice_h(about);
        if (at("net.prefer", h)) {
            heading(TR("上网方式"));
            const char *const labels[] = {TR("自动"), "Wi-Fi", "4G"};
            lv_obj_t *seg = ink_ui_segments(N.page_body, labels, 3, (int) ink_setting_get("net.prefer"), INK_MARGIN,
                                            F.y + heading_h(), width(), NULL, NULL);
            bind_choices(seg, 3, "net.prefer");
            ink_ui_notice(N.page_body, about, F.y + heading_h() + INK_SEGMENT + INK_GAP_M);
        }
        F.y += h + INK_GAP_L;
        if (ssid[0]) {
            if (block(INK_CONTROL)) {
                char args[256]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
                ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, ssid); ink_jw_raw(&w, "}");
                ink_shell_bind(ink_ui_pill(N.page_body, TR("忘掉这个网络"), INK_MARGIN, F.y, width(), INK_CONTROL,
                    false, NULL, NULL), "wifi.forget", args);
            }
            N.wifi_forget_page = F.page;
            F.y += INK_CONTROL + INK_GAP_M;
        }
        if (block(INK_CONTROL)) group_row(group(1), "wifi", TR("已存的网络"), "", 0, "shell.known_networks");
        F.y += INK_CONTROL + INK_GAP_M;
        if (block(INK_CONTROL)) ink_shell_bind(ink_ui_pill(N.page_body, TR("设置设备"), INK_MARGIN, F.y, width(), INK_CONTROL, false, NULL, NULL), "shell.launch", "{\"id\":\"boot\"}");
        F.y += INK_CONTROL + INK_GAP_L;
        const char *const sim_state = integer("cell.config_readonly") ? TR("暂不能更改 SIM 卡") : integer("cell.registered") ? TR("4G 在线，切卡不用重启") : TR("4G 还没有连上");
        h = heading_h() + INK_SEGMENT + INK_GAP_L + notice_h(sim_state);
        if (at("cell.sim_slot", h)) {
            heading(TR("SIM 卡"));
            const char *const slots[] = {TR("外置卡"), TR("内置卡")};
            lv_obj_t *sim = ink_ui_segments(N.page_body, slots, 2, (int) ink_setting_get("cell.sim_slot"), INK_MARGIN,
                                            F.y + heading_h(), width(), NULL, NULL);
            if (integer("cell.config_readonly")) lv_obj_add_state(sim, LV_STATE_DISABLED);
            else bind_choices(sim, 2, "cell.sim_slot");
            ink_ui_notice(N.page_body, sim_state, F.y + heading_h() + INK_SEGMENT + INK_GAP_L);
        }
        F.y += h;
        if (block(INK_CONTROL + INK_GAP_L)) group_row(group(1), "phone", TR("移动网络"), "", 0, "shell.mobile");
        F.y += INK_CONTROL + INK_GAP_L;
    }
}

static void body_page_clicked(lv_event_t *e) {
    ink_receipt_t rc;
    ink_cap_invoke(lv_event_get_user_data(e) == (void *) (intptr_t) 1 ? "shell.page_next" : "shell.page_prev", NULL,
                   INK_SRC_TOUCH, 0, &rc);
}

static void notify_bind(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field,
                        ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    if (!(index % (size_t)N.notify_capacity)) memset(N.notify_ids, 0, sizeof N.notify_ids);
    unsigned at = index % 6;
    ink_db_cursor_t cursor = {0};
    for (size_t i = 0; i <= index; i++) if (ink_notify_next(&cursor, &N.notify_items[at]) != 1) return;
    ink_notify_view_t *n = &N.notify_items[at];
    snprintf(N.notify_ids[at], sizeof N.notify_ids[at], "notify:%u", (unsigned)n->id);
    snprintf(N.notify_args[at], sizeof N.notify_args[at], "{\"id\":%u}", (unsigned)n->id);
    *field = (ink_ui_field_spec_t){.id = N.notify_ids[at], .role = "listitem", .label = n->title,
        .value = n->body, .object = {N.notify_ids[at], (uint32_t)n->read}};
    *action = (ink_ui_action_spec_t){.id = N.notify_ids[at], .label = TR("打开通知"), .capability = "shell.notify_open",
        .args_json = N.notify_args[at], .object = field->object, .enabled = true};
    int h = ink_ui_row_double(), text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    int delete_w = ink_ui_text_width(TR("删除"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP;
    if (delete_w < INK_PRIMARY) delete_w = INK_PRIMARY;
    int left = INK_CIRCLE_SMALL + INK_GAP, text_w = width() - left - delete_w - 2 * INK_GAP_M;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_BOLD, left, text_y, text_w);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, left,
            text_y + ink_ui_line_height(INK_FONT_LIST), text_w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), n->title); lv_label_set_text(lv_obj_get_child(slot, 1), n->body);
    while (lv_obj_get_child_count(slot) > 2) lv_obj_delete(lv_obj_get_child(slot, 2));
    ink_ui_circle(slot, !strcmp(n->channel, "message") ? "message" : "bell", INK_GAP_XS, (h - INK_CIRCLE_SMALL) / 2,
        INK_CIRCLE_SMALL, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(ink_ui_pill(slot, TR("删除"), width() - delete_w - INK_GAP_M, (h - INK_CONTROL) / 2,
        delete_w, INK_CONTROL, false, NULL, NULL), "notify.dismiss", N.notify_args[at]);
}
void draw_page_body(void) {
    if (N.page_id == PAGE_NOTIFICATIONS) {
        const ink_ui_action_spec_t actions[] = {
            {.id = "prev", .label = TR("上一页"), .capability = "shell.notify_prev"},
            {.id = "next", .label = TR("下一页"), .capability = "shell.notify_next"},
            {.id = "read_all", .label = TR("全部已读"), .capability = "notify.read", .args_json = "{\"id\":0}", .enabled = true}};
        const ink_ui_page_spec_t page = {.id = "Notifications", .title = TR("通知"), .object = {"notify:inbox", 0},
            .actions = actions, .action_count = 3};
        if (!N.notify_list) {
            int fits = (INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_GAP_M - 2 * (INK_CONTROL + INK_GAP)) /
                (ink_ui_row_double() + INK_GAP_M);
            N.notify_capacity = fits < 4 ? fits : 4; /* Keep the actions inside the production AI snapshot. */
            N.notify_list = ink_ui_list_create(N.page_body, &page, &N.notify_position, N.notify_capacity, 1,
                ink_ui_row_double(), TR("还没有通知"), notify_bind, NULL);
            lv_obj_t *read = ink_ui_pill(N.page_body, TR("全部已读"), INK_MARGIN,
                INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - INK_GAP_M,
                (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(read, "notify.read", "{\"id\":0}");
            ink_shell_bind(ink_ui_pill(N.page_body, TR("全部清除"), INK_MARGIN + (width() + INK_GAP) / 2,
                INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - INK_GAP_M,
                (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL), "notify.clear", NULL);
        }
        ink_db_cursor_t cursor = {0}; ink_notify_view_t n; size_t total = 0;
        while (ink_notify_next(&cursor, &n) == 1) total++;
        /* the rows' ids are filled again as rows are bound; an empty list binds none, so clear them here */
        memset(N.notify_ids, 0, sizeof N.notify_ids);
        if (N.notify_list) ink_ui_list_refresh(N.notify_list, total);
        return;
    }
    if (N.page_id != PAGE_NOTIFICATIONS) {
        int room = INK_CANVAS_H - body_top() - INK_GAP_M;
        F.y = F.page = 0;
        F.room = room;
        F.draw = false;
        body_blocks();
        if (F.page) { /* room for the page turner */
            room -= INK_CONTROL + INK_GAP;
            F.y = F.page = 0;
            F.room = room;
            body_blocks();
        }
        N.body_pages = F.page + 1;
        if (target_page >= 0) N.body_page = target_page; /* settings.open: the page with its row */
        target_page = -1;
        if (N.body_page >= N.body_pages) N.body_page = N.body_pages - 1;
        if (home_update()) return;
        lv_obj_clean(N.page_body);
        F.y = F.page = 0;
        F.room = room;
        F.draw = true;
        body_blocks();
        if (N.body_pages > 1)
            ink_ui_pagebar(N.page_body, N.body_page + 1, N.body_pages, room + INK_GAP, body_page_clicked);
        ink_ui_hint_obj(N.page_body, INK_REFRESH_UI);
        return;
    }
}

static void search_entered(void *ctx, const char *value) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    char args[160];
    ink_jw_t w; ink_jw_init(&w, args, sizeof args);
    ink_jw_raw(&w, "{\"query\":"); ink_jw_str(&w, value ? value : ""); ink_jw_raw(&w, "}");
    ink_receipt_t rc;
    if (!w.overflow) ink_cap_invoke_json("settings.search", args, INK_SRC_TOUCH, 0, &rc);
}
static void create_page(lv_obj_t *screen, void *arg) {
    N.page = screen;
    N.page_id = (page_t) (intptr_t) arg;
    if (N.page_id == PAGE_SETTINGS && strcmp(H.member, ink_member_current()->id)) memset(&H, 0, sizeof H); /* not theirs */
    N.body_page = N.page_id == PAGE_SETTINGS ? H.page : 0;
    static const char *const titles[] = {TR_NOOP("设置"), TR_NOOP("显示和亮度"), TR_NOOP("声音"), TR_NOOP("网络和 SIM 卡"), TR_NOOP("通知"), TR_NOOP("通知"), TR_NOOP("已存的网络")};
    _Static_assert(sizeof titles / sizeof *titles == sizeof PAGES / sizeof *PAGES, "page titles");
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, TR(titles[N.page_id]));
    if (N.page_id == PAGE_SETTINGS) { /* the one shared search field, above the rows (AOSP's search bar) */
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 32, .hint = TR("找一项设置，例如暖光"), .initial = H.query};
        lv_obj_t *head = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W, ink_textfield_height(INK_FIELD_SEARCH),
                                    0, INK_SURFACE_WHITE, false);
        lv_obj_t *field = ink_textfield_create(head, &cfg, search_entered, (void *)(uintptr_t)N.epoch, false);
        lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), 0);
        ink_textfield_style(field, INK_FIELD_SEARCH, TR("搜索"));
    }
    N.page_body = ink_ui_box(screen, 0, body_top(), INK_CANVAS_W, INK_CANVAS_H - body_top(), 0, INK_SURFACE_WHITE, false);
    if (N.page_id == PAGE_KNOWN || N.page_id == PAGE_NETWORK) { N.known_count = -1; N.known_epoch++; }
    draw_page_body();
    if (N.page_id == PAGE_KNOWN || N.page_id == PAGE_NETWORK) known_load(NULL, NULL, NULL);
}
static void destroy_page(void) {
    if (N.page_id == PAGE_SETTINGS) { H.page = N.body_page; H.n = 0; } /* back from a row's page: the same page of rows */
    if (N.page_id == PAGE_KNOWN || N.page_id == PAGE_NETWORK) N.known_epoch++;
    if (N.page_id == PAGE_NETWORK) {
        N.picked_ssid[0] = 0;
        N.result[0] = 0;
        ink_text_blur(0);
    }
    N.page = N.page_body = NULL;
    N.notify_list = NULL;
}
void turn_body(int delta) {
    int page = N.body_page + delta;
    if (!N.page_body || page < 0 || page >= N.body_pages) return;
    N.body_page = page;
    ink_ui_content_changed();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    draw_page_body();
}
static bool page_key(ink_key_t key, bool lng) {
    if (N.notify_list) return ink_ui_list_key(N.notify_list, key, lng);
    if (lng || N.body_pages < 2 || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    turn_body(key == INK_KEY_NEXT ? 1 : -1);
    return true;
}
static int page_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    if (N.page_id == PAGE_SETTINGS) { /* the groups' rows on this page, or what the search found: name, value, state, action */
        ink_jw_t w; ink_jw_init(&w, buf, cap);
        ink_jw_raw(&w, "{\"page\":\"Settings\",\"query\":"); ink_jw_str(&w, H.query);
        ink_jw_raw(&w, ",\"language\":"); ink_jw_str(&w, ink_lang_code());
        ink_jw_raw(&w, ",\"page_number\":"); ink_jw_int(&w, N.body_page + 1);
        ink_jw_raw(&w, ",\"pages\":"); ink_jw_int(&w, N.body_pages);
        ink_jw_raw(&w, ",\"rows\":[");
        for (int k = 0; k < H.n; k++) row_json(&w, H.it[k], k, !H.query[0]);
        ink_jw_raw(&w, "],\"actions\":[\"settings.search(query)\",\"settings.open(id)\",\"settings.set(id,value)\",\"shell.page_next\",\"shell.page_prev\",\"shell.back\"]}");
        return w.overflow ? -1 : (int)w.len;
    }
    if (N.notify_list) {
        int n = ink_ui_list_snapshot(N.notify_list, buf, cap);
        if (n <= 0) return n;
        ink_jw_t w = {.buf = buf, .cap = cap, .len = (size_t)n - 1};
        ink_jw_raw(&w, ",\"clear_action\":{\"label\":"); ink_jw_str(&w, TR("全部清除"));
        ink_jw_raw(&w, ",\"capability\":\"notify.clear\",\"args_json\":\"{}\"},\"dismiss_actions\":[");
        bool comma = false;
        for (unsigned i = 0; i < sizeof N.notify_ids / sizeof *N.notify_ids; i++) if (N.notify_ids[i][0]) {
            ink_jw_raw(&w, comma ? ",{\"object\":" : "{\"object\":");
            ink_jw_str(&w, N.notify_ids[i]);
            ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, TR("删除"));
            ink_jw_raw(&w, ",\"capability\":\"notify.dismiss\",\"args_json\":"); ink_jw_str(&w, N.notify_args[i]);
            ink_jw_raw(&w, "}"); comma = true;
        }
        ink_jw_raw(&w, "]}");
        return w.overflow ? -1 : (int)w.len;
    }
    if (N.page_id == PAGE_CHANNELS) {
        ink_jw_t w; ink_jw_init(&w, buf, cap);
        ink_jw_raw(&w, "{\"page\":\"SetNotifications\",\"page_number\":"); ink_jw_int(&w, N.body_page + 1);
        ink_jw_raw(&w, ",\"pages\":"); ink_jw_int(&w, N.body_pages);
        ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, TR(CHANNEL_NOTE));
        ink_jw_raw(&w, ",\"navigation\":[\"shell.page_prev\",\"shell.page_next\",\"shell.back\"],\"channels\":[");
        for (int i = N.channel_first; i >= 0 && i < N.channel_end; i++) {
            const ink_notify_channel_t *ch = ink_notify_channel_at(i);
            bool on = ink_notify_channel_on(ch->id);
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"enabled\":%s}", ch->id, on ? "false" : "true");
            ink_jw_raw(&w, i == N.channel_first ? "{\"id\":" : ",{\"id\":"); ink_jw_str(&w, ch->id);
            ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, TR(ch->title));
            ink_jw_raw(&w, on ? ",\"enabled\":true" : ",\"enabled\":false");
            ink_jw_raw(&w, ",\"capability\":\"notify.set_channel\",\"args_json\":"); ink_jw_str(&w, args);
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]}"); return w.overflow ? -1 : (int)w.len;
    }
    if (N.page_id == PAGE_KNOWN) {
        char ssid[33], state[24]; state_text("wifi.ssid", ssid, sizeof ssid); state_text("wifi.state", state, sizeof state);
        int n = snprintf(buf, cap, "KnownNetworks；已存=%d；第 %d 页，共 %d 页；", N.known_count, N.body_page + 1, N.body_pages);
        for (int i = 0; i < N.known_count && n >= 0 && (size_t)n < cap; i++)
            n += snprintf(buf + n, cap - (size_t)n, "%s=%s；", N.known_ssid[i], !strcmp(ssid, N.known_ssid[i]) ? state : "saved");
        if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "动作：wifi.connect(ssid)、wifi.forget(ssid)、system.undo(object,revision)、shell.page_next、shell.page_prev、shell.back。");
        return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
    }
    if (N.page_id == PAGE_SOUND)
        return snprintf(buf, cap, TR("SetSound；音量=%lld；勿扰=%lld；按键提示音(audio.key_tone)=%lld；振动(audio.vibration)=%lld；振动强度(audio.vibration_level)=%lld（0弱35ms、1中50ms、2强70ms）；来电铃声(audio.call_ring)=%lld；闹钟铃声(audio.alarm_ring)=%lld；铃声：0清音、1双音、2柔音；settings.get(id)、settings.set(id,value)；通知：shell.notify_settings；shell.page_next、shell.page_prev、shell.back。"), (long long)ink_setting_get("audio.volume"), (long long)ink_setting_get("notify.dnd"), (long long)ink_setting_get("audio.key_tone"), (long long)ink_setting_get("audio.vibration"), (long long)ink_setting_get("audio.vibration_level"), (long long)ink_setting_get("audio.call_ring"), (long long)ink_setting_get("audio.alarm_ring"));
    int n = snprintf(buf, cap, "%s；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；网络偏好=%lld；通知=%d。",
                     PAGES[N.page_id].name, ink_member_current()->name, (long long) ink_setting_get("display.light"),
                     (long long) ink_setting_get("display.warmth"), (long long) ink_setting_get("audio.volume"),
                     (long long) ink_setting_get("net.prefer"), ink_notify_unread());
    if (N.page_id != PAGE_NOTIFICATIONS && n >= 0 && (size_t) n < cap)
        n += snprintf(buf + n, cap - (size_t) n, "字号=%lld；锁屏画面=%lld；自动锁屏秒=%lld；拿起亮屏(power.lift_wake)=%lld；勿扰=%lld；叫名字=%lld；语言=%s；",
                      (long long) ink_setting_get("display.font_size"),
                      (long long) ink_setting_get("display.lock_style"),
                      (long long) ink_setting_get("power.lock_after_s"), (long long) ink_setting_get("power.lift_wake"),
                      (long long) ink_setting_get("notify.dnd"),
                      (long long) ink_setting_get("voice.handsfree"), ink_lang_code());
    if (N.body_pages > 1 && N.page_id != PAGE_NOTIFICATIONS && n >= 0 && (size_t) n < cap)
        n += snprintf(buf + n, cap - (size_t) n, "这一组第 %d 页，共 %d 页；动作：shell.page_next、shell.page_prev；",
                      N.body_page + 1, N.body_pages);
    if (N.page_id == PAGE_NETWORK && n >= 0 && (size_t) n < cap) {
        char ssid[64];
        state_text("wifi.ssid", ssid, sizeof ssid);
        n += snprintf(buf + n, cap - (size_t) n, "飞行模式=%lld；Wi-Fi=%lld；已连网络=%s；选中网络=%s；网络组=%d；SIM=%lld；4G在线=%lld；",
                      (long long) ink_setting_get("net.airplane"), (long long) ink_setting_get("wifi.enabled"), ssid,
                      N.picked_ssid, N.wifi_page + 1, (long long) ink_setting_get("cell.sim_slot"),
                      (long long) integer("cell.registered"));
        if (integer("cell.config_readonly") && n >= 0 && (size_t)n < cap)
            n += snprintf(buf + n, cap - (size_t)n, "SIM 卡暂不能更改；");
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2 && n >= 0 && (size_t) n < cap; i++)
            n += snprintf(buf + n, cap - (size_t) n, "网络[%d]=%s；信号=%d dBm；需要密码=%d；", i, N.wifi_ssid[i], N.wifi_info[i].rssi, N.wifi_info[i].secured);
        if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "结果=%s；", N.result);
        if (ssid[0] && N.body_page == N.wifi_forget_page && n >= 0 && (size_t)n < cap) {
            char args[256]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
            ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, ssid); ink_jw_raw(&w, "}");
            n += snprintf(buf + n, cap - (size_t)n, "忘掉这个网络：wifi.forget(%s)；", args);
        }
    }
    const char *actions =
        N.page_id == PAGE_NETWORK ? "动作：shell.known_networks、net.set_airplane、settings.set、wifi.scan、wifi.list、shell.back、shell.control、shell.wifi_"
                                      "next、shell.wifi_prev、shell.wifi_pick、shell.mobile；设置设备：shell.launch(id=boot)。"
                                    : N.page_id == PAGE_DISPLAY
                                        ? "动作：settings.set、shell.today_edit、shell.back、shell.control。"
                                    : "动作：settings.set、shell.back、shell.control。";
    if (n >= 0 && (size_t) n < cap) n += snprintf(buf + n, cap - (size_t) n, "%s", actions);
    return n < 0 ? 0 : (size_t) n < cap ? n : (int) cap - 1;
}

static void home_open(const char *query) {
    snprintf(H.query, sizeof H.query, "%s", query);
    snprintf(H.member, sizeof H.member, "%s", ink_member_current()->id);
    H.page = N.body_page = 0;
    if (!strcmp(ink_ui_page_name(), PAGES[PAGE_SETTINGS].name)) { ink_ui_content_changed(); ink_ui_redraw(); return; }
    ink_shell_close();
    ink_ui_push(&PAGES[PAGE_SETTINGS], (void *)(intptr_t)PAGE_SETTINGS);
}
void ink_shell_open_page(const char *name) {
    if (!strcmp(name, "SetBluetooth")) { ink_bluetooth_page_open(); return; }
    if (!strcmp(name, PAGES[PAGE_SETTINGS].name)) { home_open(""); return; }
    ink_settings_page_open(name);
    for (unsigned i = 0; i < sizeof PAGES / sizeof *PAGES; i++)
        if (!strcmp(name, PAGES[i].name)) { /* a child's own rows are on the display and sound pages */
            if (child() && i != PAGE_NOTIFICATIONS && i != PAGE_DISPLAY && i != PAGE_SOUND) return;
            ink_shell_close();
            ink_ui_push(&PAGES[i], (void *) (intptr_t) i);
            return;
        }
    ink_settings_family_open(name);
}

/* settings.search and settings.open: the field, a row and the AI take the same way, past each row's state. */
static void search_call(ink_call_t *c) {
    char q[48] = "", *json = osal_malloc(3072, OSAL_MEM_EXTERNAL);
    const ink_sr_item_t *found[FOUND_MAX];
    ink_args_get_str(&c->args, "query", q, sizeof q);
    ink_text_blur(0);
    home_open(q);
    int n = ink_sr_search(q, found, FOUND_MAX);
    if (json) { /* what the page lists, for the AI */
        ink_jw_t w; ink_jw_init(&w, json, 3072);
        ink_jw_raw(&w, "{\"query\":"); ink_jw_str(&w, q); ink_jw_raw(&w, ",\"results\":[");
        for (int i = 0; i < n; i++) row_json(&w, found[i], i, false);
        ink_jw_raw(&w, "]}");
        if (!w.overflow) ink_cap_result(c, json);
        osal_free(json);
    }
    if (n) snprintf(c->rc.say, sizeof c->rc.say, TR("找到 %d 项设置"), n);
    else if (q[0]) snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("没有找到这项设置"));
    snprintf(c->rc.object, sizeof c->rc.object, "screen:Settings");
}
static void open_call(ink_call_t *c) {
    char id[40] = "", why[96];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    const ink_sr_item_t *it = ink_sr_find(id);
    ink_sr_state_t s = it ? ink_sr_value(it, why, sizeof why) : INK_SR_UNSUPPORTED;
    if (!id[0]) home_open("");
    else if (s != INK_SR_AVAILABLE && s != INK_SR_UNSEARCHABLE) { /* grayed or not here: refused with what its row says */
        c->rc.status = it ? INK_RC_REJECTED : INK_RC_INVALID;
        snprintf(c->rc.say, sizeof c->rc.say, "%s", it ? why : TR("没有这项设置"));
    } else {
        ink_sr_target = it; /* its page frames the row while it is built */
        if (it->open) ink_cap_invoke(it->open, NULL, c->src, 0, &c->rc);
        else if (!strcmp(ink_ui_page_name(), it->page)) ink_ui_redraw();
        else ink_shell_open_page(it->page);
        ink_sr_target = NULL;
        if (strcmp(ink_ui_page_name(), it->page)) c->rc.status = INK_RC_REJECTED;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "screen:%s", ink_shell_board());
}
static char OPEN_INPUT[1536] INK_PSRAM_BSS; /* every id of the registry, so the AI's description is the table's */
static const ink_cap_t CAPS[] = {
    {.id = "settings.search", .title = "在设置里搜一项设置（名称或别名），首页列出结果",
     .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":32}},\"required\":[\"query\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .run = search_call},
    {.id = "settings.open", .title = "打开一项设置所在的页并定位到那一行；不给 id 打开设置首页",
     .input = OPEN_INPUT, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_call}};
int ink_settings_home_init(void) {
    ink_jw_t w; ink_jw_init(&w, OPEN_INPUT, sizeof OPEN_INPUT);
    ink_jw_raw(&w, "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"enum\":[");
    for (int i = 0; ink_sr_at(i); i++) {
        if (i) ink_jw_raw(&w, ",");
        ink_jw_str(&w, ink_sr_at(i)->id);
    }
    ink_jw_raw(&w, "]}}}");
    return w.overflow ? -1 : ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
}

static const char *const FONT_CHOICES[] = {TR_NOOP("小"), TR_NOOP("中"), TR_NOOP("大"), TR_NOOP("特大"), NULL};
static const char *const LOCK_CHOICES[] = {TR_NOOP("这一页"), TR_NOOP("时钟和待办"), TR_NOOP("壁纸"), TR_NOOP("简约时钟"), TR_NOOP("待机时钟"), TR_NOOP("月历"), NULL};
const ink_setting_def_t SETTINGS[] = {
    {.id = "display.font_size", .title = TR_NOOP("字号"), .type = INK_SET_ENUM, .def = 1, .choices = FONT_CHOICES},
    {.id = "display.lock_style",
     .title = TR_NOOP("锁屏画面"),
     .type = INK_SET_ENUM,
     .def = 0,
     .choices = LOCK_CHOICES,
     .child_may_change = true}};
