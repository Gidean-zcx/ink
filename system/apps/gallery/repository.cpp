/* C entry points for the AgentUI gallery helpers (verbatim in vendor/agentui). No heap for sorting. */
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "ink/osal.h"
#include "ink/sys/file.h"
#include "ink/sys/i18n.h"
#include "ink/sys/time.h"
#include "repository.h"
#include "vendor/agentui/camera_gallery_repository_part.h"

using namespace agent_ui::camera;

namespace {
struct SortKey { int64_t modified; std::string_view name; uint32_t at; };
struct Keys { SortKey *first, *last; SortKey *begin() { return first; } SortKey *end() { return last; } };
SortKey keys[INK_FS_INDEX_MAX] INK_PSRAM_BSS;
ink_gallery_item_t sorted[INK_FS_INDEX_MAX] INK_PSRAM_BSS;

struct Month { const char *date_key; };
struct Months {
    const ink_gallery_item_t *items; size_t n;
    size_t size() const { return n; }
    Month operator[](size_t i) const { return {items[i].month}; }
};

std::tm local(int64_t utc_s) {
    ink_tm_t t; ink_time_to_local(utc_s, &t);
    std::tm out = {};
    out.tm_year = t.year - 1900; out.tm_mon = t.month - 1; out.tm_mday = t.day;
    out.tm_hour = t.hour; out.tm_min = t.min; out.tm_sec = t.sec; out.tm_wday = t.wday;
    return out;
}
void put(const std::string &s, char *out, size_t cap) { if (cap) std::snprintf(out, cap, "%s", s.c_str()); }
}

extern "C" void ink_gallery_sort(ink_gallery_item_t *items, size_t n) {
    if (n > INK_FS_INDEX_MAX) n = INK_FS_INDEX_MAX;
    for (size_t i = 0; i < n; i++) keys[i] = {items[i].modified, items[i].name ? items[i].name : "", (uint32_t)i};
    Keys view = {keys, keys + n};
    SortNewestFirst(view);
    for (size_t i = 0; i < n; i++) sorted[i] = items[keys[i].at];
    std::memcpy(items, sorted, n * sizeof *items);
}
extern "C" void ink_gallery_day_label(int64_t utc_s, char *out, size_t cap) { put(FormatDayLabel(local(utc_s)), out, cap); }
extern "C" void ink_gallery_date_key(int64_t utc_s, char sep, char *out, size_t cap) { put(FormatDate(local(utc_s), sep), out, cap); }
extern "C" void ink_gallery_time_label(int64_t utc_s, char *out, size_t cap) { put(FormatTimeLabel(local(utc_s)), out, cap); }
extern "C" void ink_gallery_size_label(int64_t bytes, char *out, size_t cap) { put(FormatSizeLabel(bytes > 0 ? (std::size_t)bytes : 0), out, cap); }
extern "C" bool ink_gallery_is_jpg(const char *name) { return IsJpg(name); }
extern "C" void ink_gallery_month_groups(const ink_gallery_item_t *items, size_t n,
                                         void (*cb)(void *ctx, size_t begin, size_t end), void *ctx) {
    for (const auto &g : GroupByDateKey(Months{items, n})) cb(ctx, g.first, g.second);
}
