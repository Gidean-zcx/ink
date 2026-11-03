/* AUD-010 probe: integer tokens and UTF-8 decoding (system/core/ink_json.c, system/text/linebreak.c) */
#include <stdio.h>
#include <string.h>
#include "ink/sys/json.h"
#include "ink/sys/text.h"
static void num(const char *doc) {
    ink_json_t j; int64_t v = -999;
    if (ink_json_parse(&j, doc, strlen(doc)) != 0) { printf("%-34s parse error\n", doc); return; }
    bool ok = ink_json_int(&j, ink_json_find(&j, 0, "page"), &v);
    printf("%-34s -> %s %lld\n", doc, ok ? "int" : "rejected", ok ? (long long)v : 0LL);
}
static void str(const char *doc) {
    ink_json_t j; char out[16];
    ink_json_parse(&j, doc, strlen(doc));
    int n = ink_json_str(&j, ink_json_find(&j, 0, "s"), out, sizeof out);
    printf("%-34s -> %d:", doc, n);
    for (int i = 0; i < n; i++) printf(" %02X", (unsigned char)out[i]);
    printf("\n");
}
static void utf8(const char *name, const char *b, size_t n) {
    size_t i = 0; printf("%-12s ->", name);
    while (i < n) printf(" U+%04X", ink_utf8_next(b, n, &i));
    printf("\n");
}
int main(void) {
    num("{\"page\":2}"); num("{\"page\":-3}"); num("{\"page\":1.5}"); num("{\"page\":1e9}"); num("{\"page\":12abc}");
    num("{\"page\":2147483648}"); num("{\"page\":99999999999999999999}"); num("{\"page\":true}"); num("{\"page\":\"2\"}");
    str("{\"s\":\"\\ud83d\\ude00\"}"); str("{\"s\":\"\\ud83d\\u0041\"}"); str("{\"s\":\"\\udc00\"}"); str("{\"s\":\"\\ud83dzz\"}");
    utf8("C0 80", "\xC0\x80", 2); utf8("ED A0 80", "\xED\xA0\x80", 3); utf8("F4 90 80 80", "\xF4\x90\x80\x80", 4);
    utf8("E4 B8 AD", "\xE4\xB8\xAD", 3); utf8("F0 9F 98 80", "\xF0\x9F\x98\x80", 4);
    return 0;
}
