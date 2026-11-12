/* Test the actual transfer policy with a small fake file-service boundary. */
#include "../../../system/services/file/transfer.c"
#include <assert.h>
static uint64_t now;
static uint32_t epoch=1;
static bool plane;
static unsigned admissions, removals;
static char last_path[128], last_result[1152];
struct osal_mutex { int unused; };
static struct osal_mutex mutex;
static int guards;
osal_mutex_t *osal_mutex_create(void) { guards++;return &mutex; }
void osal_mutex_delete(osal_mutex_t *m) { (void)m;assert(guards>0);guards--; }
osal_err_t osal_mutex_lock(osal_mutex_t *m,int64_t t) { (void)m;(void)t; return OSAL_OK; }
void osal_mutex_unlock(osal_mutex_t *m) { (void)m; }
uint64_t osal_now_us(void) { return now; }
uint32_t ink_member_epoch(void) { return epoch; }
bool ink_member_space_open(void) { return true; }
int64_t ink_setting_get(const char *id) { (void)id;return plane; }
void hal_random_fill(void *p,size_t n) { static unsigned seed=0x42;memset(p,(int)seed++,n); }
int ink_cap_register(const ink_cap_t *c,int n) { (void)c;(void)n;return 0; }
void ink_cap_result(ink_call_t *c,const char *s) { (void)c;snprintf(last_result,sizeof last_result,"%s",s); }
const char *ink_tr(const char *s) { return s; }
void ink_svc_cancel(ink_svc_deadline_t *d) { (void)d; }
void ink_svc_at(ink_svc_deadline_t *d,uint64_t at,void (*fn)(void *),void *p) { (void)d;(void)at;(void)fn;(void)p; }
int ink_exec_post(ink_exec_t e,ink_exec_job_t *j,void (*fn)(void *),void *p) { (void)e;(void)j;(void)fn;(void)p;return 0; }
void ink_power_hold(ink_power_need_t n) { (void)n; }
void ink_power_release(ink_power_need_t n) { (void)n; }
int ink_fs_card_hold(void) { return 0; }
void ink_fs_card_release(void) {}
bool fs_context_for_call(file_context_t *a,ink_call_t *c) { (void)c;snprintf(a->member,sizeof a->member,"owner"); a->adult=true;return true; }
enum { TEST_NODES = 80 };
static struct node { char path[256]; char *bytes; size_t size; bool dir; } nodes[TEST_NODES];
struct ink_fs_file { struct node *node; size_t at; };
struct ink_fs_dir { char path[256]; unsigned at; };
static bool full;
static int path_check(const file_context_t *a, const char *p) {
    assert(!strcmp(a->member, "owner"));
    snprintf(last_path, sizeof last_path, "%.127s", p);
    const char *leaf = strrchr(p, '/');
    return !strncmp(p, "/me", 3) && !strstr(p, "..") && !strstr(p, "/.") && !strstr(p, "invalid:") && leaf && strlen(leaf + 1) < 64 ? 0 : INK_FS_ERR_PATH;
}
static struct node *find(const char *p) {
    for (unsigned i = 0; i < TEST_NODES; i++) if (!strcmp(nodes[i].path, p)) return &nodes[i];
    return NULL;
}
static struct node *make(const char *p, bool dir) {
    for (unsigned i = 0; i < TEST_NODES; i++) if (!nodes[i].path[0]) {
        snprintf(nodes[i].path, sizeof nodes[i].path, "%s", p); nodes[i].dir = dir; return &nodes[i];
    }
    return NULL;
}
int fs_open_for(const file_context_t *a, const char *p, int flags, ink_fs_file_t **f) {
    int r = path_check(a, p); if (r) return r;
    struct node *node = find(p);
    if (flags & INK_FS_WRITE) {
        admissions++;
        if (full) return INK_FS_ERR_NO_SPACE;
        if (node) return INK_FS_ERR_EXISTS;
        node = make(p, false);
    }
    if (!node || node->dir) return INK_FS_ERR_NOT_FOUND;
    *f = calloc(1, sizeof **f); assert(*f); (*f)->node = node; return 0;
}
int fs_stat_for(const file_context_t *a, const char *p, ink_fs_stat_t *s) {
    int r = path_check(a, p); if (r) return r;
    struct node *n = find(p); if (!n) return INK_FS_ERR_NOT_FOUND;
    memset(s, 0, sizeof *s); s->is_dir = n->dir; s->size = n->size; return 0;
}
int fs_dir_open_for(const file_context_t *a, const char *p, ink_fs_dir_t **d) {
    int r = path_check(a, p); if (r) return r;
    struct node *n = find(p); if (!n || !n->dir) return INK_FS_ERR_NOT_FOUND;
    *d = calloc(1, sizeof **d); assert(*d); snprintf((*d)->path, sizeof (*d)->path, "%s", p); return 0;
}
int ink_fs_dir_next(ink_fs_dir_t *d, ink_fs_stat_t *s) {
    if (!d) return INK_FS_ERR_PATH;
    size_t len = strlen(d->path);
    while (d->at < TEST_NODES) {
        struct node *n = &nodes[d->at++];
        if (strncmp(n->path, d->path, len) || n->path[len] != '/' || !n->path[len + 1] || strchr(n->path + len + 1, '/')) continue;
        memset(s, 0, sizeof *s); s->is_dir = n->dir; s->size = n->size;
        snprintf(s->name, sizeof s->name, "%.63s", n->path + len + 1); return 1;
    }
    return 0;
}
void ink_fs_dir_close(ink_fs_dir_t *d) { free(d); }
int fs_mkdir_for(const file_context_t *a, const char *p) {
    int r = path_check(a, p); if (r) return r;
    if (find(p)) return INK_FS_ERR_EXISTS;
    return make(p, true) ? 0 : INK_FS_ERR_NO_SPACE;
}
int fs_remove_for(const file_context_t *a, const char *p, uint32_t rev) {
    (void)rev; int r = path_check(a, p); if (r) return r;
    struct node *n = find(p); if (!n) return INK_FS_ERR_NOT_FOUND;
    removals++; free(n->bytes); memset(n, 0, sizeof *n); return 0;
}
int fs_move_for(const file_context_t *a, const char *p, const char *to, uint32_t rev, uint32_t *written) {
    (void)rev; (void)written; int r = path_check(a, p); if (!r) r = path_check(a, to); if (r) return r;
    if (find(to)) return INK_FS_ERR_EXISTS;
    struct node *n = find(p); if (!n) return INK_FS_ERR_NOT_FOUND;
    snprintf(n->path, sizeof n->path, "%s", to); return 0;
}
int ink_fs_free(uint64_t *f, uint64_t *t) { *f = full ? 0 : 1000000; *t = 1000000; return 0; }
int64_t ink_fs_read(ink_fs_file_t *f, void *p, size_t n) {
    if (n > f->node->size - f->at) n = f->node->size - f->at;
    if (n) memcpy(p, f->node->bytes + f->at, n);
    f->at += n; return n;
}
int64_t ink_fs_write(ink_fs_file_t *f, const void *p, size_t n) {
    admissions++; if (full) return INK_FS_ERR_NO_SPACE;
    f->node->bytes = realloc(f->node->bytes, f->at + n); assert(f->node->bytes);
    memcpy(f->node->bytes + f->at, p, n); f->at += n; f->node->size = f->at; return n;
}
int ink_fs_sync(ink_fs_file_t *f) { (void)f; return 0; }
int ink_fs_close(ink_fs_file_t *f) { free(f); return 0; }
int64_t ink_fs_tell(ink_fs_file_t *f) { return f->at; }
int test_exists(const char *p) { return find(p) != NULL; }
void test_make(const char *p) { make(p, false); }
int test_bytes(const char *p, const void *data, size_t len) { struct node *n = find(p); return n && n->size == len && (!len || !memcmp(n->bytes, data, len)); }
void test_clear_files(void) {
    for (unsigned i = 0; i < TEST_NODES; i++) { free(nodes[i].bytes); memset(&nodes[i], 0, sizeof nodes[i]); }
    make("/me", true);
}

int test_begin(void) { if(T.active) finish("传输已关闭");now=0;epoch=1;plane=false;T.active=false;full=false;test_clear_files();ink_transfer_caps_init();ink_call_t c={.src=INK_SRC_TOUCH};start(&c);return T.active; }
const hal_transfer_policy_t *test_policy(void) { return &POLICY; }
void test_expire(void) { now=120000001; }
void test_switch(void) { epoch++; }
void test_plane(bool value) { plane=value; }
void test_full(bool value) { full=value; }
unsigned test_admissions(void) { return admissions; }
unsigned test_removals(void) { return removals; }
const char *test_path(void) { return last_path; }
const char *test_status(bool ai) { ink_call_t c={.src=ai?INK_SRC_AI:INK_SRC_TOUCH};status(&c);return last_result; }
void test_code(char code[8]) { snprintf(code,8,"%06u",(unsigned)T.code); }

void hal_net_link_status(hal_link_status_t *s) { *s=(hal_link_status_t){.up=true,.type=HAL_LINK_WIFI}; }

void osal_cost_units(const char *name,uint64_t n) { (void)name;(void)n; }

void test_tick(void) { poll_upload(NULL); }
int test_airplane_start(void) { finish("传输已关闭");plane=true;ink_call_t c={.src=INK_SRC_TOUCH};start(&c);return T.active; }

int test_guards(void) { return guards; }
int test_attempt(void) { ink_call_t c={.src=INK_SRC_TOUCH};start(&c);return T.active; }
