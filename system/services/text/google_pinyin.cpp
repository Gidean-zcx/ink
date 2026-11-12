/* C session adapter; the decoder itself remains the upstream implementation. */
#include <algorithm>
#include <cctype>
#include <cstring>
#include <climits>
#include "ink/osal.h"
#include "ink/sys/file.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/probe.h"
extern "C" {
#include "text_internal.h"
}
#include "vendor/googlepinyin/include/pinyinime.h"
#include "vendor/googlepinyin/include/spellingtrie.h"
#include "vendor/kiikoread/nine_key.h"
using namespace ime_pinyin;
static constexpr int CANDIDATES = 60, BEAM = 8, LETTERS = 40;
struct Choice { char word[40], roman[LETTERS + 1]; uint8_t consumed; int id; uint16_t rank; };
struct Path { char roman[LETTERS + 1]; int cost; };
static struct {
    osal_mutex_t *lock; bool ready; char member[16];
    Choice choices[CANDIDATES]; int count; char roman[LETTERS + 1]; size_t fixed_letters, fixed_chars;
    Path paths[LETTERS + 1][BEAM]; int counts[LETTERS + 1];
} GP INK_PSRAM_BSS;
static bool current() { ink_member_identity_t who; ink_member_identity(&who); return GP.ready && who.authed && !strcmp(who.member, GP.member); }
static void lock() { osal_mutex_lock(GP.lock, OSAL_WAIT_FOREVER); }
static void unlock() { osal_mutex_unlock(GP.lock); }
static size_t utf8(const char16 *s, char *out, size_t cap) {
    size_t n = 0;
    for (; *s; s++) {
        unsigned c = *s; size_t k = c < 128 ? 1 : c < 2048 ? 2 : 3;
        if (n + k >= cap) break;
        if (k == 1) out[n++] = (char)c;
        else { if (k == 3) out[n++] = (char)(224 | (c >> 12)); out[n++] = (char)((k == 3 ? 128 : 192) | ((c >> 6) & (k == 3 ? 63 : 31))); out[n++] = (char)(128 | (c & 63)); }
    }
    out[n] = 0; return n;
}
extern "C" bool py_google_enabled(void) { return ink_setting_get("text.google") != 0; }
extern "C" void py_google_init(void) { GP.lock = osal_mutex_create(); }
extern "C" bool py_google_loaded(void) { lock(); bool ready = current(); unlock(); return ready; }
extern "C" void py_google_close(void) {
    lock(); GP.ready = false; GP.count = 0; GP.fixed_letters = GP.fixed_chars = 0; im_close_decoder(); unlock();
}
extern "C" bool py_google_open(void) {
    lock(); GP.ready = false; GP.count = 0; GP.fixed_letters = GP.fixed_chars = 0; im_close_decoder();
    ink_member_identity_t who; ink_member_identity(&who);
    if (who.authed && ink_fs_mkdir("/me/ime") == 0) {
        GP.ready = im_open_decoder("/content/ime/dict_pinyin.dat", "/me/ime/google.user");
        if (GP.ready) { strcpy(GP.member, who.member); im_set_max_lens(LETTERS, LETTERS); }
    }
    bool ready = current(); unlock(); return ready;
}
static void add(const char *roman, int id, const char16 *word, unsigned consumed, unsigned rank) {
    if (!consumed || consumed > LETTERS || GP.count == CANDIDATES) return;
    Choice c = {}; utf8(word, c.word, sizeof c.word);
    for (int i = 0; i < GP.count; i++) if (!strcmp(GP.choices[i].word, c.word) && GP.choices[i].consumed == consumed) return;
    strcpy(c.roman, roman); c.id = id; c.consumed = (uint8_t)consumed; c.rank = (uint16_t)rank;
    GP.choices[GP.count++] = c;
}
static void collect(const char *roman, int rank, size_t limit = CANDIDATES) {
    if (!GP.fixed_letters) im_reset_search();
    size_t n = im_search(roman, strlen(roman)), decoded = 0;
    strcpy(GP.roman, roman);
    im_get_sps_str(&decoded);
    const uint16 *positions; size_t syllables = im_get_spl_start_pos(positions);
    for (size_t i = 0; i < n && i < limit && GP.count < CANDIDATES; i++) {
        char16 word[LETTERS + 1] = {};
        if (!im_get_candidate(i, word, LETTERS + 1)) continue;
        size_t chars = utf16_strlen(word);
        unsigned end = i == 0 ? (unsigned)decoded : chars + GP.fixed_chars <= syllables ? positions[chars + GP.fixed_chars] : 0;
        unsigned consumed = end > GP.fixed_letters ? (unsigned)(end - GP.fixed_letters) : 0;
        add(roman, (int)i, word + (i == 0 ? GP.fixed_chars : 0), consumed, (unsigned)std::max(1, 60000 - rank - (i ? 1000 : 0) - (int)i));
    }
}
/* Retain eight legal syllable paths at each digit boundary, never a Cartesian product. */
static void nine(const char *digits) {
    size_t length = strlen(digits); memset(GP.counts, 0, sizeof GP.counts);
    GP.paths[0][0] = {}; GP.counts[0] = 1;
    SpellingTrie &trie = SpellingTrie::get_instance();
    for (size_t pos = 0; pos < length; pos++) for (int b = 0; b < GP.counts[pos]; b++) {
        Path before = GP.paths[pos][b];
        for (size_t id = kFullSplIdStart; id < kFullSplIdStart + trie.get_spelling_num(); id++) {
            char spelling[8]; const char *source = trie.get_spelling_str((uint16)id);
            size_t n = strlen(source); for (size_t i = 0; i <= n; i++) spelling[i] = (char)std::tolower((unsigned char)source[i]);
            size_t used = std::min(n, length - pos); char part[8]; memcpy(part, spelling, used); part[used] = 0;
            if (!prefix(digits + pos, part, true)) continue;
            Path next = before; memcpy(next.roman + pos, part, used + 1);
            im_reset_search(); size_t choices = im_search(next.roman, pos + used), decoded = 0; im_get_sps_str(&decoded);
            if (!choices || decoded != pos + used) continue;
            const uint16 *starts; size_t syllables = im_get_spl_start_pos(starts);
            char16 word[LETTERS + 1] = {}; im_get_candidate(choices > 1 ? 1 : 0, word, LETTERS + 1);
            next.cost = (int)syllables * 100 - (int)utf16_strlen(word) * 40 - (int)std::min(choices, (size_t)30);
            int &count = GP.counts[pos + used], at = 0;
            while (at < count && GP.paths[pos + used][at].cost <= next.cost) at++;
            bool duplicate = false;
            for (int k = 0; k < count; k++) if (!strcmp(GP.paths[pos + used][k].roman, next.roman)) duplicate = true;
            if (duplicate || at == BEAM) continue;
            if (count < BEAM) count++;
            for (int k = count - 1; k > at; k--) GP.paths[pos + used][k] = GP.paths[pos + used][k - 1];
            GP.paths[pos + used][at] = next;
        }
    }
    for (int i = 0; i < GP.counts[length]; i++) collect(GP.paths[length][i].roman, i * 100, 7);
}
extern "C" int py_google_lookup(const char *s, bool is_nine, py_cand_t *out, int max) {
    lock(); GP.count = 0;
    if (current() && strlen(s) + GP.fixed_letters <= LETTERS) {
        if (is_nine) { GP.fixed_letters = GP.fixed_chars = 0; nine(s); }
        else { char roman[LETTERS + 1]; memcpy(roman, GP.roman, GP.fixed_letters); strcpy(roman + GP.fixed_letters, s); collect(roman, 0); }
    }
    for (int i = 1; i < GP.count; i++) { Choice c = GP.choices[i]; int j = i; while (j && GP.choices[j - 1].rank < c.rank) { GP.choices[j] = GP.choices[j - 1]; j--; } GP.choices[j] = c; }
    int n = std::min(GP.count, max);
    for (int i = 0; i < n; i++) out[i] = {GP.choices[i].word, (uint16_t)strlen(GP.choices[i].word), GP.choices[i].consumed, GP.choices[i].rank};
    unlock(); return n;
}
extern "C" void py_google_pick(const char *word, unsigned consumed) {
    lock();
    if (current()) for (int i = 0; i < GP.count; i++) {
        Choice c = GP.choices[i];
        if (c.id < 0 || c.consumed != consumed || strcmp(c.word, word)) continue;
        if (!GP.fixed_letters) im_reset_search();
        im_search(c.roman, strlen(c.roman)); im_choose((size_t)c.id);
        GP.fixed_letters += c.consumed; GP.fixed_chars = im_get_fixed_len();
        if (GP.fixed_letters >= strlen(c.roman)) GP.fixed_letters = GP.fixed_chars = 0;
        im_flush_cache(); ink_probe_count("text.google_picks"); break;
    }
    unlock();
}
extern "C" int py_google_predict(const char *history, py_cand_t *out, int max) {
    lock(); GP.count = 0; GP.fixed_letters = GP.fixed_chars = 0;
    char16 chars[LETTERS + 1] = {}; size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)history; *p && n < LETTERS;) {
        unsigned c = *p++;
        if (c >= 224 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
        else if (c >= 192 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p++; }
        chars[n++] = (char16)c;
    }
    int count = 0;
    if (current() && n) {
        char16 (*words)[kMaxPredictSize + 1] = nullptr; size_t total = im_get_predicts(chars, words);
        count = (int)std::min(total, (size_t)std::min(max, CANDIDATES));
        for (int i = 0; i < count; i++) { utf8(words[i], GP.choices[i].word, sizeof GP.choices[i].word); out[i] = {GP.choices[i].word, (uint16_t)strlen(GP.choices[i].word), 0, (uint16_t)(60000 - i)}; }
    }
    unlock(); return count;
}
