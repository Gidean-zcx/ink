/* SPDX-License-Identifier: Apache-2.0
 * 中文：有界动态规划组词；常用词优先，未知词可选单字前缀，选词不丢剩余拼音。
 * English: Bounded word composition; prefer common phrases, fall back to character prefixes and retain unconsumed Pinyin.
 */
#include <stdbool.h>
#include <string.h>

static char digit(char c) {
    static const char map[] = "22233344455566677778889999";
    return c >= 'a' && c <= 'z' ? map[c - 'a'] : 0;
}

static bool prefix(const char *raw, const char *roman, bool nine) {
    size_t n = strlen(roman);
    if (strlen(raw) < n) return false;
    for (size_t i = 0; i < n; ++i) if (raw[i] != (nine ? digit(roman[i]) : roman[i])) return false;
    return true;
}
