/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：完整播放页（二级页，仅 metalio47）。曲名与进度、上一首/播放/下一首、
 * 音量、单行歌词（歌名下方）、播放列表与翻页、行内收藏，以及全屏
 * 「全部歌曲」列表（曲库有子文件夹时按目录层级浏览，没有则平铺收藏优先；
 * 按键或按钮翻页）。文字随系统字号按比例缩放（各元素保持
 * 自身基准字号，不统一放大）。播放本身由 music_player 后台任务负责，本页只读
 * 状态、发指令；切走后音乐继续。
 *
 * English: the full player (second-level page, metalio47 only). Title and progress,
 * prev/play/next, volume, one lyric line under the title, playlist
 * and paging, per-row favorites, and the full-screen "all tracks" list (favorites
 * first, paged by keys). Text scales proportionally with the system font size (each
 * element keeps its own base; nothing is flattened to one size). music_player owns
 * playback in the background; this page only reads state and issues commands, and
 * music keeps playing after navigating away.
 *
 * 冻结：不做拖动式进度（2026-10-29 用户要求点按进度条跳转，已完成；scrub 仍不提供）、
 * 不做随机播放、不做内嵌封面；耳机只做基础控制；蓝牙连接/扫描/配对管理只在设置页
 * （2026-10-29 用户确认播放页与设置页重复，播放页不再提供蓝牙界面）。
 * Frozen: no scrubbing (the user asked for tap-to-seek on 2026-10-29, done; drag scrub
 * stays out), no shuffle, no embedded cover art; the headset gets basic controls only;
 * Bluetooth connect/scan/pairing management lives only in Settings (the user confirmed
 * on 2026-10-29 that the player duplicated it, so the player no longer shows Bluetooth).
 */
/* ink adaptation 2026-11-13 (music-3): layout from theme tokens with 108 px targets, scaled with the system
 * text size; no frames, circles, tab bar, back arrow or press inversion; taps resolve to the registered
 * capabilities AI uses; the relink capsule is left out. Every change is listed in SOURCE.md. */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app.h"
#include "app_registry.h"
#include "bt_host.h"
#include "esp_log.h"
#include "music_lyrics.h"
#include "music_player.h"
#include "settings.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_nav.h"

static const char *TAG = "app_music";

#define MUSIC_ROWS (L.rows)               // 播放列表每页行数（ink：放得下几行就几行）/ playlist rows per page
#define FULL_ROWS (L.full_rows)           // 全部歌曲每页行数（不滚动，按键翻页）/ full-list rows per page (paged, never scrolled)
// 内容脏区：顶部模式按钮到翻页键（模式按钮在曲名之上，也要一起刷）。
// Content dirty span: from the top mode button through the pager (the mode button sits above
// the title and must be repainted with it).
#define MUSIC_DIRTY (L.content)
#define MODE_RECT (L.mode)                // 播放模式胶囊：列表/单曲/随机/收藏 / play-mode capsule
#define OUTPUT_RECT (L.output)            // 输出通路胶囊：耳机/本机扬声器 / output route capsule
#define FULL_DIRTY (L.content)            // 全屏列表脏区：标题到翻页键 / full-list dirty span: title to the pager
#define LYRIC_DIRTY (L.lyric)             // 歌词行：换句只刷这一行，进度条走自己的窄条 / the lyric line only: a new line refreshes just it, the bar has its own sliver

static int s_page;                        // 播放列表页 / playlist page
static int s_seen_track = -1;             // 已跟随的曲目索引 / last track index we paged to
static char s_message[96];
static bool s_show_all;                   // 全屏「全部歌曲」视图 / the full-screen "all tracks" view
static int s_full_page;                   // 全屏列表页 / full-list page
static int s_order[MUSIC_MAX_TRACKS] INK_PSRAM_BSS;  // 展示顺序：收藏优先（曲库下标）/ display order, favorites first (library indices)
static int s_order_count;
// on_tick 快照：当前歌词行与 music_serial。/ on_tick snapshot: the current lyric line plus music_serial.
static char s_lyric_seen[192 + 1] INK_PSRAM_BSS;
static uint32_t s_serial;
// 进度增量刷新：只推条上新增的一窄条与变化的时间标签，避免整块反复刷新。
// Incremental progress refresh: push only the new sliver of the bar and the changed time
// label instead of repainting a big block over and over.
static int s_bar_drawn;        // 最近一次绘制已达宽度 / filled width in the latest render
static int s_bar_shown;        // 屏上已达宽度 / filled width now on the panel
static bool s_tick_partial;    // 本轮按增量刷 / this round takes the incremental path

/* ink: one layout from theme tokens, scaled with the system text size (sy); every target is at least 108 px. */
typedef struct {
    EpdRect content, mode, output, all, title, artist, lyric, seek, bar, prev, play, next, minus, plus, volume,
        back, head, note, page_prev, page_next;
    int w, h, row0, full_row0, row_h, rows, full_rows, pager_y;
} music_layout_t;
static music_layout_t L;

static void fit(char *text, int px, int width) {
    while (*text && ui_text_fixed_width_px(px, text) > width) {
        size_t n = strlen(text) - 1;
        while (n && ((uint8_t)text[n] & 0xc0) == 0x80) --n;
        text[n] = 0;
    }
}
// 系统字号按比例换算：每个文本按自己的基准字号缩放，不统一放大到同一档。
// Scale text by the system font ratio: every element keeps its own base size instead
// of being flattened to one size.
static int sy(int px) { return (px * app_settings_system_font_size() + 50) / 100; }
static void mmss(char *dst, size_t cap, uint32_t ms) {
    uint32_t s = ms / 1000;
    snprintf(dst, cap, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}
static bool hit(EpdRect r, int x, int y) {
    return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
static int page_count(void) {
    int count = music_track_count();
    return (count + MUSIC_ROWS - 1) / MUSIC_ROWS;
}
// 收藏优先排序：先收藏组、后其余，组内保持曲库顺序。每次绘制重建（收藏位在 RAM 缓存）。
// Favorites-first order: favorites, then the rest, each group in library order. Rebuilt per
// frame (the favorite bits live in a RAM cache, so this never touches NVS).
static void rebuild_order(void) {
    const int count = music_track_count();
    s_order_count = 0;
    for (int fav = 1; fav >= 0; --fav)
        for (int i = 0; i < count && s_order_count < MUSIC_MAX_TRACKS; ++i)
            if ((music_favorite(i) ? 1 : 0) == fav) s_order[s_order_count++] = i;
}
static int full_page_count(void) {
    int pages = (s_order_count + FULL_ROWS - 1) / FULL_ROWS;
    return pages < 1 ? 1 : pages;
}

/* ---- 曲库子文件夹：层级浏览 / Sub-folders: hierarchy browsing ---- */
#define DIR_MAX 40        // 每层识别的子文件夹上限 / sub-folders recognized per level
#define DIR_NAME_MAX 72   // 文件夹显示名长度 / folder display-name length

static bool s_browse;              // 曲库有子文件夹 → 按层级浏览 / hierarchy mode: sub-folders exist
static char s_dir[160] INK_PSRAM_BSS;  // 当前目录（相对 music 根，"" 为根）/ current folder relative to the root
static int s_entry_page;           // 层级模式页码 / page in hierarchy mode
static int s_level_dirs;           // 本层子文件夹数 / sub-folders at this level
static int s_dir_first[DIR_MAX] INK_PSRAM_BSS;  // 每个子文件夹的代表曲目 / a representative track per sub-folder
// s_order / s_order_count 在层级模式下复用为「本层曲目清单（收藏优先）」。
// In hierarchy mode s_order/s_order_count hold this level's tracks (favorites first).

// 曲目所在目录（相对曲库根）："" 是根，其余形如 "周杰伦" 或 "周杰伦/叶惠美"。
// The track's folder relative to the library root: "" is the root, else like "Artist/Album".
static void track_dir(int idx, char *out, size_t cap) {
    out[0] = 0;
    const music_track_t *t = music_track(idx);
    if (!t) return;
    const char *root = music_root_of(t->path);
    const char *p = t->path;
    const size_t n = root ? strlen(root) : 0;
    if (n && strncmp(p, root, n) == 0) {
        p += n;
        while (*p == '/') ++p;
    }
    const char *slash = strrchr(p, '/');
    if (!slash) return;
    size_t len = (size_t)(slash - p);
    if (len >= cap) len = cap - 1;
    memcpy(out, p, len);
    out[len] = 0;
}
// 当前层下面的一级目录名（曲目在更深层时只取第一段）。/ The first component below the current folder.
static void child_name(int idx, char *out, size_t cap) {
    out[0] = 0;
    char dir[256];
    track_dir(idx, dir, sizeof(dir));
    const size_t n = strlen(s_dir);
    if (n) {
        if (strncmp(dir, s_dir, n) != 0 || dir[n] != '/') return;
    } else if (!dir[0]) {
        return;  // 根目录下的散曲 / a loose track at the root
    }
    const char *rest = dir + (n ? n + 1 : 0);
    if (!*rest) return;  // 正好是本层 / exactly this folder
    const char *slash = strchr(rest, '/');
    size_t len = slash ? (size_t)(slash - rest) : strlen(rest);
    if (len >= cap) len = cap - 1;
    memcpy(out, rest, len);
    out[len] = 0;
}
// 曲目是否就在该目录里（不含其子目录）。/ Whether the track sits directly in that folder.
static bool dir_is(int idx, const char *dir) {
    char d[256];
    track_dir(idx, d, sizeof(d));
    return strcmp(d, dir) == 0;
}
// 曲目是否在该目录或其子目录下。/ Whether the track lives in that folder or below it.
static bool dir_holds(int idx, const char *dir) {
    char d[256];
    track_dir(idx, d, sizeof(d));
    if (!dir[0]) return true;
    const size_t n = strlen(dir);
    return strncmp(d, dir, n) == 0 && (d[n] == 0 || d[n] == '/');
}
static int dir_track_count(const char *dir) {
    int n = 0;
    for (int i = 0, c = music_track_count(); i < c; ++i)
        if (dir_holds(i, dir)) ++n;
    return n;
}
// 曲库里是否存在子文件夹：是则「全部歌曲」按层级显示，否则沿用原平铺列表。
// Whether the library has sub-folders: if so the all-tracks view goes hierarchical, else it
// keeps the original flat list.
static bool library_has_dirs(void) {
    for (int i = 0, c = music_track_count(); i < c; ++i) {
        char d[256];
        track_dir(i, d, sizeof(d));
        if (d[0]) return true;
    }
    return false;
}
// 重建当前层：子文件夹（按名排序）在前，本层曲目（收藏优先）在后。
// Rebuild this level: sub-folders (name-sorted) first, then this folder's tracks (favorites first).
static void level_build(void) {
    const int count = music_track_count();
    s_level_dirs = 0;
    s_order_count = 0;
    for (int i = 0; i < count; ++i) {
        char name[DIR_NAME_MAX];
        child_name(i, name, sizeof(name));
        if (!name[0]) continue;
        bool dup = false;
        for (int k = 0; k < s_level_dirs && !dup; ++k) {
            char other[DIR_NAME_MAX];
            child_name(s_dir_first[k], other, sizeof(other));
            if (strcmp(other, name) == 0) dup = true;
        }
        if (!dup && s_level_dirs < DIR_MAX) s_dir_first[s_level_dirs++] = i;
    }
    for (int a = 1; a < s_level_dirs; ++a) {  // 插入排序，每层至多几十项 / insertion sort, a few dozen at most
        const int key = s_dir_first[a];
        char key_name[DIR_NAME_MAX];
        child_name(key, key_name, sizeof(key_name));
        int b = a - 1;
        while (b >= 0) {
            char other[DIR_NAME_MAX];
            child_name(s_dir_first[b], other, sizeof(other));
            if (strcmp(other, key_name) <= 0) break;
            s_dir_first[b + 1] = s_dir_first[b];
            --b;
        }
        s_dir_first[b + 1] = key;
    }
    for (int fav = 1; fav >= 0; --fav)
        for (int i = 0; i < count && s_order_count < MUSIC_MAX_TRACKS; ++i)
            if (music_favorite(i) == (fav != 0) && dir_is(i, s_dir)) s_order[s_order_count++] = i;
}
static int level_total(void) { return s_level_dirs + s_order_count; }
static int browse_page_count(void) {
    int pages = (level_total() + FULL_ROWS - 1) / FULL_ROWS;
    return pages < 1 ? 1 : pages;
}
// 进入子文件夹 / 回到上一级（已在根则清空）。/ Enter a sub-folder / go one level up (already at the root).
static void dir_enter(const char *name) {
    if (!name[0]) return;
    char path[sizeof(s_dir)];
    if (s_dir[0]) snprintf(path, sizeof(path), "%s/%s", s_dir, name);
    else strlcpy(path, name, sizeof(path));
    strlcpy(s_dir, path, sizeof(s_dir));
    s_entry_page = 0;
}

/* ink: the layout for the current entry counts; a pager row only when there is more than one page. */
static int touch(void) { return sy(INK_CONTROL) > ink_ui_px(INK_CONTROL) ? sy(INK_CONTROL) : ink_ui_px(INK_CONTROL); }
static void layout(void) {
    const int m = ink_ui_px(INK_MARGIN), g = sy(INK_GAP_S), t = touch(), w = L.w - 2 * m, third = (w - 2 * g) / 3;
    int y = g;
    L.content = (EpdRect){0, 0, L.w, L.h};
    L.all = (EpdRect){m, y, third, t};
    L.output = (EpdRect){m + third + g, y, third, t};
    L.mode = (EpdRect){m + 2 * (third + g), y, w - 2 * (third + g), t};
    y += t + g;
    L.title = (EpdRect){m, y, w, sy(INK_LINE_TITLE)};
    y += L.title.height;
    L.artist = (EpdRect){m, y, w, sy(INK_LINE_NOTE)};
    y += L.artist.height;
    L.lyric = (EpdRect){0, y, L.w, sy(INK_LINE_BODY)};
    y += L.lyric.height;
    L.seek = (EpdRect){m, y, w, t};
    L.bar = (EpdRect){m, y + t / 3, w, sy(INK_SLIDER_FILL)};
    y += t;
    L.prev = (EpdRect){m, y, third, t};
    L.play = (EpdRect){m + third + g, y, third, t};
    L.next = (EpdRect){m + 2 * (third + g), y, w - 2 * (third + g), t};
    y += t + g;
    L.minus = (EpdRect){m, y, t, t};
    L.plus = (EpdRect){m + w - t, y, t, t};
    L.volume = (EpdRect){m + t, y, w - 2 * t, t};
    L.row_h = t;
    L.row0 = y + t + g;
    L.pager_y = L.h - g - t;
    L.page_prev = (EpdRect){m, L.pager_y, third, t};
    L.page_next = (EpdRect){m + 2 * (third + g), L.pager_y, w - 2 * (third + g), t};
    L.back = (EpdRect){m, g, third, t};
    L.head = (EpdRect){m + third + g, g, w - third - g, t};
    L.note = (EpdRect){m, g + t, w, sy(INK_LINE_NOTE)};
    L.full_row0 = g + t + L.note.height;
    const int fit_rows = (L.h - g - L.row0) / t, fit_full = (L.h - g - L.full_row0) / t;
    L.rows = music_track_count() > fit_rows ? (L.pager_y - L.row0) / t : fit_rows;
    L.full_rows = fit_full;
    if (s_show_all) {
        const int entries = s_browse ? level_total() : s_order_count;
        if (entries > fit_full) L.full_rows = (L.pager_y - L.full_row0) / t;
    }
    if (L.rows < 1) L.rows = 1;
    if (L.full_rows < 1) L.full_rows = 1;
}
static EpdRect row_rect(int i, bool full) { return (EpdRect){L.title.x, (full ? L.full_row0 : L.row0) + i * L.row_h, L.title.width, L.row_h}; }

typedef struct { EpdRect r; int radius; } music_hit_t;

// 当前视图的内容脏区。/ The active view's content dirty span.
static EpdRect current_dirty(void) {
    return s_show_all ? FULL_DIRTY : MUSIC_DIRTY;
}
// 只取当前这句歌词，换句即触发刷新。/ Only the current line; a new line redraws.
static void lyric_snapshot(char *dst, size_t cap) {
    snprintf(dst, cap, "%s", music_lyrics_at(music_elapsed_ms()));
}
// 把歌词快照同步（进出视图与动作后调用，避免 on_tick 紧接着重复刷）。
// Sync the lyric snapshot (called around view switches and actions so on_tick does not
// redraw right after).
static void sync_lyric(void) { lyric_snapshot(s_lyric_seen, sizeof(s_lyric_seen)); }

// 命中表：点按区与画出来的控件同一套矩形。/ Hit table: taps and drawn controls share one set of rectangles.
static int pages_shown(void) {
    if (!s_show_all) return page_count();
    return s_browse ? browse_page_count() : full_page_count();
}
static music_hit_t target_at(int x, int y) {
    const bool pager = pages_shown() > 1;
    if (s_show_all) {
        for (int i = 0; i < FULL_ROWS; ++i)
            if (hit(row_rect(i, true), x, y)) return (music_hit_t){row_rect(i, true), 0};
        if (hit(L.back, x, y)) return (music_hit_t){L.back, 0};
        if (pager && hit(L.page_prev, x, y)) return (music_hit_t){L.page_prev, 0};
        if (pager && hit(L.page_next, x, y)) return (music_hit_t){L.page_next, 0};
        return (music_hit_t){{0,0,0,0},0};
    }
    if (hit(MODE_RECT, x, y)) return (music_hit_t){MODE_RECT, 0};  // 播放模式 / play mode
    if (hit(OUTPUT_RECT, x, y)) return (music_hit_t){OUTPUT_RECT, 0};  // 输出通路 / output route
    if (hit(L.all, x, y)) return (music_hit_t){L.all, 0};  // 全部歌曲 / all tracks
    // 进度条连同两侧时间共用一个命中区：点按跳转。/ The bar and both times share one hit: tap to seek.
    if (hit(L.seek, x, y)) return (music_hit_t){L.seek, 0};
    if (hit(L.prev, x, y)) return (music_hit_t){L.prev, 0};
    if (hit(L.play, x, y)) return (music_hit_t){L.play, 0};
    if (hit(L.next, x, y)) return (music_hit_t){L.next, 0};
    if (hit(L.minus, x, y)) return (music_hit_t){L.minus, 0};
    if (hit(L.plus, x, y)) return (music_hit_t){L.plus, 0};
    for (int i = 0; i < MUSIC_ROWS; ++i)
        if (hit(row_rect(i, false), x, y)) return (music_hit_t){row_rect(i, false), 0};
    if (pager && hit(L.page_prev, x, y)) return (music_hit_t){L.page_prev, 0};
    if (pager && hit(L.page_next, x, y)) return (music_hit_t){L.page_next, 0};
    return (music_hit_t){{0,0,0,0},0};
}

// 动作后统一出口：同步快照避免 on_tick 紧接着重复刷一遍。/ Shared action exit: sync the snapshot so on_tick does not redraw again.
static EpdRect s_area;
static app_redraw_t dirty(void) {
    s_serial = music_serial();
    sync_lyric();
    s_area = current_dirty();
    return APP_REDRAW_AREA;
}

// 播放三角 / 暂停双竖杠（白色画在深色圆上或深色画在白圆上由 caller 定）。
// Play triangle / pause bars; the caller decides dark-on-light or light-on-dark.
static void play_glyph(uint8_t *fb, int cx, int cy, int height, int width, uint8_t color, bool playing) {
    int top = cy - height / 2;
    if (playing) {
        int bar = width / 4;
        ui_fill_rect_fast((EpdRect){cx - width / 2, top, bar, height}, color, fb);
        ui_fill_rect_fast((EpdRect){cx + width / 2 - bar, top, bar, height}, color, fb);
    } else {
        // 中空三角：只描左边竖线与两条斜边，中间留白。/ Hollow triangle: left edge plus both slants.
        const int stroke = height / 16 + 1;
        int x0 = cx - width / 2, half = height / 2;
        for (int i = 0; i < height; ++i) {
            int w = (i < half ? i : height - 1 - i) * width / half;
            if (w < 1) w = 1;
            if (w + 1 <= stroke * 2) {
                // 靠近尖端：两笔并成一行，避免出现断口。/ Near the tip the two strokes merge.
                ui_fill_rect_fast((EpdRect){x0, top + i, w + 1, 1}, color, fb);
            } else {
                ui_fill_rect_fast((EpdRect){x0, top + i, stroke, 1}, color, fb);
                ui_fill_rect_fast((EpdRect){x0 + w + 1 - stroke, top + i, stroke, 1}, color, fb);
            }
        }
    }
}

/* ink: one outlined control with a centred label (no triple frame); a disabled label is lighter. */
static void button(uint8_t *fb, EpdRect r, const char *text, bool enabled) {
    ui_draw_round_rect(fb, r, sy(INK_RADIUS_KEY), UI_GRAY_BLACK);
    ui_text_fixed_ink_vc(fb, r.x + r.width / 2, r.y + r.height / 2, sy(INK_FONT_BODY), text,
                         EPD_DRAW_ALIGN_CENTER, enabled ? UI_GRAY_BLACK : UI_GRAY_SEPARATOR);
}
static void row_line(uint8_t *fb, EpdRect row) {
    ui_fill_rect_fast((EpdRect){row.x, row.y + row.height - sy(INK_DIVIDER), row.width, sy(INK_DIVIDER)}, UI_GRAY_SEPARATOR, fb);
}
// 一行文件夹：图标 + 名字 + 曲目数；含当前播放曲目的文件夹用打开图标标出。
// One folder row: icon, name and track count; the folder holding the current track gets the
// open-folder icon.
static void folder_row(uint8_t *fb, EpdRect row, const char *name, int kids, bool holds, bool pressed) {
    (void)pressed;
    char text[DIR_NAME_MAX];
    row_line(fb, row);
    ui_draw_icon(fb, row.x + L.row_h / 2, row.y + row.height / 2, sy(INK_ICON_MEDIUM), holds ? UI_ICON_FOLDER_OPEN : UI_ICON_FOLDER,
                 UI_GRAY_BLACK);
    strlcpy(text, name, sizeof(text));
    fit(text, sy(INK_FONT_BODY), row.width - L.row_h - sy(INK_BLOCK_L));
    ui_text_fixed_vc(fb, row.x + L.row_h, row.y + row.height / 2, sy(INK_FONT_BODY), text, EPD_DRAW_ALIGN_LEFT, false);
    char t[32];
    snprintf(t, sizeof(t), TR("%d 首"), kids);
    ui_text_fixed_ink_vc(fb, row.x + row.width, row.y + row.height / 2, sy(INK_FONT_NOTE), t, EPD_DRAW_ALIGN_RIGHT, UI_GRAY_SECONDARY);
}
// 一行曲目：书签列切换收藏、时长靠右。/ One track row: bookmark column and duration on the right.
static void track_row(uint8_t *fb, EpdRect row, int idx, bool selected, bool pressed) {
    (void)pressed;
    const music_track_t *track = music_track(idx);
    if (!track) return;
    char text[192];
    row_line(fb, row);
    const bool fav = music_favorite(idx);
    ui_draw_icon(fb, row.x + L.row_h / 2, row.y + row.height / 2, sy(INK_ICON_MEDIUM), fav ? UI_ICON_BOOKMARK_CHECK : UI_ICON_BOOKMARK,
                 fav ? UI_GRAY_BLACK : UI_GRAY_SEPARATOR);
    strlcpy(text, track->title, sizeof(text));
    fit(text, sy(INK_FONT_BODY), row.width - L.row_h - sy(INK_BLOCK_L));
    ui_text_fixed_vc(fb, row.x + L.row_h, row.y + row.height / 2, sy(INK_FONT_BODY), text, EPD_DRAW_ALIGN_LEFT, false);
    // ink: the playing row says so in words instead of a black frame; a format we cannot play says that.
    const char *note = selected ? TR("正在放") : music_playable(track->path) ? NULL : TR("不能放");
    if (note || track->duration_ms) {
        char t[16];
        if (!note) mmss(t, sizeof(t), track->duration_ms);
        ui_text_fixed_ink_vc(fb, row.x + row.width, row.y + row.height / 2, sy(INK_FONT_NOTE), note ? note : t, EPD_DRAW_ALIGN_RIGHT,
                             note ? UI_GRAY_BLACK : UI_GRAY_SECONDARY);
    }
}
/* ink: the pager row is drawn only when there is a second page. */
static void pager(uint8_t *fb, int page, int pages) {
    char text[32];
    if (pages <= 1) return;
    button(fb, L.page_prev, TR("上一页"), page > 0);
    button(fb, L.page_next, TR("下一页"), page + 1 < pages);
    snprintf(text, sizeof(text), "%d / %d", page + 1, pages);
    ui_text_fixed_ink_vc(fb, L.w / 2, L.pager_y + L.row_h / 2, sy(INK_FONT_NOTE), text, EPD_DRAW_ALIGN_CENTER, UI_GRAY_SECONDARY);
}

// 全屏「全部歌曲」：曲库有子文件夹时按目录层级浏览（点文件夹进下一层、返回键回上一层），
// 没有子文件夹时沿用原平铺列表（收藏优先）。点书签列切换收藏、点行播放；不滚动，靠按键或
// 按钮翻页。
// Full-screen "all tracks": with sub-folders it browses the folder tree (tap a folder to descend,
// back to go up); a flat library keeps the original favorites-first list. Tap the bookmark column
// to toggle a favorite, the row to play; no scrolling — page with the keys or the on-screen
// buttons.
static void render_full_list(uint8_t *fb) {
    char text[192];
    const int total = music_track_count();
    int pages = 1;
    if (s_browse) {
        level_build();
        // 目录消失（换卡、改名）：回根，别停在一个空层。
        // Folder gone (new card, rename): back to the root instead of an empty level.
        if (s_dir[0] && level_total() == 0) {
            s_dir[0] = 0;
            s_entry_page = 0;
            level_build();
        }
        layout();
        pages = browse_page_count();
        if (s_entry_page >= pages) s_entry_page = pages - 1;
        if (s_entry_page < 0) s_entry_page = 0;
        if (s_dir[0]) {
            // ink: the back button left of the title goes up a level; the title is the folder.
            snprintf(text, sizeof(text), "%s", s_dir);
        } else {
            snprintf(text, sizeof(text), TR("全部歌曲 · %d 首"), total);
        }
        fit(text, sy(INK_FONT_BODY), L.head.width);
        ui_text_fixed_vc(fb, L.head.x, L.head.y + L.head.height / 2, sy(INK_FONT_BODY), text, EPD_DRAW_ALIGN_LEFT, false);
        if (s_dir[0]) snprintf(text, sizeof(text), TR("%d 首"), s_order_count);
        else snprintf(text, sizeof(text), TR("%d 个文件夹"), s_level_dirs);
        ui_text_fixed_ink_vc(fb, L.note.x, L.note.y + L.note.height / 2, sy(INK_FONT_NOTE), text, EPD_DRAW_ALIGN_LEFT, UI_GRAY_SECONDARY);
    } else {
        rebuild_order();
        layout();
        pages = full_page_count();
        if (s_full_page >= pages) s_full_page = pages - 1;
        if (s_full_page < 0) s_full_page = 0;
        snprintf(text, sizeof(text), TR("全部歌曲 · %d 首"), total);
        fit(text, sy(INK_FONT_BODY), L.head.width);
        ui_text_fixed_vc(fb, L.head.x, L.head.y + L.head.height / 2, sy(INK_FONT_BODY), text, EPD_DRAW_ALIGN_LEFT, false);
        ui_text_fixed_ink_vc(fb, L.note.x, L.note.y + L.note.height / 2, sy(INK_FONT_NOTE), TR("已收藏优先"), EPD_DRAW_ALIGN_LEFT, UI_GRAY_SECONDARY);
    }
    button(fb, L.back, s_browse && s_dir[0] ? TR("上一级") : TR("播放页"), true);

    const int current = music_track_index();
    const int page = s_browse ? s_entry_page : s_full_page;
    const int entries = s_browse ? level_total() : s_order_count;
    for (int i = 0; i < FULL_ROWS; ++i) {
        const int pos = page * FULL_ROWS + i;
        if (pos >= entries) break;
        EpdRect row = row_rect(i, true);
        const bool pressed = false;
        if (s_browse && pos < s_level_dirs) {
            char name[DIR_NAME_MAX];
            child_name(s_dir_first[pos], name, sizeof(name));
            if (!name[0]) continue;
            char full[256];
            if (s_dir[0]) snprintf(full, sizeof(full), "%s/%s", s_dir, name);
            else strlcpy(full, name, sizeof(full));
            folder_row(fb, row, name, dir_track_count(full),
                       current >= 0 && dir_holds(current, full), pressed);
        } else {
            const int idx = s_browse ? s_order[pos - s_level_dirs] : s_order[pos];
            track_row(fb, row, idx, idx == current, pressed);
        }
    }
    // 翻页行与播放列表页同一套坐标。/ The pager shares the playlist's coordinates.
    pager(fb, s_browse ? s_entry_page : s_full_page, pages);
}

static void render(app_ctx_t *ctx, uint8_t *fb) {
    (void)ctx;
    ui_clear_page(fb);
    if (s_show_all) {
        render_full_list(fb);
        return;
    }
    layout();
    // 播放模式胶囊：点一下循环切换。/ Play-mode capsule: tap to cycle.
    button(fb, MODE_RECT, TR(music_mode_label()), true);
    // 输出通路胶囊：耳机 / 本机扬声器。两种状态同一套观感，只有文字不同。
    // Output capsule: headset / local speaker. Both states share one look; only the word differs.
    button(fb, OUTPUT_RECT, music_output() == MUSIC_OUT_SPK ? TR("扬声器") : TR("耳机"), true);
    // 全部歌曲：进全屏列表（收藏优先、按键翻页）。/ All tracks: open the full-screen list (favorites first, key-paged).
    button(fb, L.all, TR("全部歌曲"), true);
    char text[192];
    music_state_t state = music_state();

    // 曲名一行 + 艺人 / Title (one line) + artist
    strlcpy(text, music_title(), sizeof(text));
    if (!text[0]) strlcpy(text, state == MUSIC_ERROR ? music_error() : TR("暂无音乐"), sizeof(text));
    const int title_px = sy(INK_FONT_TITLE);
    fit(text, title_px, L.title.width);
    ui_text_fixed_vc(fb, L.w / 2, L.title.y + L.title.height / 2, title_px, text, EPD_DRAW_ALIGN_CENTER, false);
    const char *artist = music_artist();
    if (artist[0]) {
        char a[64];
        strlcpy(a, artist, sizeof(a));
        fit(a, sy(INK_FONT_NOTE), L.artist.width);
        ui_text_fixed_ink_vc(fb, L.w / 2, L.artist.y + L.artist.height / 2, sy(INK_FONT_NOTE), a, EPD_DRAW_ALIGN_CENTER, UI_GRAY_SECONDARY);
    }

    // 歌词只显示当前一句，放在歌名下方；提示占用同一行。
    // Only the current lyric line shows, below the title; a notice takes the same row.
    const int lyric_y = L.lyric.y + L.lyric.height / 2;
    if (s_message[0]) {
        ui_text_fixed_ink_vc(fb, L.w / 2, lyric_y, sy(INK_FONT_BODY), s_message, EPD_DRAW_ALIGN_CENTER, UI_GRAY_SECONDARY);
    } else {
        const char *lyric = music_lyrics_at(music_elapsed_ms());
        if (lyric[0]) {
            char l[192];
            strlcpy(l, lyric, sizeof(l));
            fit(l, sy(INK_FONT_BODY), L.title.width);
            ui_text_fixed_vc(fb, L.w / 2, lyric_y, sy(INK_FONT_BODY), l, EPD_DRAW_ALIGN_CENTER, false);
        }
    }

    // 进度 + 两侧时间 / progress bar + times on both sides
    uint32_t dur = music_duration_ms(), el = music_elapsed_ms();
    EpdRect bar = L.bar;
    int filled = 0;
    if (dur) filled = (int)((uint64_t)bar.width * (el > dur ? dur : el) / dur);
    ui_fill_rect_fast(bar, UI_GRAY_SEPARATOR, fb);
    if (filled) {
        EpdRect done = bar;
        done.width = filled;
        ui_fill_rect_fast(done, UI_GRAY_BLACK, fb);
    }
    s_bar_drawn = filled;
    // 只画总时长：已播时间每秒变化，墨水屏上只会带来无谓的局部刷新。
    // Total time only: the elapsed counter changes every second and would only cause needless
    // partial refreshes on e-paper.
    char t2[16];
    if (dur) mmss(t2, sizeof(t2), dur);
    else strlcpy(t2, "--:--", sizeof(t2));
    ui_text_fixed_ink_vc(fb, bar.x + bar.width, L.seek.y + L.seek.height * 3 / 4, sy(INK_FONT_NOTE), t2, EPD_DRAW_ALIGN_RIGHT, UI_GRAY_SECONDARY);

    // 传输键：上一首 / 播放暂停 / 下一首 / transport: prev / play-pause / next
    button(fb, L.prev, TR("上一首"), true);
    button(fb, L.next, TR("下一首"), true);
    bool playing = state == MUSIC_PLAYING;
    // ink：播放键是这一屏唯一的主按钮（黑底白字形），不画圆圈。/ The play key is the screen's one primary control.
    ui_fill_round_rect(fb, L.play, sy(INK_RADIUS_KEY), UI_GRAY_BLACK);
    play_glyph(fb, L.play.x + L.play.width / 2, L.play.y + L.play.height / 2, sy(INK_ICON_MEDIUM), sy(INK_ICON_MEDIUM) * 3 / 4, UI_GRAY_WHITE, playing);

    // 音量行：减/加在两端，中间显示「音量 NN%」。
    // Volume row: minus/plus at both ends, "音量 NN%" in the middle.
    button(fb, L.minus, "-", true);
    button(fb, L.plus, "+", true);
    unsigned pct = music_volume();
    snprintf(text, sizeof(text), TR("音量 %u%%"), pct);
    ui_text_fixed_vc(fb, L.w / 2, L.volume.y + L.volume.height / 2, sy(INK_FONT_BODY), text, EPD_DRAW_ALIGN_CENTER, false);

    // 播放列表 / The playlist
    int count = music_track_count();
    if (!count) {
        ui_text_fixed_ink_vc(fb, L.w / 2, L.row0 + L.row_h / 2, sy(INK_FONT_NOTE), TR("还没有歌。用传书传上来，或者放进 文件 › 音乐"),
                             EPD_DRAW_ALIGN_CENTER, UI_GRAY_SECONDARY);
    }
    int current = music_track_index();
    ui_fill_rect_fast((EpdRect){L.title.x, L.row0 - sy(INK_DIVIDER), L.title.width, sy(INK_DIVIDER)}, UI_GRAY_SEPARATOR, fb);
    for (int i = 0; i < MUSIC_ROWS; ++i) {
        int idx = s_page * MUSIC_ROWS + i;
        if (idx >= count) break;
        track_row(fb, row_rect(i, false), idx, idx == current, false);
    }

    // 翻页 / paging
    int pages = page_count();
    if (pages < 1) pages = 1;
    pager(fb, s_page, pages);
}

// 左侧已播时间标签区（按系统字号算，不写死，避免大字号被裁）：标签以 y=462 居中。
// The left elapsed-time label box, sized by the system font (never hardcoded, so larger
// sizes are not clipped); the label is centered on y=462.
static void present(void) {
    // 整块刷新已经把条一起推上屏。/ A block refresh already pushed the bar up.
    s_bar_shown = s_bar_drawn;
    s_tick_partial = false;
}

static void on_enter(app_ctx_t *ctx) {
    (void)ctx;
    s_message[0] = 0;
    s_tick_partial = false;
    s_show_all = false;  // 每次进页从播放页开始 / always enter at the player, not the full list
    s_dir[0] = 0;        // 全屏列表下次从曲库根开始 / the full list starts at the library root next time
    s_entry_page = 0;
    // ink：每次进页重扫曲库（传书或文件里新放的歌），在 BG 执行器上。/ Rescan on every entry, on the BG executor.
    music_scan();
    layout();
    int current = music_track_index();
    s_seen_track = current;
    s_page = current >= 0 ? current / MUSIC_ROWS : 0;
    s_serial = music_serial();
    sync_lyric();
    s_area = MUSIC_DIRTY;
}
// ink：播放读不了（read_failed）时在歌词行写一句；原文是存储卡移除的提示。/ A read failure notice on the lyric line.
static void on_media_lost(app_ctx_t *ctx) {
    (void)ctx;
    strlcpy(s_message, TR("读不了这首歌"), sizeof(s_message));
    s_dir[0] = 0;  // 卡拔了，层级路径作废 / the folder path is void once the card is gone
    s_entry_page = 0;
}

static app_redraw_t on_tick(app_ctx_t *ctx) {
    (void)ctx;
    uint32_t serial = music_serial();
    const bool in_view = s_show_all;
    // 歌词随播放位置推进：只在播放页检测，换句才刷新。
    // Lyrics advance with playback: tracked on the player only, redrawn when the line changes.
    bool lyric_changed = false;
    if (!in_view) {
        char snap[sizeof(s_lyric_seen)];
        lyric_snapshot(snap, sizeof(snap));
        lyric_changed = strcmp(snap, s_lyric_seen) != 0;
        if (lyric_changed) memcpy(s_lyric_seen, snap, sizeof(snap));
    }
    bool state_changed = serial != s_serial;
    if (state_changed) {
        int current = music_track_index();
        if (current != s_seen_track) {  // 换曲时列表跟随到所在页 / follow the list to the playing track's page
            s_seen_track = current;
            if (current >= 0) s_page = current / MUSIC_ROWS;
        }
        s_serial = serial;
        s_tick_partial = false;
        s_area = current_dirty();
        return APP_REDRAW_AREA;
    }
    if (lyric_changed && s_message[0]) lyric_changed = false;  // ink: a notice holds the lyric line
    if (lyric_changed) {
        // 歌词变了就只刷歌词行。/ A lyric change refreshes the lyric line only.
        s_tick_partial = false;
        s_area = LYRIC_DIRTY;
        return APP_REDRAW_AREA;
    }
    if (in_view) return APP_REDRAW_NONE;
    // 纯进度推进：条只推新增的一窄条（像素慢慢变黑，不整块重刷），时间文字不再每秒刷。
    // Pure progress: the bar pushes only the newly filled sliver (pixels darken incrementally,
    // never a block repaint); the time label is not repainted every second.
    const uint32_t dur = music_duration_ms(), el = music_elapsed_ms();
    const int filled = dur ? (int)((uint64_t)L.bar.width * (el > dur ? dur : el) / dur) : 0;
    EpdRect strip = {0, 0, 0, 0};
    if (filled >= s_bar_shown + sy(INK_GAP_S))  // ink：至少长出 8 px 才刷 / at least 8 px of growth per refresh
        strip = (EpdRect){L.bar.x + s_bar_shown, L.bar.y, filled - s_bar_shown, L.bar.height};
    else if (filled < s_bar_shown)
        strip = L.bar;  // 后退（跳转/换曲）：整条重画 / a rewind repaints the whole bar
    if (strip.width == 0) return APP_REDRAW_NONE;
    s_area = strip;
    s_tick_partial = true;
    return APP_REDRAW_AREA;
}

/* ink: a tap resolves to the capability AI uses (music_request), in the original hit order; the result
 * comes back as a redraw from that capability. */
static app_redraw_t on_gesture(app_ctx_t *ctx, int x, int y) {
    (void)ctx;
    const bool pager = pages_shown() > 1;
    if (s_show_all) {
        // 层级模式先列子文件夹再列曲目，平铺模式只有曲目。
        // Hierarchy mode lists sub-folders then tracks; the flat list only has tracks.
        const int dirs = s_browse ? s_level_dirs : 0;
        const int entries = s_browse ? level_total() : s_order_count;
        const int page = s_browse ? s_entry_page : s_full_page;
        if (hit(L.back, x, y)) {  // 返回：先退一层文件夹、再退视图 / Back: up a folder, then the view
            if (s_browse && s_dir[0]) {
                char up[sizeof(s_dir)];
                strlcpy(up, s_dir, sizeof(up));
                char *slash = strrchr(up, '/');
                if (slash) *slash = 0;
                else strlcpy(up, "/", sizeof(up));
                music_request_path("music.list", up);
            } else music_request("music.list", "{\"view\":\"player\"}");
            return APP_REDRAW_NONE;
        }
        for (int i = 0; i < FULL_ROWS; ++i) {
            if (!hit(row_rect(i, true), x, y)) continue;
            const int pos = page * FULL_ROWS + i;
            if (pos >= entries) break;
            if (pos < dirs) {
                char name[DIR_NAME_MAX], path[sizeof(s_dir) + DIR_NAME_MAX];
                child_name(s_dir_first[pos], name, sizeof(name));
                if (s_dir[0]) snprintf(path, sizeof(path), "%s/%s", s_dir, name);
                else strlcpy(path, name, sizeof(path));
                if (name[0]) music_request_path("music.list", path);
                return APP_REDRAW_NONE;
            }
            const int idx = s_order[pos - dirs];
            // 书签列切换收藏 / the bookmark column toggles the favorite
            music_request_path(x < L.title.x + L.row_h ? "music.favorite" : "music.play", music_track(idx)->path);
            return APP_REDRAW_NONE;
        }
        if (pager && hit(L.page_prev, x, y)) {
            if (page > 0) music_request("music.list", "{\"page\":%d}", page - 1);
            return APP_REDRAW_NONE;
        }
        if (pager && hit(L.page_next, x, y)) {
            if (page + 1 < pages_shown()) music_request("music.list", "{\"page\":%d}", page + 1);
            return APP_REDRAW_NONE;
        }
        return APP_REDRAW_NONE;
    }
    // 模式胶囊：循环切换列表/单曲/随机/收藏循环，只刷这一小块。
    // Mode capsule: cycle list/single/shuffle/favorites, repainting only this capsule.
    if (hit(MODE_RECT, x, y)) {
        music_request("music.mode", "{\"mode\":\"cycle\"}");
        return APP_REDRAW_NONE;
    }
    // 输出胶囊：ink 没有切换通路的单步能力，打开蓝牙页（shell.bluetooth）。
    // Output capsule: no single-step route switch here; it opens the Bluetooth page.
    if (hit(OUTPUT_RECT, x, y)) {
        music_request("shell.bluetooth", NULL);
        return APP_REDRAW_NONE;
    }
    // 进度条点按跳转：按落点比例换算绝对毫秒。/ Tap the progress bar to seek: map the tap to absolute milliseconds.
    if (hit(L.seek, x, y)) {
        const uint32_t dur = music_duration_ms();
        if (dur) {
            int rel = x - L.bar.x;
            if (rel < 0) rel = 0;
            if (rel > L.bar.width) rel = L.bar.width;
            music_request("music.seek", "{\"ms\":%u}", (unsigned)((uint64_t)dur * (uint32_t)rel / (uint32_t)L.bar.width));
        }
        return APP_REDRAW_NONE;
    }
    // 全部歌曲入口：有子文件夹按目录层级从根开始，否则沿用平铺的收藏优先列表；
    // 打开时定位到当前曲目所在页。
    // All-tracks entry: start at the library root when sub-folders exist, otherwise keep the flat
    // favorites-first list; open on the page holding the current track.
    if (hit(L.all, x, y)) {
        music_request("music.list", "{\"view\":\"all\"}");
        return APP_REDRAW_NONE;
    }
    if (hit(L.prev, x, y)) { music_request("music.prev", NULL); return APP_REDRAW_NONE; }
    if (hit(L.play, x, y)) {
        music_request(music_state() == MUSIC_PLAYING ? "music.pause" : "music.play", NULL);
        return APP_REDRAW_NONE;
    }
    if (hit(L.next, x, y)) { music_request("music.next", NULL); return APP_REDRAW_NONE; }
    if (hit(L.minus, x, y) || hit(L.plus, x, y)) {
        int vol = (int)music_volume() + (hit(L.plus, x, y) ? 6 : -6);  // ink: 0..100, ±6 ≈ the original's 8/127
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        music_request("settings.set", "{\"id\":\"audio.volume\",\"value\":%d}", vol);
        return APP_REDRAW_NONE;
    }
    for (int i = 0; i < MUSIC_ROWS; ++i) {
        if (!hit(row_rect(i, false), x, y)) continue;
        int idx = s_page * MUSIC_ROWS + i;
        if (idx >= music_track_count()) break;
        // 书签列切换收藏 / the bookmark column toggles the favorite
        music_request_path(x < L.title.x + L.row_h ? "music.favorite" : "music.play", music_track(idx)->path);
        return APP_REDRAW_NONE;
    }
    int pages = page_count();
    if (pager && hit(L.page_prev, x, y)) {
        if (s_page > 0) music_request("music.list", "{\"view\":\"list\",\"page\":%d}", s_page - 1);
        return APP_REDRAW_NONE;
    }
    if (pager && hit(L.page_next, x, y)) {
        if (s_page + 1 < pages) music_request("music.list", "{\"view\":\"list\",\"page\":%d}", s_page + 1);
        return APP_REDRAW_NONE;
    }
    return APP_REDRAW_NONE;
}

/* ink: the cover keys page the current list; Back goes up a folder, then back to the player. */
static bool on_key(app_ctx_t *ctx, int key) {
    (void)ctx;
    const int page = s_show_all ? (s_browse ? s_entry_page : s_full_page) : s_page;
    if (key == UI_KEY_1 || key == UI_KEY_3) {
        const int to = page + (key == UI_KEY_3 ? 1 : -1);
        if (to >= 0 && to < pages_shown()) music_request("music.list", "{\"page\":%d}", to);
        return true;
    }
    if (key == UI_KEY_2 && s_show_all) return on_gesture(ctx, L.back.x, L.back.y) == APP_REDRAW_NONE;
    return false;
}

/* ---- ink: entry points for the glue (apps/music/music.c), in place of app_desc_t ---- */
void music_page_enter(int width, int height) {
    L.w = width;
    L.h = height;
    on_enter(NULL);
}
void music_page_reset(void) {
    s_page = s_full_page = s_entry_page = s_order_count = s_level_dirs = 0;
    s_seen_track = -1;
    s_show_all = s_browse = false;
    s_message[0] = s_dir[0] = s_lyric_seen[0] = 0;
    s_bar_drawn = s_bar_shown = 0;
}
bool music_page_tap(int x, int y) {
    if (!target_at(x, y).r.width) return false;
    on_gesture(NULL, x, y);
    return true;
}
bool music_page_key(int key) { return on_key(NULL, key); }
app_redraw_t music_page_tick(void) { return on_tick(NULL); }
void music_page_message(const char *text) {
    if (!text) s_message[0] = 0;
    else if (!text[0]) on_media_lost(NULL);
    else strlcpy(s_message, text, sizeof(s_message));
}
/* view: -1 keep, 0 player, 1 player list, 2 all tracks; page -1 keeps it; dir NULL keeps it, "/" is the root. */
app_redraw_t music_page_list(int view, int page, const char *dir) {
    const bool was = s_show_all;
    if (view == 2 && !s_show_all) {
        s_show_all = true;
        s_browse = library_has_dirs();
        s_dir[0] = 0;
        s_entry_page = 0;
        s_full_page = 0;
        const int current = music_track_index();
        if (s_browse) {
            level_build();
            layout();
            for (int i = 0; i < s_order_count; ++i)
                if (s_order[i] == current) { s_entry_page = i / FULL_ROWS; break; }
        } else {
            rebuild_order();
            layout();
            for (int i = 0; i < s_order_count; ++i)
                if (s_order[i] == current) { s_full_page = i / FULL_ROWS; break; }
        }
    }
    if (view >= 0) s_show_all = view == 2;
    if (dir && s_show_all) {
        if (!strcmp(dir, "/") || !s_browse) s_dir[0] = 0;
        else strlcpy(s_dir, dir, sizeof(s_dir));
        s_entry_page = 0;
    }
    if (page >= 0) {
        if (!s_show_all) s_page = page;
        else if (s_browse) s_entry_page = page;
        else s_full_page = page;
    }
    s_serial = music_serial();
    sync_lyric();
    s_area = current_dirty();
    return was != s_show_all ? APP_REDRAW_PAGE : APP_REDRAW_AREA;
}
/* What a capability changed: only that control is redrawn (D-127). */
app_redraw_t music_page_changed(const char *what, const char *path) {
    if (!strcmp(what, "mode") && !s_show_all) {
        s_serial = music_serial();
        s_area = MODE_RECT;
        return APP_REDRAW_AREA;
    }
    if (!strcmp(what, "favorite") && !s_show_all) {
        s_serial = music_serial();
        const int idx = music_find_path(path);
        for (int i = 0; i < MUSIC_ROWS; ++i)
            if (idx >= 0 && s_page * MUSIC_ROWS + i == idx) {
                s_area = row_rect(i, false);
                return APP_REDRAW_AREA;
            }
        return APP_REDRAW_NONE;
    }
    if ((!strcmp(what, "volume") || !strcmp(what, "output") || !strcmp(what, "message")) && !s_show_all) {
        s_area = what[0] == 'v' ? L.volume : what[0] == 'o' ? OUTPUT_RECT : LYRIC_DIRTY;
        return APP_REDRAW_AREA;
    }
    const int current = music_track_index();
    if (!s_show_all && current == s_seen_track && !strcmp(what, "play") && music_serial() == s_serial) return APP_REDRAW_NONE;  // on_tick drew it
    if (!s_show_all && current == s_seen_track && (!strcmp(what, "state") || !strcmp(what, "seek"))) {
        s_serial = music_serial();
        sync_lyric();
        s_area = !strcmp(what, "seek") ? L.seek : L.play;
        return APP_REDRAW_AREA;
    }
    if (current != s_seen_track) {  // 换曲时列表跟随到所在页 / follow the list to the playing track's page
        s_seen_track = current;
        if (current >= 0) s_page = current / MUSIC_ROWS;
    }
    return dirty();
}
EpdRect music_page_draw(uint8_t *fb) {
    if (fb) render(NULL, fb);
    return s_area;
}
void music_page_presented(void) {
    if (s_tick_partial) s_bar_shown = s_bar_drawn > s_bar_shown ? s_bar_drawn : s_bar_shown;
    else present();
    s_tick_partial = false;
}
bool music_page_all(void) { return s_show_all; }
const char *music_page_dir(void) { return s_dir; }
const char *music_page_notice(void) { return s_message; }
/* The rows on screen now, for the snapshot; returns the count and fills page/pages. */
int music_page_rows(music_row_t *out, int max, int *page, int *pages) {
    int n = 0;
    *pages = pages_shown();
    if (!s_show_all) {
        *page = s_page;
        for (int i = 0; i < MUSIC_ROWS && n < max; ++i) {
            const int idx = s_page * MUSIC_ROWS + i;
            if (idx >= music_track_count()) break;
            out[n++] = (music_row_t){.index = idx};
        }
        return n;
    }
    if (s_browse) level_build();
    else rebuild_order();
    *page = s_browse ? s_entry_page : s_full_page;
    const int entries = s_browse ? level_total() : s_order_count;
    for (int i = 0; i < FULL_ROWS && n < max; ++i) {
        const int pos = *page * FULL_ROWS + i;
        if (pos >= entries) break;
        if (s_browse && pos < s_level_dirs) {
            music_row_t *r = &out[n++];
            *r = (music_row_t){.index = -1, .count = 0};
            child_name(s_dir_first[pos], r->folder, sizeof(r->folder));
            char full[256];
            if (s_dir[0]) snprintf(full, sizeof(full), "%s/%s", s_dir, r->folder);
            else strlcpy(full, r->folder, sizeof(full));
            r->count = dir_track_count(full);
        } else out[n++] = (music_row_t){.index = s_browse ? s_order[pos - s_level_dirs] : s_order[pos]};
    }
    return n;
}
/* Every tap target of the current view, for the size test. */
int music_page_targets(EpdRect *out, int max) {
    int n = 0;
    for (int y = 0; y < L.h && n < max; y += 8)
        for (int x = 0; x < L.w && n < max; x += 8) {
            const music_hit_t t = target_at(x, y);
            bool seen = !t.r.width;
            for (int k = 0; k < n && !seen; ++k) seen = !memcmp(&out[k], &t.r, sizeof t.r);
            if (!seen) out[n++] = t.r;
        }
    return n;
}
