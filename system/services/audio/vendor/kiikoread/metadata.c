/* Keep the upstream parsers intact; add album and Ogg Vorbis fields. */
#define music_tags_read ink_vendor_tags_read
#define music_tags_lyrics ink_vendor_tags_lyrics
#include "music_tags.c"
#undef music_tags_read
#undef music_tags_lyrics
#define STB_VORBIS_HEADER_ONLY
#include "../../../../../third_party/audio_formats/stb_vorbis.c"
void music_tags_read(const char *path, char *title, size_t tc, char *artist, size_t ac, uint32_t *ms) { ink_vendor_tags_read(path, title, tc, artist, ac, ms); }
size_t music_tags_lyrics(const char *path, char *out, size_t n) { return ink_vendor_tags_lyrics(path, out, n); }
void ink_music_tags(const char *path, char *title, char *artist, char *album, size_t cap) {
    uint32_t ms; album[0] = 0; music_tags_read(path, title, cap, artist, cap, &ms);
    const char *ext = strrchr(path, '.'); FILE *f = fopen(path, "rb");
    if (!f || !ext) { if (f) fclose(f); return; }
    if (!strcasecmp(ext, ".ogg") || !strcasecmp(ext, ".oga")) {
        int err; stb_vorbis *v = stb_vorbis_open_file(f, 0, &err, NULL);
        if (v) {
            stb_vorbis_comment comments = stb_vorbis_get_comment(v);
            for (int i = 0; i < comments.comment_list_length; i++) {
                const char *s = comments.comment_list[i]; char *out = NULL; size_t off = 0;
                if (!strncasecmp(s, "TITLE=", 6)) { out = title; off = 6; }
                if (!strncasecmp(s, "ARTIST=", 7)) { out = artist; off = 7; }
                if (!strncasecmp(s, "ALBUM=", 6)) { out = album; off = 6; }
                if (out) utf8_copy((const uint8_t *)s + off, strlen(s + off), out, cap);
            }
            stb_vorbis_close(v);
        }
    } else if (!strcasecmp(ext, ".mp3")) {
        uint8_t h[10];
        if (fread(h, 1, 10, f) == 10 && !memcmp(h, "ID3", 3) && (h[3] == 3 || h[3] == 4)) {
            uint32_t left = rd_syncsafe(h + 6), version = h[3];
            while (left >= 10 && fread(h, 1, 10, f) == 10) {
                uint32_t n = version == 4 ? rd_syncsafe(h + 4) : rd_be32(h + 4); left -= 10;
                if (!n || n > left) break;
                if (!memcmp(h, "TALB", 4) && n > 1 && n <= 65536) {
                    uint8_t *b = malloc(n);
                    if (b && fread(b, 1, n, f) == n) id3_text(b[0], b + 1, n - 1, album, cap);
                    free(b); break;
                }
                if (fseek(f, n, SEEK_CUR)) break;
                left -= n;
            }
        }
    } else if (!strcasecmp(ext, ".flac")) {
        uint8_t h[4];
        if (fread(h, 1, 4, f) == 4 && !memcmp(h, "fLaC", 4)) for (int block = 0; block < 128; block++) {
            if (fread(h, 1, 4, f) != 4) break;
            uint32_t n = ((uint32_t)h[1] << 16) | ((uint32_t)h[2] << 8) | h[3];
            if ((h[0] & 127) == 4 && n <= 65536) {
                uint8_t *b = malloc(n); uint32_t p = 0;
                if (b && fread(b, 1, n, f) == n && n >= 4 && rd_le32(b) <= n - 4) {
                    p = 4 + rd_le32(b);
                    if (n - p >= 4) {
                        uint32_t count = rd_le32(b + p); p += 4;
                        for (uint32_t i = 0; i < count && n - p >= 4; i++) {
                            uint32_t len = rd_le32(b + p); p += 4;
                            if (len > n - p) break;
                            if (len >= 6 && !strncasecmp((char *)b + p, "ALBUM=", 6)) utf8_copy(b + p + 6, len - 6, album, cap);
                            p += len;
                        }
                    }
                }
                free(b); break;
            }
            if (h[0] & 128 || fseek(f, n, SEEK_CUR)) break;
        }
    }
    fclose(f);
}
