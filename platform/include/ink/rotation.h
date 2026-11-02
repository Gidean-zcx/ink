/*
 * Mapping between native panel coordinates and logical (UI) coordinates.
 *
 * Convention: hal_panel_info_t.rotation_deg is the CLOCKWISE rotation that
 * turns the native image into the logical one. The Plus panel is natively
 * 1216x684 (landscape) and used in portrait: rotation 90 -> logical 684x1216.
 * Shared by the display/input services and by the simulator, so touch,
 * rendering and screenshots always agree.
 */
#ifndef INK_ROTATION_H
#define INK_ROTATION_H

static inline void ink_logical_size(int native_w, int native_h, int rot, int *lw, int *lh) {
    if (rot == 90 || rot == 270) {
        *lw = native_h;
        *lh = native_w;
    } else {
        *lw = native_w;
        *lh = native_h;
    }
}

/* native (nx, ny) -> logical (lx, ly) */
static inline void ink_native_to_logical(int native_w, int native_h, int rot, int nx, int ny, int *lx, int *ly) {
    switch (rot) {
    case 90: *lx = native_h - 1 - ny; *ly = nx; break;
    case 180: *lx = native_w - 1 - nx; *ly = native_h - 1 - ny; break;
    case 270: *lx = ny; *ly = native_w - 1 - nx; break;
    default: *lx = nx; *ly = ny; break;
    }
}

/* logical (lx, ly) -> native (nx, ny) */
static inline void ink_logical_to_native(int native_w, int native_h, int rot, int lx, int ly, int *nx, int *ny) {
    switch (rot) {
    case 90: *nx = ly; *ny = native_h - 1 - lx; break;
    case 180: *nx = native_w - 1 - lx; *ny = native_h - 1 - ly; break;
    case 270: *nx = native_w - 1 - ly; *ny = lx; break;
    default: *nx = lx; *ny = ly; break;
    }
}

#endif
