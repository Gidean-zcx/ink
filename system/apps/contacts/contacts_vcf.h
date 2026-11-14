/* vCard reading and writing for contacts (our own code, after the behaviour of Fossify's VcfImporter/VcfExporter). Pure functions. */
#ifndef INK_CONTACTS_VCF_H
#define INK_CONTACTS_VCF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { INK_VC_NUMBERS = 5, INK_VC_NAME = 97, INK_VC_NUMBER = 25, INK_VC_NOTE = 193 };
enum { INK_VC_MOBILE, INK_VC_HOME, INK_VC_WORK, INK_VC_OTHER };
/* One person: what a phone.contacts record holds. def: the default number's index, -1 for none. */
typedef struct {
    char name[INK_VC_NAME], note[INK_VC_NOTE], num[INK_VC_NUMBERS][INK_VC_NUMBER];
    uint8_t type[INK_VC_NUMBERS], count;
    int8_t def;
    bool star;
} ink_vcard_t;
typedef enum { INK_VCF_OK, INK_VCF_BAD, INK_VCF_NO_NUMBER } ink_vcf_status_t;
/* Every BEGIN:VCARD .. END:VCARD in text: fn gets the card, how it read and how many numbers past five it dropped. */
void ink_vcf_parse(const char *text, size_t len, void (*fn)(void *ctx, const ink_vcard_t *card, ink_vcf_status_t status, int dropped), void *ctx);
/* One card as vCard 3.0 (UTF-8, CRLF, folded at 75 bytes between characters). Bytes written, or -1 when it does not fit. */
int ink_vcf_write(char *out, size_t cap, const ink_vcard_t *card);
/* The stored number rule: digits, spaces, -() and a leading +, at least one digit, at most 24 bytes. */
bool ink_vcard_number_ok(const char *s);
/* At most `chars` characters, cut between characters. */
void ink_vcard_cut(char *s, size_t chars);
#endif
