/*
 * Assets in the simulator: host files from the --assets directories, loaded
 * once and treated as flash-mapped (no heap cost). The total must fit the
 * profile's assets partition.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "sim.h"

#define MAX_ASSETS 64

static struct {
    char name[96];
    void *data;
    size_t size;
    int maps;
} AS[MAX_ASSETS];
static int nAS;
static size_t total_bytes;

const void *hal_asset_map(const char *name, size_t *size) {
    for (int i = 0; i < nAS; i++)
        if (!strcmp(AS[i].name, name)) {
            AS[i].maps++;
            if (size) *size = AS[i].size;
            return AS[i].data;
        }
    if (nAS >= MAX_ASSETS || strstr(name, "..")) return NULL;
    for (int d = 0; d < g_sim.nasset_dirs; d++) {
        char path[600];
        snprintf(path, sizeof path, "%s/%s", g_sim.asset_dirs[d], name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *buf = malloc((size_t)n ? (size_t)n : 1);
        if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
            fclose(f);
            free(buf);
            return NULL;
        }
        fclose(f);
        snprintf(AS[nAS].name, sizeof AS[nAS].name, "%s", name);
        AS[nAS].data = buf;
        AS[nAS].size = (size_t)n;
        AS[nAS].maps = 1;
        nAS++;
        total_bytes += (size_t)n;
        sim_metric_set("assets.mapped_bytes", (double)total_bytes);
        if (g_profile.assets_partition_bytes && total_bytes > g_profile.assets_partition_bytes)
            sim_warn("assets: %zu KiB mapped exceeds the %zu KiB assets partition of this device", total_bytes / 1024,
                     g_profile.assets_partition_bytes / 1024);
        sim_charge_ns_always(SIM_US(200)); /* partition table lookup + MMU mapping */
        if (size) *size = (size_t)n;
        return buf;
    }
    sim_warn("asset '%s' not found in the --assets directories", name);
    return NULL;
}

void hal_asset_unmap(const void *data) {
    for (int i = 0; i < nAS; i++)
        if (AS[i].data == data && AS[i].maps > 0) AS[i].maps--;
}
