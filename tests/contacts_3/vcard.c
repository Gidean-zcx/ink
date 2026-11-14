/* contacts-3-vcard: the vCard reader and writer on the host (system/apps/contacts/contacts_vcf.c), with the five-card fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../system/apps/contacts/contacts_vcf.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
enum { MAX = 16 };
static ink_vcard_t got[MAX];
static ink_vcf_status_t status[MAX];
static int dropped[MAX], count;
static void collect(void *ctx, const ink_vcard_t *card, ink_vcf_status_t s, int extra) {
    (void)ctx; if (count < MAX) { got[count] = *card; status[count] = s; dropped[count] = extra; } count++;
}
static void parse(const char *text) { count = 0; memset(got, 0, sizeof got); ink_vcf_parse(text, strlen(text), collect, NULL); }
static char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    char *buf = calloc(1, 65536); size_t n = buf ? fread(buf, 1, 65535, f) : 0;
    fclose(f); *len = n; return buf;
}

int main(int argc, char **argv) {
    size_t len = 0; char *five = argc > 1 ? slurp(argv[1], &len) : NULL;
    CHECK(five && len > 0, "read the fixture %s", argc > 1 ? argv[1] : "(none)");
    if (!five) return 1;
    count = 0; ink_vcf_parse(five, len, collect, NULL);
    CHECK(count == 5, "five cards (%d)", count);
    /* 1: vCard 2.1, the name in QUOTED-PRINTABLE with a soft line break, a bare CELL type */
    CHECK(status[0] == INK_VCF_OK && !strcmp(got[0].name, "测试乙"), "2.1 QUOTED-PRINTABLE name (%s)", got[0].name);
    CHECK(got[0].count == 1 && !strcmp(got[0].num[0], "138 0000 0031") && got[0].type[0] == INK_VC_MOBILE && got[0].def == -1, "TEL;CELL is a mobile number");
    /* 2: 3.0 TYPE=WORK, a folded note with escapes */
    CHECK(status[1] == INK_VCF_OK && !strcmp(got[1].name, "测试丙") && got[1].type[0] == INK_VC_WORK, "3.0 TYPE=WORK,VOICE is work");
    CHECK(!strcmp(got[1].note, "第一行\n第二行, 带逗号接着写"), "folded note, \\n and \\, decoded (%s)", got[1].note);
    /* 3: 4.0, item1. group, quoted TYPE, a tel: URI with pref, a long PHOTO stepped over */
    CHECK(status[2] == INK_VCF_OK && got[2].count == 2 && got[2].type[0] == INK_VC_MOBILE && got[2].type[1] == INK_VC_HOME, "item1.TEL and TYPE=\"cell\"; home");
    CHECK(!strcmp(got[2].num[1], "+86-010-0000-0034") && got[2].def == 1, "VALUE=uri tel: number is the default (%s, %d)", got[2].num[1], got[2].def);
    CHECK(status[3] == INK_VCF_OK && !strcmp(got[3].num[0], "+86 138 0000 0021"), "the fourth card reads (the importer finds it a duplicate)");
    CHECK(status[4] == INK_VCF_BAD, "a card without END:VCARD is not understood");
    free(five);

    /* names: N alone (Chinese together, English with a space), ORG alone */
    parse("BEGIN:VCARD\nN:张;三;;;\nTEL:00000000101\nEND:VCARD\nBEGIN:VCARD\nN:Smith;John;;;\nTEL:00000000102\nEND:VCARD\n"
          "BEGIN:VCARD\nORG:合成公司;部门\nTEL:00000000103\nEND:VCARD\n");
    CHECK(count == 3 && !strcmp(got[0].name, "张三") && !strcmp(got[1].name, "John Smith") && !strcmp(got[2].name, "合成公司"), "N and ORG names (%s|%s|%s)", got[0].name, got[1].name, got[2].name);
    /* types: FAX and PAGER and a custom one are other; only VOICE is other */
    parse("BEGIN:VCARD\nFN:类型\nTEL;TYPE=HOME,FAX:00000000201\nTEL;TYPE=PAGER:00000000202\nTEL;TYPE=VOICE:00000000203\nTEL;TYPE=X-CUSTOM:00000000204\nTEL;TYPE=HOME:00000000205\nEND:VCARD\n");
    CHECK(got[0].count == 5 && got[0].type[0] == INK_VC_OTHER && got[0].type[1] == INK_VC_OTHER && got[0].type[2] == INK_VC_OTHER && got[0].type[3] == INK_VC_OTHER && got[0].type[4] == INK_VC_HOME, "fax, pager, voice, custom are other");
    /* six numbers: five kept, one counted */
    parse("BEGIN:VCARD\nFN:六个\nTEL:00000000301\nTEL:00000000302\nTEL:00000000303\nTEL:00000000304\nTEL:00000000305\nTEL:00000000306\nEND:VCARD\n");
    CHECK(got[0].count == 5 && dropped[0] == 1, "five numbers and one dropped (%d, %d)", got[0].count, dropped[0]);
    /* not UTF-8, no number, no END, a line with nothing on it */
    parse("BEGIN:VCARD\nFN;CHARSET=GBK:\xd5\xc5\xc8\xfd\nTEL:00000000401\nEND:VCARD\nBEGIN:VCARD\nFN:没有号码\nEMAIL:a@example.com\nEND:VCARD\n");
    CHECK(count == 2 && status[0] == INK_VCF_BAD && status[1] == INK_VCF_NO_NUMBER, "GBK is not understood, no number is its own case");
    parse("BEGIN:VCARD\nFN:\xff\xfe\nTEL:00000000402\nEND:VCARD\n");
    CHECK(count == 1 && status[0] == INK_VCF_BAD, "bytes that are not UTF-8");
    /* long name and note are cut to 32 and 64 characters, between characters */
    char text[2048], name[200] = "", note[400] = "";
    for (int i = 0; i < 40; i++) strcat(name, "名");
    for (int i = 0; i < 70; i++) strcat(note, "注");
    snprintf(text, sizeof text, "BEGIN:VCARD\nFN:%s\nNOTE:%s\nTEL:00000000501\nEND:VCARD\n", name, note);
    parse(text);
    CHECK(strlen(got[0].name) == 32 * 3 && strlen(got[0].note) == 64 * 3, "32 and 64 characters (%zu, %zu)", strlen(got[0].name), strlen(got[0].note));

    /* writing: 3.0, CRLF, folded at 75 bytes between characters, read back the same */
    ink_vcard_t c = {.name = "测试甲;逗号,好", .count = 2, .def = 1, .star = true};
    snprintf(c.num[0], sizeof c.num[0], "138 0000 0021"); c.type[0] = INK_VC_MOBILE;
    snprintf(c.num[1], sizeof c.num[1], "010 0000 0022"); c.type[1] = INK_VC_WORK;
    snprintf(c.note, sizeof c.note, "%s", "这是一条比较长的备注，要折成好几行才放得下，而且折行的地方不能切断汉字。第二句也一样长。");
    char out[4096]; int n = ink_vcf_write(out, sizeof out, &c);
    CHECK(n > 0 && strstr(out, "VERSION:3.0\r\n") && strstr(out, "TEL;TYPE=WORK,PREF:010 0000 0022\r\n") && strstr(out, "TEL;TYPE=CELL:138 0000 0021\r\n"), "3.0 with TYPE and PREF");
    CHECK(strstr(out, "FN:测试甲\\;逗号\\,好\r\n") && !strstr(out, "STAR") && !strstr(out, "X-"), "escaped name; the star is not written");
    bool short_lines = true, whole_chars = true;
    for (char *line = out; *line;) {
        char *end = strstr(line, "\r\n"); if (!end) { short_lines = false; break; }
        short_lines &= end - line <= 75;
        if (*line == ' ') whole_chars &= (line[1] & 0xC0) != 0x80;
        line = end + 2;
    }
    CHECK(short_lines && whole_chars && strstr(out, "\r\n "), "folded at 75 bytes, never inside a character");
    parse(out);
    CHECK(count == 1 && status[0] == INK_VCF_OK && !strcmp(got[0].name, c.name) && !strcmp(got[0].note, c.note) && got[0].count == 2 && got[0].def == 1 &&
          !strcmp(got[0].num[0], c.num[0]) && got[0].type[1] == INK_VC_WORK, "read back the same (%s | %s)", got[0].name, got[0].note);
    CHECK(ink_vcf_write(out, 40, &c) == -1, "a buffer too small says so");
    CHECK(ink_vcard_number_ok("+86 (010) 0000-0001") && !ink_vcard_number_ok("12+3") && !ink_vcard_number_ok("abc") && !ink_vcard_number_ok("1234567890123456789012345"), "the number rule");
    printf("contacts-3-vcard: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
