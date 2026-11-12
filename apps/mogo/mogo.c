#include <page.h>
#include "ink/sys/i18n.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/file_picker.h"
#include <ink/ui_theme.h>
#include <book_reader_input.h>
#define INK_MOGO_PORT_IMPL
#include <port/runtime.h>

static struct {
    ink_member_identity_t owner;
    ink_exec_job_t work, done;
    ink_call_t *call;
    uint8_t *pixels;
    int w, h, direction, px;
    uint32_t file_id;
    bool active, busy, loaded, ok;
    char path[INK_FS_PATH_MAX], description[1024];
} M;
static const ink_activity_t INK_MOGO_ACTIVITY;
static void ink_mogo_cleanup(void) { ink_mogo_page_close(); ink_app_free(M.pixels); M.pixels = NULL; M.loaded = false; }
static void ink_mogo_finished(void *arg) {
    (void)arg;
    if (M.active && ink_graft_current()) {
        if (M.ok) {
            lv_area_t a = {0,0,M.w-1,M.h-1};
            M.ok = !ink_graft_present(M.pixels,M.w,&a,INK_GRAFT_PAGE);
        }
        ink_ui_content_changed();
    } else M.ok = false;
    M.busy = false;
    if (M.call) {
        M.call->rc.status = M.ok ? INK_RC_DONE : INK_RC_FAILED;
        ink_cap_done(M.call); M.call = NULL;
    }
}
static void ink_mogo_work(void *arg) {
    (void)arg;
    if (M.active) {
        if (M.direction) ink_mogo_page_turn(M.direction);
        else {
            ink_mogo_page_close();
            if (M.file_id) {
                ink_fs_entry_t e;
                if (!ink_fs_lookup(M.file_id,&e)) ink_mogo_snprintf(M.path,sizeof M.path,"%s",e.path);
                else M.path[0] = 0;
            }
            M.loaded = ink_mogo_page_open(M.path,&M.owner,M.w,M.h,M.px);
        }
        M.ok = M.loaded;
        if (M.ok) ink_mogo_page_draw(M.pixels);
        ink_mogo_page_snapshot(M.description,sizeof M.description);
    }
    if (!M.active) ink_mogo_cleanup();
    ink_exec_post(INK_EXEC_UI,&M.done,ink_mogo_finished,NULL);
}
static bool ink_mogo_start(int direction, ink_call_t *call) {
    if (M.busy || !M.active || !ink_graft_current() || (direction && !M.loaded)) return false;
    M.busy = true; M.direction = direction; M.call = call;
    if (call) ink_cap_defer(call);
    if (ink_exec_post(INK_EXEC_BG,&M.work,ink_mogo_work,NULL)) {
        M.busy = false; M.call = NULL;
        if (call) { call->rc.status = INK_RC_FAILED; ink_cap_done(call); }
        return false;
    }
    return true;
}
static bool ink_mogo_opened(void *ctx, const ink_member_identity_t *owner, int w, int h) {
    (void)ctx;
    if (M.busy) return false;
    M.owner = *owner; M.w = w; M.h = h; M.px = ink_ui_px(48); M.active = true;
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
    return ink_mogo_snprintf(out,cap,"%s",M.busy ? TR_NOOP("新阅读 正在排版") : M.description);
}
static bool ink_mogo_input(void *ctx, const ink_graft_input_t *e) {
    (void)ctx;
    int dir = 0;
    if (e->kind == INK_GRAFT_TAP)
        dir = book_reader_tap_direction((EpdRect){0,0,M.w,M.h},e->x,e->y,false,false);
    else if (e->kind == INK_GRAFT_KEY && !e->held)
        dir = e->value == INK_KEY_NEXT ? 1 : e->value == INK_KEY_PREV ? -1 : 0;
    if (!dir || M.busy) return false;
    ink_call_opts_t opts = {.src = e->source};
    return !ink_cap_call(dir > 0 ? "mogo.next" : "mogo.previous",NULL,&opts);
}
static const ink_graft_client_t INK_MOGO_CLIENT = {.app = "mogo",.open = ink_mogo_opened,.close = ink_mogo_closed,
    .input = ink_mogo_input,.snapshot = ink_mogo_snapshot};
static void ink_mogo_selected(void *ctx, uint32_t id) {
    (void)ctx;
    if (!id) return;
    M.file_id = id;
    ink_mogo_start(0,NULL);
}
static void ink_mogo_create(lv_obj_t *screen, void *arg) {
    if (ink_graft_attach(screen,&INK_MOGO_CLIENT)) return;
    M.file_id = 0;
    if (arg) {
        ink_mogo_snprintf(M.path,sizeof M.path,"%s",(const char *)arg);
        ink_mogo_start(0,NULL);
    } else ink_file_picker_open(INK_FS_ANY,ink_mogo_selected,NULL);
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
        ink_mogo_snprintf(M.path,sizeof M.path,"%s",path); M.file_id = 0;
        if (!ink_mogo_start(0,call)) call->rc.status = INK_RC_FAILED;
    } else if (!ink_mogo_start(!strcmp(call->cap->id,"mogo.next") ? 1 : -1,call)) call->rc.status = INK_RC_STALE;
}
static const ink_cap_t INK_MOGO_CAPS[] = {
    {.id="mogo.open",.app="mogo",.title="打开新阅读图书",.effect=INK_FX_NAVIGATE,.child_ok=true,
     .input="{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.next",.app="mogo",.title="新阅读下一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action},
    {.id="mogo.previous",.app="mogo",.title="新阅读上一页",.effect=INK_FX_NAVIGATE,.child_ok=true,.activity="MogoReader",.timeout_ms=120000,.run=ink_mogo_action}
};
static const ink_activity_t INK_MOGO_ACTIVITY = {.name="MogoReader",.app="mogo",.create=ink_mogo_create,
    .destroy=ink_graft_detach,.on_key=ink_graft_key,.on_swipe=ink_graft_swipe,.settle=ink_graft_settle,.snapshot=ink_graft_snapshot};
const ink_app_def_t ink_appdef_mogo = {.id="mogo",.title=TR_NOOP("新阅读"),.entry=&INK_MOGO_ACTIVITY,.system_app=true,.child_ok=true};
int ink_mogo_init(void) { return ink_cap_register(INK_MOGO_CAPS,sizeof INK_MOGO_CAPS/sizeof *INK_MOGO_CAPS); }
