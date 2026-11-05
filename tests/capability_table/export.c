/* Export every registered capability, including owner-only and inactive pages. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define ink_main ink_product_main
#include "../../system/boot/main.c"
#undef ink_main

static void json_string(FILE *file, const char *text) { /* host context */
    if (fputc('"', file) == EOF) abort();
    for (const unsigned char *p = (const unsigned char *)(text ? text : ""); *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (fputc('\\', file) == EOF || fputc(*p, file) == EOF) abort();
        } else if (*p < 0x20) {
            if (fprintf(file, "\\u%04x", (unsigned)*p) < 0) abort();
        } else if (fputc(*p, file) == EOF) abort();
    }
    if (fputc('"', file) == EOF) abort();
}

static void save_on_host(void *arg) {
    const char *settings = arg;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/capability-inventory.json", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *file = fopen(path, "wb");
    if (!file) abort();
    if (fputs("{\"capabilities\":[", file) == EOF) abort();
    for (int i = 0; ink_cap_at(i); i++) {
        const ink_cap_t *c = ink_cap_at(i);
        if (fprintf(file, "%s{\"id\":", i ? "," : "") < 0) abort();
        json_string(file, c->id);
        if (fputs(",\"title\":", file) == EOF) abort();
        json_string(file, c->title);
        if (fprintf(file, ",\"effect\":%d,\"form\":%d,\"expose\":%u,\"child_ok\":%s,\"while_locked\":%s,\"ask_by_hand\":%s,\"parent_pin\":%s,\"activity\":",
                    (int)c->effect, (int)c->form, (unsigned)c->expose, c->child_ok ? "true" : "false", c->while_locked ? "true" : "false",
                    c->ask_by_hand ? "true" : "false", c->parent_pin ? "true" : "false") < 0) abort();
        json_string(file, c->activity);
        if (fputc('}', file) == EOF) abort();
    }
    if (fprintf(file, "],\"settings\":%s}\n", settings) < 0) abort();
    if (fclose(file)) abort();
}

void ink_main(void) {
    ink_product_main();
    char *settings = osal_malloc(16384, OSAL_MEM_EXTERNAL);
    if (!settings || ink_settings_json(settings, 16384) < 0) osal_panic("cannot export the settings catalogue");
    vtk_host_call(save_on_host, settings);
    osal_free(settings);
}
