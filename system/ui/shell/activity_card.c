/* F6: one presentation of media, calls and real member jobs. */
/* ui-scale-runtime: dimension y,h,w,text_y Theme-sized activity and list rows. */
#include <stdio.h>
#include <string.h>
#include "activity_card.h"
#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_list.h"

typedef struct {
    char title[64], detail[96], id[32], args[32];
    bool running, cancel, heading;
} job_row_t;
static struct {
    char title[128], detail[128], member[16];
    const char *action, *cap, *icon;
    job_row_t rows[18];
    size_t count;
    bool jobs_ok;
    ink_exec_job_t refresh;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
} A INK_PSRAM_BSS;
static const ink_activity_t TASKS;

static void jobs_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    const char *p = rc->result;
    A.count = 0;
    A.jobs_ok = j && rc->status == INK_RC_DONE && p && *p++ == '[';
    bool ended = false;
    while (A.jobs_ok && *p != ']' && A.count < 17) {
        const char *end = p;
        bool quoted = false;
        for (; *end; end++) {
            if (quoted && *end == '\\' && end[1]) end++;
            else if (*end == '"') quoted = !quoted;
            else if (*end == '}' && !quoted) break;
        }
        A.jobs_ok = *end && !ink_json_parse(j, p, (size_t)(end - p + 1));
        if (!A.jobs_ok) break;
        bool running = ink_json_eq(j, ink_json_find(j, 0, "state"), "running");
        if (!running && !ended) {
            A.rows[A.count++] = (job_row_t){.title = "做好了", .id = "finished", .heading = true};
            ended = true;
        }
        job_row_t *r = &A.rows[A.count++];
        *r = (job_row_t){.running = running};
        int64_t id = 0;
        ink_json_int(j, ink_json_find(j, 0, "id"), &id);
        snprintf(r->id, sizeof r->id, "job:%lld", (long long)id);
        snprintf(r->args, sizeof r->args, "{\"id\":%lld}", (long long)id);
        ink_json_str(j, ink_json_find(j, 0, "title"), r->title, sizeof r->title);
        ink_json_str(j, ink_json_find(j, 0, running ? "stage" : "result"), r->detail, sizeof r->detail);
        if (ink_json_eq(j, ink_json_find(j, 0, "state"), "cancelled")) snprintf(r->detail, sizeof r->detail, "已取消");
        else if (ink_json_eq(j, ink_json_find(j, 0, "state"), "failed")) snprintf(r->detail, sizeof r->detail, "没做好");
        r->cancel = running && ink_json_eq(j, ink_json_find(j, 0, "can_cancel"), "true");
        p = end + 1;
        if (*p == ',') p++;
    }
    if (!A.jobs_ok) A.count = 0;
    else if (!A.count) A.rows[A.count++] = (job_row_t){.title = "还没有正在做的事", .id = "empty", .heading = true};
    else if (!ended) A.rows[A.count++] = (job_row_t){.title = "做好了", .detail = "还没有做好的事", .id = "finished", .heading = true};
    osal_free(j);
}
static void read_activity(void) {
    A.title[0] = A.detail[0] = 0;
    A.cap = A.action = NULL;
    if (!ink_member_space_open()) return;
    A.count = 0; A.jobs_ok = false;
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = jobs_reply};
    ink_cap_call("job.list", NULL, &opts);
    A.icon = "phone";
    if (ink_phone_activity(A.title, sizeof A.title)) {
        snprintf(A.detail, sizeof A.detail, "通话中");
        A.action = "回到通话"; A.cap = "phone.return";
        return;
    }
    ink_media_status_t st;
    ink_media_status(&st);
    if (st.state != INK_MEDIA_IDLE) {
        const char *base = strrchr(st.path, '/');
        snprintf(A.title, sizeof A.title, "%s", base ? base + 1 : st.path);
        char *ext = strrchr(A.title, '.');
        if (ext && ext != A.title) *ext = 0;
        snprintf(A.detail, sizeof A.detail, "%s", st.state == INK_MEDIA_PLAYING ? "正在听" : "已暂停");
        A.action = st.state == INK_MEDIA_PLAYING ? "暂停" : "接着放";
        A.cap = st.state == INK_MEDIA_PLAYING ? "media.pause" : "media.resume";
        A.icon = "headphones";
    } else if (A.count && A.rows[0].running) {
        job_row_t row = A.rows[0];
        snprintf(A.title, sizeof A.title, "%s", row.title);
        snprintf(A.detail, sizeof A.detail, "%s", row.detail);
        A.action = "查看"; A.cap = "shell.tasks"; A.icon = "download";
    }
}
bool ink_activity_listening(void) {
    ink_media_status_t st;
    ink_media_status(&st);
    char playing[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), playing, sizeof playing);
    return st.state == INK_MEDIA_PLAYING || !strcmp(playing, "voice");
}
int ink_activity_card_draw(lv_obj_t *parent, int y) {
    read_activity();
    if (!A.cap) return 0;
    int h = ink_ui_row_double(), w = INK_CANVAS_W - 2 * INK_MARGIN;
    lv_obj_t *card = ink_ui_box(parent, INK_MARGIN, y, w, h, -1, INK_SURFACE_WHITE, true);
    ink_ui_circle(card, A.icon, INK_GAP, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    int text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    int text_x = INK_CIRCLE + 2 * INK_GAP, button_w = !strcmp(A.cap, "phone.return") ? INK_CIRCLE_LARGE : INK_CIRCLE;
    ink_ui_line(card, A.title, INK_FONT_LIST, INK_WEIGHT_BOLD, text_x, text_y, w - text_x - button_w - INK_GAP);
    ink_ui_line(card, A.detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, text_x, text_y + ink_ui_line_height(INK_FONT_LIST), w - text_x - button_w - INK_GAP);
    lv_obj_t *button = !strcmp(A.icon, "headphones") ?
        ink_ui_circle(card, !strcmp(A.cap, "media.pause") ? "pause" : "play", w - button_w - INK_GAP, (h - INK_CONTROL) / 2, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL) :
        ink_ui_pill(card, A.action, w - button_w - INK_GAP, (h - INK_CONTROL) / 2, button_w, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(button, A.cap, "{}");
    return h + INK_GAP_M;
}
int ink_activity_card_snapshot(char *out, size_t cap) {
    return A.cap ? snprintf(out, cap, "正在进行=%s；%s；动作=%s（%s）。", A.title, A.detail, A.action, A.cap) : snprintf(out, cap, "没有正在进行的事。");
}
static void bind_job(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *f, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    job_row_t *r = &A.rows[index];
    lv_obj_clean(slot);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, h = ink_ui_row_double();
    lv_obj_set_style_bg_opa(slot, r->heading ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(slot, r->heading ? 0 : ink_ui_px(INK_STROKE), 0);
    if (r->heading) {
        lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(slot, 0, 0);
    } else {
        ink_ui_circle(slot, "download", INK_GAP, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    }
    int x = r->heading ? 0 : INK_CIRCLE + 2 * INK_GAP;
    int y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    ink_ui_line(slot, r->title, INK_FONT_LIST, INK_WEIGHT_BOLD, x, y, w - x - (r->cancel ? INK_CIRCLE + INK_GAP_L : INK_GAP));
    ink_ui_line(slot, r->detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, y + ink_ui_line_height(INK_FONT_LIST), w - x - (r->cancel ? INK_CIRCLE + INK_GAP_L : INK_GAP));
    if (r->cancel) {
        lv_obj_t *button = ink_ui_pill(slot, "取消", w - INK_CIRCLE - INK_GAP, (h - INK_CONTROL) / 2, INK_CIRCLE, INK_CONTROL, false, NULL, NULL);
        ink_shell_bind(button, "job.cancel", r->args);
    }
    *f = (ink_ui_field_spec_t){r->id, r->heading ? "heading" : "listitem", r->title, r->detail, {r->id, 1}, false};
    *action = (ink_ui_action_spec_t){r->id, "取消", "job.cancel", r->args, {r->id, 1}, r->cancel};
}
static void tasks_nav(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.tasks")) { ink_shell_close(); ink_ui_push(&TASKS, NULL); }
    else if (A.list) ink_ui_list_turn(A.list, !strcmp(c->cap->id, "tasks.next") ? 1 : -1);
}
static const ink_ui_action_spec_t PAGE_ACTIONS[] = {
    {.id = "prev", .label = "上一页", .capability = "tasks.prev"},
    {.id = "next", .label = "下一页", .capability = "tasks.next"}
};
static const ink_ui_page_spec_t MODEL = {.id = "Tasks", .title = "正在做的事", .object = {"job.list", 1}, .actions = PAGE_ACTIONS, .action_count = 2};
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(A.member, ink_member_current()->id)) memset(&A.position, 0, sizeof A.position);
    snprintf(A.member, sizeof A.member, "%s", ink_member_current()->id);
    read_activity();
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, "正在做的事");
    int y = INK_STATUS_BAR + INK_CONTROL + INK_GAP_L;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    A.list = ink_ui_list_create(body, &MODEL, &A.position, 5, 1, ink_ui_row_double(), A.jobs_ok ? "还没有正在做的事" : "没能读取正在做的事", bind_job, NULL);
    if (A.list) ink_ui_list_refresh(A.list, A.count);
}
static void destroy(void) { A.list = NULL; }
static int snapshot(char *out, size_t cap) { return A.list ? ink_ui_list_snapshot(A.list, out, cap) : -1; }
static bool page_key(ink_key_t key, bool lng) { return A.list && ink_ui_list_key(A.list, key, lng); }
static const ink_activity_t TASKS = {.name = "Tasks", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static void refresh(void *arg) {
    (void)arg;
    if (!ink_member_space_open() || ink_power_state() == INK_PWR_LOCKED) return;
    char title[sizeof A.title], detail[sizeof A.detail];
    memcpy(title, A.title, sizeof title); memcpy(detail, A.detail, sizeof detail);
    const char *cap = A.cap;
    read_activity();
    if (A.list && !strcmp(A.member, ink_member_current()->id)) {
        ink_ui_list_refresh(A.list, A.count);
    }
    if (cap != A.cap || strcmp(title, A.title) || strcmp(detail, A.detail)) ink_shell_activity_refresh();
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    ink_exec_post(INK_EXEC_UI, &A.refresh, refresh, NULL);
}
static void event(void *ctx, const char *id, const ink_args_t *args) {
    (void)args; changed(ctx, 0, id);
}
static const ink_cap_t CAPS[] = {
    {.id = "shell.tasks", .title = "查看正在做的事和做好了的事", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = tasks_nav},
    {.id = "tasks.prev", .title = "正在做的事上一页", .activity = "Tasks", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = tasks_nav},
    {.id = "tasks.next", .title = "正在做的事下一页", .activity = "Tasks", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = tasks_nav}
};
int ink_activity_card_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_state_watch("media.state", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("audio.now_playing", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("job.revision", changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("call.", event, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("job.finished", event, NULL, INK_EXEC_UI) ? -1 : 0;
}

int ink_activity_home_offset(void) {
    read_activity();
    return !A.cap ? 0 : !strcmp(A.cap, "phone.return") ? INK_GAP_L : -INK_GAP_L;
}
bool ink_activity_home_draw(lv_obj_t *parent, int y) {
    if (!A.cap || strcmp(A.cap, "phone.return")) return ink_activity_card_draw(parent, y) != 0;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    lv_obj_t *banner = ink_ui_box(parent, INK_MARGIN, 0, w, INK_CONTROL, -1, INK_SURFACE_BLACK, false);
    ink_ui_icon(banner, "phone", INK_ICON_SMALL, INK_ICON_SOLID, INK_GAP_L, (INK_CONTROL - INK_ICON_SMALL) / 2, true);
    ink_ui_line(banner, A.title, INK_FONT_NOTE, INK_WEIGHT_BOLD, INK_GAP_L + INK_ICON_LIST,
        (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2, w - INK_ICON_LIST - INK_CIRCLE_LARGE - 2 * INK_GAP_L);
    lv_obj_t *button = ink_ui_pill(banner, "回到通话", w - INK_CIRCLE_LARGE - INK_GAP, 0, INK_CIRCLE_LARGE, INK_CONTROL, true, NULL, NULL);
    ink_ui_pill_text(button, INK_FONT_NOTE, INK_WEIGHT_BOLD);
    ink_shell_bind(button, "phone.return", "{}");
    return false;
}
