/* Contacts after Fossify Contacts @5664636 (docs/tasks/contacts-3/对照表.md): a list with the starred first, a detail page of
 * actions per number (as Lineage's QuickContact), an editor with several typed numbers, picking a number, vCard import and
 * export, duplicates shown and merged. One reading of phone.contacts for the phone, SMS and do-not-disturb (LilyPhone's match). */
/* ui-scale-runtime: dimension x,y,w,h,top,bottom,room,row_h,block,half,pill_w,need,left,used,field_h,note_h,dup_h,bar Theme tokens and measured text lines. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "contacts_vcf.h"

/* schema 2 adds numbers [{n,t}], default and star; a record without numbers is schema 1: one mobile number (D-138) */
static const ink_db_def_t CONTACTS = {.name = "phone.contacts", .title = "联系人", .scope = INK_DB_MEMBER,
    .schema = 2, .json = true, .ai_read = true, .max_records = 200};
enum { LIMIT = 200, NUMS = INK_VC_NUMBERS, VISIBLE = 6, DUPS = 4, FIELDS = 16, ACTIONS = 26, FILE_MAX = 2 << 20 };
enum { LIST, DETAIL, EDIT, CHOOSE, TRANSFER };
enum { VIEW, PICK, PHONE, ADD }; /* what a tap on a list row does */
typedef ink_vcard_t person_t;
typedef struct { uint32_t id; person_t p; bool dup; } row_t;
static const char *const TYPE_IDS[] = {"mobile", "home", "work", "other"};
static const char *const TYPE_NAMES[] = {TR_NOOP("手机"), TR_NOOP("家里"), TR_NOOP("工作"), TR_NOOP("其他")};
extern bool phone_number_match(const char *a, const char *b);
extern void ink_phone_tabs(lv_obj_t *screen, int selected);
static const ink_activity_t PAGES[5];
static struct {
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    char member[16], query[97], add_number[INK_VC_NUMBER], said[160], result[256], err[NUMS + 2][64];
    int page, mode, panel, form_page, number_page, focus, step;
    uint32_t id, ver;
    bool draft_ready, saving, busy;
    person_t cur, draft, want; /* cur: as stored; draft: the editor's; want: what a save writes */
    lv_obj_t *notice, *search, *star_button, *back_button, *panel_root, *name_field, *note_field, *num_fields[NUMS], *type_pills[NUMS];
    row_t rows[LIMIT];
    uint8_t order[LIMIT];
    size_t total, shown;
    struct { uint32_t id; char name[INK_VC_NAME], number[INK_VC_NUMBER]; bool same_name; } dups[DUPS];
    int ndups;
    ink_ui_field_spec_t fields[FIELDS];
    ink_ui_action_spec_t actions[ACTIONS];
    ink_ui_page_spec_t model;
    char ids[VISIBLE][32], args[VISIBLE][64], lines[VISIBLE][96], marks[VISIBLE][48], values[VISIBLE][160];
    char labels[FIELDS][128], argv[ACTIONS][96], put_args[512], numbers_args[512], delete_args[384], snap[2048];
    ink_exec_job_t refresh, shown_job;
} P INK_PSRAM_BSS;

/* ---- records ---- */
static const char *default_of(const person_t *p) { return p->num[p->def >= 0 && p->def < p->count ? p->def : 0]; }
static int type_of(const char *id) { for (int t = 0; t < 4; t++) if (!strcmp(id, TYPE_IDS[t])) return t; return -1; }
static int after(const ink_json_t *j, int t) { int end = j->tok[t].end; while (++t < j->ntok && j->tok[t].start < end) {} return t; }
static bool parse_person(const char *raw, size_t len, person_t *p) {
    ink_json_t *j = ink_app_alloc(sizeof *j, true); memset(p, 0, sizeof *p); p->def = -1;
    bool ok = j && !ink_json_parse(j, raw, len) && ink_json_str(j, ink_json_find(j, 0, "name"), p->name, sizeof p->name) > 0;
    if (ok) {
        ink_json_str(j, ink_json_find(j, 0, "note"), p->note, sizeof p->note);
        int list = ink_json_find(j, 0, "numbers");
        if (list > 0 && j->tok[list].type == 2) { /* schema 2 */
            for (int k = 0, t = list + 1; k < j->tok[list].size && t < j->ntok; k++, t = after(j, t)) {
                char type[8] = "";
                if (p->count == NUMS || ink_json_str(j, ink_json_find(j, t, "n"), p->num[p->count], INK_VC_NUMBER) <= 0) continue;
                ink_json_str(j, ink_json_find(j, t, "t"), type, sizeof type);
                p->type[p->count++] = (uint8_t)(type_of(type) < 0 ? INK_VC_OTHER : type_of(type));
            }
            int64_t d = -1;
            if (ink_json_int(j, ink_json_find(j, 0, "default"), &d) && d >= 0 && d < p->count) p->def = (int8_t)d;
            p->star = ink_json_eq(j, ink_json_find(j, 0, "star"), "true");
        } else if (ink_json_str(j, ink_json_find(j, 0, "number"), p->num[0], INK_VC_NUMBER) > 0) p->count = 1; /* schema 1 */
        ok = p->count > 0;
    }
    ink_app_free(j); return ok;
}
static uint32_t read_person(ink_db_t *db, uint32_t id, person_t *p) {
    char raw[1024]; ink_rec_t rec = {0};
    if (!db || !id || ink_db_get(db, id, &rec, raw, sizeof raw - 1) != INK_DB_OK) return 0;
    raw[rec.len < sizeof raw - 1 ? rec.len : sizeof raw - 1] = 0;
    return parse_person(raw, strlen(raw), p) ? rec.ver : 0;
}
static bool next_person(ink_db_t *db, ink_db_cursor_t *cur, ink_rec_t *rec, person_t *p) { /* the next readable record */
    char raw[1024];
    while (ink_db_next(db, cur, rec, raw, sizeof raw - 1) == 1) {
        raw[rec->len < sizeof raw - 1 ? rec->len : sizeof raw - 1] = 0;
        if (parse_person(raw, strlen(raw), p)) return true;
    }
    return false;
}
static int write_person(ink_db_t *db, uint32_t id, const person_t *p, uint8_t src, uint32_t *ver, uint32_t *out) {
    char raw[1024]; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw); /* the top-level number stays the default one: old readers */
    ink_jw_raw(&w, "{\"name\":"); ink_jw_str(&w, p->name); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, default_of(p));
    ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, p->note); ink_jw_raw(&w, ",\"numbers\":[");
    for (int i = 0; i < p->count; i++) {
        ink_jw_raw(&w, i ? ",{\"n\":" : "{\"n\":"); ink_jw_str(&w, p->num[i]); ink_jw_raw(&w, ",\"t\":"); ink_jw_str(&w, TYPE_IDS[p->type[i] & 3]); ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    if (p->def >= 0) { ink_jw_raw(&w, ",\"default\":"); ink_jw_int(&w, p->def); }
    if (p->star) ink_jw_raw(&w, ",\"star\":true");
    ink_jw_raw(&w, "}");
    ink_db_write_t wr = {.id = id, .data = raw, .len = w.len, .src = src, .written_ver = ver};
    return w.overflow ? -1 : ink_db_put(db, &wr, out);
}
static void drop_number(person_t *p, int k, int into) { /* k goes; the default follows it into `into` */
    memmove(p->num[k], p->num[k + 1], (size_t)(p->count - k - 1) * sizeof p->num[0]);
    memmove(p->type + k, p->type + k + 1, (size_t)(p->count - k - 1));
    p->count--;
    if (p->def == k) p->def = (int8_t)into;
    if (p->def > k) p->def--;
}
static void dedupe(person_t *p) {
    for (int i = 0; i < p->count; i++)
        for (int k = p->count - 1; k > i; k--) if (phone_number_match(p->num[i], p->num[k])) drop_number(p, k, i);
}
static void apply_put(person_t *p, const char *number) { /* contacts.put: the default (or first) number replaced, the rest kept */
    int d = p->def >= 0 ? p->def : 0;
    if (!p->count) { p->count = 1; p->type[0] = INK_VC_MOBILE; p->def = -1; d = 0; }
    snprintf(p->num[d], sizeof p->num[d], "%s", number); dedupe(p);
}
static bool same_numbers(const person_t *a, const person_t *b) {
    if (a->count != b->count || (a->count > 1 && a->def != b->def)) return false;
    for (int i = 0; i < a->count; i++) if (strcmp(a->num[i], b->num[i]) || a->type[i] != b->type[i]) return false;
    return true;
}
static void key_of(const char *name, char *out, size_t cap) { /* trimmed, English in lower case: what "同名" compares */
    while (*name == ' ') name++;
    size_t n = 0; for (; name[n] && n + 1 < cap; n++) out[n] = name[n] >= 'A' && name[n] <= 'Z' ? (char)(name[n] + 32) : name[n];
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
}
static int by_name(const void *a, const void *b) {
    const row_t *x = a, *y = b; char kx[INK_VC_NAME], ky[INK_VC_NAME]; key_of(x->p.name, kx, sizeof kx); key_of(y->p.name, ky, sizeof ky);
    int c = strcmp(kx, ky); return c ? c : strcmp(x->p.name, y->p.name) ? strcmp(x->p.name, y->p.name) : (x->id > y->id) - (x->id < y->id);
}
static bool same_name(const char *a, const char *b) { char ka[INK_VC_NAME], kb[INK_VC_NAME]; key_of(a, ka, sizeof ka); key_of(b, kb, sizeof kb); return ka[0] && !strcmp(ka, kb); }
static size_t load_rows(ink_db_t *db, row_t *rows) { /* every readable person, by name, the same names marked */
    ink_db_cursor_t cur = {0}; ink_rec_t r; size_t n = 0;
    while (n < LIMIT && next_person(db, &cur, &r, &rows[n].p)) { rows[n].id = r.id; rows[n].dup = false; n++; }
    qsort(rows, n, sizeof *rows, by_name);
    for (size_t i = 1; i < n; i++) if (same_name(rows[i - 1].p.name, rows[i].p.name)) rows[i - 1].dup = rows[i].dup = true;
    return n;
}

/* ---- what the phone, SMS and do-not-disturb read: cached member data only, safe in the UI task ---- */
int ink_contacts_match(const char *query, bool exact, char *name, char *number) {
    ink_db_t *db = ink_db_open_cached_member(&CONTACTS, ink_member_current()->id);
    if (!db) return -1;
    ink_db_cursor_t cur = {0}; ink_rec_t r; person_t *p = ink_app_alloc(sizeof *p, true); int found = 0;
    while (p && query[0] && !found && next_person(db, &cur, &r, p)) {
        int hit = !exact && strstr(p->name, query) ? (p->def >= 0 ? p->def : 0) : -1;
        for (int i = 0; i < p->count && hit < 0; i++) if (exact ? phone_number_match(p->num[i], query) : strstr(p->num[i], query) != NULL) hit = i;
        if (hit < 0) continue;
        found = (int)r.id; snprintf(name, INK_VC_NAME, "%s", p->name); snprintf(number, INK_VC_NUMBER, "%s", p->num[hit]);
    }
    ink_app_free(p); ink_db_close(db); return found;
}
/* Every number, by name; a person's numbers together, the default first. -1: not read yet; else how many. */
int ink_contacts_each(const char *member, void (*fn)(void *ctx, uint32_t id, const char *name, const char *type, const char *number, bool star), void *ctx) {
    ink_db_t *db = ink_db_open_cached_member(&CONTACTS, member);
    if (!db) return -1;
    row_t *rows = ink_app_alloc(LIMIT * sizeof *rows, true); size_t n = rows ? load_rows(db, rows) : 0; int sent = 0;
    for (size_t i = 0; i < n; i++) {
        const person_t *p = &rows[i].p; int d = p->def >= 0 ? p->def : 0;
        for (int k = -1; k < p->count; k++) if (k != d) { int at = k < 0 ? d : k; fn(ctx, rows[i].id, p->name, TYPE_IDS[p->type[at] & 3], p->num[at], p->star); sent++; }
    }
    ink_app_free(rows); ink_db_close(db); return sent;
}
bool ink_contacts_starred(const char *number) {
    ink_db_t *db = ink_db_open_cached_member(&CONTACTS, ink_member_current()->id);
    ink_db_cursor_t cur = {0}; ink_rec_t r; person_t *p = db ? ink_app_alloc(sizeof *p, true) : NULL; bool star = false;
    while (p && number && !star && next_person(db, &cur, &r, p))
        for (int i = 0; i < p->count && p->star && !star; i++) star = phone_number_match(p->num[i], number);
    ink_app_free(p); ink_db_close(db); return star;
}

/* ---- writes (BG: the first load and durable writes may wait) ---- */
static void list_numbers(char *out, size_t cap, const person_t *p) { /* "手机 138…、工作 010…" */
    size_t n = 0; out[0] = 0;
    for (int i = 0; i < p->count && n < cap; i++) n += (size_t)snprintf(out + n, cap - n, "%s%s %s", i ? "、" : "", TR(TYPE_NAMES[p->type[i] & 3]), p->num[i]);
}
static int number_at(const person_t *p, const char *number) { for (int i = 0; i < p->count; i++) if (phone_number_match(p->num[i], number)) return i; return -1; }
static ink_rc_t put_numbers(ink_call_t *c, person_t *p) { /* contacts.numbers: the whole list, from JSON [{n,t}] */
    char text[INK_ARGS_TEXT]; int64_t d = -1; person_t list = *p; list.count = 0; list.def = -1;
    ink_json_t *j = ink_app_alloc(sizeof *j, true); ink_rc_t rc = INK_RC_INVALID;
    int len = ink_args_get_str(&c->args, "numbers", text, sizeof text), arr = 0;
    if (j && len > 0 && !ink_json_parse(j, text, (size_t)len) && j->tok[0].type == 2) {
        rc = INK_RC_DONE;
        for (int k = 0, t = 1; k < j->tok[arr].size && rc == INK_RC_DONE; k++, t = after(j, t)) {
            char type[8] = "", number[INK_VC_NUMBER + 1] = "";
            ink_json_str(j, ink_json_find(j, t, "n"), number, sizeof number); ink_json_str(j, ink_json_find(j, t, "t"), type, sizeof type);
            if (list.count == NUMS || !ink_vcard_number_ok(number) || type_of(type) < 0 || number_at(&list, number) >= 0) rc = INK_RC_INVALID;
            else { snprintf(list.num[list.count], INK_VC_NUMBER, "%s", number); list.type[list.count++] = (uint8_t)type_of(type); }
        }
    }
    ink_app_free(j);
    if (ink_args_get_int(&c->args, "default", &d) && (d < 0 || d >= list.count)) rc = INK_RC_INVALID;
    if (rc == INK_RC_DONE && list.count) { list.def = (int8_t)d; *p = list; }
    else snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("号码要 1 到 5 个，各不相同，只能有数字、空格、-()，+ 只能在开头"));
    return rc == INK_RC_DONE && list.count ? INK_RC_DONE : INK_RC_INVALID;
}
static void refresh(void *arg);
static void record(ink_call_t *c) {
    const char *op = c->cap->id + sizeof "contacts." - 1;
    int64_t id = 0, expected = 0, from = 0; bool star = false;
    ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "if_revision", &expected); ink_args_get_int(&c->args, "from", &from);
    ink_db_t *db = ink_db_open_member(&CONTACTS, ink_cap_call_member(c));
    person_t *p = ink_app_alloc(sizeof *p, true), *q = ink_app_alloc(sizeof *q, true);
    char name[INK_VC_NAME] = "", number[INK_VC_NUMBER + 1] = "", raw[256];
    bool put = !strcmp(op, "put"), del = !strcmp(op, "delete"), select = !strcmp(op, "select"), write = !select && !del;
    uint32_t ver = db && p && id ? read_person(db, (uint32_t)id, p) : 0, got = (uint32_t)id;
    c->rc.say[0] = 0;
    if (!db || !p || !q) c->rc.status = INK_RC_FAILED;
    else if ((!put && !id) || (id && !ver)) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("找不到这个联系人")); }
    else if (expected && expected != ver) c->rc.status = INK_RC_STALE;
    else if (put) {
        if (!id) { memset(p, 0, sizeof *p); p->def = -1; }
        if (ink_args_find(&c->args, "note")) ink_args_get_str(&c->args, "note", p->note, sizeof p->note);
        int n = ink_args_get_str(&c->args, "name", name, sizeof name), len = ink_args_get_str(&c->args, "number", number, sizeof number);
        if (n <= 0 || n >= (int)sizeof name || len <= 0 || !ink_vcard_number_ok(number)) c->rc.status = INK_RC_INVALID;
        else { snprintf(p->name, sizeof p->name, "%s", name); apply_put(p, number); }
    } else if (!strcmp(op, "numbers")) c->rc.status = put_numbers(c, p);
    else if (!strcmp(op, "star")) { ink_args_get_bool(&c->args, "star", &star); p->star = star; }
    else if (!strcmp(op, "merge")) { /* from's numbers, note and star go into id; then from goes */
        if (from == id || !read_person(db, (uint32_t)from, q)) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("找不到要合并的联系人")); }
        for (int i = 0; c->rc.status == INK_RC_DONE && i < q->count; i++) {
            if (number_at(p, q->num[i]) >= 0) continue;
            if (p->count == NUMS) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("合并后超过 5 个号码，请先删掉几个再合并")); break; }
            snprintf(p->num[p->count], INK_VC_NUMBER, "%s", q->num[i]); p->type[p->count] = q->type[i];
            if (p->def < 0 && q->def == i) p->def = (int8_t)p->count;
            p->count++;
        }
        if (q->note[0] && strcmp(q->note, p->note)) { size_t at = strlen(p->note); snprintf(p->note + at, sizeof p->note - at, "%s%.192s", at ? "；" : "", q->note); ink_vcard_cut(p->note, 64); }
        p->star |= q->star;
    } else if (del) {
        const ink_arg_t *n = ink_args_find(&c->args, "name"), *m = ink_args_find(&c->args, "number");
        if (!n || !m || strcmp(p->name, c->args.text + n->off) || strcmp(default_of(p), c->args.text + m->off)) c->rc.status = INK_RC_STALE;
        else if (ink_db_del_if_ver(db, (uint32_t)id, c->src, (uint32_t)expected) != INK_DB_OK) c->rc.status = INK_RC_STALE;
        c->rc.revision = ink_db_last_ver(db);
    } else if (select) { /* the number asked for, or the only or default one; else say which there are */
        bool asked = ink_args_get_str(&c->args, "number", number, sizeof number) > 0; int at = asked ? number_at(p, number) : p->count == 1 || p->def >= 0 ? (p->def >= 0 ? p->def : 0) : -1;
        char all[160]; list_numbers(all, sizeof all, p);
        if (at < 0) c->rc.status = INK_RC_INVALID;
        if (at < 0 && asked) snprintf(c->rc.say, sizeof c->rc.say, TR("这个号码不是%.40s的，有：%.100s"), p->name, all);
        else if (at < 0) snprintf(c->rc.say, sizeof c->rc.say, TR("有 %d 个号码：%.120s，请说用哪一个"), p->count, all);
        else snprintf(number, sizeof number, "%s", p->num[at]);
    }
    if (write && c->rc.status == INK_RC_DONE) {
        if (write_person(db, (uint32_t)id, p, c->src, &c->rc.revision, &got) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
        else if (!strcmp(op, "merge") && ink_db_del(db, (uint32_t)from, c->src) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
        if (!strcmp(op, "merge")) c->rc.revision = ink_db_last_ver(db);
        snprintf(number, sizeof number, "%s", default_of(p));
    }
    if (c->rc.status == INK_RC_DONE && !select) {
        if (ink_db_wait(db, c->rc.revision, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0) c->rc.status = INK_RC_FAILED;
        ink_exec_post(INK_EXEC_UI, &P.refresh, refresh, NULL);
    }
    if (c->rc.status == INK_RC_DONE) {
        snprintf(c->rc.object, sizeof c->rc.object, "contact:%u", (unsigned)got);
        ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, got); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, number); ink_jw_raw(&w, "}"); ink_cap_result(c, raw);
        if (select) { ink_args_t e; ink_args_init(&e); ink_args_int(&e, "id", got); ink_args_str(&e, "number", number); ink_cap_emit_for("contacts.selected", &e, ink_cap_call_member(c)); }
    }
    if (!c->rc.say[0]) snprintf(c->rc.say, sizeof c->rc.say, "%s", c->rc.status == INK_RC_STALE ? TR("联系人已被修改，请返回列表重新打开后重试") : c->rc.status != INK_RC_DONE ? TR("没有完成，请检查姓名和号码后重试") :
        put ? TR("已保存联系人") : del ? TR("已删除联系人") : select ? TR("已选择这个号码") : !strcmp(op, "star") ? (star ? TR("已收藏") : TR("已取消收藏")) : !strcmp(op, "merge") ? TR("已合并联系人") : TR("已保存号码"));
    ink_app_free(q); ink_app_free(p); ink_db_close(db);
}

/* ---- vCard import and export (BG; Fossify's SettingsActivity import/export, the result stays on the page) ---- */
static struct { ink_db_t *db; uint8_t src; uint32_t ver; int nknown, total, added, skipped, bad, empty, full, dropped; char known[LIMIT * NUMS][INK_VC_NUMBER], say[256]; } I INK_PSRAM_BSS;
static void transfer_show(void *arg);
static void say_progress(const char *text) { snprintf(I.say, sizeof I.say, "%s", text); ink_exec_post(INK_EXEC_UI, &P.shown_job, transfer_show, NULL); }
static void imported(void *ctx, const ink_vcard_t *card, ink_vcf_status_t status, int dropped) {
    (void)ctx; person_t p = *card; p.star = false;
    if (status != INK_VCF_OK) { if (status == INK_VCF_BAD) I.bad++; else I.empty++; return; }
    dedupe(&p); /* Fossify inserts everything; here a number already kept is not kept twice */
    for (int k = p.count - 1; k >= 0; k--) for (int i = 0; i < I.nknown; i++) if (phone_number_match(I.known[i], p.num[k])) { drop_number(&p, k, -1); break; }
    if (!p.count) { I.skipped++; return; }
    if (I.total >= LIMIT || write_person(I.db, 0, &p, I.src, &I.ver, NULL) != INK_DB_OK) { I.full++; return; }
    I.total++; I.added++; I.dropped += dropped;
    for (int k = 0; k < p.count && I.nknown < LIMIT * NUMS; k++) snprintf(I.known[I.nknown++], INK_VC_NUMBER, "%s", p.num[k]);
}
static void count_say(char *out, size_t cap) {
    const int n[] = {I.added, I.skipped, I.bad, I.empty, I.full, I.dropped};
    const char *words[] = {TR("导入 %d 位"), TR("跳过 %d 位（号码都已在通讯录里）"), TR("%d 条看不懂"), TR("%d 位没有号码，没导入"), TR("满了，%d 位没导入"), TR("多出的 %d 个号码没导入")};
    size_t len = 0; out[0] = 0;
    for (int k = 0; k < 6 && len + 8 < cap; k++) if (n[k]) {
        if (len) len += (size_t)snprintf(out + len, cap - len, "%s", "；");
        if (len < cap) len += (size_t)snprintf(out + len, cap - len, words[k], n[k]);
    }
    if (!out[0]) snprintf(out, cap, "%s", TR("文件里没有联系人"));
}
static const char *file_error(int e, bool writing) {
    return e == INK_FS_ERR_NO_CARD || e == INK_FS_ERR_USB ? TR("存储卡不在") : e == INK_FS_ERR_NO_SPACE ? TR("存储卡满了，没有导出") : writing ? TR("没有写进去，请重试") : TR("文件读不出来");
}
static bool caller_here(ink_call_t *c) { /* files are the current member's (ink_fs_open) */
    if (!strcmp(ink_cap_call_member(c), ink_member_current()->id)) return true;
    c->rc.status = INK_RC_STALE; return false;
}
static bool vcf_name(const char *path) { size_t n = strlen(path); return n > 4 && path[n - 4] == '.' && (path[n - 3] | 32) == 'v' && (path[n - 2] | 32) == 'c' && (path[n - 1] | 32) == 'f'; }
static void import_file(ink_call_t *c) {
    int64_t file = 0, got = 0; ink_fs_entry_t e; ink_fs_file_t *f = NULL; char *text = NULL, say[256], raw[160];
    if (!caller_here(c)) return;
    ink_args_get_int(&c->args, "file", &file); say_progress(TR("正在导入，请稍候"));
    int err = ink_fs_lookup((uint32_t)file, &e);
    const char *why = err ? file_error(err, false) : !vcf_name(e.path) ? TR("这不是联系人文件（.vcf）") : e.size > FILE_MAX ? TR("文件太大，读不出来") : NULL;
    if (!why && (!(text = ink_app_alloc((size_t)e.size + 1, true)) || (err = ink_fs_open(e.path, INK_FS_READ, &f)) != 0)) why = file_error(err, false);
    while (!why && got < e.size) { /* in pieces, into PSRAM */
        int64_t r = ink_fs_read(f, text + got, (size_t)(e.size - got < 16384 ? e.size - got : 16384));
        if (r <= 0) why = TR("文件读不出来"); else got += r;
    }
    if (f) ink_fs_close(f);
    memset(&I, 0, offsetof(__typeof__(I), known)); I.src = c->src;
    if (!why && !(I.db = ink_db_open_member(&CONTACTS, ink_cap_call_member(c)))) why = TR("暂时打不开联系人，请返回重试");
    if (!why) { /* every number kept so far: a card whose numbers are all here is skipped */
        row_t *rows = ink_app_alloc(LIMIT * sizeof *rows, true); size_t have = rows ? load_rows(I.db, rows) : 0; I.total = (int)have;
        for (size_t i = 0; i < have; i++) for (int k = 0; k < rows[i].p.count; k++) snprintf(I.known[I.nknown++], INK_VC_NUMBER, "%s", rows[i].p.num[k]);
        ink_app_free(rows); text[got] = 0; ink_vcf_parse(text, (size_t)got, imported, NULL);
        if (I.added && ink_db_wait(I.db, I.ver, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0) why = TR("没有写进去，请重试");
        if (I.added) ink_exec_post(INK_EXEC_UI, &P.refresh, refresh, NULL);
    }
    if (why) snprintf(say, sizeof say, "%s", why); else count_say(say, sizeof say);
    c->rc.status = why ? INK_RC_FAILED : INK_RC_DONE; snprintf(c->rc.say, sizeof c->rc.say, "%.150s", say);
    snprintf(raw, sizeof raw, "{\"imported\":%d,\"skipped\":%d,\"unreadable\":%d,\"no_number\":%d,\"full\":%d,\"dropped\":%d}", I.added, I.skipped, I.bad, I.empty, I.full, I.dropped);
    if (!why) ink_cap_result(c, raw);
    say_progress(say); ink_app_free(text); ink_db_close(I.db); I.db = NULL;
}
static void export_file(ink_call_t *c) { /* /me/联系人-YYYYMMDD.vcf, then -2, -3: nothing to type */
    if (!caller_here(c)) return;
    ink_db_t *db = ink_db_open_member(&CONTACTS, ink_cap_call_member(c)); row_t *rows = ink_app_alloc(LIMIT * sizeof *rows, true);
    size_t n = db && rows ? load_rows(db, rows) : 0, len = 0, cap = n * 1024 + 1; char *out = n ? ink_app_alloc(cap, true) : NULL, path[INK_FS_PATH_MAX], say[200];
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); ink_fs_stat_t st; int err = 0;
    for (size_t i = 0; out && i < n; i++) { int w = ink_vcf_write(out + len, cap - len, &rows[i].p); if (w > 0) len += (size_t)w; }
    snprintf(path, sizeof path, "/me/联系人-%04d%02d%02d.vcf", t.year % 10000, t.month % 100, t.day % 100);
    for (int k = 2; out && k < 100 && (err = ink_fs_stat(path, &st)) == 0; k++) snprintf(path, sizeof path, "/me/联系人-%04d%02d%02d-%d.vcf", t.year % 10000, t.month % 100, t.day % 100, k);
    if (out && (err == 0 || err == INK_FS_ERR_NOT_FOUND)) { ink_fs_mkdir("/me"); err = ink_fs_write_atomic(path, out, len); }
    if (!n) snprintf(say, sizeof say, "%s", db ? TR("还没有联系人，没有可导出的") : TR("暂时打不开联系人，请返回重试"));
    else if (!out || err) snprintf(say, sizeof say, "%s", file_error(out ? err : INK_FS_ERR_IO, true));
    else snprintf(say, sizeof say, TR("已导出 %d 位到 我的文件 > %.60s"), (int)n, path + 4);
    c->rc.status = n && out && !err ? INK_RC_DONE : n ? INK_RC_FAILED : INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "%.150s", say);
    if (c->rc.status == INK_RC_DONE) {
        char raw[200]; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
        ink_jw_raw(&w, "{\"count\":"); ink_jw_int(&w, (int64_t)n); ink_jw_raw(&w, ",\"path\":"); ink_jw_str(&w, path); ink_jw_raw(&w, "}"); ink_cap_result(c, raw);
    }
    say_progress(say); ink_app_free(out); ink_app_free(rows); ink_db_close(db);
}

/* ---- pages ---- */
static void request(const char *cap, const ink_args_t *args, void (*done)(void *, const ink_receipt_t *)) {
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = P.member, .reply_exec = INK_EXEC_UI, .done = done, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &opts) != 0 && P.notice) lv_label_set_text(P.notice, TR("没有完成，请重试"));
}
static void id_args(char *out, size_t cap, uint32_t id, const char *number) { /* {"id":…,"number":…}, each when given */
    ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{");
    if (id) { ink_jw_raw(&w, "\"id\":"); ink_jw_int(&w, id); }
    if (number) { ink_jw_raw(&w, id ? ",\"number\":" : "\"number\":"); ink_jw_str(&w, number); }
    ink_jw_raw(&w, "}");
}
static void action(const char *id, const char *label, const char *cap, const char *args, bool enabled) {
    if (P.model.action_count < ACTIONS) P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = id, .label = label, .capability = cap, .args_json = args, .enabled = enabled};
}
static void field(const char *id, const char *role, const char *label, const char *value) {
    if (P.model.field_count < FIELDS) P.fields[P.model.field_count++] = (ink_ui_field_spec_t){.id = id, .role = role, .label = label, .value = value, .object = P.model.object};
}
static int fit(const char *text) { return LV_MAX(INK_CONTROL, ink_ui_text_width(text, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_M); }
static int top_button(lv_obj_t *screen, int x, const char *text, const char *also, lv_event_cb_t cb, const char *cap, const char *args, lv_obj_t **out) { /* right to left */
    int w = LV_MAX(fit(text), also ? fit(also) : 0); x -= w;
    lv_obj_t *b = ink_ui_text_button(screen, text, x, INK_STATUS_BAR + INK_GAP_S, w, cb, NULL);
    if (cap) ink_shell_bind(b, cap, args);
    if (out) *out = b;
    return x;
}
static void top_title(lv_obj_t *screen, const char *title, int right) { /* the top bar's title, clear of written buttons */
    int left = INK_MARGIN + INK_CONTROL + INK_GAP;
    ink_ui_line(screen, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, left, INK_STATUS_BAR + INK_GAP_S + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, right - left - INK_GAP_S);
}
static int ordered(const person_t *p, int *at) { /* the default first, then mobile, home, work, other (ViewContactActivity.kt:395-406) */
    int n = 0; if (p->def >= 0) at[n++] = p->def;
    for (int t = 0; t < 4; t++) for (int i = 0; i < p->count; i++) if (i != p->def && p->type[i] == t) at[n++] = i;
    return n;
}
static const char *number_line(char *out, size_t cap, const person_t *p, int i) { /* 手机 · 138 0000 0021 · 默认 */
    snprintf(out, cap, "%s · %s%s", TR(TYPE_NAMES[p->type[i] & 3]), p->num[i], p->count > 1 && i == p->def ? TR(" · 默认") : "");
    return out;
}

/* the list: Fossify's contacts and favourites tabs in one, the starred first; rows from a table read once per page */
static int hit_of(const row_t *r, const char *digits) { /* 2 from the start, 1 inside, 0 not (MyViewPagerFragment.kt:310-356) */
    if (!P.query[0]) return 2;
    int hit = !strncmp(r->p.name, P.query, strlen(P.query)) ? 2 : strstr(r->p.name, P.query) || strstr(r->p.note, P.query) ? 1 : 0;
    for (int i = 0; digits[0] && i < r->p.count && hit < 2; i++) {
        char d[INK_VC_NUMBER]; size_t n = 0;
        for (const char *s = r->p.num[i]; *s; s++) if (*s >= '0' && *s <= '9') d[n++] = *s;
        d[n] = 0;
        if (strstr(d, digits)) hit = !strncmp(d, digits, strlen(digits)) ? 2 : LV_MAX(hit, 1);
    }
    return hit;
}
static void filter(void) { /* hits from the start first, the starred first in each */
    char digits[32]; size_t n = 0;
    for (const char *s = P.query; *s && n + 1 < sizeof digits; s++) if (*s >= '0' && *s <= '9') digits[n++] = *s;
    digits[n] = 0; P.shown = 0;
    for (int pass = 0; pass < 4; pass++) for (size_t i = 0; i < P.total; i++) {
        int hit = hit_of(&P.rows[i], digits);
        if (hit && (hit == 2) == (pass < 2) && P.rows[i].p.star == !(pass & 1)) P.order[P.shown++] = (uint8_t)i;
    }
}
static void searched(lv_event_t *e) {
    snprintf(P.query, sizeof P.query, "%s", ink_textfield_text(lv_event_get_target_obj(e)));
    P.position.page = 0; filter(); if (P.list) ink_ui_list_refresh(P.list, P.shown); /* the list area only */
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *spec, ink_ui_action_spec_t *act, void *ctx) {
    (void)ctx; unsigned at = index % VISIBLE; const row_t *r = &P.rows[P.order[index]]; const person_t *p = &r->p;
    int x = INK_GAP + INK_CIRCLE + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN - x - INK_GAP, h = ink_ui_row_double();
    snprintf(P.marks[at], sizeof P.marks[at], "%s%s%s", p->star ? TR("收藏") : "", p->star && r->dup ? " · " : "", r->dup ? TR("同名") : "");
    if (p->count > 1) snprintf(P.lines[at], sizeof P.lines[at], TR("%s 等 %d 个号码"), default_of(p), p->count);
    else snprintf(P.lines[at], sizeof P.lines[at], "%s", default_of(p));
    snprintf(P.values[at], sizeof P.values[at], "%s%s%s", P.lines[at], P.marks[at][0] ? " · " : "", P.marks[at]);
    int mark_w = P.marks[at][0] ? ink_ui_text_width(P.marks[at], INK_FONT_NOTE, INK_WEIGHT_MEDIUM) + INK_GAP_S : 0;
    lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(slot, 0, 0); lv_obj_set_style_radius(slot, 0, 0);
    lv_obj_clean(slot);
    ink_ui_line(slot, p->name, INK_FONT_LIST, INK_WEIGHT_MEDIUM, x, INK_GAP_S, w - mark_w);
    if (mark_w) { lv_obj_t *m = ink_ui_line(slot, P.marks[at], INK_FONT_NOTE, INK_WEIGHT_MEDIUM, x + w - mark_w, INK_GAP_S, mark_w); lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_RIGHT, 0); }
    ink_ui_line(slot, P.lines[at], INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, INK_GAP_S + ink_ui_line_height(INK_FONT_LIST), w);
    lv_obj_t *avatar = ink_ui_box(slot, INK_GAP, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_CIRCLE, -1, INK_SURFACE_WHITE, false); /* the one circle the language allows */
    size_t first = 1; while (p->name[first] && (p->name[first] & 0xC0) == 0x80) first++;
    char initial[5]; snprintf(initial, sizeof initial, "%.*s", (int)first, p->name);
    lv_obj_t *l = ink_ui_line(avatar, initial, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, (INK_CIRCLE - ink_ui_line_height(INK_FONT_LIST)) / 2, INK_CIRCLE);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    snprintf(P.ids[at], sizeof P.ids[at], "contact:%u", (unsigned)r->id);
    bool direct = p->count == 1 || p->def >= 0, pick = P.mode == PICK || P.mode == PHONE;
    id_args(P.args[at], sizeof P.args[at], r->id, P.mode == ADD ? P.add_number : NULL);
    *spec = (ink_ui_field_spec_t){.id = P.ids[at], .role = "listitem", .label = p->name, .value = P.values[at], .object = {P.ids[at], ink_db_last_ver(P.db)}};
    *act = (ink_ui_action_spec_t){.id = P.ids[at], .label = P.mode == ADD ? TR("把号码加给这位") : pick ? TR("选这位联系人") : TR("查看联系人"),
        .capability = P.mode == ADD ? "contacts.edit" : !pick ? "contacts.view" : direct ? "contacts.select" : "contacts.choose", .args_json = P.args[at], .object = spec->object, .enabled = true};
}
static void list_page(lv_obj_t *screen) {
    const ink_ui_action_spec_t PAGING[] = {{.id = "prev", .label = TR("上一页"), .capability = "contacts.prev"}, {.id = "next", .label = TR("下一页"), .capability = "contacts.next"}};
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, bottom = INK_CANVAS_H - INK_MARGIN - (P.mode == ADD ? 0 : INK_PRIMARY + INK_GAP_M), row_h = ink_ui_row_double();
    P.total = load_rows(P.db, P.rows); filter();
    id_args(P.argv[0], sizeof P.argv[0], 0, P.add_number);
    if (P.mode == VIEW) top_button(screen, INK_CANVAS_W - INK_MARGIN, TR("导入导出"), NULL, NULL, "contacts.transfer", NULL, NULL);
    action("search", TR("搜索联系人"), "contacts.search", NULL, P.total > 0);
    action("add", P.mode == ADD ? TR("新建联系人") : TR("添加联系人"), "contacts.edit", P.mode == ADD ? P.argv[0] : NULL, true);
    if (P.mode == VIEW) action("transfer", TR("导入导出"), "contacts.transfer", NULL, true);
    if (P.mode == ADD) { /* InsertOrEditContactActivity.kt:329-358: the first row makes a new contact with the number */
        ink_shell_bind(ink_ui_pill(screen, TR("新建联系人"), INK_MARGIN, top, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL), "contacts.edit", P.argv[0]);
        top += INK_CONTROL + INK_GAP_M;
    }
    if (P.total) { /* nothing to search in an empty book: the list's shared empty state says so */
    P.fields[0] = (ink_ui_field_spec_t){.id = "query", .role = "searchbox", .label = TR("搜索联系人")};
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .initial = P.query, .hint = TR("搜索姓名、号码或备注"), .max = 96};
    P.search = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    ink_textfield_style(P.search, INK_FIELD_SEARCH, TR("搜索")); lv_obj_set_y(P.search, ink_ui_px(top));
    P.model.field_count = 1; ink_textfield_bind_short(P.search, &P.model, &P.fields[0], &P.actions[0]);
    lv_obj_add_event_cb(P.search, searched, LV_EVENT_VALUE_CHANGED, NULL);
    top += ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_S;
    }
    int capacity = LV_MAX(1, LV_MIN(VISIBLE, (bottom - top - INK_CONTROL - INK_GAP) / (row_h + INK_GAP_M)));
    ink_ui_page_spec_t model = {.id = PAGES[LIST].name, .title = TR("联系人"), .object = {"phone.contacts", ink_db_last_ver(P.db)}, .actions = PAGING, .action_count = 2};
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, bottom - top, 0, INK_SURFACE_WHITE, false);
    P.list = ink_ui_list_create(body, &model, &P.position, (unsigned)capacity, 1, row_h, P.total ? TR("没有匹配的联系人") : TR("还没有联系人"), bind_row, NULL);
    if (P.list) ink_ui_list_refresh(P.list, P.shown);
    if (P.mode != ADD) ink_shell_bind(ink_ui_pill(screen, TR("添加联系人"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL), "contacts.edit", "{}");
}

/* the detail page: ViewContactActivity, read only; each number a row with its own 打电话 and 发短信 (Lineage QuickContact :1730-1802) */
static int pages_now; /* the detail page's number pages, or the editor's form pages */
static const char *const NUMBER_IDS[] = {"number1", "number2", "number3", "number4", "number5"};
static const char *const CALL_IDS[] = {"call1", "call2", "call3", "call4", "call5"}, *const SMS_IDS[] = {"sms1", "sms2", "sms3", "sms4", "sms5"};
static const char *const TYPE_ACTS[] = {"type1", "type2", "type3", "type4", "type5"}, *const DEFAULT_ACTS[] = {"default1", "default2", "default3", "default4", "default5"};
static const char *const DUP_IDS[] = {"duplicate1", "duplicate2", "duplicate3", "duplicate4"}, *const ROW_IDS[] = {"mobile", "home", "work", "other", "default"};
static lv_obj_t *item_field(int item);
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current()) return;
    if (rc->status != INK_RC_DONE) { if (P.notice) lv_label_set_text(P.notice, rc->say[0] ? rc->say : TR("暂时打不开联系人，请返回重试")); ink_ui_content_changed(); return; }
    snprintf(P.said, sizeof P.said, "%s", rc->say);
    if (rc->revision && P.page == EDIT) { /* deleted: the detail page under the editor goes too */
        P.draft_ready = false; ink_ui_pop();
        if (P.page == DETAIL && !read_person(P.db, P.id, &P.cur)) ink_ui_pop();
    } else ink_ui_redraw();
}
static void quiet_reply(void *ctx, const ink_receipt_t *rc) { if (rc->status != INK_RC_DONE) reply(ctx, rc); } /* the refresh draws the change */
static void bar_clicked(lv_event_t *e) { request((intptr_t)lv_event_get_user_data(e) > 0 ? "contacts.next" : "contacts.prev", NULL, NULL); }
static void star_clicked(lv_event_t *e) { (void)e; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", P.id); ink_args_bool(&a, "star", !P.cur.star); request("contacts.star", &a, quiet_reply); }
static void merge_clicked(lv_event_t *e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e); ink_args_t a; ink_args_init(&a);
    ink_args_int(&a, "id", P.id); ink_args_int(&a, "from", P.dups[d].id); ink_args_int(&a, "if_revision", P.ver); request("contacts.merge", &a, reply);
}
static void back_clicked(lv_event_t *e) { (void)e; request("shell.back", NULL, NULL); }
static void find_dups(void) { /* G1 and G5: the same name or a shared number, looked for once, when the page opens */
    ink_db_cursor_t cur = {0}; ink_rec_t r; person_t *q = ink_app_alloc(sizeof *q, true); P.ndups = 0;
    while (q && P.ndups < DUPS && next_person(P.db, &cur, &r, q)) {
        int shared = -1; bool name = r.id != P.id && same_name(q->name, P.cur.name);
        for (int i = 0; i < q->count && shared < 0 && r.id != P.id; i++) if (number_at(&P.cur, q->num[i]) >= 0) shared = i;
        if (!name && shared < 0) continue;
        P.dups[P.ndups].id = r.id; P.dups[P.ndups].same_name = name;
        snprintf(P.dups[P.ndups].name, INK_VC_NAME, "%s", q->name); snprintf(P.dups[P.ndups++].number, INK_VC_NUMBER, "%s", shared >= 0 ? q->num[shared] : default_of(q));
    }
    ink_app_free(q);
}
static void detail_page(lv_obj_t *screen) {
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_M, bottom = INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_S, w = INK_CANVAS_W - 2 * INK_MARGIN, at[NUMS], n = ordered(&P.cur, at);
    id_args(P.argv[0], sizeof P.argv[0], P.id, NULL);
    snprintf(P.argv[1], sizeof P.argv[1], "{\"id\":%u,\"star\":%s}", (unsigned)P.id, P.cur.star ? "false" : "true");
    int x = top_button(screen, INK_CANVAS_W - INK_MARGIN, TR("编辑"), NULL, NULL, "contacts.edit", P.argv[0], NULL);
    x = top_button(screen, x, P.cur.star ? TR("已收藏") : TR("收藏"), P.cur.star ? TR("收藏") : TR("已收藏"), star_clicked, NULL, NULL, &P.star_button);
    top_title(screen, P.cur.name, x);
    find_dups();
    field("star", "status", TR("收藏"), P.cur.star ? TR("已收藏") : TR("没收藏"));
    action("star", P.cur.star ? TR("取消收藏") : TR("收藏"), "contacts.star", P.argv[1], true);
    action("edit", TR("编辑"), "contacts.edit", P.argv[0], true);
    int line = ink_ui_line_height(INK_FONT_LIST), note = ink_ui_line_height(INK_FONT_NOTE);
    int text_h = P.cur.note[0] ? LV_MIN(ink_ui_text_height(P.cur.note, INK_FONT_LIST, INK_WEIGHT_REGULAR, w), 3 * line) : 0;
    int note_h = text_h ? note + text_h + INK_GAP_M : 0, shown = LV_MIN(P.ndups, 2), dup_h = shown * (2 * note + INK_CONTROL + INK_GAP_M);
    int block = line + INK_CONTROL + INK_GAP_M, room = bottom - top - note_h - dup_h;
    int per = LV_MAX(1, room / block), pages = (n + per - 1) / per; /* ActivityViewContact scrolls: here the numbers turn by page */
    if (pages > 1) { per = LV_MAX(1, (room - INK_CONTROL - INK_GAP_S) / block); pages = (n + per - 1) / per; }
    P.number_page = LV_MIN(P.number_page, pages - 1); pages_now = pages;
    int y = top;
    for (int k = 0; k < n; k++) {
        int i = at[k]; number_line(P.labels[k], sizeof P.labels[k], &P.cur, i); id_args(P.argv[2 + k], sizeof P.argv[0], 0, P.cur.num[i]);
        field(NUMBER_IDS[k], "text", TR(TYPE_NAMES[P.cur.type[i] & 3]), P.labels[k]);
        action(CALL_IDS[k], TR("打电话"), "call.dial", P.argv[2 + k], true); action(SMS_IDS[k], TR("发短信"), "sms.compose", P.argv[2 + k], true);
        if (k / per != P.number_page) continue;
        ink_ui_line(screen, P.labels[k], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w);
        int bw = fit(TR("打电话"));
        ink_shell_bind(ink_ui_text_button(screen, TR("打电话"), INK_MARGIN, y + line, bw, NULL, NULL), "call.dial", P.argv[2 + k]);
        ink_shell_bind(ink_ui_text_button(screen, TR("发短信"), INK_MARGIN + bw + INK_GAP, y + line, fit(TR("发短信")), NULL, NULL), "sms.compose", P.argv[2 + k]);
        y += block;
    }
    if (text_h) { /* setupNotes: only when there is one */
        lv_obj_set_style_text_color(ink_ui_line(screen, TR("备注"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w), ink_ui_secondary(), 0);
        lv_obj_t *t = ink_ui_label(screen, P.cur.note, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y + note, w);
        lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_set_height(t, ink_ui_px(text_h));
        field("note", "text", TR("备注"), P.cur.note); y += note_h;
    }
    for (int d = 0; d < P.ndups; d++) { /* in place of merging on display: who it may be, and 合并到这位 */
        if (P.dups[d].same_name) snprintf(P.labels[8 + d], sizeof P.labels[0], TR("还有一位同名的联系人：%.40s · %.24s"), P.dups[d].name, P.dups[d].number);
        else snprintf(P.labels[8 + d], sizeof P.labels[0], TR("%.40s 也用这个号码：%.24s"), P.dups[d].name, P.dups[d].number);
        snprintf(P.argv[7 + d], sizeof P.argv[0], "{\"id\":%u,\"from\":%u,\"if_revision\":%u}", (unsigned)P.id, (unsigned)P.dups[d].id, (unsigned)P.ver);
        snprintf(P.ids[d], sizeof P.ids[d], "contact:%u", (unsigned)P.dups[d].id);
        field(DUP_IDS[d], "text", TR("可能重复"), P.labels[8 + d]); P.fields[P.model.field_count - 1].object = (ink_ui_object_ref_t){P.ids[d], 0};
        action(DUP_IDS[d], TR("合并到这位"), "contacts.merge", P.argv[7 + d], true);
        if (d >= shown) continue;
        lv_obj_t *l = ink_ui_label(screen, P.labels[8 + d], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_set_height(l, ink_ui_px(2 * note));
        ink_ui_pill(screen, TR("合并到这位"), INK_MARGIN, y + 2 * note, fit(TR("合并到这位")), INK_CONTROL, false, merge_clicked, (void *)(intptr_t)d);
        y += 2 * note + INK_CONTROL + INK_GAP_M;
    }
    if (pages > 1) {
        ink_ui_pagebar(screen, P.number_page + 1, pages, bottom - INK_CONTROL, bar_clicked);
        action("prev", TR("上一页"), "contacts.prev", NULL, P.number_page > 0); action("next", TR("下一页"), "contacts.next", NULL, P.number_page + 1 < pages);
    }
}
/* which number: Commons initiateCall's chooser as a page, each number a 108 px row */
static void choose_page(lv_obj_t *screen) {
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_M, w = INK_CANVAS_W - 2 * INK_MARGIN, at[NUMS], n = ordered(&P.cur, at);
    ink_ui_line(screen, P.cur.name, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_LIST) + INK_GAP_M;
    field("name", "text", TR("联系人"), P.cur.name);
    for (int k = 0; k < n; k++) {
        number_line(P.labels[k], sizeof P.labels[k], &P.cur, at[k]); id_args(P.argv[2 + k], sizeof P.argv[0], P.id, P.cur.num[at[k]]);
        ink_shell_bind(ink_ui_list_row(screen, "phone", P.labels[k], NULL, INK_MARGIN, y, w, false, NULL, NULL), "contacts.select", P.argv[2 + k]);
        field(NUMBER_IDS[k], "listitem", TR(TYPE_NAMES[P.cur.type[at[k]] & 3]), P.labels[k]); action(NUMBER_IDS[k], TR("用这个号码"), "contacts.select", P.argv[2 + k], true);
        y += INK_CONTROL + INK_GAP_S;
    }
}
/* import and export: Fossify's settings rows as a page of their own; the result stays until the next one */
static void transfer_show(void *arg) {
    (void)arg; snprintf(P.result, sizeof P.result, "%s", I.say);
    if (P.page != TRANSFER || !P.notice || !ink_ui_page_current()) return;
    bool busy = !strcmp(P.result, TR("正在导入，请稍候"));
    if (strcmp(lv_label_get_text(P.notice), P.result)) { lv_label_set_text(P.notice, P.result); ink_ui_content_changed(); }
    if (P.back_button) lv_obj_set_flag(P.back_button, LV_OBJ_FLAG_HIDDEN, busy || !P.result[0]);
}
static void picked(void *ctx, uint32_t id) { /* the picker closes and 正在导入 shows in the same frame */
    (void)ctx; if (!id) return;
    snprintf(I.say, sizeof I.say, "%s", TR("正在导入，请稍候")); transfer_show(NULL);
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "file", id); request("contacts.import", &a, NULL);
}
static void transfer_page(lv_obj_t *screen) {
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_M, w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_shell_bind(ink_ui_list_row(screen, "sd", TR("从存储卡导入（.vcf）"), TR("选一个 .vcf 文件"), INK_MARGIN, y, w, true, NULL, NULL), "contacts.import_pick", NULL);
    y += ink_ui_row_double() + INK_GAP_S;
    ink_shell_bind(ink_ui_list_row(screen, "folder", TR("导出到我的文件"), TR("收藏不写进去"), INK_MARGIN, y, w, true, NULL, NULL), "contacts.export", NULL);
    y += ink_ui_row_double() + INK_GAP_L;
    lv_obj_set_y(P.notice, ink_ui_px(y)); lv_label_set_text(P.notice, P.result);
    P.back_button = ink_ui_pill(screen, TR("回到联系人"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, true, back_clicked, NULL);
    lv_obj_set_flag(P.back_button, LV_OBJ_FLAG_HIDDEN, !P.result[0] || !strcmp(P.result, TR("正在导入，请稍候")));
    field("result", "status", TR("结果"), P.result);
    action("import", TR("从存储卡导入（.vcf）"), "contacts.import_pick", NULL, true); action("export", TR("导出到我的文件"), "contacts.export", NULL, true);
    action("back", TR("回到联系人"), "shell.back", NULL, P.result[0] != 0);
}

/* the editor: EditContactActivity with one name, typed numbers on the phone keyboard and a note; it pages when it does not fit */
static void harvest(void) { /* the fields into the draft: a redraw or another form page keeps every word */
    if (P.page != EDIT) return;
    if (P.name_field) snprintf(P.draft.name, sizeof P.draft.name, "%s", ink_textfield_text(P.name_field));
    if (P.note_field) snprintf(P.draft.note, sizeof P.draft.note, "%s", ink_textfield_text(P.note_field));
    for (int i = 0; i < P.draft.count; i++) if (P.num_fields[i]) snprintf(P.draft.num[i], INK_VC_NUMBER, "%s", ink_textfield_text(P.num_fields[i]));
}
static void want(void) { /* what a save writes: the empty numbers go (EditContactActivity.kt:1247-1271) */
    P.want = P.draft;
    for (int k = P.want.count - 1; k >= 0; k--) {
        char *s = P.want.num[k]; while (*s == ' ') s++;
        if (!*s) drop_number(&P.want, k, -1);
    }
}
static bool check(void) { /* each error on its own field; nothing typed is lost */
    const char *s = P.draft.name; bool any = false; while (*s == ' ') s++;
    memset(P.err, 0, sizeof P.err);
    if (!*s) snprintf(P.err[0], sizeof P.err[0], "%s", TR("请填写姓名"));
    for (int i = 0; i < P.draft.count; i++) {
        const char *n = P.draft.num[i]; while (*n == ' ') n++;
        if (!*n) continue;
        any = true;
        if (!ink_vcard_number_ok(P.draft.num[i])) snprintf(P.err[1 + i], sizeof P.err[0], "%s", TR("号码只能有数字、空格、-()，+ 只能在开头"));
        for (int k = 0; k < i && !P.err[1 + i][0]; k++) if (P.draft.num[k][0] && phone_number_match(P.draft.num[k], P.draft.num[i])) snprintf(P.err[1 + i], sizeof P.err[0], "%s", TR("和上面的号码重复了"));
    }
    if (!any) snprintf(P.err[1], sizeof P.err[0], "%s", TR("至少要有一个号码"));
    for (int k = 0; k < NUMS + 2; k++) if (P.err[k][0]) return false;
    return true;
}
static bool second_step(const person_t *stored, person_t *after) { /* what contacts.put leaves; contacts.numbers only when that is not what is wanted */
    *after = *stored; apply_put(after, default_of(&P.want)); return !same_numbers(after, &P.want);
}
static void numbers_json(char *out, size_t cap, uint32_t id, uint32_t revision) {
    ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{");
    if (id) { ink_jw_raw(&w, "\"id\":"); ink_jw_int(&w, id); ink_jw_raw(&w, ",\"if_revision\":"); ink_jw_int(&w, revision); ink_jw_raw(&w, ","); }
    ink_jw_raw(&w, "\"numbers\":[");
    for (int i = 0; i < P.want.count; i++) {
        ink_jw_raw(&w, i ? ",{\"n\":" : "{\"n\":"); ink_jw_str(&w, P.want.num[i]); ink_jw_raw(&w, ",\"t\":"); ink_jw_str(&w, TYPE_IDS[P.want.type[i] & 3]); ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    if (P.want.def >= 0) { ink_jw_raw(&w, ",\"default\":"); ink_jw_int(&w, P.want.def); }
    ink_jw_raw(&w, "}");
}
static void saved(void *ctx, const ink_receipt_t *rc) { /* two steps (the argument table holds 416 bytes), one refresh: leaving */
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || P.page != EDIT) { P.saving = false; return; }
    if (rc->status != INK_RC_DONE) {
        P.saving = false; lv_label_set_text(P.notice, P.step == 2 ? TR("姓名已存，号码没存上，请重试") : rc->say[0] ? rc->say : TR("没有完成，请检查姓名和号码后重试"));
        ink_ui_content_changed(); return;
    }
    if (P.step == 1) {
        unsigned id = 0; person_t after; sscanf(rc->object, "contact:%u", &id);
        bool more = second_step(&P.cur, &after);
        snprintf(after.name, sizeof after.name, "%s", P.want.name); snprintf(after.note, sizeof after.note, "%s", P.want.note);
        P.id = id; P.ver = rc->revision; P.cur = after;
        if (more) {
            char json[512]; ink_args_t a; ink_args_init(&a); numbers_json(json, sizeof json, 0, 0);
            const char *list = strchr(json, '['), *end = strrchr(json, ']');
            ink_args_int(&a, "id", P.id); ink_args_int(&a, "if_revision", P.ver); ink_args_json(&a, "numbers", list, (size_t)(end - list + 1));
            if (P.want.def >= 0) ink_args_int(&a, "default", P.want.def);
            P.step = 2; request("contacts.numbers", &a, saved); return;
        }
    }
    P.saving = false; P.draft_ready = false; ink_ui_pop();
}
static void save_now(void) {
    if (P.saving) return;
    harvest();
    if (!check()) { ink_ui_redraw(); return; }
    want(); ink_args_t a; ink_args_init(&a);
    if (P.id) { ink_args_int(&a, "id", P.id); ink_args_int(&a, "if_revision", P.ver); }
    ink_args_str(&a, "name", P.want.name); ink_args_str(&a, "number", default_of(&P.want)); ink_args_str(&a, "note", P.want.note);
    P.step = 1; P.saving = true; request("contacts.put", &a, saved);
}
static void save_clicked(lv_event_t *e) { (void)e; save_now(); }
static void delete_clicked(lv_event_t *e) {
    (void)e; ink_args_t a; ink_args_init(&a);
    ink_args_int(&a, "id", P.id); ink_args_int(&a, "if_revision", P.ver); ink_args_str(&a, "name", P.cur.name); ink_args_str(&a, "number", default_of(&P.cur));
    request("contacts.delete", &a, reply);
}
static void type_clicked(lv_event_t *e) { ink_args_t a; ink_args_init(&a); ink_args_int(&a, "index", (intptr_t)lv_event_get_user_data(e)); request("contacts.number_type", &a, NULL); }
static void panel_close(void) { if (P.panel_root) { lv_obj_delete(P.panel_root); ink_ui_content_changed(); } P.panel_root = NULL; P.panel = -1; }
static void panel_close_clicked(lv_event_t *e) { (void)e; panel_close(); }
static void leave_clicked(lv_event_t *e) { (void)e; P.draft_ready = false; ink_ui_pop(); }
static bool edited(void) { harvest(); return strcmp(P.draft.name, P.cur.name) || strcmp(P.draft.note, P.cur.note) || (want(), !same_numbers(&P.want, &P.cur)); }
static void panel_open(int i) { /* the shared bottom panel: a number's type, or (i < 0) leaving with changes (EditContactActivity.kt:385-404) */
    panel_close(); P.panel = i;
    bool leave = i < 0, can_default = !leave && P.draft.count > 1 && P.draft.def != i;
    int rows = leave ? 3 : 5 + can_default, line = ink_ui_line_height(INK_FONT_SUBTITLE), w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_MARGIN + line + INK_GAP_S;
    P.panel_root = ink_ui_panel(lv_screen_active(), y + rows * INK_CONTROL + INK_MARGIN, true);
    if (!leave) snprintf(P.labels[14], sizeof P.labels[14], TR("号码 %d 的类型"), i + 1);
    ink_ui_line(P.panel_root, leave ? TR("改动还没保存") : P.labels[14], INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN, w);
    if (leave) {
        ink_ui_text_button(P.panel_root, TR("保存"), INK_MARGIN, y, w, save_clicked, NULL);
        ink_ui_text_button(P.panel_root, TR("不保存"), INK_MARGIN, y + INK_CONTROL, w, leave_clicked, NULL);
        ink_ui_text_button(P.panel_root, TR("继续编辑"), INK_MARGIN, y + 2 * INK_CONTROL, w, panel_close_clicked, NULL);
        return;
    }
    for (int t = 0; t < 4 + can_default; t++, y += INK_CONTROL) {
        if (t < 4) snprintf(P.labels[15 + t], sizeof P.labels[0], t == P.draft.type[i] ? TR("%s · 现在是") : "%s", TR(TYPE_NAMES[t]));
        if (t < 4) snprintf(P.argv[20 + t], sizeof P.argv[0], "{\"index\":%d,\"type\":\"%s\"}", i, TYPE_IDS[t]);
        else snprintf(P.argv[20 + t], sizeof P.argv[0], "{\"index\":%d}", i);
        ink_shell_bind(ink_ui_text_button(P.panel_root, t < 4 ? P.labels[15 + t] : TR("设为默认号码"), INK_MARGIN, y, w, NULL, NULL), t < 4 ? "contacts.number_type" : "contacts.number_default", P.argv[20 + t]);
    }
    ink_ui_text_button(P.panel_root, TR("关闭"), INK_MARGIN, y, w, panel_close_clicked, NULL);
}
static int item_h(int item) { /* -1 the name, 0..4 a number, NUMS 加一个号码, NUMS + 1 the note */
    int k = item < 0 ? 0 : item < NUMS ? item + 1 : NUMS + 1;
    if (item == NUMS) return INK_CONTROL + INK_GAP_S;
    return ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_S + (P.err[k][0] ? ink_ui_line_height(INK_FONT_FIELD_LABEL) + INK_FIELD_LABEL_GAP : 0);
}
static int type_w(void) { int w = 0; for (int t = 0; t < 4; t++) w = LV_MAX(w, fit(TR(TYPE_NAMES[t]))); return w; }
static const char *number_label(int i) {
    snprintf(P.labels[2 + i], sizeof P.labels[0], "%s", P.draft.count > 1 && P.draft.def == i ? TR("号码 · 默认") : TR("号码"));
    return P.labels[2 + i];
}
static void draw_item(lv_obj_t *screen, int item, int y) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (item < 0 || item == NUMS + 1) { /* the name and the note: shared short fields */
        bool name = item < 0; int k = name ? 0 : NUMS + 1;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = name ? P.draft.name : P.draft.note, .hint = name ? TR("姓名") : TR("备注"), .max = name ? 96 : 192};
        lv_obj_t *f = ink_textfield_create(screen, &cfg, NULL, NULL, false);
        ink_textfield_style(f, INK_FIELD_SINGLE, cfg.hint); lv_obj_set_pos(f, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_width(f, ink_ui_px(w));
        ink_textfield_bind_short(f, &P.model, &P.fields[name ? 0 : 1], &P.actions[name ? 0 : 1]);
        if (P.err[k][0]) ink_textfield_error(f, P.err[k]);
        if (name) P.name_field = f; else P.note_field = f;
    } else if (item == NUMS) { /* 加一个号码, or the limit; how a number goes */
        int bw = P.draft.count < NUMS ? fit(TR("加一个号码")) : 0;
        if (bw) ink_shell_bind(ink_ui_text_button(screen, TR("加一个号码"), INK_MARGIN, y, bw, NULL, NULL), "contacts.number_add", NULL);
        lv_obj_t *l = ink_ui_label(screen, bw ? TR("清空就删掉这个号码") : TR("最多 5 个号码 · 清空就删掉这个号码"), INK_FONT_NOTE, INK_WEIGHT_REGULAR,
            INK_MARGIN + bw + (bw ? INK_GAP_S : 0), y + (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2, w - bw - (bw ? INK_GAP_S : 0));
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
    } else { /* a number: its type on the left, the phone keyboard's field on the right */
        int pill_w = type_w(), label_h = ink_ui_line_height(INK_FONT_FIELD_LABEL) + INK_FIELD_LABEL_GAP;
        P.type_pills[item] = ink_ui_pill(screen, TR(TYPE_NAMES[P.draft.type[item] & 3]), INK_MARGIN, y + label_h, pill_w, INK_CONTROL, false, type_clicked, (void *)(intptr_t)item);
        ink_text_cfg_t cfg = {.type = INK_TEXT_PHONE, .enter = INK_ENTER_DONE, .initial = P.draft.num[item], .hint = number_label(item), .max = INK_VC_NUMBER - 1};
        lv_obj_t *f = ink_textfield_create(screen, &cfg, NULL, NULL, false);
        ink_textfield_style(f, INK_FIELD_SINGLE, cfg.hint);
        lv_obj_set_pos(f, ink_ui_px(INK_MARGIN + pill_w + INK_GAP_S), ink_ui_px(y)); lv_obj_set_width(f, ink_ui_px(w - pill_w - INK_GAP_S));
        if (P.err[1 + item][0]) ink_textfield_error(f, P.err[1 + item]);
        P.num_fields[item] = f;
    }
}
static void edit_page(lv_obj_t *screen) {
    if (!P.draft_ready) { /* opened: the stored person, with a number brought along (EditContactActivity.kt:263-273) */
        P.draft = P.cur; memset(P.err, 0, sizeof P.err); P.form_page = 0; P.focus = 0;
        if (!P.draft.count) { P.draft.count = 1; P.draft.type[0] = INK_VC_MOBILE; snprintf(P.draft.num[0], INK_VC_NUMBER, "%s", P.add_number); }
        else if (P.add_number[0] && P.draft.count < NUMS && number_at(&P.draft, P.add_number) < 0) {
            snprintf(P.draft.num[P.draft.count], INK_VC_NUMBER, "%s", P.add_number); P.draft.type[P.draft.count] = INK_VC_MOBILE; P.focus = P.draft.count++ + 2;
        }
        P.add_number[0] = 0; P.draft_ready = true;
    }
    int w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP) / 2, by = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    bool stacked = P.id && LV_MAX(fit(TR("删除联系人")), fit(TR("保存联系人"))) > half;
    ink_ui_pill(screen, TR("保存联系人"), P.id && !stacked ? INK_MARGIN + half + INK_GAP : INK_MARGIN, by, P.id && !stacked ? half : w, INK_PRIMARY, true, save_clicked, NULL);
    if (P.id) ink_ui_pill(screen, TR("删除联系人"), INK_MARGIN, stacked ? by - INK_CONTROL - INK_GAP_M : by, stacked ? w : half, INK_CONTROL, false, delete_clicked, NULL);
    int notice_y = (stacked ? by - INK_CONTROL - INK_GAP_M : by) - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE);
    lv_obj_set_y(P.notice, ink_ui_px(notice_y));
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, bottom = notice_y - INK_GAP_S, room = bottom - top, seq[NUMS + 3], page_of[NUMS + 3], n = 0, total = 0, used = 0;
    seq[n++] = -1; for (int i = 0; i < P.draft.count; i++) seq[n++] = i;
    seq[n++] = NUMS; seq[n++] = NUMS + 1;
    for (int k = 0; k < n; k++) total += item_h(seq[k]);
    if (total > room) room -= INK_CONTROL + INK_GAP_S;
    pages_now = 1;
    for (int k = 0; k < n; k++) { int h = item_h(seq[k]); if (used && used + h > room) { pages_now++; used = 0; } page_of[k] = pages_now - 1; used += h; }
    for (int k = 0; k < n && P.focus; k++) if (seq[k] + 2 == P.focus) P.form_page = page_of[k]; /* focus: an item + 2, 0 for none */
    P.form_page = LV_MIN(P.form_page, pages_now - 1);
    for (int k = 0, y = top; k < n; k++) if (page_of[k] == P.form_page) { draw_item(screen, seq[k], y); y += item_h(seq[k]); }
    if (pages_now > 1) ink_ui_pagebar(screen, P.form_page + 1, pages_now, bottom - INK_CONTROL, bar_clicked);
    lv_obj_t *f = P.focus ? item_field(P.focus - 2) : NULL; /* the number brought along, 加一个号码's new row, or a field asked for */
    if (f) ink_textfield_short_focus(f);
    P.focus = 0;
}

/* ---- the page shell ---- */
static bool turn(int delta) {
    if (P.list) return ink_ui_list_turn(P.list, delta);
    int *at = P.page == DETAIL ? &P.number_page : P.page == EDIT ? &P.form_page : NULL;
    if (!at || *at + delta < 0 || *at + delta >= pages_now) return false;
    harvest(); *at += delta; ink_ui_redraw(); return true;
}
static lv_obj_t *item_field(int item) { return item < 0 ? P.name_field : item < NUMS ? P.num_fields[item] : P.note_field; }
static int focus_item(int item) { /* on another form page: that page first */
    lv_obj_t *f = item_field(item);
    if (f) return ink_textfield_short_focus(f);
    harvest(); P.focus = item + 2; ink_ui_redraw(); return 0;
}
static void star_label(void *arg) { /* a star changed and nothing else: the button alone */
    (void)arg; if (P.page != DETAIL || !P.star_button || !ink_ui_page_current()) return;
    lv_label_set_text(lv_obj_get_child(P.star_button, 0), P.cur.star ? TR("已收藏") : TR("收藏"));
    snprintf(P.argv[1], sizeof P.argv[1], "{\"id\":%u,\"star\":%s}", (unsigned)P.id, P.cur.star ? "false" : "true");
    P.fields[0].value = P.cur.star ? TR("已收藏") : TR("没收藏"); P.actions[0].label = P.cur.star ? TR("取消收藏") : TR("收藏");
    ink_ui_content_changed();
}
static void refresh(void *arg) { /* a write here, elsewhere or by the AI: redraw only what this page shows and changed */
    (void)arg;
    if (!P.db || !ink_ui_page_current() || strcmp(P.member, ink_member_current()->id)) return;
    person_t now; uint32_t ver = P.id && P.page != LIST ? read_person(P.db, P.id, &now) : 0;
    if (P.page == EDIT) { if (P.id && !ver && !P.saving) ink_ui_pop(); return; } /* deleted under the editor */
    if (P.page == LIST) { ink_ui_redraw(); return; }
    if (P.page != DETAIL && P.page != CHOOSE) return;
    bool star = ver && now.star != P.cur.star; if (ver) now.star = P.cur.star;
    if (ver && !memcmp(&now, &P.cur, sizeof now) && (P.page == CHOOSE || !P.ndups)) {
        P.ver = ver; P.cur.star ^= star;
        if (star && P.page == DETAIL) ink_ui_after_press(star_label, NULL);
    } else if (!ver || ver != P.ver) ink_ui_redraw();
}
static void create(lv_obj_t *screen, void *arg) {
    static const char *const TITLES[] = {TR_NOOP("联系人"), "", TR_NOOP("编辑联系人"), TR_NOOP("用哪个号码"), TR_NOOP("导入导出")};
    int page = 0; while (page < TRANSFER && strcmp(PAGES[page].name, ink_ui_page_name())) page++;
    P.page = page; P.list = NULL; P.search = P.star_button = P.back_button = P.panel_root = P.name_field = P.note_field = NULL; P.panel = -1;
    memset(P.num_fields, 0, sizeof P.num_fields); memset(P.type_pills, 0, sizeof P.type_pills); memset(P.fields, 0, sizeof P.fields); memset(P.actions, 0, sizeof P.actions);
    if (strcmp(P.member, ink_member_current()->id)) {
        P.query[0] = P.result[0] = 0; memset(&P.position, 0, sizeof P.position); P.draft_ready = false; snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id);
    }
    if (page == LIST) P.mode = (int)(intptr_t)arg;
    else if (page != TRANSFER) P.id = (uint32_t)(uintptr_t)arg;
    const char *title = page == LIST ? (P.mode == PHONE ? "" : P.mode == PICK ? TR("选联系人") : P.mode == ADD ? TR("加到联系人") : TR("联系人")) : page == EDIT && !P.id ? TR("新建联系人") : TR(TITLES[page]);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, title);
    if (page == LIST && P.mode == PHONE) ink_phone_tabs(screen, 2);
    P.notice = ink_ui_notice(screen, P.said, page == LIST ? INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L : INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE));
    P.said[0] = 0;
    P.model = (ink_ui_page_spec_t){.id = PAGES[page].name, .title = title[0] ? title : TR("联系人"), .object = {"phone.contacts", 0}, .fields = P.fields, .actions = P.actions};
    P.db = ink_db_open_cached_member(&CONTACTS, P.member);
    if (!P.db) { /* not read yet: said, read in the background, drawn once when it is there */
        lv_label_set_text(P.notice, TR("正在读取联系人"));
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "collection", CONTACTS.name); ink_args_int(&a, "limit", 1); request("data.list", &a, reply);
        return;
    }
    bool fresh = page != EDIT || !P.draft_ready;
    if (fresh && page != LIST && page != TRANSFER) { memset(&P.cur, 0, sizeof P.cur); P.cur.def = -1; P.ver = P.id ? read_person(P.db, P.id, &P.cur) : 0; }
    snprintf(P.labels[13], sizeof P.labels[13], "contact:%u", (unsigned)P.id);
    if (page != LIST && page != TRANSFER) P.model.object = (ink_ui_object_ref_t){P.labels[13], P.ver};
    if (page != LIST && page != TRANSFER && P.id && !P.ver) { /* ViewContactActivity closes with a toast; here it says so and stays */
        ink_ui_empty(screen, TR("找不到这个联系人"), TR("回到列表"), back_clicked, NULL);
        field("status", "status", TR("状态"), TR("找不到这个联系人")); action("back", TR("回到列表"), "shell.back", NULL, true);
        return;
    }
    if (page == EDIT) {
        P.fields[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = TR("姓名"), .object = P.model.object};
        P.fields[1] = (ink_ui_field_spec_t){.id = "note", .role = "textbox", .label = TR("备注"), .object = P.model.object};
        P.actions[0] = (ink_ui_action_spec_t){.id = "name", .label = TR("输入姓名"), .capability = "contacts.name", .enabled = true};
        P.actions[1] = (ink_ui_action_spec_t){.id = "note", .label = TR("输入备注"), .capability = "contacts.note", .enabled = true};
        P.model.field_count = P.model.action_count = 2;
    }
    if (page == LIST) list_page(screen);
    else if (page == DETAIL) detail_page(screen);
    else if (page == EDIT) edit_page(screen);
    else if (page == CHOOSE) choose_page(screen);
    else transfer_page(screen);
}
static void destroy(void) {
    harvest(); ink_db_close(P.db); P.db = NULL; P.list = NULL;
    P.notice = P.search = P.star_button = P.back_button = P.panel_root = P.name_field = P.note_field = NULL;
    memset(P.num_fields, 0, sizeof P.num_fields); memset(P.type_pills, 0, sizeof P.type_pills);
}
static int edit_snapshot(char *out, size_t cap) { /* the two saving steps written out: the AI calls them as the button does */
    person_t after; harvest(); want(); bool more = second_step(&P.cur, &after);
    P.model.field_count = P.model.action_count = 2;
    if (!P.name_field) P.fields[0].value = P.draft.name;
    if (!P.note_field) P.fields[1].value = P.draft.note;
    for (int i = 0; i < P.draft.count; i++) {
        snprintf(P.labels[8 + i], sizeof P.labels[0], TR("号码 %d · %s%s"), i + 1, TR(TYPE_NAMES[P.draft.type[i] & 3]), P.draft.count > 1 && P.draft.def == i ? TR(" · 默认") : "");
        snprintf(P.argv[2 + i], sizeof P.argv[0], "{\"index\":%d}", i);
        field(NUMBER_IDS[i], "textbox", P.labels[8 + i], P.draft.num[i]);
        action(NUMBER_IDS[i], TR("输入号码"), "contacts.number", P.argv[2 + i], true);
        action(TYPE_ACTS[i], TR("选号码类型"), "contacts.number_type", P.argv[2 + i], true);
        if (P.draft.count > 1) action(DEFAULT_ACTS[i], TR("设为默认号码"), "contacts.number_default", P.argv[2 + i], P.draft.def != i);
    }
    action("number_add", TR("加一个号码"), "contacts.number_add", NULL, P.draft.count < NUMS);
    ink_jw_t w; ink_jw_init(&w, P.put_args, sizeof P.put_args); ink_jw_raw(&w, "{");
    if (P.id) { ink_jw_raw(&w, "\"id\":"); ink_jw_int(&w, P.id); ink_jw_raw(&w, ",\"if_revision\":"); ink_jw_int(&w, P.ver); ink_jw_raw(&w, ","); }
    ink_jw_raw(&w, "\"name\":"); ink_jw_str(&w, P.want.name); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, default_of(&P.want));
    ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, P.want.note); ink_jw_raw(&w, "}");
    numbers_json(P.numbers_args, sizeof P.numbers_args, P.id, P.ver);
    action("save", TR("保存联系人"), "contacts.put", P.put_args, !P.saving && !w.overflow);
    action("save_numbers", TR("保存号码：保存联系人之后调，id 和 if_revision 用它回执里的"), "contacts.numbers", P.numbers_args, more);
    if (P.id) {
        ink_jw_init(&w, P.delete_args, sizeof P.delete_args);
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, P.id); ink_jw_raw(&w, ",\"if_revision\":"); ink_jw_int(&w, P.ver);
        ink_jw_raw(&w, ",\"name\":"); ink_jw_str(&w, P.cur.name); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, default_of(&P.cur)); ink_jw_raw(&w, "}");
        action("delete", TR("删除联系人"), "contacts.delete", P.delete_args, !w.overflow);
    }
    for (int t = 0; P.panel_root && P.panel >= 0 && t < 4 + (P.draft.count > 1 && P.draft.def != P.panel); t++)
        action(ROW_IDS[t], t < 4 ? P.labels[15 + t] : TR("设为默认号码"), t < 4 ? "contacts.number_type" : "contacts.number_default", P.argv[20 + t], true);
    if (pages_now > 1) { action("prev", TR("上一页"), "contacts.prev", NULL, P.form_page > 0); action("next", TR("下一页"), "contacts.next", NULL, P.form_page + 1 < pages_now); }
    lv_obj_t *focused = P.note_field && lv_obj_has_state(P.note_field, LV_STATE_FOCUSED) ? P.note_field : P.name_field;
    return focused ? ink_textfield_short_snapshot(focused, out, cap) : ink_ui_semantics_snapshot(&P.model, out, cap);
}
static int snapshot(char *out, size_t cap) {
    static const char *const MODES[] = {"view", "pick", "phone", "add_to"};
    if (P.page == LIST && P.list) { /* the rows (name, default number, how many, starred, same name) and the search */
        int n = ink_ui_list_snapshot(P.list, out, cap), m = P.search ? ink_textfield_short_snapshot(P.search, P.snap, sizeof P.snap) : 0;
        if (n < 0 || m < 0) return -1;
        ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
        ink_jw_raw(&w, ",\"mode\":"); ink_jw_str(&w, MODES[P.mode & 3]);
        if (P.mode == ADD) { ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, P.add_number); }
        if (m) { ink_jw_raw(&w, ",\"search\":"); ink_jw_raw(&w, P.snap); }
        ink_jw_raw(&w, ",\"controls\":"); m = ink_ui_semantics_snapshot(&P.model, P.snap, sizeof P.snap); ink_jw_raw(&w, m > 0 ? P.snap : "{}");
        ink_jw_raw(&w, "}"); return w.overflow ? -1 : (int)w.len;
    }
    if (P.page == EDIT && P.db && P.draft_ready && (!P.id || P.ver)) return edit_snapshot(out, cap);
    if (P.db && P.model.field_count + P.model.action_count) return ink_ui_semantics_snapshot(&P.model, out, cap);
    return snprintf(out, cap, TR("联系人：%s"), P.notice ? lv_label_get_text(P.notice) : TR("暂时打不开"));
}
static bool page_key(ink_key_t k, bool lng) {
    if (lng) return false;
    if (k == INK_KEY_BACK && P.panel_root) { panel_close(); return true; } /* the panel first */
    if (k == INK_KEY_BACK && P.page == EDIT && P.draft_ready && !P.saving && P.db && edited()) { panel_open(-1); return true; }
    if (P.list) return ink_ui_list_key(P.list, k, lng);
    return (k == INK_KEY_NEXT || k == INK_KEY_PREV) && turn(k == INK_KEY_NEXT ? 1 : -1);
}
static void navigate(ink_call_t *c) {
    const char *op = c->cap->id + sizeof "contacts." - 1; int64_t id = 0, index = 0; char number[INK_VC_NUMBER + 1] = "", type[8] = "";
    ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "index", &index);
    ink_args_get_str(&c->args, "number", number, sizeof number); ink_args_get_str(&c->args, "type", type, sizeof type);
    bool editing = P.page == EDIT && P.draft_ready && !strcmp(ink_ui_page_name(), PAGES[EDIT].name), list = !strcmp(op, "open") || !strcmp(op, "pick") || !strcmp(op, "add_to");
    bool record_page = !strcmp(op, "view") || !strcmp(op, "edit") || !strcmp(op, "choose");
    if (number[0] && !ink_vcard_number_ok(number) && (list || record_page)) c->rc.status = INK_RC_INVALID;
    else if (list) {
        P.query[0] = 0; memset(&P.position, 0, sizeof P.position); snprintf(P.add_number, sizeof P.add_number, "%s", op[0] == 'a' ? number : "");
        ink_ui_drop_app("contacts"); ink_ui_push(&PAGES[LIST], (void *)(intptr_t)(op[0] == 'a' ? ADD : op[1] == 'i' ? PICK : VIEW));
    } else if (record_page) {
        P.number_page = 0;
        if (op[0] == 'e') { P.draft_ready = false; snprintf(P.add_number, sizeof P.add_number, "%s", number); }
        else if (!strcmp(ink_ui_page_name(), PAGES[DETAIL].name)) ink_ui_pop();
        ink_ui_push(&PAGES[op[0] == 'v' ? DETAIL : op[0] == 'e' ? EDIT : CHOOSE], (void *)(uintptr_t)id);
    } else if (!strcmp(op, "transfer")) { P.result[0] = I.say[0] = 0; ink_ui_push(&PAGES[TRANSFER], NULL); }
    else if (!strcmp(op, "import_pick")) { if (ink_file_picker_open(INK_FS_OTHER, picked, NULL) != 0) c->rc.status = INK_RC_FAILED; }
    else if (!strcmp(op, "next") || !strcmp(op, "prev")) { if (!turn(op[0] == 'n' ? 1 : -1)) c->rc.status = INK_RC_STALE; }
    else if (!strcmp(op, "search")) { if (!P.search || ink_textfield_short_focus(P.search)) c->rc.status = INK_RC_STALE; }
    else if (!editing) c->rc.status = INK_RC_STALE;
    else if (!strcmp(op, "name") || !strcmp(op, "note")) { if (focus_item(op[1] == 'a' ? -1 : NUMS + 1)) c->rc.status = INK_RC_STALE; }
    else if (!strcmp(op, "number_add")) {
        harvest();
        if (P.draft.count >= NUMS) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("最多 5 个号码")); }
        else { P.draft.num[P.draft.count][0] = 0; P.draft.type[P.draft.count] = INK_VC_MOBILE; P.focus = P.draft.count++ + 2; ink_ui_redraw(); }
    } else if (index < 0 || index >= P.draft.count) c->rc.status = INK_RC_INVALID;
    else if (!strcmp(op, "number")) { if (focus_item((int)index)) c->rc.status = INK_RC_STALE; }
    else if (!strcmp(op, "number_type") && !type[0]) panel_open((int)index);
    else if (!strcmp(op, "number_type")) {
        if (type_of(type) < 0) { c->rc.status = INK_RC_INVALID; return; }
        harvest(); P.draft.type[index] = (uint8_t)type_of(type);
        if (P.type_pills[index]) lv_label_set_text(lv_obj_get_child(P.type_pills[index], 0), TR(TYPE_NAMES[P.draft.type[index]]));
        panel_close(); ink_ui_content_changed();
    } else { /* number_default */
        if (P.draft.count < 2) { c->rc.status = INK_RC_INVALID; return; }
        harvest(); P.draft.def = (int8_t)index;
        for (int k = 0; k < P.draft.count; k++) if (P.num_fields[k]) ink_textfield_style(P.num_fields[k], INK_FIELD_SINGLE, number_label(k));
        panel_close(); ink_ui_content_changed();
    }
}
#define PAGE(name_) {.name = name_, .app = "contacts", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("PhoneContacts"), PAGE("ContactDetail"), PAGE("ContactEdit"), PAGE("ContactNumbers"), PAGE("ContactsTransfer")};
extern const ink_activity_t ink_phone_page;
static const ink_app_def_t APP = {.id = "phone", .title = TR_NOOP("电话"), .icon = INK_UI_ICON(phone), .entry = &ink_phone_page, .system_app = true, .child_ok = true};
static const ink_app_def_t CONTACT_APP = {.id = "contacts", .title = TR_NOOP("联系人"), .icon = INK_UI_ICON(person), .entry = &PAGES[LIST], .system_app = true, .child_ok = true};
#define ID "\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define RECORD "{\"type\":\"object\",\"properties\":{" ID "},\"required\":[\"id\"]}"
#define VERSION "\"if_revision\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define NUMBER "\"number\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":24}"
#define DETAILS "\"name\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":32}," NUMBER ",\"note\":{\"type\":\"string\",\"maxLength\":64}"
#define INDEX "\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4}"
#define NAV(id_, title_, input_) {.id = id_, .app = "phone", .title = title_, .input = input_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
#define FORM(id_, title_, input_) {.id = id_, .app = "phone", .title = title_, .input = input_, .activity = "ContactEdit", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
#define WRITE(id_, title_, input_, run_) {.id = id_, .app = "phone", .title = title_, .input = input_, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = run_}
static const ink_cap_t CAPS[] = {
    {.id = "contacts.open", .app = "phone", .locked_owner_only = true, .title = "打开联系人", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    NAV("contacts.pick", "挑选联系人号码", NULL),
    {.id = "contacts.search", .app = "phone", .title = "搜索联系人", .activity = "PhoneContacts", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    NAV("contacts.view", "查看联系人详情", RECORD),
    FORM("contacts.note", "输入联系人备注", NULL),
    WRITE("contacts.put", "添加或修改联系人：新建时一个手机号码；修改时 number 换掉默认号码（没有默认就是第一个），别的号码不动", "{\"type\":\"object\",\"properties\":{" ID "," VERSION "," DETAILS "},\"required\":[\"name\",\"number\"]}", record),
    {.id = "contacts.delete", .app = "phone", .title = "删除联系人", .ask = TR_NOOP("删除联系人 {name}（{number}），不能撤销"), .input = "{\"type\":\"object\",\"properties\":{" ID "," VERSION "," DETAILS "},\"required\":[\"id\",\"if_revision\",\"name\",\"number\"]}", .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .exec = INK_EXEC_BG, .run = record},
    {.id = "contacts.select", .app = "phone", .title = "选联系人和号码，返回编号和号码（几个号码又没有默认时要给 number）", .input = "{\"type\":\"object\",\"properties\":{" ID "," NUMBER "},\"required\":[\"id\"]}", .output = "{\"type\":\"object\",\"properties\":{" ID ",\"number\":{\"type\":\"string\"}},\"required\":[\"id\",\"number\"]}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = record},
    NAV("contacts.edit", "打开联系人编辑页或添加联系人（number：新建时填好，已有的人加在号码最后）", "{\"type\":\"object\",\"properties\":{" ID "," NUMBER "}}"),
    NAV("contacts.next", "联系人下一页", NULL),
    NAV("contacts.prev", "联系人上一页", NULL),
    FORM("contacts.name", "输入联系人姓名", NULL),
    FORM("contacts.number", "输入联系人号码（index：第几个号码，从 0 起）", "{\"type\":\"object\",\"properties\":{" INDEX "}}"),
    WRITE("contacts.numbers", "改联系人的全部号码：1 到 5 个 {n 号码, t mobile/home/work/other}，default 是默认号码的下标", "{\"type\":\"object\",\"properties\":{" ID "," VERSION ",\"numbers\":{\"type\":\"array\"}," "\"default\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4}},\"required\":[\"id\",\"if_revision\",\"numbers\"]}", record),
    WRITE("contacts.star", "收藏或取消收藏联系人", "{\"type\":\"object\",\"properties\":{" ID ",\"star\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"star\"]}", record),
    WRITE("contacts.merge", "把 from 合并到 id：号码合在一起（最多 5 个），备注接上，有一位收藏就收藏，然后删掉 from", "{\"type\":\"object\",\"properties\":{" ID ",\"from\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}," VERSION "},\"required\":[\"id\",\"from\",\"if_revision\"]}", record),
    WRITE("contacts.import", "从存储卡的 .vcf 文件导入联系人（file 是文件编号；号码都已在通讯录里的人跳过）", "{\"type\":\"object\",\"properties\":{\"file\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"file\"]}", import_file),
    WRITE("contacts.export", "把这位家人的全部联系人导出到我的文件（联系人-日期.vcf）", NULL, export_file),
    NAV("contacts.choose", "打开“用哪个号码”页（几个号码又没有默认时）", RECORD),
    NAV("contacts.add_to", "把号码加到联系人：第一行新建，点一位就加在他的号码最后", "{\"type\":\"object\",\"properties\":{" NUMBER "},\"required\":[\"number\"]}"),
    NAV("contacts.transfer", "打开联系人的导入导出页", NULL),
    NAV("contacts.import_pick", "选存储卡上的 .vcf 文件来导入", NULL),
    FORM("contacts.number_add", "编辑页加一个号码（最多 5 个）", NULL),
    FORM("contacts.number_type", "编辑页：第 index 个号码的类型（不给 type 就打开类型面板）", "{\"type\":\"object\",\"properties\":{" INDEX ",\"type\":{\"type\":\"string\",\"enum\":[\"mobile\",\"home\",\"work\",\"other\"]}},\"required\":[\"index\"]}"),
    FORM("contacts.number_default", "编辑页：把第 index 个号码设为默认号码", "{\"type\":\"object\",\"properties\":{" INDEX "},\"required\":[\"index\"]}"),
};
int ink_contacts_init(void) {
    return ink_db_register(&CONTACTS) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_app_register(&CONTACT_APP) ? -1 : 0;
}
void ink_contacts_open(void) { P.query[0] = 0; memset(&P.position, 0, sizeof P.position); ink_ui_drop_app("contacts"); ink_ui_push(&PAGES[LIST], (void *)PHONE); }
