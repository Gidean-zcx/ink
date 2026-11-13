/**
 * Contact book and message log, kept in RAM and mirrored to SPIFFS.
 *
 * Both tables live in PSRAM when it is available; the message log alone is a
 * little over 20KB, which is more than we want to take out of internal DRAM.
 * Every mutation rewrites the whole file - the tables are small and writes are
 * rare (a message arriving, a contact being edited), so an append-and-compact
 * scheme would only buy complexity.
 */
/* Copied from LilyPhone (github.com/dparnell/lilyphone1 @de10fc3, src/apps/phone_store.cpp, by Daniel Parnell;
 * the repository has no licence file). Whole functions as they are: SOURCE.md lists every changed line. */
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "lily_compat.h"

// Numbers are considered the same contact when this many trailing digits agree.
#define MATCH_DIGITS 11

static contact_t  *contact_list INK_PSRAM_BSS;
static int         contact_num  INK_PSRAM_BSS;

bool phone_number_match(const char *a, const char *b)
{
    if(a == NULL || b == NULL) return false;

    // Compare from the back, digits only, so that "+61412345678", "0412345678"
    // and "412345678" all resolve to the same contact.
    int ia = strlen(a);
    int ib = strlen(b);
    int matched = 0;

    while(ia > 0 && ib > 0) {
        while(ia > 0 && !isdigit((unsigned char)a[ia - 1])) ia--;
        while(ib > 0 && !isdigit((unsigned char)b[ib - 1])) ib--;
        if(ia == 0 || ib == 0) break;
        if(a[ia - 1] != b[ib - 1]) return false;
        ia--;
        ib--;
        if(++matched >= MATCH_DIGITS) return true;
    }

    // Short numbers (service codes) have to agree over their whole length.
    while(ia > 0 && !isdigit((unsigned char)a[ia - 1])) ia--;
    while(ib > 0 && !isdigit((unsigned char)b[ib - 1])) ib--;
    return (ia == 0 && ib == 0 && matched > 0);
}

int contacts_find_by_number(const char *number)
{
    for(int i = 0; i < contact_num; i++) {
        if(phone_number_match(contact_list[i].number, number)) return i;
    }
    return -1;
}

const char *contacts_display_name(const char *number)
{
    int idx = contacts_find_by_number(number);
    if(idx >= 0 && contact_list[idx].name[0]) return contact_list[idx].name;
    return number;
}
/* Glue (ours, not LilyPhone): the table is in PSRAM in phone.c, filled from the member's contacts in one place. */
void lily_contacts_bind(contact_t *table, int count) { contact_list = table; contact_num = count; }
