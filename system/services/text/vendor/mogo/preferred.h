/* ink: Mogo's preferred-reading filter, copied from Mogo 864f2f9 without changes (SOURCE.md). */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
/* ---- main/ui/ui_ime.c:1-4 ---- */
/* SPDX-License-Identifier: Apache-2.0
 * 中文：有界动态规划组词；常用词优先，未知词可选单字前缀，选词不丢剩余拼音。
 * English: Bounded word composition; prefer common phrases, fall back to character prefixes and retain unconsumed Pinyin.
 */
/* ---- main/ui/ui_ime.c:91-93 ---- */
static bool preferred_ok(const char *roman, const char *preferred, size_t at) {
    return at || !preferred || !preferred[0] || !strncmp(roman, preferred, strlen(preferred));
}
