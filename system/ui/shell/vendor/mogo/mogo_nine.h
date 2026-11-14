/* ink: Mogo's nine-key reading list and composition line, copied from Mogo 864f2f9 without changes (SOURCE.md).
 * ---- ink shim (written for ink, not Mogo's): the copied code reads the keyboard's state by Mogo's names. ---- */
#include <stdbool.h>
#include <stdio.h>
#include "mogo_syllables.h"
#define UI_IME_RAW_MAX 48
#define s_raw ink_text_composing()
#define s_chinese (ink_text_type() == INK_TEXT_PLAIN && !ink_text_english())
#define s_nine (ink_setting_get("text.nine_key") != 0)
#define s_syllable_offset K.reading_offset
#define s_best_roman ink_text_preferred_reading()
#define s_best_consume strlen(ink_text_preferred_reading())
bool ui_ime_digits(const char *roman, char *out, size_t cap);
/* ---- main/ui/ui_keyboard.c:1-14 ---- */
/* SPDX-License-Identifier: Apache-2.0
 * 中文：九宫格、全键盘、数字符号与中英文共用交互；键盘常亮，不做闪烁刷新。
 * English: Shared T9, QWERTY, symbols and bilingual input; steady keys without timed screen flashing.
 * 用户修订：普通输入只画变化区域，布局切换才重画键盘，不累计整页清屏次数。
 * User revision: paint changed input regions; repaint keys only on layout switches, without counting whole-page cleanup.
 * 用户修订：九宫格只输入中文拼音，英文使用全键盘；数字仅在数字页输入，内部九键编码不展示。
 * User revision: Chinese Pinyin only in T9, English in QWERTY, digits only in the numeric panel; never display internal T9 codes.
 * 用户修订：删除键长按连续删除；抬手、移开、多点或输入会话结束立即取消，仅局部快刷。
 * 用户修订：按键按下加深、下沉两像素，松开复原，只刷该键；同音字可继续翻页。
 * User revision: darken/inset a pressed key, restore on release with local updates, and page through all homophones.
 * User revision: held backspace repeats; release, movement, multitouch or session exit cancels immediately, with local fast updates only.
 * 用户修订：键盘与候选使用白底黑字；候选变化三次、停手400ms后仅整理词栏，不打断连打、不全屏黑闪。
 * User revision: white keys/candidates with black text; settle the word strip after three changes and a 400ms pause, without interrupting typing or flashing the whole screen.
 */
/* ---- main/ui/ui_keyboard.c:48 ---- */
static const char *const groups[] = {"", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};
/* ---- main/ui/ui_keyboard.c:161-192 ---- */
static size_t syllables(const char *out[4], bool *more) {
    size_t count = 0, seen = 0;
    if (more) *more = false;
    if (!s_chinese || !s_nine || !s_raw[0]) return 0;
    // 先展示匹配最长的完整读音；更多键可遍历全部读音，避免只保留四种解释。
    // Prefer longer matching syllables; More cycles through every reading instead of keeping only four.
    for (size_t length = 8; length; --length) for (size_t i = 0;; ++i) {
        const char *word = book_search_syllable(i); if (!word) break;
        if (strlen(word) != length) continue;
        char digits[16]; if (!ui_ime_digits(word, digits, sizeof(digits))) continue;
        if (strlen(s_raw) < length || strncmp(s_raw, digits, length)) continue;
        if (seen++ < s_syllable_offset) continue;
        if (count < 3) out[count++] = word;
        else { if (more) *more = true; return count; }
    }
    return count;
}
// 未识别的九键编码显示为字母组，避免将数字误当作拼音或可提交的输入。
// Show unresolved T9 codes as letter groups, never as Pinyin or committable digits.
static void composition_text(char out[UI_IME_RAW_MAX * 7 + 1]) {
    if (!s_nine) { snprintf(out, UI_IME_RAW_MAX * 7 + 1, "%s", s_raw); return; }
    size_t consumed = 0, used = 0, length = strlen(s_raw);
    out[0] = 0;
    if (s_best_consume <= length && s_best_consume == strlen(s_best_roman)) {
        consumed = s_best_consume; used = strlen(s_best_roman);
        memcpy(out, s_best_roman, used + 1);
    }
    for (size_t i = consumed; i < length; ++i) {
        const char *letters = s_raw[i] >= '2' && s_raw[i] <= '9' ? groups[s_raw[i] - '1'] : "?";
        used += (size_t)snprintf(out + used, UI_IME_RAW_MAX * 7 + 1 - used, "%s[%s]", used ? " " : "", letters);
    }
}
/* ---- main/ui/ui_ime.c:1-4 ---- */
/* SPDX-License-Identifier: Apache-2.0
 * 中文：有界动态规划组词；常用词优先，未知词可选单字前缀，选词不丢剩余拼音。
 * English: Bounded word composition; prefer common phrases, fall back to character prefixes and retain unconsumed Pinyin.
 */
/* ---- main/ui/ui_ime.c:47-57 ---- */
static char digit(char c) {
    static const char map[] = "22233344455566677778889999";
    return c >= 'a' && c <= 'z' ? map[c - 'a'] : 0;
}
bool ui_ime_digits(const char *roman, char *out, size_t cap) {
    if (!roman || !out || !cap) return false;
    size_t n = strlen(roman);
    if (n >= cap) return false;
    for (size_t i = 0; i < n; ++i) { out[i] = digit(roman[i]); if (!out[i]) return false; }
    out[n] = 0; return true;
}
/* ---- end of the copy ---- */
#undef s_raw
#undef s_chinese
#undef s_nine
#undef s_syllable_offset
#undef s_best_roman
#undef s_best_consume
