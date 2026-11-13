/* LilyPhone's phone_number_match, contacts_find_by_number and contacts_display_name (vendor/lilyphone/phone_store.c)
 * on the host. LilyPhone has no host tests of its own (its test/ holds a README only). phone-3-match-reverse builds the
 * same checks against MATCH_DIGITS 7, LilyPhone's original value: the 138/139 case must fail there. */
#include <stdio.h>
#include <string.h>
#include "lily_compat.h"

static int failures;
static void same(const char *a, const char *b, bool expected, const char *tag) {
    bool got = phone_number_match(a, b), back = phone_number_match(b, a);
    if (got != expected || back != expected) { printf("FAIL %s: %s vs %s gave %d/%d\n", tag, a, b, got, back); failures++; }
    else printf("ok %s: %s vs %s\n", tag, a, b);
}

int main(void) {
    same("+8613812345678", "13812345678", true, "+86");
    same("008613812345678", "13812345678", true, "0086");
    same("138 1234 5678", "13812345678", true, "spaces");
    same("138-1234-5678", "+86 138 1234 5678", true, "dashes");
    same("10086", "10086", true, "service number");
    same("10086", "010086", false, "service number needs all its digits");
    same("10086", "+8610086", false, "service number with a country code");
    same("62345678", "01062345678", false, "landline with and without the area code");
    same("13812345678", "13912345678", false, "138/139");
    same("", "13812345678", false, "empty");
    static contact_t book[2] = {{"妈妈", "138 1234 5678"}, {"Phillips Thomas", "+86 139 0000 1111"}};
    lily_contacts_bind(book, 2);
    const char *name = contacts_display_name("+8613812345678");
    if (strcmp(name, "妈妈")) { printf("FAIL caller name: %s\n", name); failures++; }
    if (contacts_find_by_number("13900001111") != 1) { printf("FAIL second contact\n"); failures++; }
    if (strcmp(contacts_display_name("13700000000"), "13700000000")) { printf("FAIL unknown number keeps the number\n"); failures++; }
    printf("%d failures\n", failures);
    return failures != 0;
}
