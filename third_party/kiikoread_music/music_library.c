/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：音乐播放器核心（见 music_player.h）。解码任务（core1、优先级 3）读文件 →
 * esp_audio_simple_dec 解码 → 重采样到 16k/2ch/s16 → 入 384KB PSRAM 环形缓冲；
 * 蓝牙模组的拉流回调只从环里取数，欠载由 bt_host 补静音。单生产者（解码任务）
 * 单消费者（喂帧任务），环位置用「模 2×容量」的原子计数。
 *
 * English: player core (see music_player.h). The decode task (core 1, priority 3)
 * reads the file, decodes via esp_audio_simple_dec, resamples to 16k/2ch/s16 and
 * feeds a 384 KB PSRAM ring; the module pull callback only drains it (bt_host pads
 * silence on underrun). One producer (decode task), one consumer (feed task); ring
 * positions are atomic counts modulo 2×capacity.
 *
 * 冻结：不绘制、不发起蓝牙连接、不递归到 music/ 之外的目录。
 * Frozen: no drawing, no Bluetooth connects, no directory recursion outside music/.
 */

/* Whole functions copied from main/music/music_player.c @746ce316, in source order; see SOURCE.md. */
/* ---- 模块状态 / Module state ---- */

static music_track_t *s_tracks;  // PSRAM
static int s_count;
static char s_last_path[240];  // NVS 恢复用 / restored from NVS

static atomic_int s_index = ATOMIC_VAR_INIT(-1);
static atomic_int s_state = ATOMIC_VAR_INIT(MUSIC_IDLE);
static atomic_uint s_serial;
static atomic_int s_req = ATOMIC_VAR_INIT(REQ_NONE);

static atomic_int s_mode = ATOMIC_VAR_INIT(MUSIC_MODE_LIST);  // music_mode_t，NVS 持久化 / persisted

static uint8_t s_fav[MUSIC_MAX_TRACKS];  // 收藏缓存，按曲目下标对齐 s_tracks / favorite cache aligned with s_tracks

// 单曲直放：曲库外的音频（文件管理里点开），不进播放列表，播完即停。
// Direct play: audio outside the library (opened from the file manager) never joins the
// playlist and stops when it ends.
static atomic_bool s_single;
static char s_single_path[256];
static char s_single_title[96];
static char s_single_artist[64];

/* ---- 扫描 / Library scan ---- */

static bool ext_wanted(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".mp3") == 0 || strcasecmp(dot, ".m4a") == 0 ||
           strcasecmp(dot, ".aac") == 0 || strcasecmp(dot, ".flac") == 0 ||
           strcasecmp(dot, ".wav") == 0 || strcasecmp(dot, ".opus") == 0 ||
           strcasecmp(dot, ".ogg") == 0;  // Opus/Vorbis 走 OGG 容器解析 / Ogg container (Opus/Vorbis)
}

static void scan_dir(const char *path, int depth, music_track_t *out, int *count) {
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *ent;
    while (*count < MUSIC_MAX_TRACKS && (ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char child[300];
        const int n = snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        if (n <= 0 || n >= (int)sizeof(child)) continue;
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth + 1 < 2) scan_dir(child, depth + 1, out, count);  // 深度 2 / depth 2
        } else if (S_ISREG(st.st_mode) && ext_wanted(ent->d_name)) {
            music_track_t *t = &out[*count];
            memset(t, 0, sizeof(*t));
            strlcpy(t->path, child, sizeof(t->path));
            music_tags_read(child, t->title, sizeof(t->title), t->artist, sizeof(t->artist),
                            &t->duration_ms);
            ++(*count);
        }
    }
    closedir(dir);
}

// 收藏循环：从 cur 之后找下一首收藏，绕回开头；一首收藏都没有时返回 -1。
// Favorites loop: the next favorited track after cur, wrapping; -1 when nothing is favorited.
static int next_fav_index(int cur) {
    if (s_count <= 0) return -1;
    for (int step = 1; step <= s_count; ++step) {
        const int i = (cur + step) % s_count;
        if (s_fav[i]) return i;
    }
    return -1;
}

static void finish_track(void) {
    close_track();
    if (atomic_load(&s_single)) {
        // 单曲直放播完即停，不接曲库里的下一首。/ A direct file stops here; no library follow-up.
        atomic_store(&s_single, false);
        time_freeze();
        bt_trace("MUSIC finish (single) -> idle");
        set_state(MUSIC_IDLE);
        bump();
        return;
    }
    const int count = s_count;
    if (count > 0 && atomic_load(&s_state) != MUSIC_IDLE && !atomic_load(&s_media_lost)) {
        // 曲末自动切歌按模式走；手动上一首/下一首始终按完整曲库顺序。
        // Auto-advance at EOF follows the mode; manual prev/next always walks the whole library.
        const int cur = atomic_load(&s_index);
        int next = cur < 0 ? 0 : cur;
        switch ((music_mode_t)atomic_load(&s_mode)) {
        case MUSIC_MODE_ONE:
            next = cur >= 0 ? cur : 0;  // 单曲循环：同一首重播 / repeat the same track
            break;
        case MUSIC_MODE_SHUFFLE:
            // 随机：count>1 时挑一个不是当前的下标。/ Shuffle: pick any other index when possible.
            if (count > 1) {
                int pick = (int)(esp_random() % (uint32_t)count);
                if (pick == cur) pick = (pick + 1 + (int)(esp_random() % (uint32_t)(count - 1))) % count;
                next = pick;
            } else next = 0;
            break;
        case MUSIC_MODE_FAV: {
            // 收藏循环：只在收藏里绕；收藏被清空则退回列表循环，不至于莫名停止。
            // Favorites loop: walk favorites only; with none left, fall back to the list loop
            // instead of stopping unexpectedly.
            const int fav = next_fav_index(cur < 0 ? -1 : cur);
            next = fav >= 0 ? fav : (cur + 1) % count;
            break;
        }
        case MUSIC_MODE_LIST:
        default:
            next = (cur + 1) % count;
            break;
        }
        atomic_store(&s_req, next);
    } else {
        // 曲末走到这里就是「没下一首 / 丢卡」：实机上表现为播放器突然回到 IDLE，
        // 和唤醒链路里的假丢卡长得一模一样，所以把原因一起写下来。
        // Landing here at EOF means "no next track / card gone": on device that looks exactly
        // like a false card-loss at wake, so record which one it was.
        bt_trace("MUSIC finish (count=%d lost=%d state=%d) -> idle", count,
                 (int)atomic_load(&s_media_lost), (int)atomic_load(&s_state));
        time_freeze();
        set_state(MUSIC_IDLE);
    }
}

int music_find_path(const char *path) {
    if (!path || !s_tracks) return -1;
    for (int i = 0; i < s_count; ++i)
        if (!strcmp(s_tracks[i].path, path)) return i;
    return -1;
}

int music_scan(void) {
    if (music_player_start() != ESP_OK) return s_count;
    // 扫得动根目录就说明卡在位：清掉丢卡闩锁（USB 占用期间目录打不开，闩锁保留）。
    // Opening the root means the card is back: clear the media-lost latch (while USB owns
    // the card the directory does not open, so the latch stays).
    (void)music_media_probe();
    music_track_t *fresh = heap_caps_malloc(sizeof(music_track_t) * MUSIC_MAX_TRACKS,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fresh) fresh = malloc(sizeof(music_track_t) * MUSIC_MAX_TRACKS);
    if (!fresh) return s_count;
    memset(fresh, 0, sizeof(music_track_t) * MUSIC_MAX_TRACKS);

    int count = 0;
    scan_dir(music_root(), 0, fresh, &count);

    // 收藏一次读全：单句柄批量查，翻列表时不再碰 NVS。
    // Load every favorite in one handle pass so listing never touches NVS afterwards.
    memset(s_fav, 0, sizeof(s_fav));
    nvs_handle_t fav_h;
    if (nvs_open(NVS_NS, NVS_READONLY, &fav_h) == ESP_OK) {
        for (int i = 0; i < count; ++i) {
            char key[13];
            uint8_t value = 0;
            fav_key(fresh[i].path, key);
            if (nvs_get_u8(fav_h, key, &value) == ESP_OK && value == 1) s_fav[i] = 1;
        }
        nvs_close(fav_h);
    }

    // 播放中重扫：按路径把当前曲目映射到新表，别让标题指错。
    // Rescan while playing: remap the current file by path so the title cannot point elsewhere.
    char playing_path[240] = {0};
    const int cur = atomic_load(&s_index);
    const int st = atomic_load(&s_state);
    if (s_tracks && cur >= 0 && cur < s_count &&
        (st == MUSIC_PLAYING || st == MUSIC_PAUSED || st == MUSIC_LOADING)) {
        strlcpy(playing_path, s_tracks[cur].path, sizeof(playing_path));
    }

    music_track_t *old = s_tracks;
    s_tracks = fresh;
    s_count = count;
    bump();

    int remap = -1;
    if (playing_path[0]) {
        for (int i = 0; i < count; ++i)
            if (strcmp(s_tracks[i].path, playing_path) == 0) {
                remap = i;
                break;
            }
    }
    if (remap >= 0) {
        atomic_store(&s_index, remap);
    } else if (playing_path[0]) {
        atomic_store(&s_index, -1);  // 曲目被删还在播：继续放，结束后回列表头 / deleted while
                                      // playing: keep playing, restart from the top afterwards
    } else if (atomic_load(&s_index) < 0 && s_last_path[0]) {
        for (int i = 0; i < count; ++i)
            if (strcmp(s_tracks[i].path, s_last_path) == 0) {
                atomic_store(&s_index, i);
                break;
            }
    }
    // 开机续播：本会话仅试一次；上次在播且卡在位时，从恢复出的曲目接着放（位置由 open_track 续）。
    // Boot resume: try once per session; if it was playing last time and the card is present,
    // keep playing from the restored track (position resumed by open_track).
    if (!s_boot_resume_done) {
        s_boot_resume_done = true;
        if (s_resume_was_playing && !atomic_load(&s_media_lost)) {
            const int cur = atomic_load(&s_index);
            if (cur >= 0 && atomic_load(&s_state) == MUSIC_IDLE) music_play_index(cur);
        }
    }
    if (old) free(old);
    ESP_LOGI(TAG, "scan: %d track(s)", count);
    return count;
}

int music_track_count(void) { return s_count; }

const music_track_t *music_track(int index) {
    if (!s_tracks || index < 0 || index >= s_count) return NULL;
    return &s_tracks[index];
}

void music_next(void) {
    const int count = s_count;
    if (!count) return;
    int cur = atomic_load(&s_index);
    // 单曲直放时按下一首 = 回到曲库从头播。/ Next from a direct file starts the library over.
    if (atomic_load(&s_single)) cur = -1;
    music_play_index((cur < 0 ? 0 : (cur + 1) % count));
}

void music_prev(void) {
    const int count = s_count;
    if (!count) return;
    int cur = atomic_load(&s_index);
    // 单曲直放时按上一首 = 从曲库末尾倒着进。/ Prev from a direct file enters at the library's end.
    if (atomic_load(&s_single)) cur = 0;
    music_play_index(cur <= 0 ? count - 1 : cur - 1);
}

const char *music_title(void) {
    if (atomic_load(&s_single)) return s_single_title;
    const int i = atomic_load(&s_index);
    if (!s_tracks || i < 0 || i >= s_count) return "";
    return s_tracks[i].title;
}

const char *music_artist(void) {
    if (atomic_load(&s_single)) return s_single_artist;
    const int i = atomic_load(&s_index);
    if (!s_tracks || i < 0 || i >= s_count) return "";
    return s_tracks[i].artist;
}

int music_track_index(void) { return atomic_load(&s_index); }

uint32_t music_serial(void) { return atomic_load(&s_serial); }

bool music_favorite(int index) {
    if (index < 0 || index >= s_count) return false;
    return s_fav[index] != 0;
}

void music_favorite_toggle(int index) {
    if (index < 0 || index >= s_count || !s_tracks) return;
    const uint8_t value = s_fav[index] ? 0 : 1;
    s_fav[index] = value;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        char key[13];
        fav_key(s_tracks[index].path, key);
        esp_err_t err = value ? nvs_set_u8(h, key, 1) : nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND && !value) err = ESP_OK;
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    bump();
}

int music_favorite_count(void) {
    int n = 0;
    for (int i = 0; i < s_count; ++i) if (s_fav[i]) ++n;
    return n;
}

music_mode_t music_mode(void) { return (music_mode_t)atomic_load(&s_mode); }

void music_set_mode(music_mode_t mode) {
    if ((int)mode < 0 || (int)mode > MUSIC_MODE_FAV) mode = MUSIC_MODE_LIST;
    atomic_store(&s_mode, (int)mode);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_u8(h, NVS_KEY_MODE, (uint8_t)mode) == ESP_OK) (void)nvs_commit(h);
        nvs_close(h);
    }
    bump();
}

void music_cycle_mode(void) {
    // 列表循环 → 单曲循环 → 随机播放 → 收藏循环 → 列表循环；没有收藏时跳过收藏循环。
    // List → single → shuffle → favorites → list; the favorites mode is skipped with no favorites.
    int mode = ((int)music_mode() + 1) % (MUSIC_MODE_FAV + 1);
    if (mode == MUSIC_MODE_FAV && music_favorite_count() == 0) mode = MUSIC_MODE_LIST;
    music_set_mode((music_mode_t)mode);
}

const char *music_mode_label(void) {
    switch (music_mode()) {
    case MUSIC_MODE_ONE:     return "单曲循环";
    case MUSIC_MODE_SHUFFLE: return "随机播放";
    case MUSIC_MODE_FAV:     return "收藏循环";
    case MUSIC_MODE_LIST:
    default:                 return "列表循环";
    }
}
