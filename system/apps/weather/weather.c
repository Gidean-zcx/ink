/* Weather (D-125): the device names a city, the hub fetches the forecast; the last answer is kept for offline. */
/* ui-scale-runtime: dimension y,w Theme-derived page geometry. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/hub.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>
typedef struct { char w[24], i[12]; int hi, lo, p; } day_t;
static const ink_db_def_t DB = {.name = "weather.city", .title = "天气城市和上次的天气", .scope = INK_DB_DEVICE,
    .schema = 1, .json = true, .ai_read = true, .max_records = 2, .max_bytes = 2048};
static struct {
    int64_t id, at, asked_s;
    char city[32], last[768], reply[768], found_reply[768], status[48], query[64];
    int t, ndays, reply_status, found_status;
    day_t now, days[3];
    bool have, offline, asking, finding;
    ink_call_t *waiting[4], *find_call;
    int nwait, nfound;
    struct { int64_t id; char name[32], area[48], args[96]; } found[5];
    ink_exec_job_t got_job, found_job;
    char texts[7][96], updated[48], ranges[4][24];
    ink_ui_field_spec_t fields[8];
    ink_ui_action_spec_t actions[8];
    ink_ui_page_spec_t page;
    lv_obj_t *field;
} W INK_PSRAM_BSS;
static const ink_activity_t WEATHER, CITY;
static const ink_ui_action_spec_t FOCUS = {.id = "name", .label = "输入城市名", .capability = "weather.focus", .enabled = true};

static bool online(void) { int64_t v = 0; return ink_state_get_int(ink_state_find("net.online"), &v) && v; }
static bool on_top(void) { return ink_ui_page_current() && (!strcmp(ink_ui_page_name(), "Weather") || !strcmp(ink_ui_page_name(), "WeatherCity")); }
static void known_icon(char *name) { /* a name the device has no icon for would stop it: those five only */
    static const char *const ICONS[] = {"sun", "cloudsun", "cloud", "rain", "snow"};
    for (unsigned k = 0; k < 5; k++) if (!strcmp(name, ICONS[k])) return;
    strcpy(name, "cloud");
}
/* The hub's answer (protocol weather/get): what the pages, the card and weather.get read. */
static bool parse(const char *text) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t t = 0, hi = 0, lo = 0, p = -1;
    bool ok = j && ink_json_parse(j, text, strlen(text)) == 0 && ink_json_int(j, ink_json_find(j, 0, "at"), &W.at) &&
              ink_json_int(j, ink_json_find(j, 0, "t"), &t);
    if (ok) {
        ink_json_int(j, ink_json_find(j, 0, "hi"), &hi); ink_json_int(j, ink_json_find(j, 0, "lo"), &lo); ink_json_int(j, ink_json_find(j, 0, "p"), &p);
        W.t = (int)t; W.now = (day_t){.hi = (int)hi, .lo = (int)lo, .p = (int)p};
        ink_json_str(j, ink_json_find(j, 0, "w"), W.now.w, sizeof W.now.w); ink_json_str(j, ink_json_find(j, 0, "i"), W.now.i, sizeof W.now.i); known_icon(W.now.i);
        int days = ink_json_find(j, 0, "days"); /* [[text, icon, high, low, rain %], ...]: flat, six tokens a day */
        for (W.ndays = 0; days > 0 && W.ndays < 3 && days + 6 * W.ndays + 6 < j->ntok && j->tok[days + 6 * W.ndays + 1].size == 5; W.ndays++) {
            int k = days + 6 * W.ndays + 1; day_t *d = &W.days[W.ndays]; int64_t v[3] = {0, 0, -1};
            ink_json_str(j, k + 1, d->w, sizeof d->w); ink_json_str(j, k + 2, d->i, sizeof d->i); known_icon(d->i);
            for (int m = 0; m < 3; m++) ink_json_int(j, k + 3 + m, &v[m]);
            d->hi = (int)v[0]; d->lo = (int)v[1]; d->p = (int)v[2];
        }
    }
    osal_free(j);
    return ok;
}
static void save(uint32_t id, const char *json) {
    ink_db_t *db = ink_db_open(&DB, NULL);
    ink_db_write_t w = {.id = id, .data = json, .len = strlen(json), .src = INK_SRC_SYSTEM};
    if (db) ink_db_put(db, &w, NULL);
    ink_db_close(db);
}
static void day_name(int i, char *out, size_t cap) {
    static const char *const NAMES[] = {"今天", "明天", "后天"}, *const WEEK[] = {"日", "一", "二", "三", "四", "五", "六"};
    ink_tm_t tm; ink_time_to_local(W.at + i * 86400, &tm);
    if (i < 3) snprintf(out, cap, "%s", NAMES[i]);
    else snprintf(out, cap, "周%s", WEEK[tm.wday]);
}
static void describe(void) {
    ink_tm_t tm; ink_time_to_local(W.at, &tm);
    snprintf(W.updated, sizeof W.updated, "%d月%d日 %02d:%02d 更新", tm.month, tm.day, tm.hour, tm.min);
}
static void answer(ink_call_t *c) {
    if (!W.id) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "还没有设天气城市，请在设置里选城市"); return; }
    if (!W.have) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "还没取到%s的天气，请联网后再试", W.city); return; }
    char json[768], name[16]; ink_jw_t w; ink_jw_init(&w, json, sizeof json);
    ink_jw_raw(&w, "{\"city\":"); ink_jw_str(&w, W.city); ink_jw_raw(&w, ",\"updated\":"); ink_jw_str(&w, W.updated);
    ink_jw_raw(&w, W.offline ? ",\"fresh\":false,\"now\":{\"t\":" : ",\"fresh\":true,\"now\":{\"t\":"); ink_jw_int(&w, W.t);
    ink_jw_raw(&w, ",\"w\":"); ink_jw_str(&w, W.now.w); ink_jw_raw(&w, "},\"days\":[");
    for (int i = 0; i <= W.ndays; i++) {
        const day_t *d = i ? &W.days[i - 1] : &W.now;
        day_name(i, name, sizeof name);
        ink_jw_raw(&w, i ? ",{\"day\":" : "{\"day\":"); ink_jw_str(&w, name); ink_jw_raw(&w, ",\"w\":"); ink_jw_str(&w, d->w);
        ink_jw_raw(&w, ",\"hi\":"); ink_jw_int(&w, d->hi); ink_jw_raw(&w, ",\"lo\":"); ink_jw_int(&w, d->lo);
        ink_jw_raw(&w, ",\"rain\":"); ink_jw_int(&w, d->p); ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]}");
    if (!w.overflow) ink_cap_result(c, json);
    snprintf(c->rc.say, sizeof c->rc.say, "%s现在%d°，%s，最高%d°，最低%d°%s", W.city, W.t, W.now.w, W.now.hi, W.now.lo,
             W.offline ? "（没联网，这是上次的天气）" : "");
}
static void got(void *arg) { /* UI task */
    (void)arg; W.asking = false;
    W.offline = W.reply_status != INK_HUB_OK || !parse(W.reply);
    if (W.offline) { if (W.have) parse(W.last); }
    else { W.have = true; snprintf(W.last, sizeof W.last, "%s", W.reply); save(2, W.last); describe(); }
    for (int i = 0; i < W.nwait; i++) { answer(W.waiting[i]); ink_cap_done(W.waiting[i]); }
    W.nwait = 0;
    ink_app_card_changed("weather");
    if (on_top()) ink_ui_redraw();
}
static void copy_reply(char *out, size_t cap, int *status, int st, const ink_json_t *j, int t) {
    int n = st == INK_HUB_OK && t >= 0 ? j->tok[t].end - j->tok[t].start : -1;
    *status = n > 0 && (size_t)n < cap && j->tok[t].type == 1 ? INK_HUB_OK : INK_HUB_ERR_REMOTE;
    snprintf(out, cap, "%.*s", *status == INK_HUB_OK ? n : 0, *status == INK_HUB_OK ? j->text + j->tok[t].start : "");
}
static void hub_got(void *ctx, int st, const ink_json_t *j, int t) { /* hub task */
    (void)ctx; copy_reply(W.reply, sizeof W.reply, &W.reply_status, st, j, t);
    ink_exec_post(INK_EXEC_UI, &W.got_job, got, NULL);
}
static void fetch(ink_call_t *c) {
    if (c) { if (W.nwait == 4) { c->rc.status = INK_RC_FAILED; return; } ink_cap_defer(c); W.waiting[W.nwait++] = c; }
    if (W.asking) return;
    char params[48]; snprintf(params, sizeof params, "{\"id\":%lld}", (long long)W.id);
    W.asking = true; W.asked_s = ink_time_now_s(); W.reply_status = INK_HUB_ERR_OFFLINE;
    if (!online() || (ink_hub_demand(), ink_hub_call("weather/get", params, 8000000, hub_got, NULL) < 0)) got(NULL);
}
static bool stale(void) { return W.id && !W.asking && (!W.have || ink_time_now_s() - W.at >= 1800) && ink_time_now_s() - W.asked_s >= 1800; }
static void found(void *arg) { /* UI task */
    (void)arg; ink_call_t *c = W.find_call;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    W.finding = false; W.find_call = NULL; W.nfound = 0;
    if (j && W.found_status == INK_HUB_OK && ink_json_parse(j, W.found_reply, strlen(W.found_reply)) == 0) {
        int arr = ink_json_find(j, 0, "cities");
        for (int k = arr + 1; arr > 0 && W.nfound < 5 && k + 6 < j->ntok && j->tok[k].type == 1; k += 7) {
            ink_json_int(j, ink_json_find(j, k, "id"), &W.found[W.nfound].id);
            ink_json_str(j, ink_json_find(j, k, "name"), W.found[W.nfound].name, sizeof W.found[0].name);
            ink_json_str(j, ink_json_find(j, k, "area"), W.found[W.nfound].area, sizeof W.found[0].area);
            snprintf(W.found[W.nfound].args, sizeof W.found[0].args, "{\"id\":%lld}", (long long)W.found[W.nfound].id);
            W.nfound++;
        }
        snprintf(W.status, sizeof W.status, "%s", W.nfound ? "选一个城市" : "没有找到这个城市");
        if (c) ink_cap_result(c, W.found_reply);
    } else {
        snprintf(W.status, sizeof W.status, "%s", "没联网，找不到城市");
        if (c) c->rc.status = INK_RC_FAILED;
    }
    osal_free(j);
    if (c) { snprintf(c->rc.say, sizeof c->rc.say, "%s", W.status); ink_cap_done(c); }
    if (on_top()) ink_ui_redraw();
}
static void hub_found(void *ctx, int st, const ink_json_t *j, int t) {
    (void)ctx; copy_reply(W.found_reply, sizeof W.found_reply, &W.found_status, st, j, t);
    ink_exec_post(INK_EXEC_UI, &W.found_job, found, NULL);
}
static void run(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "weather.open")) { ink_ui_push(&WEATHER, NULL); return; }
    if (!strcmp(id, "weather.focus")) { if (!W.field || ink_textfield_short_focus(W.field)) c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(id, "weather.city")) { W.nfound = 0; W.status[0] = 0; ink_ui_push(&CITY, NULL); return; }
    if (!strcmp(id, "weather.get")) {
        bool person = c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY; /* 刷新 always asks; others take a fresh copy */
        if (!W.id || (!person && W.have && !W.offline && ink_time_now_s() - W.at < 1800)) answer(c);
        else fetch(c);
        return;
    }
    if (!strcmp(id, "weather.find")) {
        char params[96]; ink_jw_t w; ink_jw_init(&w, params, sizeof params);
        if (W.finding || ink_args_get_str(&c->args, "name", W.query, sizeof W.query) <= 0) { c->rc.status = INK_RC_INVALID; return; }
        ink_jw_raw(&w, "{\"name\":"); ink_jw_str(&w, W.query); ink_jw_raw(&w, "}");
        ink_cap_defer(c); W.finding = true; W.find_call = c; W.found_status = INK_HUB_ERR_OFFLINE;
        snprintf(W.status, sizeof W.status, "正在找…"); if (on_top()) ink_ui_redraw();
        if (w.overflow || !online() || (ink_hub_demand(), ink_hub_call("weather/find", params, 8000000, hub_found, NULL) < 0)) found(NULL);
        return;
    }
    int64_t pick = 0; int at = -1; /* weather.set_city: one of the cities just found */
    ink_args_get_int(&c->args, "id", &pick);
    for (int i = 0; i < W.nfound; i++) if (W.found[i].id == pick) at = i;
    if (at < 0) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "请先找城市，再从结果里选"); return; }
    bool first = !W.id; /* the first city also puts the card on Today, if there is room; later ones leave Today alone */
    W.id = pick; W.have = W.offline = false; W.at = W.asked_s = 0;
    snprintf(W.city, sizeof W.city, "%s", W.found[at].name);
    char json[96]; ink_jw_t w; ink_jw_init(&w, json, sizeof json);
    ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, W.id); ink_jw_raw(&w, ",\"name\":"); ink_jw_str(&w, W.city); ink_jw_raw(&w, "}");
    save(1, json); save(2, "{}"); snprintf(c->rc.say, sizeof c->rc.say, "天气城市改成%s了", W.city);
    ink_receipt_t today; if (first) ink_cap_invoke_json("shell.today_card", "{\"card\":\"weather:now\"}", INK_SRC_SYSTEM, 0, &today);
    if (ink_ui_page_current() && !strcmp(ink_ui_page_name(), "WeatherCity")) ink_ui_pop(); /* back where the person came from */
    fetch(NULL);
}
const char *ink_weather_city(void) { return W.id ? W.city : "还没有设"; }
static void field(int i, const char *id, const char *label, const char *value) {
    W.fields[i] = (ink_ui_field_spec_t){.id = id, .role = "text", .label = label, .value = value};
}
static void action(int i, const char *id, const char *label, const char *cap, const char *args) {
    W.actions[i] = (ink_ui_action_spec_t){.id = id, .label = label, .capability = cap, .args_json = args, .enabled = true};
}
static void weather_create(lv_obj_t *screen, void *arg) {
    (void)arg; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, n = 0;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, W.id ? W.city : "天气");
    action(0, "refresh", "刷新", "weather.get", "{}"); action(1, "city", "换城市", "weather.city", "{}"); action(2, "back", "返回", "shell.back", "{}");
    if (!W.have) {
        const char *empty = !W.id ? "还没有设天气城市" : W.offline ? "没联网，还没取到天气" : "正在取天气…";
        ink_ui_notice(screen, empty, y); field(n++, "empty", "天气", empty);
    } else {
        lv_obj_t *card = ink_ui_card(screen, INK_MARGIN, y, w, 2 * INK_GAP_L + INK_LINE_NUMBER + INK_LINE_LIST, INK_SURFACE_WHITE);
        snprintf(W.texts[0], sizeof W.texts[0], "%d°", W.t);
        ink_ui_line(card, W.texts[0], INK_FONT_NUMBER, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_L, w / 2);
        ink_ui_icon(card, W.now.i, INK_ICON_STATE, INK_ICON_LINE, w - INK_GAP_L - INK_ICON_STATE, INK_GAP_L, false);
        snprintf(W.texts[1], sizeof W.texts[1], "%s · 最高 %d° 最低 %d°", W.now.w, W.now.hi, W.now.lo);
        ink_ui_line(card, W.texts[1], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L, INK_GAP_L + INK_LINE_NUMBER, w - 2 * INK_GAP_L);
        field(n++, "now", "现在", W.texts[0]); field(n++, "today", "今天", W.texts[1]);
        y += 2 * INK_GAP_L + INK_LINE_NUMBER + INK_LINE_LIST + INK_GAP;
        lv_obj_t *group = ink_ui_card(screen, INK_MARGIN, y, w, (W.ndays + 1) * INK_CONTROL, INK_SURFACE_STATIC);
        static const char *const IDS[] = {"rain", "day1", "day2", "day3"};
        for (int i = 0; i <= W.ndays; i++) {
            const day_t *d = i ? &W.days[i - 1] : &W.now;
            char name[16], what[24]; day_name(i, name, sizeof name); memcpy(what, d->w, sizeof what); /* gcc: not from W into W */
            snprintf(W.texts[2 + i], sizeof W.texts[2 + i], i ? "%s · %s" : "%s降水概率", name, what);
            char *range = W.ranges[i]; /* the page's snapshot reads it later */
            snprintf(range, sizeof W.ranges[i], "不知道");
            if (i) snprintf(range, sizeof W.ranges[i], "%d° / %d°", d->hi, d->lo); else if (d->p >= 0) snprintf(range, sizeof W.ranges[i], "%d%%", d->p);
            ink_ui_list_row(group, i ? d->i : "rain", W.texts[2 + i], range, 0, i * INK_CONTROL, w, false, NULL, NULL);
            field(n++, IDS[i], W.texts[2 + i], i ? d->w : range);
        }
        y += (W.ndays + 1) * INK_CONTROL + INK_GAP_S;
        snprintf(W.texts[6], sizeof W.texts[6], "%s%s\n数据来自 Open-Meteo.com", W.offline ? "没联网，" : "", W.updated); /* 2..5: the rows */
        ink_ui_notice(screen, W.texts[6], y); field(n++, "updated", "更新", W.texts[6]);
    }
    int half = (w - INK_GAP) / 2, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    ink_shell_bind(ink_ui_pill(screen, W.id ? "换城市" : "选城市", INK_MARGIN, bottom, W.id ? half : w, INK_PRIMARY, !W.id, NULL, NULL), "weather.city", "{}");
    if (W.id) ink_shell_bind(ink_ui_pill(screen, "刷新", INK_MARGIN + half + INK_GAP, bottom, half, INK_PRIMARY, true, NULL, NULL), "weather.get", "{}");
    W.page = (ink_ui_page_spec_t){.id = "Weather", .title = "天气", .object = {"weather.city", (uint32_t)W.at}, .fields = W.fields,
        .field_count = (size_t)n, .actions = W.actions, .action_count = 3};
    if (stale()) fetch(NULL);
}
static void entered(void *ctx, const char *value) {
    (void)ctx; char args[96]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
    ink_jw_raw(&w, "{\"name\":"); ink_jw_str(&w, value); ink_jw_raw(&w, "}");
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = args, .reply_exec = INK_EXEC_UI};
    if (!w.overflow) ink_cap_call("weather.find", NULL, &o);
}
static void city_create(lv_obj_t *screen, void *arg) {
    (void)arg; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, "天气城市");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 60, .hint = "输入城市名", .initial = W.query};
    W.field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    lv_obj_set_pos(W.field, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_size(W.field, ink_ui_px(w), ink_ui_px(INK_CONTROL));
    y += INK_CONTROL + INK_GAP;
    snprintf(W.texts[0], sizeof W.texts[0], "现在：%s", ink_weather_city());
    ink_ui_notice(screen, W.status[0] ? W.status : W.texts[0], y);
    field(0, "city", "天气城市", W.texts[0]); field(1, "status", "查找", W.status);
    W.fields[2] = (ink_ui_field_spec_t){.id = "name", .role = "searchbox", .label = "城市名，按搜索找（weather.find）"};
    ink_textfield_bind_short(W.field, &W.page, &W.fields[2], &FOCUS);
    y += INK_LINE_NOTE + INK_GAP;
    if (W.nfound) {
        lv_obj_t *group = ink_ui_card(screen, INK_MARGIN, y, w, W.nfound * ink_ui_row_double(), INK_SURFACE_STATIC);
        for (int i = 0; i < W.nfound; i++) {
            lv_obj_t *row = ink_ui_list_row(group, "globe", W.found[i].name, W.found[i].area, 0, i * ink_ui_row_double(), w, true, NULL, NULL);
            ink_shell_bind(row, "weather.set_city", W.found[i].args);
            W.actions[i] = (ink_ui_action_spec_t){.id = W.found[i].args, .label = W.found[i].name, .capability = "weather.set_city", .args_json = W.found[i].args, .enabled = true};
        }
    }
    W.actions[W.nfound] = FOCUS; action(W.nfound + 1, "back", "返回", "shell.back", "{}");
    W.page = (ink_ui_page_spec_t){.id = "WeatherCity", .title = "天气城市", .object = {"weather.city", (uint32_t)W.id}, .fields = W.fields,
        .field_count = 3, .actions = W.actions, .action_count = (size_t)W.nfound + 2};
}
static int snapshot(char *out, size_t cap) {
    int n = W.field ? ink_textfield_short_snapshot(W.field, out, cap) : -1; /* the city page adds where its input is */
    return n >= 0 ? n : ink_ui_semantics_snapshot(&W.page, out, cap);
}
static void city_destroy(void) { W.field = NULL; }
static const ink_activity_t WEATHER = {.name = "Weather", .app = "weather", .create = weather_create, .snapshot = snapshot};
static const ink_activity_t CITY = {.name = "WeatherCity", .app = "weather", .create = city_create, .destroy = city_destroy, .snapshot = snapshot};
static int card(int index, ink_card_t *out) {
    if (index || !W.id) return 0; /* offered for Today once there is a city */
    memset(out, 0, sizeof *out);
    snprintf(out->id, sizeof out->id, "now"); snprintf(out->title, sizeof out->title, "%s", W.city); snprintf(out->action, sizeof out->action, "weather.open");
    if (!W.have) snprintf(out->text, sizeof out->text, "还没取到天气");
    else { snprintf(out->text, sizeof out->text, "%d° %s %d°/%d°", W.t, W.now.w, W.now.hi, W.now.lo); snprintf(out->icon, sizeof out->icon, "%s", W.now.i); }
    out->rev = (uint32_t)W.at;
    if (stale()) fetch(NULL);
    return 1;
}
static const ink_cap_t CAPS[] = {
    {.id = "weather.get", .title = "查天气：现在、今天和之后几天，没联网时给上次的", .effect = INK_FX_READ, .child_ok = true,
     .qos = INK_QOS_NETWORK, .timeout_ms = 10000, .run = run},
    {.id = "weather.open", .title = "打开天气", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = run},
    {.id = "weather.city", .title = "设置天气城市", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = run},
    {.id = "weather.focus", .title = "在天气城市页输入城市名", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "WeatherCity", .run = run},
    {.id = "weather.find", .title = "按名字找天气城市", .input = "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\",\"maxLength\":60}},\"required\":[\"name\"]}",
     .effect = INK_FX_READ, .child_ok = true, .qos = INK_QOS_NETWORK, .timeout_ms = 10000, .run = run},
    {.id = "weather.set_city", .title = "从找到的城市里选一个当天气城市", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = false, .run = run},
};
static const ink_app_def_t APP = {.id = "weather", .title = "天气", .icon = INK_UI_ICON(cloudsun), .entry = &WEATHER,
    .system_app = true, .child_ok = true, .card = card};
int ink_weather_init(void) {
    if (ink_db_register(&DB) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP)) return -1;
    ink_db_t *db = ink_db_open(&DB, NULL);
    char json[96] = ""; ink_rec_t rec;
    if (db && ink_db_get(db, 1, &rec, json, sizeof json - 1) == INK_DB_OK) {
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        if (j && ink_json_parse(j, json, strlen(json)) == 0 && ink_json_int(j, ink_json_find(j, 0, "id"), &W.id))
            ink_json_str(j, ink_json_find(j, 0, "name"), W.city, sizeof W.city);
        osal_free(j);
        if (ink_db_get(db, 2, &rec, W.last, sizeof W.last - 1) == INK_DB_OK && parse(W.last)) { W.have = W.offline = true; describe(); }
    }
    ink_db_close(db);
    return 0;
}
