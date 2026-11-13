/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：TF 卡音乐播放器（/sdcard/music）。扫描曲库、Simple Decoder 解码、重采样
 * 到 44.1k/2ch/s16 后入 PCM 环，由 A2DP 拉流回调取走；UI 只通过本头文件的
 * API 与它交互，不碰内部状态。
 *
 * English: TF-card music player (/sdcard/music). Scans the library, decodes with
 * the Simple Decoder, resamples to 44.1k/2ch/s16 into a PCM ring that the A2DP
 * pull callback drains; the UI talks to it only through this header.
 *
 * 冻结：不绘制、不管理蓝牙连接（连接在播放页做）、不做网络。
 * Frozen: no drawing, no Bluetooth connection management (the player page owns
 * that), no networking.
 */
/* ink adaptation 2026-11-13 (music-3): the page's interface over the media service; see SOURCE.md. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_MAX_TRACKS 200
#define MUSIC_ROOT "/me/音乐"
#define MUSIC_FAMILY_ROOT "/family/audio"

typedef enum {
    MUSIC_IDLE = 0,   // 未选曲 / nothing selected
    MUSIC_LOADING,    // 打开文件/解码器中 / opening file or decoder
    MUSIC_PLAYING,
    MUSIC_PAUSED,
    MUSIC_ERROR,      // music_error() 给原因 / see music_error() for the cause
} music_state_t;

typedef struct {
    char path[240];
    char title[96];
    char artist[64];
    uint32_t duration_ms;  // 0 = 未知（进度条只画时间）/ unknown (progress shows time only)
    int64_t size;          // ink: the file's size when its tags were read (a rescan reuses unchanged tags)
} music_track_t;

// 曲库根目录（默认 MUSIC_ROOT，可在文件管理里改）。/ Library root (defaults to MUSIC_ROOT, editable from the file manager).
const char *music_root(void);

// 重扫曲库根目录（深度 2、≤200 首），返回曲目数。/ Rescan the library root (depth 2, ≤200 tracks); returns the count.
int music_scan(void);
int music_track_count(void);
// 越界返回 NULL；曲库在 PSRAM，扫描期间索引不移动。/ NULL when out of range; the library lives in PSRAM and is not reallocated during reads.
const music_track_t *music_track(int index);

void music_play_index(int index);  // 越界忽略 / ignored when out of range
// 按完整路径找曲库下标，找不到返回 -1（文件管理用它判断走曲库还是单曲直放）。
// Library index for a full path, -1 when absent (the file manager uses it to choose library vs direct play).
int music_find_path(const char *path);
void music_next(void);
void music_prev(void);

music_state_t music_state(void);
const char *music_error(void);
const char *music_title(void);   // 当前曲目；无曲目时 "" / current track; "" when none
const char *music_artist(void);
uint32_t music_elapsed_ms(void);
uint32_t music_duration_ms(void);
int music_track_index(void);
// 变化计数：任何可感知的状态变化 +1；UI 用它做节流刷新。
// Change counter: bumped on any observable change; UI throttles redraws with it.
uint32_t music_serial(void);

uint8_t music_volume(void);      // ink: audio.volume, 0..100

/// 输出通路：蓝牙耳机 / 本机扬声器（模组模式 1 + NS4150 功放）。
/// Output route: Bluetooth headset / local speaker (module mode 1 + NS4150 PA).
typedef enum {
    MUSIC_OUT_BT = 0,   // 模式 2：I2S PCM 经模组发射到已配对耳机 / mode 2, PCM to the headset
    MUSIC_OUT_SPK = 1,  // 模式 1：PCM 直接走本机喇叭 / mode 1, PCM straight to the speaker
} music_output_t;

/// 当前输出通路（NVS rp_music:out，缺省蓝牙耳机）。/ Current route (persisted; defaults to headset).
music_output_t music_output(void);
/// 收藏：按曲目完整路径持久化，重扫后仍跟随文件。/ Favorite: persisted by full track path; survives rescans as long as the file does.
bool music_favorite(int index);
/// 切换收藏并落盘。/ Toggle the favorite flag and persist it.
void music_favorite_toggle(int index);
/// 当前曲库里的收藏数。/ How many tracks in the current library are favorites.
int music_favorite_count(void);

/// 播放模式：列表循环 / 单曲循环 / 随机播放 / 收藏循环。
/// Play mode: list loop / single repeat / shuffle / favorites loop.
typedef enum {
    MUSIC_MODE_LIST = 0,
    MUSIC_MODE_ONE,
    MUSIC_MODE_SHUFFLE,
    MUSIC_MODE_FAV,
} music_mode_t;

/// 当前模式（NVS rp_music:mode，缺省列表循环）。/ Current mode (persisted; defaults to list loop).
music_mode_t music_mode(void);
/// 直接设置并落盘。/ Set a mode and persist it.
void music_set_mode(music_mode_t mode);
/// 顺序切换到下一个模式；没有收藏时跳过「收藏循环」。
/// Cycle to the next mode; skips the favorites loop when nothing is favorited.
void music_cycle_mode(void);
/// 模式的中文短标签，UI 直接显示。/ Short Chinese label for the UI.
const char *music_mode_label(void);

/* ink: the folder a track's path is under (/me/音乐 or /family/audio), and the glue's hooks. */
const char *music_root_of(const char *path);
void music_port_favorites_load(const music_track_t *tracks, int count, uint8_t *fav);
bool music_port_favorite_store(const char *path, bool favorite); /* false: the collection refused (full) */
void music_port_mode_store(music_mode_t mode);
void music_library_reset(void);
void music_library_restore(const char *last, int mode);
void music_library_now(int state, const char *path, const char *title, const char *artist);
int music_library_pick(void);
bool music_library_truncated(void);

#ifdef __cplusplus
}
#endif
