#define INK_MOGO_PORT_IMPL
#include <runtime.h>
#include <nvs.h>
#include <book_bookmark_format.h>
#include <page.h>
#include "ink/probe.h"
#include "ink/sys/ui.h"
#define INK_MOGO_VALUE_MAX 1024
typedef struct { char space[16], key[16]; uint8_t type, data[INK_MOGO_VALUE_MAX]; } ink_mogo_value_t;
_Static_assert(sizeof(reader_bookmarks_t) <= INK_MOGO_VALUE_MAX, "native bookmarks fit NVS value");
struct ink_mogo_nvs { ink_db_t *db; char space[16]; bool writable; ink_mogo_value_t value; };
struct ink_mogo_iterator { nvs_handle_t handle; ink_db_cursor_t cursor; nvs_type_t type; };
/* Not registered: backups keep its files as the member's state (see the reader-one-1 report). */
static const ink_db_def_t INK_MOGO_PREFS = {.name="mogo.preferences",.title="阅读偏好与进度",
    .scope=INK_DB_MEMBER,.schema=1,.max_records=256,.max_bytes=131072};
esp_err_t nvs_open(const char *space, int mode, nvs_handle_t *out) {
    *out = NULL;
    if (ink_ui_is_current()) ink_probe_count("reader.ui_records"); /* BG only (graft.h): the tests count that it stays 0 */
    if (strlen(space) >= sizeof ((nvs_handle_t)0)->space) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h = ink_mogo_calloc(1,sizeof *h);
    if (!h) return ESP_ERR_NO_MEM;
    h->db = ink_graft_preferences_open(ink_mogo_current_owner(),"mogo",&INK_MOGO_PREFS);
    if (!h->db) { ink_app_free(h); return ESP_ERR_INVALID_STATE; }
    strcpy(h->space,space); h->writable = mode == NVS_READWRITE; *out = h;
    return ESP_OK;
}
void nvs_close(nvs_handle_t h) { ink_db_close(h->db); ink_app_free(h); }
static uint32_t ink_mogo_find(nvs_handle_t h, const char *key, ink_rec_t *rec) {
    ink_db_cursor_t cursor = {0};
    while (ink_db_next(h->db,&cursor,rec,&h->value,sizeof h->value) > 0)
        if (rec->len >= offsetof(ink_mogo_value_t,data) && !strncmp(h->value.space,h->space,16) && !strncmp(h->value.key,key,16)) return rec->id;
    return 0;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len) {
    ink_rec_t rec;
    if (!ink_mogo_find(h,key,&rec)) return ESP_ERR_NVS_NOT_FOUND;
    size_t n = rec.len - offsetof(ink_mogo_value_t,data), cap = *len;
    *len = n;
    if (n > sizeof h->value.data || (out && cap < n)) return ESP_ERR_NVS_INVALID_LENGTH;
    if (out) memcpy(out,h->value.data,n);
    return ESP_OK;
}
static esp_err_t ink_mogo_put(nvs_handle_t h, const char *key, const void *data, size_t len, nvs_type_t type) {
    if (!h->writable || strlen(key) >= sizeof h->value.key || len > sizeof h->value.data) return ESP_ERR_INVALID_ARG;
    ink_rec_t rec;
    uint32_t id = ink_mogo_find(h,key,&rec);
    memset(&h->value,0,sizeof h->value);
    strcpy(h->value.space,h->space); strcpy(h->value.key,key); h->value.type = (uint8_t)type;
    memcpy(h->value.data,data,len);
    ink_db_write_t w = {.id=id,.data=&h->value,.len=offsetof(ink_mogo_value_t,data)+len,.src=INK_SRC_SYSTEM};
    return ink_db_put(h->db,&w,NULL) ? ESP_FAIL : ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *p, size_t n) { return ink_mogo_put(h,k,p,n,NVS_TYPE_BLOB); }
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *p, size_t *n) { return nvs_get_blob(h,k,p,n); }
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *p) { return ink_mogo_put(h,k,p,strlen(p)+1,NVS_TYPE_STR); }
esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *p) { size_t n=sizeof *p; return nvs_get_blob(h,k,p,&n); }
esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t p) { return ink_mogo_put(h,k,&p,sizeof p,NVS_TYPE_U32); }
esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *p) { size_t n=sizeof *p; return nvs_get_blob(h,k,p,&n); }
esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t p) { return ink_mogo_put(h,k,&p,sizeof p,NVS_TYPE_U8); }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *k) {
    ink_rec_t rec;
    uint32_t id = ink_mogo_find(h,k,&rec);
    if (!h->writable) return ESP_ERR_INVALID_STATE;
    return !id ? ESP_ERR_NVS_NOT_FOUND : ink_db_del(h->db,id,INK_SRC_SYSTEM) ? ESP_FAIL : ESP_OK;
}
/* Accepted and on its way to the flash, like the old reader's position files: waiting here would hold the shared BG
 * executor for as long as a recording or a waveform holds the flash back. */
bool ink_mogo_quiet_flush;
esp_err_t nvs_commit(nvs_handle_t h) {
    if (ink_mogo_quiet_flush) ink_db_flush_background(h->db);
    else ink_db_flush(h->db);
    return ESP_OK;
}
esp_err_t nvs_entry_next(nvs_iterator_t *it) {
    nvs_iterator_t i = *it;
    ink_rec_t rec;
    while (ink_db_next(i->handle->db,&i->cursor,&rec,&i->handle->value,sizeof i->handle->value) > 0)
        if (!strncmp(i->handle->value.space,i->handle->space,16) && (!i->type || i->type == i->handle->value.type)) return ESP_OK;
    return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_entry_find(const char *part, const char *space, nvs_type_t type, nvs_iterator_t *out) {
    (void)part; *out = ink_mogo_calloc(1,sizeof **out);
    if (!*out) return ESP_ERR_NO_MEM;
    esp_err_t err = nvs_open(space,NVS_READONLY,&(*out)->handle);
    if (err) { ink_app_free(*out); *out=NULL; return err; }
    (*out)->type = type;
    err=nvs_entry_next(out);
    if (err) { nvs_release_iterator(*out); *out=NULL; }
    return err;
}
esp_err_t nvs_entry_info(nvs_iterator_t i, nvs_entry_info_t *out) {
    memcpy(out->namespace_name,i->handle->value.space,16); memcpy(out->key,i->handle->value.key,16);
    out->type = (nvs_type_t)i->handle->value.type; return ESP_OK;
}
void nvs_release_iterator(nvs_iterator_t i) { if (i) { nvs_close(i->handle); ink_app_free(i); } }
/* The card's record without the NVS layer (whose owner is the session's): read for `member`, then check the book is there. */
bool ink_mogo_card_load(const char *member, ink_mogo_card_t *out) {
    ink_db_t *db = ink_db_open_member(&INK_MOGO_PREFS, member);
    ink_mogo_value_t *v = ink_mogo_calloc(1, sizeof *v);
    ink_db_cursor_t cursor = {0}; ink_rec_t rec; bool found = false;
    while (db && v && !found && ink_db_next(db, &cursor, &rec, v, sizeof *v) > 0)
        found = rec.len == offsetof(ink_mogo_value_t, data) + sizeof *out && !strcmp(v->space, "ink_card") && !strcmp(v->key, "last");
    if (found) memcpy(out, v->data, sizeof *out);
    ink_app_free(v); ink_db_close(db);
    if (!found || !memchr(out->path, 0, sizeof out->path)) return false;
    ink_fs_file_t *f = NULL;
    if (!strncmp(out->path, "asset:", 6)) return ink_app_asset_read(out->path + 6, NULL, 0) > 0;
    if (ink_fs_open_for(member, out->path, INK_FS_READ, &f)) return false;
    ink_fs_close(f);
    return true;
}
