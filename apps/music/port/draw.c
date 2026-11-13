/* The music page's own drawing on its L8 surface (never Mogo's drawing.c); text goes through Mogo's TrueType
 * engine, which writes through epd_draw_pixel with the size set by ink_mogo_dimensions. Lines and corners are
 * the theme's stroke and radius, scaled with the text size like the rest of the page. */
#include <ttf_font.h>

static int W, H;
void ink_music_canvas(int w, int h) { W = w; H = h; }
static int stroke(void) { int s = (INK_STROKE * ink_music_percent() + 50) / 100; return s < 1 ? 1 : s; }
static void span(uint8_t *fb, int x, int y, int w, uint8_t gray) {
    if (y < 0 || y >= H) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > W) w = W - x;
    if (w > 0) memset(fb + (size_t)y * W + x, gray, (size_t)w);
}
/* How far a rounded corner pulls row y of a box h tall in. */
static int inset(int radius, int y, int h) {
    int dy = y < radius ? radius - y : y >= h - radius ? y - (h - radius - 1) : 0, i = 0;
    while (i < radius && (radius - i) * (radius - i) + dy * dy > radius * radius) ++i;
    return i;
}
void ink_music_clear(uint8_t *fb) { memset(fb, 255, (size_t)W * H); }
void ink_music_fill(EpdRect r, uint8_t gray, uint8_t *fb) {
    for (int y = 0; y < r.height; ++y) span(fb, r.x, r.y + y, r.width, gray);
}
void ink_music_fill_round(uint8_t *fb, EpdRect r, int radius, uint8_t gray) {
    for (int y = 0; y < r.height; ++y) {
        const int i = inset(radius, y, r.height);
        span(fb, r.x + i, r.y + y, r.width - 2 * i, gray);
    }
}
void ink_music_frame(uint8_t *fb, EpdRect r, int radius, uint8_t gray) {
    const int s = stroke();
    for (int y = 0; y < r.height; ++y) {
        const int i = inset(radius, y, r.height), j = inset(radius - s, y - s, r.height - 2 * s);
        if (y < s || y >= r.height - s) span(fb, r.x + i, r.y + y, r.width - 2 * i, gray);
        else {
            span(fb, r.x + i, r.y + y, s + j - i, gray);
            span(fb, r.x + r.width - s - j, r.y + y, s + j - i, gray);
        }
    }
}
int ink_music_text_width(int px, const char *text) { return ttf_text_width_px(px, text); }
void ink_music_text_ink(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, uint8_t gray) {
    int above = 0, below = 0;
    ttf_measure_line_px(px, text, &above, &below);
    ttf_draw_text_px(fb, x, y + (above - below) / 2, px, text, align, gray >> 4, 15);
}
void ink_music_text(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, bool inverted) {
    ink_music_text_ink(fb, x, y, px, text, align, inverted ? 255 : 0);
}
/* Bookmark (outlined, or filled when saved) and folder (outlined, or filled when it holds the playing song):
 * the state is in the shape, never in a colour. */
void ink_music_icon(uint8_t *fb, int cx, int cy, int size, int icon, uint8_t gray) {
    const int s = stroke(), w = size * 3 / 5, x = cx - w / 2, y = cy - size / 2;
    const bool filled = icon == UI_ICON_BOOKMARK_CHECK || icon == UI_ICON_FOLDER_OPEN;
    if (icon == UI_ICON_BOOKMARK || icon == UI_ICON_BOOKMARK_CHECK) {
        for (int i = 0; i < size; ++i) {
            const int cut = i * 4 > size * 3 ? (i * 4 - size * 3) * w / (2 * size) : 0;
            if (filled || i < s) {
                span(fb, x, y + i, w / 2 - cut, gray);
                span(fb, cx + cut, y + i, x + w - cx - cut, gray);
            } else {
                span(fb, x, y + i, s, gray);
                span(fb, x + w - s, y + i, s, gray);
                if (cut) {
                    span(fb, cx - cut - s, y + i, s, gray);
                    span(fb, cx + cut, y + i, s, gray);
                }
            }
        }
        return;
    }
    const int fw = size, fx = cx - fw / 2, tab = size / 5, body = size * 3 / 4;
    const EpdRect top = {fx, cy - body / 2 - tab, fw * 2 / 5, tab + s}, box = {fx, cy - body / 2, fw, body};
    if (filled) {
        ink_music_fill(top, gray, fb);
        ink_music_fill(box, gray, fb);
    } else {
        ink_music_frame(fb, top, 0, gray);
        ink_music_frame(fb, box, 0, gray);
    }
}
