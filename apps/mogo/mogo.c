#include <page.h>
#include "ink/sys/i18n.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/card_files.h"
#include <ink/ui_theme.h>
#include "ink/sys/settings.h"
#include <book_reader_input.h>
#define INK_MOGO_PORT_IMPL
#include <port/runtime.h>

static struct {
    ink_member_identity_t owner;
    ink_exec_job_t work, done;
    ink_call_t *call;
    uint8_t *pixels;
    lv_obj_t *screen, *notice;
    int w, h, direction, px;
    const char *ink_mogo_operation;
    bool active, busy, loaded, ok;
    char path[INK_FS_PATH_MAX], description[2048];
} M;
static const ink_activity_t INK_MOGO_ACTIVITY;
static void ink_mogo_reopen(void);
static void ink_mogo_notice_gone(lv_event_t *e) { (void)e; M.notice = NULL; }
static void ink_mogo_cleanup(void) { ink_mogo_page_close(); ink_app_free(M.pixels); M.pixels = NULL; M.loaded = false; memset(&M.owner,0,sizeof M.owner); M.path[0]=0; M.ink_mogo_operation=NULL; }
static void ink_mogo_finished(void *arg) {
    (void)arg;
    if (M.active && ink_graft_current()) {
        if (M.ok) {
            lv_area_t a = {0,0,M.w-1,M.h-1};
            M.ok = !ink_graft_present(M.pixels,M.w,&a,INK_GRAFT_PAGE);
            if (M.ok && M.notice) lv_obj_delete(M.notice); /* the page is drawn under it */
        } else if (ink_mogo_font_missing) { /* not a blank page */
            M.notice = ink_card_files_notice(M.screen,"mogo",TR("字体"),ink_mogo_reopen);
            lv_obj_add_event_cb(M.notice,ink_mogo_notice_gone,LV_EVENT_DELETE,NULL);
        }
        ink_ui_content_changed();
    } else M.ok = false;
    M.busy = false;
    if (M.call) {
        ink_call_t *call=M.call; M.call=NULL;
        call->rc.status = M.ok ? INK_RC_DONE : INK_RC_FAILED;
        if (!M.ok && ink_mogo_font_missing) ink_mogo_snprintf(call->rc.detail,sizeof call->rc.detail,"font not on the card");
        ink_cap_done(call);
    }
}
static void ink_mogo_work(void *arg) {
    (void)arg;
    if (M.active) {
        if (M.ink_mogo_operation) M.ok=ink_mogo_page_command(M.ink_mogo_operation,M.direction);
        else {
            ink_mogo_page_close();
            M.loaded = M.path[0] ? ink_mogo_page_open(M.path,&M.owner,M.w,M.h,M.px) : ink_mogo_page_shelf(&M.owner,M.w,M.h,M.px);
            M.ok=M.loaded;
        }
        if (M.ok) ink_mogo_page_draw(M.pixels);
        ink_mogo_page_snapshot(M.description,sizeof M.description);
    }
    if (!M.active) ink_mogo_cleanup();
    ink_exec_post(INK_EXEC_UI,&M.done,ink_mogo_finished,NULL);
}
static bool ink_mogo_start(const char *ink_mogo_operation,int direction, ink_call_t *call) {
    if (M.busy || !M.active || !ink_graft_current() || (ink_mogo_operation && !M.loaded)) return false;
    M.busy = true; M.ink_mogo_operation=ink_mogo_operation; M.direction = direction; M.call = call;
    if (call) ink_cap_defer(call);
    if (ink_exec_post(INK_EXEC_BG,&M.work,ink_mogo_work,NULL)) {
        M.busy = false; M.call = NULL;
        if (call) { call->rc.status = INK_RC_FAILED; ink_cap_done(call); }
        return false;
    }
    return true;
}
static void ink_mogo_reopen(void) { ink_mogo_start(NULL,0,NULL); } /* the files are in place: the same book, this screen */
static bool ink_mogo_opened(void *ctx, const ink_member_identity_t *owner, int w, int h) {
    (void)ctx;
    if (M.busy) return false;
    M.owner = *owner; M.w = w; M.h = h; int level = ink_member_current()->role == INK_ROLE_CHILD ? 2 : (int)ink_setting_get("display.font_size");
    static const int percent[] = {90,100,110,120};
    M.px = ink_ui_px(percent[level < 0 ? 0 : level > 3 ? 3 : level]); M.active = true;
    M.pixels = ink_app_alloc((size_t)w*h,true);
    return M.pixels != NULL;
}
static void ink_mogo_closed(void *ctx) {
    (void)ctx; M.active = false; M.description[0] = 0;
    if (!M.busy) {
        M.busy = true;
        ink_exec_post(INK_EXEC_BG,&M.work,ink_mogo_work,NULL);
    }
}
static int ink_mogo_snapshot(void *ctx, char *out, size_t cap) {
    (void)ctx;
    if (!M.busy && ink_mogo_font_missing) return ink_card_files_notice_snapshot("mogo",out,cap);
    return ink_mogo_snprintf(out,cap,"%s",M.busy ? TR_NOOP("新阅读 正在排版") : M.description);
}
static const char *ink_mogo_argument_key(const char *ink_mogo_operation) {
    if (!strcmp(ink_mogo_operation,"mogo.goto_chapter")) return "chapter";
    if (!strcmp(ink_mogo_operation,"mogo.goto_bookmark") || !strcmp(ink_mogo_operation,"mogo.shelf_open")) return "index";
    return !strcmp(ink_mogo_operation,"mogo.set_font_size")?"size":"value";
}
static bool ink_mogo_input(void *ctx, const ink_graft_input_t *e) {
    (void)ctx;
    int dir = 0;
    if (M.busy) return false;
    const char *ink_mogo_operation=NULL; int value=0;
    if (e->kind==INK_GRAFT_HOLD || (e->kind==INK_GRAFT_SWIPE && e->value==INK_SWIPE_UP)) ink_mogo_operation="mogo.tools";
    else if (e->kind==INK_GRAFT_TAP) ink_mogo_operation=ink_mogo_page_hit(e->x,e->y,&value);
    if (ink_mogo_operation) {
        if (!*ink_mogo_operation) return false;
        char args[48]; ink_mogo_snprintf(args,sizeof args,"{\"%s\":%d}",ink_mogo_argument_key(ink_mogo_operation),value);
        ink_call_opts_t opts={.src=e->source,.args_json=args};
        return !ink_cap_call(ink_mogo_operation,NULL,&opts);
    }
    if (e->kind == INK_GRAFT_TAP)
        dir = book_reader_tap_direction((EpdRect){0,0,M.w,M.h},e->x,e->y,false,false);
    else if (e->kind==INK_GRAFT_SHAKE) dir=e->value;
    else if (e->kind == INK_GRAFT_KEY && !e->held)
        dir = e->value == INK_KEY_NEXT ? 1 : e->value == INK_KEY_PREV ? -1 : 0;
    if (!dir || M.busy) return false;
    ink_call_opts_t opts = {.src = e->source};
    return !ink_cap_call(dir > 0 ? "mogo.next" : "mogo.previous",NULL,&opts);
}
static const ink_graft_client_t INK_MOGO_CLIENT = {.app = "mogo",.open = ink_mogo_opened,.close = ink_mogo_closed,
    .input = ink_mogo_input,.snapshot = ink_mogo_snapshot};
static void ink_mogo_create(lv_obj_t *screen, void *arg) {
    M.screen = screen;
    if (ink_graft_attach(screen,&INK_MOGO_CLIENT)) return;
    if (arg) {
        ink_mogo_snprintf(M.path,sizeof M.path,"%s",(const char *)arg);
        ink_mogo_start(NULL,0,NULL);
    } else { M.path[0]=0; ink_mogo_start(NULL,0,NULL); }
}
static void ink_mogo_action(ink_call_t *call) {
    if (!strcmp(call->cap->id,"mogo.open")) {
        char path[INK_FS_PATH_MAX];
        if (!ink_args_get_str(&call->args,"path",path,sizeof path) || strncmp(path,"/me/",4) || M.busy) {
            call->rc.status = INK_RC_INVALID; return;
        }
        if (!M.active || !ink_graft_current()) {
            if (ink_app_open("mogo",NULL,path)) call->rc.status = INK_RC_FAILED;
            else call->rc.status = INK_RC_QUEUED;
            return;
        }
        ink_mogo_snprintf(M.path,sizeof M.path,"%s",path);
        if (!ink_mogo_start(NULL,0,call)) call->rc.status = INK_RC_FAILED;
    } else {
        int64_t value=0;
        const char *ink_mogo_key=ink_mogo_argument_key(call->cap->id);
        if (!ink_args_get_int(&call->args,ink_mogo_key,&value)) (void)ink_args_get_int(&call->args,"value",&value);
        if (value<-100 || value>10000) { call->rc.status=INK_RC_INVALID; return; }
        if (!ink_mogo_start(call->cap->id,(int)value,call)) call->rc.status=INK_RC_STALE;
    }
}
static const ink_cap_t INK_MOGO_CAPS[] = {
    {.id="mogo.open",.app="mogo",.title="打开新阅读图书",.effect=INK_FX_NAVIGATE,.child_ok=true,
     .input="{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.next",.app="mogo",.title="新阅读下一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.previous",.app="mogo",.title="新阅读上一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.toc",.app="mogo",.title="打开新阅读目录",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.goto_chapter",.app="mogo",.title="新阅读跳章",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"chapter\":{\"type\":\"integer\"}},\"required\":[\"chapter\"]}",.run=ink_mogo_action},
    {.id="mogo.add_bookmark",.app="mogo",.title="新阅读加书签",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.bookmarks",.app="mogo",.title="打开新阅读书签",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.goto_bookmark",.app="mogo",.title="跳到新阅读书签",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\"}},\"required\":[\"index\"]}",.run=ink_mogo_action},
    {.id="mogo.settings",.app="mogo",.title="打开新阅读设置",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.set_font_size",.app="mogo",.title="新阅读字号",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"size\":{\"type\":\"integer\"}},\"required\":[\"size\"]}",.run=ink_mogo_action},
    {.id="mogo.set_line_spacing",.app="mogo",.title="新阅读行距",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_paragraph_spacing",.app="mogo",.title="新阅读段距",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_margin",.app="mogo",.title="新阅读边距",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_weight",.app="mogo",.title="新阅读字重",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_indent",.app="mogo",.title="新阅读缩进",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_header",.app="mogo",.title="新阅读页眉",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.set_footer",.app="mogo",.title="新阅读页脚",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action},
    {.id="mogo.shelf",.app="mogo",.title="打开新阅读书架",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.shelf_open",.app="mogo",.title="从新阅读书架打开",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\"}},\"required\":[\"index\"]}",.run=ink_mogo_action},
    {.id="mogo.reader",.app="mogo",.title="返回新阅读正文",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.tools",.app="mogo",.title="打开新阅读工具",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.toc_percent",.app="mogo",.title="新阅读目录位置",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.input="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},\"required\":[\"value\"]}",.run=ink_mogo_action}
};
static const ink_activity_t INK_MOGO_ACTIVITY = {.name="MogoReader",.app="mogo",.create=ink_mogo_create,
    .destroy=ink_graft_detach,.on_key=ink_graft_key,.on_swipe=ink_graft_swipe,.settle=ink_graft_settle,.snapshot=ink_graft_snapshot};
const ink_app_def_t ink_appdef_mogo = {.id="mogo",.title=TR_NOOP("新阅读"),.entry=&INK_MOGO_ACTIVITY,.system_app=true,.child_ok=true};
int ink_mogo_init(void) { return ink_cap_register(INK_MOGO_CAPS,sizeof INK_MOGO_CAPS/sizeof *INK_MOGO_CAPS); }
