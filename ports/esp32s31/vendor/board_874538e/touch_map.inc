/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
static void map_to_logical(int raw_x, int raw_y, int* lx, int* ly) {
    int x = raw_x;
    int y = raw_y;
    if (raw_y >= EPD_LOGICAL_W && raw_x < EPD_LOGICAL_W) {
        x = raw_x;
        y = raw_y;
    } else if (raw_x >= EPD_LOGICAL_W) {
        x = EPD_LOGICAL_W - 1 - raw_y;
        y = raw_x;
    }
    if (x < 0) {
        x = 0;
    } else if (x >= EPD_LOGICAL_W) {
        x = EPD_LOGICAL_W - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= EPD_LOGICAL_H) {
        y = EPD_LOGICAL_H - 1;
    }
    *lx = x;
    *ly = y;
}
