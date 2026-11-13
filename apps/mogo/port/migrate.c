/* The old reader's data, moved into Mogo's records for each member (D-138). The old files are only read, so an older
 * firmware still finds them; a marker with their fingerprint makes it run again after a restored backup. */
#include <page.h>
#include <nvs.h>
#include <book_source.h>
#include <book_progress.h>
#include <book_bookmark_format.h>
#include <settings.h>
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/probe.h"

/* The old reader's definition, unchanged but for the title, so restoring an older backup brings its marks back. */
static const ink_db_def_t INK_MOGO_OLD_MARKS = {.name = "reader.bookmarks", .title = "旧阅读书签（迁移用）", .scope = INK_DB_MEMBER, .schema = 1, .json = true};
int ink_mogo_migrate_init(void) { return ink_db_register(&INK_MOGO_OLD_MARKS); }

typedef struct { uint32_t last_crc, marks_ver, marks_count, layout_crc, pos_found; } ink_mogo_print_t;
/* The old key (crc32 names the .pos file), the old physical path (bookmarks) and the new path, for one book. */
typedef struct { char key[96], old[200], path[288]; } ink_mogo_book_t;
static struct {
    char member[16], last_key[96], text[1024], json_text[2048], layout[160], inbox[160], name[128], pos[96], scratch[288], files[96];
    ink_mogo_book_t book, other;
    ink_mogo_card_t card;
    uint32_t last_offset, epoch;
    int next, done, failed;
    ink_mogo_print_t print;
    ink_db_t *marks;
    ink_json_t json;
    html_text_t chapter;
    size_t chapter_no;
} R INK_PSRAM_BSS;

static int ink_mogo_old_read(const char *member, const char *name, char *out, size_t cap) {
    char path[160];
    if (ink_layout_member(path, sizeof path, member, name) < 0) return -1;
    int n = ink_storage_read(path, out, cap - 1);
    out[n > 0 ? n : 0] = 0;
    return n;
}
static int ink_mogo_old_number(const char *text, const char *key) { const char *p = strstr(text, key); return p ? atoi(p + strlen(key)) : -1; }
/* The books the old reader listed, in its folders on the card (read like it did: no file index scan). */
static bool ink_mogo_book_at(int index, ink_mogo_book_t *b) {
    char *inbox = R.inbox, *name = R.name; bool dir; int seen = 0;
    if (ink_layout_member_files(R.files, sizeof R.files, R.member) < 0) return false;
    snprintf(inbox, sizeof R.inbox, "%s/%s", R.files, TR_NOOP("收件箱"));
    for (int r = 0; r < 2; r++) {
        ink_dir_t *d = ink_dir_open(r ? inbox : "/content/books");
        while (d && ink_dir_next(d, name, sizeof R.name, &dir) == 1) {
            const char *dot = strrchr(name, '.');
            if (dir || !dot || (strcmp(dot, ".txt") && strcmp(dot, ".epub")) || seen++ != index) continue;
            if (r) snprintf(b->key, sizeof b->key, "inbox/%.89s", name);
            else snprintf(b->key, sizeof b->key, "%.*s", strcmp(dot, ".epub") ? (int)(dot - name) : (int)strlen(name), name);
            snprintf(b->old, sizeof b->old, "%s/%s", r ? inbox : "/content/books", name);
            if (r) snprintf(b->path, sizeof b->path, "/me/%s/%s", TR_NOOP("收件箱"), name);
            else snprintf(b->path, sizeof b->path, "/family/books/%s", name);
            ink_dir_close(d);
            return true;
        }
        ink_dir_close(d);
    }
    if (seen != index) return false;
    snprintf(b->key, sizeof b->key, "%s", TR_NOOP("夜邮（示例）"));
    snprintf(b->old, sizeof b->old, "sample/welcome.txt");
    snprintf(b->path, sizeof b->path, "asset:sample/welcome.txt");
    return true;
}
static int ink_mogo_pos(const char *key, char *out, size_t cap) {
    char name[32];
    snprintf(name, sizeof name, "reader.%08x.pos", (unsigned)ink_crc32(0, key, strlen(key)));
    return ink_mogo_old_read(R.member, name, out, cap);
}
static bool ink_mogo_chapter(size_t c) {
    if (R.chapter.utf8 && R.chapter_no == c) return true;
    html_text_free(&R.chapter); R.chapter_no = c;
    return book_chapter_load_blocks(c, &R.chapter) == ESP_OK;
}
/* Old position -> Mogo's (chapter, UTF-8 offset): TXT from the byte offset in the file, EPUB from the block. */
static bool ink_mogo_place_old(bool epub, uint32_t chapter, int64_t block, uint64_t offset, const char *excerpt, uint16_t *ch, uint32_t *off) {
    size_t c = 0, pos;
    if (epub) {
        if (chapter >= book_chapter_count() || !ink_mogo_chapter(chapter)) return false;
        c = chapter;
        if (block >= 0 && R.chapter.blocks && R.chapter.count) {
            const blk_t *b = &R.chapter.blocks[(size_t)block < R.chapter.count ? (size_t)block : R.chapter.count - 1];
            pos = b->offset + (offset < b->len ? offset : b->len);
        } else pos = offset;
    } else {
        while (c + 1 < book_chapter_count() && book_chapter_byte_offset(c + 1) <= offset) c++;
        uint32_t start = book_chapter_byte_offset(c), end = c + 1 < book_chapter_count() ? book_chapter_byte_offset(c + 1) : book_total_bytes();
        if (!ink_mogo_chapter(c)) return false;
        uint64_t delta = offset > start ? offset - start : 0;
        pos = R.chapter.len == end - start ? delta : end > start ? delta * R.chapter.len / (end - start) : 0;
    }
    if (pos > R.chapter.len) pos = R.chapter.len;
    size_t n = excerpt ? strcspn(excerpt, "\n") : 0;
    if (n > 30) n = 30;
    while (n && ((unsigned char)excerpt[n] & 0xc0) == 0x80) n--;
    for (size_t at = pos > 4096 ? pos - 4096 : 0; n && at + n <= R.chapter.len && at <= pos + 4096; at++)
        if (!memcmp(R.chapter.utf8 + at, excerpt, n)) { pos = at; break; }
    while (pos && pos < R.chapter.len && ((unsigned char)R.chapter.utf8[pos] & 0xc0) == 0x80) pos--;
    *ch = (uint16_t)c; *off = (uint32_t)pos;
    return true;
}
static void ink_mogo_mark_key(const char *path, char key[11]) {
    uint32_t hash = UINT32_C(2166136261);
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) hash = (hash ^ *p) * UINT32_C(16777619);
    snprintf(key, 11, "m_%08lx", (unsigned long)hash);
}
static int ink_mogo_by_time(const void *a, const void *b) {
    uint32_t x = ((const reader_bookmark_entry_t *)a)->saved_s, y = ((const reader_bookmark_entry_t *)b)->saved_s;
    return x < y ? -1 : x > y;
}
/* Merge one book's old marks into rp_marks: (chapter, offset) once, the newest BOOKMARK_MAX by time. */
static bool ink_mogo_marks(const ink_mogo_book_t *b, bool epub, uint32_t size) {
    struct { reader_bookmarks_t marks; reader_bookmark_entry_t all[BOOKMARK_MAX + 64]; } *m = ink_app_alloc(sizeof *m, true);
    reader_bookmarks_t *marks = m ? &m->marks : NULL;
    nvs_handle_t h; char key[11]; size_t n = sizeof *marks; int added = 0;
    if (!m || nvs_open("rp_marks", NVS_READWRITE, &h)) { ink_app_free(m); return false; }
    ink_mogo_mark_key(b->path, key);
    if (nvs_get_blob(h, key, marks, &n) || n != sizeof *marks || marks->magic != UINT32_C(0x52504d32) || marks->file_size != size || strcmp(marks->path, b->path))
        *marks = (reader_bookmarks_t){.magic = UINT32_C(0x52504d32), .file_size = size};
    snprintf(marks->path, sizeof marks->path, "%s", b->path);
    reader_bookmark_entry_t *all = m->all;
    ink_db_cursor_t cursor = {0}; ink_rec_t rec; int count = marks->count <= BOOKMARK_MAX ? marks->count : 0;
    memcpy(all, marks->entries, (size_t)count * sizeof *all);
    while (R.marks && count < BOOKMARK_MAX + 64 && ink_db_next(R.marks, &cursor, &rec, R.json_text, sizeof R.json_text - 1) == 1) {
        int64_t v[4]; char excerpt[64] = ""; uint16_t ch; uint32_t off;
        if (rec.len >= sizeof R.json_text || ink_json_parse(&R.json, R.json_text, rec.len) ||
            ink_json_str(&R.json, ink_json_find(&R.json, 0, "book"), R.scratch, sizeof R.scratch) < 0 || strcmp(R.scratch, b->old)) continue;
        static const char *const fields[] = {"chapter", "block", "offset", "added_at"};
        bool ok = true;
        for (int i = 0; i < 4; i++) ok = ok && ink_json_int(&R.json, ink_json_find(&R.json, 0, fields[i]), &v[i]);
        ink_json_str(&R.json, ink_json_find(&R.json, 0, "excerpt"), excerpt, sizeof excerpt);
        if (!ok || !ink_mogo_place_old(epub, (uint32_t)v[0], epub ? v[1] : -1, (uint64_t)v[2], excerpt, &ch, &off)) { ink_probe_count("reader.migration_skipped"); continue; }
        bool dup = false;
        for (int i = 0; i < count; i++) dup = dup || (all[i].chapter == ch && all[i].byte_off == off);
        if (!dup) { all[count++] = (reader_bookmark_entry_t){.chapter = ch, .byte_off = off, .saved_s = (uint32_t)v[3]}; added++; }
    }
    ink_mogo_qsort(all, (size_t)count, sizeof *all, ink_mogo_by_time);
    for (int i = BOOKMARK_MAX; i < count; i++) ink_probe_count("reader.migration_skipped");
    marks->count = (uint16_t)(count > BOOKMARK_MAX ? BOOKMARK_MAX : count);
    memcpy(marks->entries, all + count - marks->count, marks->count * sizeof *all);
    esp_err_t err = added && marks->count ? nvs_set_blob(h, key, marks, sizeof *marks) : ESP_OK;
    if (!err && added) err = nvs_commit(h);
    for (int i = 0; !err && i < added; i++) ink_probe_count("reader.migrated_bookmarks");
    nvs_close(h); ink_app_free(m);
    return !err;
}
/* One book: its saved position (unless Mogo has one) and its marks; the card and "last" when it was read last. */
static bool ink_mogo_book(const ink_mogo_book_t *b) {
    char *pos = R.pos; bool last = !strcmp(b->key, R.last_key);
    int found = ink_mogo_pos(b->key, pos, sizeof R.pos);
    struct stat st;
    if (stat(b->path, &st) || book_open(b->path) != ESP_OK) return true;
    bool epub = book_kind() == BOOK_KIND_EPUB, ok = true;
    uint32_t size = (uint32_t)st.st_size;
    book_progress_t p; uint16_t ch; uint32_t off;
    uint64_t offset = found > 0 ? (uint64_t)ink_mogo_old_number(pos, "offset=") : last ? R.last_offset : 0;
    int chapter = found > 0 ? ink_mogo_old_number(pos, "chapter=") : last ? ink_mogo_old_number(R.text, "chapter=") : 0;
    if ((found > 0 || last) && !book_progress_load(b->path, size, &p) && ink_mogo_place_old(epub, (uint32_t)(chapter < 0 ? 0 : chapter), -1, offset, NULL, &ch, &off)) {
        uint32_t start = book_chapter_byte_offset(ch), end = ch + 1u < book_chapter_count() ? book_chapter_byte_offset(ch + 1u) : book_total_bytes();
        uint32_t total = book_total_bytes(), at = start + (R.chapter.len ? (uint32_t)((uint64_t)(end - start) * off / R.chapter.len) : 0);
        p = (book_progress_t){.file_size = size, .chapter = ch, .byte_off = off, .px = 48, .pct = (uint8_t)(total ? (uint64_t)at * 100 / total : 0)};
        ok = book_progress_save(b->path, &p) == ESP_OK;
        if (ok) ink_probe_count("reader.migrated_progress");
        nvs_handle_t h; ink_mogo_card_t *card = &R.card; size_t n = 0;
        if (ok && last && !nvs_open("ink_card", NVS_READWRITE, &h)) {
            *card = (ink_mogo_card_t){.chapter_no = ch, .pct = p.pct};
            snprintf(card->path, sizeof card->path, "%s", b->path); ink_mogo_book_title(b->path, card->title, sizeof card->title);
            if (book_chapter_title(ch, card->chapter, sizeof card->chapter) != ESP_OK) card->chapter[0] = 0;
            if (nvs_get_blob(h, "last", NULL, &n)) ok = !nvs_set_blob(h, "last", card, sizeof *card) && !nvs_commit(h);
            nvs_close(h);
        }
    }
    if (ok && last && !book_progress_last_path(R.scratch, sizeof R.scratch)) ok = book_progress_set_last_path(b->path) == ESP_OK;
    ok = ink_mogo_marks(b, epub, size) && ok;
    html_text_free(&R.chapter); book_close();
    return ok;
}
/* Per-member layout keys in the settings file, kept there by the settings service after the old reader left. */
static void ink_mogo_layout_read(void) {
    static const char *const keys[] = {"font_size", "line_spacing", "paragraph_spacing", "margins", "footer_mask"};
    char path[64], *text = ink_app_alloc(2048, true); int n = -1, len = 0;
    R.layout[0] = 0;
    if (text && ink_layout_sys(path, sizeof path, "settings.json") >= 0) n = ink_storage_read(path, text, 2047);
    if (n <= 0) { ink_app_free(text); return; }
    text[n] = 0;
    for (int i = 0; i < 5; i++) {
        char key[64]; snprintf(key, sizeof key, "\"reader.%s:%s\":", keys[i], R.member);
        const char *at = strstr(text, key);
        if (at) len += snprintf(R.layout + len, sizeof R.layout - (size_t)len, "%d=%d;", i, atoi(at + strlen(key)));
    }
    ink_app_free(text);
}
static bool ink_mogo_layout_old(void) {
    static const int px[] = {40, 48, 56, 72}, line[] = {110, 120, 135, 150}, gap[] = {25, 50, 75, 100}, margin[] = {16, 24, 40, 56};
    int v[5] = {1, 1, 1, 1, 3}; bool changed = false;
    for (int i = 0; i < 5; i++) {
        char key[8]; snprintf(key, sizeof key, "%d=", i);
        const char *at = strstr(R.layout, key);
        if (at) v[i] = atoi(at + 2);
        changed = changed || (i == 4 ? v[i] != 3 : v[i] != 1);
    }
    nvs_handle_t h; size_t size = 0;
    if (!changed || nvs_open("reading", NVS_READWRITE, &h)) return !changed;
    bool ok = true;
    if (nvs_get_blob(h, "layout", NULL, &size)) {
        ink_mogo_settings_t s = {.px = 48, .line = 150, .paragraph = 50, .margin = {36, 36, 0, 0}, .indent = 2};
        if (v[0] != 1 && v[0] >= 0 && v[0] < 4) s.px = (uint8_t)px[v[0]];
        if (v[1] != 1 && v[1] >= 0 && v[1] < 4) s.line = (uint8_t)line[v[1]];
        if (v[2] != 1 && v[2] >= 0 && v[2] < 4) s.paragraph = (uint8_t)gap[v[2]];
        if (v[3] != 1 && v[3] >= 0 && v[3] < 4) s.margin[0] = s.margin[1] = (uint8_t)margin[v[3]];
        s.footer = v[4] != 0;
        ok = !nvs_set_blob(h, "layout", &s, sizeof s) && !nvs_commit(h);
    }
    nvs_close(h);
    return ok;
}
static bool ink_mogo_marker(bool write) {
    nvs_handle_t h; ink_mogo_print_t old; size_t n = sizeof old; bool same = false;
    if (nvs_open("ink_migr", write ? NVS_READWRITE : NVS_READONLY, &h)) return false;
    if (write) same = !nvs_set_blob(h, "reader1", &R.print, sizeof R.print) && !nvs_commit(h);
    else same = !nvs_get_blob(h, "reader1", &old, &n) && n == sizeof old && !memcmp(&old, &R.print, sizeof old);
    nvs_close(h);
    return same;
}
/* Start a run: read reader.last, open the old marks, take the fingerprint; false when the marker already has it. */
static bool ink_mogo_begin(const ink_member_identity_t *owner) {
    ink_db_close(R.marks);
    memset(&R.print, 0, sizeof R.print);
    snprintf(R.member, sizeof R.member, "%s", owner->member); R.epoch = owner->epoch; R.next = R.done = R.failed = 0;
    int n = ink_mogo_old_read(R.member, "reader.last", R.text, sizeof R.text);
    R.print.last_crc = n > 0 ? ink_crc32(0, R.text, (size_t)n) : 0;
    const char *file = strstr(R.text, "\nfile=");
    R.last_key[0] = 0;
    if (file) sscanf(file, "\nfile=%95[^\n]", R.last_key);
    R.last_offset = n > 0 ? (uint32_t)ink_mogo_old_number(R.text, "\noffset=") : 0;
    R.marks = ink_db_open_member(&INK_MOGO_OLD_MARKS, R.member);
    if (R.marks) { R.print.marks_ver = ink_db_last_ver(R.marks); R.print.marks_count = (uint32_t)ink_db_count(R.marks); }
    ink_mogo_layout_read();
    R.print.layout_crc = R.layout[0] ? ink_crc32(0, R.layout, strlen(R.layout)) : 0;
    if (!R.print.last_crc && !R.print.marks_count && !R.print.layout_crc) return false; /* nothing of the old reader: not even the card is read */
    for (int i = 0; ink_mogo_book_at(i, &R.book); i++) R.print.pos_found += ink_mogo_pos(R.book.key, R.pos, sizeof R.pos) > 0;
    return !ink_mogo_marker(false);
}
int ink_mogo_migrate_step(const ink_member_identity_t *owner, const char *only) {
    int64_t card = 0;
    ink_mogo_owner(owner);
    if (!ink_state_get_int(ink_state_find("storage.card"), &card) || !card) return -1; /* the card is out: not "the book is gone" */
    if ((only || strcmp(R.member, owner->member) || R.epoch != owner->epoch || !R.next) && !ink_mogo_begin(owner)) return 0;
    ink_mogo_book_t *b = &R.book, *other = &R.other;
    if (only) { /* this session's book and the last one now; the rest and the marker in the background run */
        for (int i = 0; ink_mogo_book_at(i, b); i++)
            if (!strcmp(b->key, R.last_key) || !strcmp(b->path, only)) (void)ink_mogo_book(b);
        R.next = 0;
        return 0;
    }
    if (ink_mogo_book_at(R.next, b)) {
        R.failed += !ink_mogo_book(b);
        R.next++;
        return 1;
    }
    if (R.last_key[0]) { /* the book read last is gone */
        bool there = false;
        for (int i = 0; ink_mogo_book_at(i, other); i++) there = there || !strcmp(other->key, R.last_key);
        if (!there) ink_probe_count("reader.migration_skipped");
    }
    ink_db_cursor_t c = {0}; ink_rec_t rec; /* marks whose book is gone */
    while (R.marks && ink_db_next(R.marks, &c, &rec, R.json_text, sizeof R.json_text - 1) == 1) {
        bool there = false;
        if (rec.len >= sizeof R.json_text || ink_json_parse(&R.json, R.json_text, rec.len) || ink_json_str(&R.json, ink_json_find(&R.json, 0, "book"), R.scratch, sizeof R.scratch) < 0) continue;
        for (int i = 0; !there && ink_mogo_book_at(i, other); i++) there = !strcmp(other->old, R.scratch);
        if (!there) ink_probe_count("reader.migration_skipped");
    }
    R.failed += !ink_mogo_layout_old();
    bool ok = !R.failed && ink_mogo_marker(true);
    ink_db_close(R.marks); R.marks = NULL; R.next = 0;
    return ok ? 0 : -1;
}
