/* Run the current product composition root; export its generated AI manifest. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

#define ink_main ink_product_main
#include "../../system/boot/main.c"
#undef ink_main

typedef struct { const char *tools, *actions; bool overflow, first, last; } export_t;

static void save_on_host(void *arg) {
    export_t *e = arg;
    char path[600];
    snprintf(path, sizeof path, "%s/caps.json", g_sim.out_dir);
    FILE *file = fopen(path, e->first ? "wb" : "ab");
    if (!file) abort();
    if (e->first && fprintf(file, "{\"overflow\":%s,\"tools\":[", e->overflow ? "true" : "false") < 0) abort();
    if (fprintf(file, "%s%.*s", e->first ? "" : ",", (int)strlen(e->tools) - 2, e->tools + 1) < 0) abort();
    if (e->last && fprintf(file, "],\"actions\":%s}\n", e->actions) < 0) abort();
    if (fclose(file)) abort();
}

void ink_main(void) {
    ink_product_main();
    char *tools = osal_malloc(65536, OSAL_MEM_EXTERNAL);
    char *actions = osal_malloc(16384, OSAL_MEM_EXTERNAL);
    char *small = osal_malloc(768, OSAL_MEM_EXTERNAL);
    if (!tools || !actions || !small || ink_cap_ids_json(actions, 16384) < 0)
        osal_panic("cannot export firmware capabilities");
    bool overflow = ink_cap_ids_json(small, 768) < 0, first = true;
    int next = 0;
    while (next >= 0) {
        if (ink_cap_manifest_page(next, NULL, tools, 65536, &next) < 2) osal_panic("cannot export firmware capabilities");
        export_t e = {tools, actions, overflow, first, next < 0};
        vtk_host_call(save_on_host, &e);
        first = false;
    }
    osal_free(tools); osal_free(actions); osal_free(small);
}
