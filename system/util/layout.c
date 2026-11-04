/* Paths of the data areas (see ink/sys/layout.h). */
#include <stdio.h>
#include <string.h>

#include "ink/sys/layout.h"

bool ink_layout_member_ok(const char *id) {
    size_t n = id ? strlen(id) : 0;
    if (n < 1 || n >= INK_MEMBER_ID_MAX) return false;
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

bool ink_layout_name_ok(const char *name) {
    size_t n = name ? strlen(name) : 0;
    if (n < 1 || n > 63 || name[0] == '.') return false;
    for (size_t i = 0; i < n; i++)
        if (name[i] == '/' || name[i] == '\\' || (unsigned char)name[i] < 0x20) return false;
    return true;
}

static int checked(int n, size_t cap) { return n > 0 && (size_t)n < cap ? n : -1; }

int ink_layout_sys(char *out, size_t cap, const char *name) {
    if (!ink_layout_name_ok(name)) return -1;
    return checked(snprintf(out, cap, "/user/sys/%s", name), cap);
}

int ink_layout_member(char *out, size_t cap, const char *member, const char *name) {
    if (!ink_layout_member_ok(member) || !ink_layout_name_ok(name)) return -1;
    return checked(snprintf(out, cap, "/user/m/%s/%s", member, name), cap);
}

int ink_layout_member_files(char *out, size_t cap, const char *member) {
    if (!ink_layout_member_ok(member)) return -1;
    return checked(snprintf(out, cap, "/content/.m/%s", member), cap);
}
