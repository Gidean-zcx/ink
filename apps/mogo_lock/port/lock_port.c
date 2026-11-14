/* What Mogo's lock styles call that this device answers differently: files, settings, text, rules and the battery.
 * Every call here runs on the background executor, inside mogo_lock.c's draw, for the member it draws for. */
#include <runtime.h>
#include <lock_port.h>
#include <epdiy.h>
#include <settings.h>
#include <ui_kit.h>
#include <ui_font.h>
#include <home_components.h>
#include <esp_random.h>
#include <book_store.h>
#include <book_cover_jobs.h>
#include <book_progress.h>
#include <image_decode.h>
#include <errno.h>
#include "ink/sys/app.h"
#include <ink/sys/image.h>
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include <mogo_lock.h>

const ink_lock_request_t *ink_mogo_lock_request;
typedef struct { ink_fs_dir_t *dir; struct dirent entry; } lock_dir_t;

static bool owner_ok(void) {
    ink_member_identity_t now; ink_member_identity(&now);
    const ink_member_identity_t *o = ink_mogo_current_owner();
    return now.epoch == o->epoch && !strcmp(now.member, o->member) && ink_member_space_open();
}
/* Mogo keeps its caches under /sdcard/.readpico; here they are the member's own, under /me/mogo (a dot name is not a card name here). */
static const char *mapped(const char *path, char *out, size_t cap) {
    if (strncmp(path, "/sdcard/.readpico", 17)) return path;
    snprintf(out, cap, "/me/mogo%s", path + 17);
    return out;
}
FILE *ink_mogo_lock_fopen(const char *path, const char *mode) {
    char m[INK_FS_PATH_MAX];
    path = mapped(path, m, sizeof m);
    if (!strcmp(mode, "rb")) return ink_mogo_fopen(path, mode);
    if (strcmp(mode, "wb") || !owner_ok()) return NULL;
    (void)ink_fs_remove_for(ink_mogo_current_owner()->member, path); /* "wb" starts empty */
    ink_fs_file_t *f = NULL;
    if (ink_graft_file_open(ink_mogo_current_owner(), path, INK_FS_WRITE | INK_FS_CREATE, &f)) return NULL;
    FILE *s = ink_mogo_stream_wrap(f); /* runtime.c's stream, so its fread/fseek/fclose take it */
    if (!s) ink_fs_close(f);
    return s;
}
size_t ink_mogo_lock_fwrite(const void *p, size_t size, size_t n, FILE *f) { return ink_mogo_stream_write(p, size, n, f); }
int ink_mogo_lock_mkdir(const char *path) {
    char m[INK_FS_PATH_MAX];
    path = mapped(path, m, sizeof m);
    if (!owner_ok() || strncmp(path, "/me/", 4)) { errno = EACCES; return -1; }
    if (ink_fs_mkdir_for(ink_mogo_current_owner()->member, path)) { errno = EIO; return -1; }
    errno = EEXIST; /* made or there: Mogo only goes on when errno says it exists */
    return -1;
}
int ink_mogo_lock_rename(const char *from, const char *to) {
    char a[INK_FS_PATH_MAX], b[INK_FS_PATH_MAX];
    from = mapped(from, a, sizeof a); to = mapped(to, b, sizeof b);
    return owner_ok() && !strncmp(from, "/me/", 4) && !strncmp(to, "/me/", 4) && !ink_fs_move(from, to) ? 0 : -1;
}
int ink_mogo_lock_remove(const char *path) {
    char m[INK_FS_PATH_MAX];
    path = mapped(path, m, sizeof m);
    return owner_ok() && !strncmp(path, "/me/", 4) && !ink_fs_remove_for(ink_mogo_current_owner()->member, path) ? 0 : -1;
}
int ink_mogo_lock_stat(const char *path, struct stat *st) {
    if (!ink_mogo_stat(path, st)) return 0;
    ink_fs_stat_t s;
    if (!owner_ok() || strncmp(path, "/me/", 4) || ink_fs_stat(path, &s) || !s.is_dir) return -1;
    memset(st, 0, sizeof *st); st->st_mode = S_IFDIR;
    return 0;
}
/* A member who never had a book has no /me/books: here that is an empty shelf, not Mogo's unreadable card. */
DIR *ink_mogo_lock_opendir(const char *path) {
    lock_dir_t *d = owner_ok() && !strncmp(path, "/me/", 4) ? ink_mogo_calloc(1, sizeof *d) : NULL;
    ink_fs_stat_t s;
    if (d && ink_fs_dir_open(path, &d->dir) && (strcmp(path, "/me/books") || !ink_fs_stat(path, &s))) { ink_app_free(d); d = NULL; }
    return (DIR *)d;
}
struct dirent *ink_mogo_lock_readdir(DIR *dir) {
    lock_dir_t *d = (lock_dir_t *)dir;
    ink_fs_stat_t s;
    errno = 0;
    if (!d->dir) return NULL;
    int r = ink_fs_dir_next(d->dir, &s);
    if (r < 0) errno = EIO;
    if (r != 1) return NULL;
    snprintf(d->entry.d_name, sizeof d->entry.d_name, "%s", s.name);
    return &d->entry;
}
int ink_mogo_lock_closedir(DIR *dir) { lock_dir_t *d = (lock_dir_t *)dir; if (d->dir) ink_fs_dir_close(d->dir); ink_app_free(d); return 0; }

/* Mogo counts days in Beijing time (book_ticket.c local_day: +8 h); shifted by the zone's offset its day is ours. */
static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}
time_t ink_mogo_lock_time(time_t *out) {
    int64_t now = ink_time_now_s();
    ink_tm_t t; ink_time_to_local(now, &t);
    int64_t local = days_from_civil(t.year, t.month, t.day) * 86400 + t.hour * 3600 + t.min * 60 + t.sec;
    time_t shifted = (time_t)(now + (local - now) - 8 * 3600);
    if (!ink_time_valid()) shifted = 0;
    if (out) *out = shifted;
    return shifted;
}
/* The mini library's random pick only needs to differ between draws: xorshift from the monotonic clock. */
uint32_t ink_mogo_lock_random(void) {
    static uint32_t s;
    if (!s) s = (uint32_t)ink_now_us() | 1u;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

/* Mogo's reader stores the last book when it opens one (app_book.c:7867) and the ticket reads it back; ours does not store it
 * (reader-2). So each call asks again and saves nothing: the book whose progress was saved last (book_progress_save numbers
 * every save in last_open_s), and only with no progress at all the stored one. Nothing is written while the lock draws. */
bool (book_progress_last_path)(char *out, size_t cap); /* book_progress.h's, which lock_port.h's macro renames here */
typedef struct { char *out; size_t cap; uint32_t when; bool any; } latest_t;
static bool latest(const char *path, const book_progress_t *p, void *ctx) {
    latest_t *l = ctx;
    if ((!l->any || p->last_open_s > l->when) && strlen(path) < l->cap) {
        snprintf(l->out, l->cap, "%s", path); l->when = p->last_open_s; l->any = true;
    }
    return true;
}
bool ink_mogo_lock_last_path(char *out, size_t cap) {
    if (!out || !cap) return false;
    latest_t l = {.out = out, .cap = cap};
    out[0] = 0;
    if (book_progress_list(latest, &l) == ESP_OK && l.any) return true;
    return (book_progress_last_path)(out, cap);
}

/* Settings the styles read, answered by the lock style's own settings and the member drawn for. */
app_ticket_background_t app_settings_ticket_background(void) {
    return ink_mogo_lock_request && ink_mogo_lock_request->option == 1 ? APP_TICKET_BACKGROUND_TRANSPARENT : APP_TICKET_BACKGROUND_COVER;
}
const char *app_settings_device_name(void) { return ink_member_current()->name; }
bool app_settings_main_fast_refresh(void) { return false; }
uint8_t app_settings_system_contrast(void) { return 100; }
const char *app_settings_books_dir(void) { return "/me/books"; }
bool app_home_library_recent(void) { return !ink_mogo_lock_request || ink_mogo_lock_request->option == 0; }
bool app_home_library_set_recent(bool recent) { (void)recent; return false; } /* the mode is changed in settings only */
esp_err_t book_store_roots(book_store_root_t out[BOOK_STORE_ROOT_MAX], int *n) {
    memset(out, 0, sizeof *out); snprintf(out[0].path, sizeof out[0].path, "/me/books"); *n = 1;
    return ESP_OK;
}
bool book_store_roots_degraded(void) { return false; }
bool book_store_flash_ready(void) { return false; }
unsigned book_store_revision(void) { return 0; }

/* Mogo's cover queue runs a decoder task; here one cover is made when asked, on the same executor. */
static book_cover_job_t *g_job; /* PSRAM: about 700 B, kept from the first cover */
static bool g_job_set;
bool book_cover_jobs_submit(unsigned generation, unsigned slot, const book_cover_job_t *request) {
    (void)generation; (void)slot;
    if (!g_job && !(g_job = ink_mogo_malloc(sizeof *g_job))) return false;
    *g_job = *request; g_job_set = true; return true;
}
int book_cover_jobs_take(unsigned generation, unsigned slot, book_cover_result_t *result) {
    (void)generation; (void)slot;
    if (!g_job_set) return -1;
    g_job_set = false;
    struct stat st;
    uint8_t *pixels = ink_mogo_malloc(BOOK_COVER_W * BOOK_COVER_H);
    unsigned w = BOOK_COVER_W, h = BOOK_COVER_H;
    if (!pixels || ink_mogo_stat(g_job->path, &st) ||
        !book_cover_load_list_gray(g_job->path, g_job->title, g_job->author, pixels, true, NULL, &w, &h)) { ink_app_free(pixels); return -1; }
    *result = (book_cover_result_t){.pixels = pixels, .width = w, .height = h, .size = st.st_size};
    return 1;
}
/* Decoding is ours (ink_image_decode); Mogo's sampling to the size asked for stays its own idea: nearest pixel. */
bool book_image_grayscale(const uint8_t *data, size_t size, bool png, unsigned w, unsigned h, uint8_t *out) {
    (void)png;
    ink_image_t image = {0};
    if (!w || !h || ink_image_decode(data, size, &image)) return false;
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++)
            out[(size_t)y * w + x] = image.pixels[(size_t)(y * image.height / h) * image.width + x * image.width / w];
    ink_image_free(&image);
    return true;
}

/* Text: every size through ink_mogo_scaled (the system text size); Mogo's own fitting decides what fits. */
int ink_mogo_lock_width(int px, const char *text) { return ttf_text_width_px(ink_mogo_scaled(px), text); }
static void put_text(uint8_t *fb, int x, int y, bool center, int px, const char *text, enum EpdFontFlags align, bool inverted) {
    int above = 0, below = 0;
    ttf_measure_line_px(px, text, &above, &below);
    ttf_draw_text_px(fb, x, center ? y + (above - below) / 2 : y + above, px, text, align, inverted ? 15 : 0, inverted ? 0 : 15);
}
void ink_mogo_lock_text(uint8_t *fb, int x, int y, int px, const char *t, enum EpdFontFlags a, bool inv) { put_text(fb, x, y, false, ui_text_effective_px(px), t, a, inv); }
void ink_mogo_lock_text_vc(uint8_t *fb, int x, int y, int px, const char *t, enum EpdFontFlags a, bool inv) { put_text(fb, x, y, true, ui_text_effective_px(px), t, a, inv); }
void ink_mogo_lock_text_fixed(uint8_t *fb, int x, int y, int px, const char *t, enum EpdFontFlags a, bool inv) { put_text(fb, x, y, false, ink_mogo_scaled(px), t, a, inv); }
void ink_mogo_lock_text_fixed_vc(uint8_t *fb, int x, int y, int px, const char *t, enum EpdFontFlags a, bool inv) { put_text(fb, x, y, true, ink_mogo_scaled(px), t, a, inv); }
void ink_mogo_lock_title_vc(uint8_t *fb, int x, int y, int px, const char *t, const char *sample, enum EpdFontFlags a) {
    (void)sample; put_text(fb, x, y, true, px, t, a, false); /* px comes fitted from ui_text_effective_px and ui_text_title_fit */
}
/* The built-in UI face of book_cover_auto.c: the card font's coverage, mixed into the template's 4-bit canvas. */
bool ui_font_has_text(const char *text) { (void)text; return ttf_font_ready(); }
int ui_font_text_width_px(int px, const char *text) { return ttf_text_width_px(px, text); }
int ui_font_ascender_px(int px) { return ttf_ascender_px(px); }
void ui_font_draw_text_px(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, uint8_t fg, uint8_t bg, bool bold) {
    (void)bold;
    int above = 0, below = 0, w = ttf_text_width_px(px, text);
    ttf_measure_line_px(px, text, &above, &below);
    unsigned mw = (unsigned)w + 4, mh = (unsigned)(above + below) + 4, stride = (unsigned)epd_width() / 2;
    uint8_t *mask = w > 0 ? ink_mogo_calloc(mw, mh) : NULL;
    if (!mask) return;
    if (ttf_text_mask_px(mask, mw, mh, 2, 2 + above, px, text)) {
        int left = (align == EPD_DRAW_ALIGN_CENTER ? x - w / 2 : align == EPD_DRAW_ALIGN_RIGHT ? x - w : x) - 2, top = y - above - 2;
        for (unsigned j = 0; j < mh; j++)
            for (unsigned i = 0; i < mw; i++) {
                int px_x = left + (int)i, px_y = top + (int)j, a = mask[j * mw + i];
                if (!a || px_x < 0 || px_y < 0 || px_x >= epd_width() || px_y >= epd_height()) continue;
                uint8_t g = (uint8_t)((bg * (255 - a) + fg * a + 127) / 255), *at = fb + (size_t)px_y * stride + (unsigned)px_x / 2;
                *at = px_x & 1 ? (uint8_t)((*at & 0x0f) | (g << 4)) : (uint8_t)((*at & 0xf0) | (g & 15));
            }
    }
    ink_app_free(mask);
}

/* Rules, the portrait panel and the battery. */
void epd_draw_rect(EpdRect r, uint8_t gray, uint8_t *fb) {
    epd_fill_rect((EpdRect){r.x, r.y, r.width, 1}, gray, fb); epd_fill_rect((EpdRect){r.x, r.y + r.height - 1, r.width, 1}, gray, fb);
    epd_fill_rect((EpdRect){r.x, r.y, 1, r.height}, gray, fb); epd_fill_rect((EpdRect){r.x + r.width - 1, r.y, 1, r.height}, gray, fb);
}
enum EpdRotation epd_get_rotation(void) { return EPD_ROT_LANDSCAPE; }
int epd_width(void) { return INK_LOCK_CANVAS_W; }
int epd_height(void) { return INK_LOCK_CANVAS_H; }
int epd_rotated_display_width(void) { return INK_LOCK_CANVAS_W; }
int epd_rotated_display_height(void) { return INK_LOCK_CANVAS_H; }
uint8_t ui_contrast_gray(uint8_t gray) {
    int g = 128 + ((int)gray - 128) * app_settings_system_contrast() / 100;
    return (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
}
void ui_hairline(uint8_t *fb, int y, int x, int w, uint8_t color) { epd_fill_rect((EpdRect){x, y, w, 1}, color, fb); }
void ui_draw_separator(uint8_t *fb, int y, int x, int width, uint8_t color) { epd_fill_rect((EpdRect){x, y, width, 1}, color, fb); }
/* Mogo's Lucide masks are not here: the open book is drawn as two pages, the shuffle arrow not at all (no shuffle). */
void ui_draw_icon(uint8_t *fb, int cx, int cy, int size, ui_icon_t icon, uint8_t gray) {
    if (icon != UI_ICON_BOOK_OPEN) return;
    int h = size * 2 / 3, w = size / 2;
    epd_draw_rect((EpdRect){cx - w, cy - h / 2, w, h}, gray, fb); epd_draw_rect((EpdRect){cx, cy - h / 2, w, h}, gray, fb);
}
void ui_nav_battery_draw(uint8_t *fb, int x, int cy) {
    int64_t level = -1, charging = 0;
    int h = ink_state_find("battery.level");
    if (h < 0 || !ink_state_get_int(h, &level)) level = -1;
    h = ink_state_find("battery.charging");
    if (h >= 0) (void)ink_state_get_int(h, &charging);
    EpdRect body = {x, cy - 10, 36, 20};
    epd_draw_rect(body, 0, fb); epd_draw_rect(ui_inset_rect(body, 1), 0, fb);
    epd_fill_rect((EpdRect){x + 36, cy - 4, 3, 8}, 0, fb);
    if (level > 0) epd_fill_rect((EpdRect){x + 4, cy - 6, (int)(28 * (level > 100 ? 100 : level) / 100), 12}, 0, fb);
    char s[16];
    if (level >= 0) snprintf(s, sizeof s, charging ? "%d%% +" : "%d%%", (int)level);
    else snprintf(s, sizeof s, "--");
    ink_mogo_lock_text_fixed_vc(fb, x + 48, cy, 20, s, EPD_DRAW_ALIGN_LEFT, false);
}
