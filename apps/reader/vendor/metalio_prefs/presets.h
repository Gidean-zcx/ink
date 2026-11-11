/* Metalio 874538e, MIT; see SOURCE.md. */
#ifndef INK_METALIO_PREFS_H
#define INK_METALIO_PREFS_H
enum {
    kBookReaderSpacingPresetCount = 4,
    kBookReaderParaGapPresetCount = 4,
    kBookReaderMarginPresetCount = 4,
    kBookReaderUnderlineOff = 0,
    kBookReaderUnderlineSolid = 1,
    kBookReaderUnderlineDashed = 2,
    kBookReaderUnderlineModeCount = 3,
    kBookReaderFooterTitle = 1 << 0,     // 章节名称
    kBookReaderFooterProgress = 1 << 1,  // 百分比 + 页码
    kBookReaderFooterBattery = 1 << 2,   // 电量
    kBookReaderFooterTime = 1 << 3,      // 时间
    kBookReaderFooterAll = kBookReaderFooterTitle | kBookReaderFooterProgress |
                           kBookReaderFooterBattery | kBookReaderFooterTime,
    kBookReaderFooterDefault = kBookReaderFooterTitle | kBookReaderFooterProgress,
};

static const int kDefaultSpacingPreset = 1;  // 标准
static const int kDefaultParaGapPreset = 1; // 标准，56px
static const int kDefaultMarginPreset = 1;   // 标准
static const int kDefaultFooterMask = kBookReaderFooterDefault;
struct BookMarginPreset {
    int left;
    int right;
    int top;
    int bottom;
};

static const int kSpacingTable[kBookReaderSpacingPresetCount] = {0, 10, 25, 40}; // 行间留白占字体行框的百分比
static const int kParaGapTable[kBookReaderParaGapPresetCount] = {32, 56, 80, 104}; // 段前额外留白，单位像素

// 成套页边距：与分页 viewport 共用，避免只改 UI pad 导致裁切/留白不一致
static const struct BookMarginPreset kMarginTable[kBookReaderMarginPresetCount] = {
    {16, 16, 16, 12},
    {24, 24, 24, 16},
    {40, 40, 36, 24},
    {56, 56, 48, 32},
};

#endif
