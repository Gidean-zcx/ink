/* Interface languages (see ink/sys/i18n.h): the generated tables and the language setting. */
#include <stddef.h>
#include <string.h>

#include "ink/probe.h"
#include "ink/sys/i18n.h"
#include "ink/sys/settings.h"

typedef struct { const char *zh, *tr; } ink_i18n_pair_t;
typedef struct { const char *code, *name; const ink_i18n_pair_t *pairs; size_t count; } ink_i18n_lang_t;
#include "ink/i18n_table.h"

#define LANGS (sizeof I18N_LANGS / sizeof *I18N_LANGS)

static size_t lang(void) {
    int64_t v = ink_setting_get("system.language");
    return v > 0 && (size_t)v < LANGS ? (size_t)v : 0;
}

const char *ink_tr(const char *zh) {
    const ink_i18n_lang_t *l = &I18N_LANGS[lang()];
    if (!zh || !l->pairs) return zh;
    size_t lo = 0, hi = l->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(zh, l->pairs[mid].zh);
        if (!c) return l->pairs[mid].tr;
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    ink_probe_count("i18n.missing");
    return zh;
}

const char *ink_lang_code(void) { return I18N_LANGS[lang()].code; }

const char *ink_lang_name(int index) { return index >= 0 && (size_t)index < LANGS ? I18N_LANGS[index].name : NULL; }

int ink_i18n_init(void) {
    static const ink_setting_def_t LANGUAGE = {.id = "system.language", .title = TR_NOOP("语言"), .type = INK_SET_ENUM,
        .def = 0, .choices = I18N_NAMES, .system = true};
    return ink_settings_define(&LANGUAGE, 1);
}
