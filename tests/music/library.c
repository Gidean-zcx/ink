/* The kiikoread library rules as copied (third_party/kiikoread_music/music_library.c), on a fake file index:
 * the end-of-song choice for each mode, the favorites loop falling back to the list, shuffle never repeating the
 * song, manual next/previous over the whole library, a rescan finding the playing song again, the 200 cap and a
 * file played on its own stopping at its end. kiikoread carries no host test of its own for these. */
#include <stdio.h>
#include "music_player.h"
#include "music_tags.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char files[260][INK_FS_PATH_MAX];
static int nfiles, played = -2;
static uint8_t saved_fav[260];
static int saved_mode = -1;
static uint32_t seed = 12345; /* fixed: the same picks every run */

uint32_t ink_music_random(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
int ink_fs_index(const char *path, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *out) {
    (void)type;
    for (; *cursor < (uint32_t)nfiles; ++*cursor)
        if (!strncmp(files[*cursor], path, strlen(path))) {
            memset(out, 0, sizeof *out);
            snprintf(out->path, sizeof out->path, "%s", files[(*cursor)++]);
            return 1;
        }
    return 0;
}
void music_tags_read(const char *path, char *title, size_t tc, char *artist, size_t ac, uint32_t *ms) {
    snprintf(title, tc, "%s", strrchr(path, '/') + 1);
    snprintf(artist, ac, "%s", "");
    *ms = 3000;
}
const char *music_root(void) { return MUSIC_ROOT; }
static int file_index(const char *path) {
    for (int i = 0; i < nfiles; i++) if (!strcmp(files[i], path)) return i;
    return -1;
}
void music_port_favorites_load(const music_track_t *tracks, int count, uint8_t *fav) {
    for (int i = 0; i < count; i++) fav[i] = saved_fav[file_index(tracks[i].path)];
}
void music_port_favorite_store(const char *path, bool favorite) { saved_fav[file_index(path)] = favorite; }
void music_port_mode_store(music_mode_t mode) { saved_mode = (int)mode; }
void music_play_index(int index) { played = index; }

static void library(int n) {
    music_library_reset();
    memset(saved_fav, 0, sizeof saved_fav);
    nfiles = 0;
    for (int i = 0; i < n; i++) snprintf(files[nfiles++], sizeof files[0], MUSIC_ROOT "/%03d.mp3", i);
    music_scan();
}
static void playing(int index) { music_library_now(MUSIC_PLAYING, music_track(index)->path, "", ""); }
static int path_index(const char *name) {
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "%s", name);
    return music_find_path(path);
}

int main(void) {
    /* Order, depth 2, both roots, and formats the media service cannot play are still listed. */
    nfiles = 0;
    const char *const tree[] = {MUSIC_ROOT "/b.mp3", MUSIC_ROOT "/a.flac", MUSIC_ROOT "/专辑/c.wav", MUSIC_ROOT "/专辑/深/d.mp3",
                                MUSIC_ROOT "/e.m4a", MUSIC_ROOT "/notes.txt", MUSIC_FAMILY_ROOT "/f.ogg", "/me/书/g.mp3"};
    for (size_t i = 0; i < sizeof tree / sizeof *tree; i++) snprintf(files[nfiles++], sizeof files[0], "%s", tree[i]);
    music_library_reset();
    music_scan();
    CHECK(music_track_count() == 5, "five songs (depth 2, audio only, two roots): %d", music_track_count());
    CHECK(path_index(MUSIC_FAMILY_ROOT "/f.ogg") == 0 && path_index(MUSIC_ROOT "/a.flac") == 1 && path_index(MUSIC_ROOT "/b.mp3") == 2 &&
          path_index(MUSIC_ROOT "/e.m4a") == 3 && path_index(MUSIC_ROOT "/专辑/c.wav") == 4,
          "sorted by path, not by index order");
    CHECK(path_index(MUSIC_ROOT "/专辑/深/d.mp3") < 0 && path_index(MUSIC_ROOT "/e.m4a") >= 0 && path_index(MUSIC_FAMILY_ROOT "/f.ogg") >= 0,
          "a third level is left out, m4a and the family root are in");

    /* Over 200: the first 200, and the library says so. */
    library(230);
    CHECK(music_track_count() == MUSIC_MAX_TRACKS && music_library_truncated(), "200 cap: %d", music_track_count());
    library(5);
    CHECK(!music_library_truncated(), "no cap note under 200");

    /* List loop: the next one, the last wraps to the first. */
    music_set_mode(MUSIC_MODE_LIST);
    CHECK(saved_mode == MUSIC_MODE_LIST, "mode stored");
    playing(2);
    CHECK(music_library_pick() == 3, "list: next");
    playing(4);
    CHECK(music_library_pick() == 0, "list: the end wraps to the first");
    /* Single repeat: the same one. */
    music_set_mode(MUSIC_MODE_ONE);
    playing(2);
    CHECK(music_library_pick() == 2, "one: the same song again");
    /* Shuffle: never the song that just played, while there is another. */
    music_set_mode(MUSIC_MODE_SHUFFLE);
    for (int i = 0; i < 20; i++) {
        playing(i % 5);
        const int next = music_library_pick();
        CHECK(next >= 0 && next < 5 && next != i % 5, "shuffle %d: %d after %d", i, next, i % 5);
    }
    /* Favorites loop: only favorites, wrapping; with none left it falls back to the list loop. */
    music_set_mode(MUSIC_MODE_FAV);
    music_favorite_toggle(1);
    music_favorite_toggle(4);
    CHECK(music_favorite_count() == 2 && saved_fav[1] && saved_fav[4], "favorites stored by path");
    playing(2);
    CHECK(music_library_pick() == 4, "favorites: the next favorite");
    playing(4);
    CHECK(music_library_pick() == 1, "favorites: wraps to the first favorite");
    music_favorite_toggle(1);
    music_favorite_toggle(4);
    playing(2);
    CHECK(music_library_pick() == 3, "favorites cleared: back to the list loop");
    /* Cycling skips the favorites loop without favorites. */
    music_set_mode(MUSIC_MODE_SHUFFLE);
    music_cycle_mode();
    CHECK(music_mode() == MUSIC_MODE_LIST, "cycle skips favorites without any: %s", music_mode_label());
    music_favorite_toggle(0);
    music_set_mode(MUSIC_MODE_SHUFFLE);
    music_cycle_mode();
    CHECK(music_mode() == MUSIC_MODE_FAV, "cycle reaches favorites once there is one");

    /* Manual next and previous walk the whole library whatever the mode. */
    playing(2);
    music_next();
    CHECK(played == 3, "next: whole library in the favorites loop (%d)", played);
    playing(0);
    music_prev();
    CHECK(played == 4, "previous from the first: the last (%d)", played);
    playing(4);
    music_next();
    CHECK(played == 0, "next from the last: the first (%d)", played);

    /* A rescan finds the playing song by its path, even when it moved in the list. */
    playing(2);
    char was[240];
    snprintf(was, sizeof was, "%s", music_track(2)->path);
    snprintf(files[nfiles++], sizeof files[0], MUSIC_ROOT "/000a.mp3");
    music_scan();
    CHECK(music_track_index() == 3 && !strcmp(music_track(3)->path, was), "rescan remaps the playing song (%d)", music_track_index());
    CHECK(music_favorite(0) && saved_fav[0], "favorites survive the rescan by path");

    /* A file played on its own (not in the library) stops at its end; next starts the library over. */
    music_library_now(MUSIC_PLAYING, "/family/elsewhere/x.mp3", "x", "");
    CHECK(music_library_pick() == -1, "a single file stops at its end");
    CHECK(!strcmp(music_title(), "x"), "its title shows");
    music_next();
    CHECK(played == 0, "next after a single file starts the library over (%d)", played);

    printf("music-library: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
