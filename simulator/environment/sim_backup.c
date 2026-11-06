/* Synthetic hub blob/backup model. Called in host event context, never on a device task stack. */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cJSON.h"
#include "ink/sys/backup_format.h"
#include "ink/sys/sha256.h"
#include "sim.h"

#define BLOCK 16384u
static struct {
    struct hal_ws *ws;
    char member[16], identity[33], sha[65];
    uint32_t size;
    uint16_t id;
} PUT;
static uint16_t NEXT_ID, XFER;
static unsigned FAULTS;
/* Faults are test-only, configured in event context. */
void sim_backup_faults(unsigned faults) { FAULTS = faults; }

static bool valid(const char *s, bool hash) {
    if (!s || (hash ? strlen(s) != 64 : !*s || strlen(s) > 15)) return false;
    for (const char *p = s; *p; p++) if (hash ? !((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')) :
        !((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return false;
    return true;
}
static const char *str(const cJSON *o, const char *key) { return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(o, key)); }
static bool num(const cJSON *o, const char *key, uint32_t *out) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(n) || n->valuedouble < 0 || n->valuedouble > UINT32_MAX || n->valuedouble != (uint32_t)n->valuedouble) return false;
    *out = (uint32_t)n->valuedouble; return true;
}
static void digest(const uint8_t *p, char out[65]) {
    static const char h[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[i * 2] = h[p[i] >> 4]; out[i * 2 + 1] = h[p[i] & 15]; }
    out[64] = 0;
}
static bool valid_identity(const char *s) {
    if (!s || strlen(s) != 32) return false;
    for (int i = 0; i < 32; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}
static bool path(char out[1024], const char *member, const char *identity, const char *sha, const char *suffix) {
    const char *root = getenv("INK_SIM_BACKUP_DIR");
    if (!root || !valid(member, false) || !valid_identity(identity) || (sha && !valid(sha, true))) return false;
    int n = snprintf(out, 1024, "%s/%s", root, member);
    if (n < 0 || n >= 1024 || (mkdir(out, 0700) != 0 && errno != EEXIST)) return false;
    n = snprintf(out, 1024, "%s/%s/%s", root, member, identity);
    if (n < 0 || n >= 1024 || (mkdir(out, 0700) != 0 && errno != EEXIST)) return false;
    n = snprintf(out, 1024, "%s/%s/%s/%s%s", root, member, identity, sha ? sha : "", suffix);
    return n > 0 && n < 1024;
}
static int64_t size_of(const char *name) { struct stat st; return stat(name, &st) == 0 && S_ISREG(st.st_mode) ? st.st_size : -1; }
static bool file_hash(const char *name, char out[65]) {
    FILE *f = fopen(name, "rb"); if (!f) return false;
    uint8_t *buf = sim_host_malloc(BLOCK), h[32];
    ink_sha256_t sha; ink_sha256_init(&sha);
    size_t n;
    while ((n = fread(buf, 1, BLOCK, f)) > 0) ink_sha256_update(&sha, buf, n);
    bool ok = !ferror(f); if (fclose(f) != 0) ok = false;
    sim_host_free(buf);
    if (ok) { ink_sha256_final(&sha, h); digest(h, out); }
    return ok;
}
static bool write_file(const char *name, const void *data, size_t len, const char *mode) {
    FILE *f = fopen(name, mode); if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f) != 0) ok = false;
    return ok;
}
static void send(struct hal_ws *ws, uint64_t t, const cJSON *msg) {
    char *text = cJSON_PrintUnformatted(msg);
    if (text) sim_net_hub_send_at(ws, t, text, strlen(text), false);
    cJSON_free(text);
}
static cJSON *response(const cJSON *id, cJSON **result) {
    cJSON *r = cJSON_CreateObject(); cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddItemToObject(r, "id", cJSON_Duplicate(id, true)); *result = cJSON_AddObjectToObject(r, "result"); return r;
}
static void error(struct hal_ws *ws, uint64_t t, const cJSON *id) {
    cJSON *r = cJSON_CreateObject(); cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddItemToObject(r, "id", cJSON_Duplicate(id, true));
    cJSON *e = cJSON_AddObjectToObject(r, "error"); cJSON_AddNumberToObject(e, "code", -32004);
    cJSON_AddStringToObject(e, "message", "synthetic backup unavailable"); send(ws, t + SIM_MS(5), r); cJSON_Delete(r);
}
static bool manifest_complete(const char *name, const char *member, const char *identity) {
    int64_t n = size_of(name);
    if (n < (int64_t)offsetof(ink_backup_manifest_t, item) || n > (int64_t)sizeof(ink_backup_manifest_t)) return false;
    ink_backup_manifest_t *m = sim_host_calloc(1, sizeof *m);
    FILE *f = fopen(name, "rb");
    bool ok = f && fread(m, 1, (size_t)n, f) == (size_t)n;
    if (f && fclose(f) != 0) ok = false;
    ok = ok && m->magic == INK_BACKUP_MAGIC && m->format == 2 && m->count <= INK_BACKUP_ITEMS &&
         n == (int64_t)offsetof(ink_backup_manifest_t, item) + (int64_t)m->count * (int64_t)sizeof(ink_backup_item_t) && memchr(m->member, 0, sizeof m->member) && !strcmp(m->member, member);
    char owned[65];
    /* digest expands 32 bytes; the identity is exactly its first 16 bytes. */
    uint8_t padded[32] = {0}; memcpy(padded, m->identity, 16); digest(padded, owned); owned[32] = 0;
    ok = ok && !strcmp(owned, identity);
    for (unsigned i = 0; ok && i < m->count; i++) {
        char sha[65], filename[1024], actual[65]; digest(m->item[i].hash, sha);
        ok = path(filename, member, identity, sha, ".blob") && size_of(filename) == m->item[i].size && file_hash(filename, actual) && !strcmp(actual, sha);
    }
    sim_host_free(m); return ok;
}

bool sim_backup_frame(struct hal_ws *ws, uint64_t t, const uint8_t *d, size_t len) {
    if (!getenv("INK_SIM_BACKUP_DIR") || len < 16 || d[0] != 7 || d[1]) return false;
    uint16_t id = (uint16_t)(d[10] | d[11] << 8);
    uint32_t off = (uint32_t)d[12] | (uint32_t)d[13] << 8 | (uint32_t)d[14] << 16 | (uint32_t)d[15] << 24;
    char name[1024];
    if (PUT.ws != ws || id != PUT.id || len - 16 > BLOCK || off > PUT.size || len - 16 > PUT.size - off ||
        !path(name, PUT.member, PUT.identity, PUT.sha, ".part") || size_of(name) != off) { sim_metric_add("backup.mock_rejected_frame", 1); return true; }
    if (!write_file(name, d + 16, len - 16, "ab")) return true;
    sim_metric_add("backup.mock_uploaded_bytes", (double)(len - 16));
    if (off + len - 16 != PUT.size && (off + len - 16) % 65536u) return true;
    if (FAULTS & 1) { FAULTS &= ~1u; sim_metric_add("backup.mock_lost_ack", 1); return true; }
    cJSON *r = cJSON_CreateObject(); cJSON_AddStringToObject(r, "jsonrpc", "2.0"); cJSON_AddStringToObject(r, "method", "blob/put.ack");
    cJSON *p = cJSON_AddObjectToObject(r, "params"); cJSON_AddStringToObject(p, "identity", PUT.identity); cJSON_AddNumberToObject(p, "upload", id); cJSON_AddNumberToObject(p, "offset", off + len - 16);
    send(ws, t + SIM_MS(5), r); cJSON_Delete(r); return true;
}

bool sim_backup_request(struct hal_ws *ws, uint64_t t, const char *method, const cJSON *params, const cJSON *id) {
    if (!getenv("INK_SIM_BACKUP_DIR") || (strncmp(method, "blob/", 5) && strncmp(method, "backup/", 7))) return false;
    const char *member = str(params, "member"), *identity = str(params, "identity"), *sha = str(params, "sha256");
    char name[1024], part[1024], actual[65];
    cJSON *value, *r = response(id, &value);
    bool ok = false;
    if (!valid(member, false) || !valid_identity(identity)) goto done;
    cJSON_AddStringToObject(value, "identity", (FAULTS & 8) ? "00000000000000000000000000000000" : identity);
    if (!strcmp(method, "blob/put.begin")) {
        uint32_t size;
        if (valid(member, false) && valid(sha, true) && num(params, "size", &size) && size <= 64u * 1024u * 1024u &&
            path(name, member, identity, sha, ".blob") && path(part, member, identity, sha, ".part")) {
            int64_t off = size_of(name);
            if (off < 0) {
                off = size_of(part);
                if (off < 0 || off > size) { if (!write_file(part, "", 0, "wb")) goto done; off = 0; }
            }
            PUT.ws = ws; snprintf(PUT.member, sizeof PUT.member, "%s", member); snprintf(PUT.sha, sizeof PUT.sha, "%s", sha);
            snprintf(PUT.identity, sizeof PUT.identity, "%s", identity);
            PUT.size = size; if (!++NEXT_ID) NEXT_ID++; PUT.id = NEXT_ID;
            cJSON_AddNumberToObject(value, "upload", PUT.id); cJSON_AddNumberToObject(value, "offset", (double)off); ok = true;
            if (off > 0) sim_metric_add("backup.mock_resumed_upload", 1);
        }
    } else if (!strcmp(method, "blob/put.end")) {
        uint32_t upload;
        if (num(params, "upload", &upload) && upload == PUT.id && PUT.ws == ws && !strcmp(member, PUT.member) && !strcmp(identity, PUT.identity) &&
            path(name, PUT.member, PUT.identity, PUT.sha, ".blob") && path(part, PUT.member, PUT.identity, PUT.sha, ".part")) {
            bool complete = size_of(name) == PUT.size && file_hash(name, actual) && !strcmp(actual, PUT.sha);
            if (!complete) complete = size_of(part) == PUT.size && file_hash(part, actual) && !strcmp(actual, PUT.sha) && rename(part, name) == 0;
            if (complete) { char blob[80]; snprintf(blob, sizeof blob, "sha256:%s", PUT.sha); cJSON_AddStringToObject(value, "id", blob); ok = true; }
        }
    } else if (!strcmp(method, "blob/status")) {
        const char *blob = str(params, "id");
        if (blob && !strncmp(blob, "sha256:", 7) && path(name, member, identity, blob + 7, ".blob") && size_of(name) >= 0) {
            cJSON_AddBoolToObject(value, "durable", !(FAULTS & 4)); ok = true;
        }
    } else if (!strcmp(method, "blob/fetch")) {
        const char *blob = str(params, "id"); uint32_t off, len;
        if (blob && !strncmp(blob, "sha256:", 7) && path(name, member, identity, blob + 7, ".blob") && num(params, "offset", &off) && num(params, "len", &len) && len <= BLOCK &&
            size_of(name) >= 0 && off <= (uint64_t)size_of(name) && len <= (uint64_t)size_of(name) - off) {
            uint8_t *frame = sim_host_calloc(1, len + 16); FILE *f = fopen(name, "rb");
            bool read_ok = f && fseek(f, off, SEEK_SET) == 0 && fread(frame + 16, 1, len, f) == len;
            if (f && fclose(f) != 0) read_ok = false;
            if (!read_ok) { sim_host_free(frame); goto done; }
            if (!++XFER) XFER++;
            cJSON_AddNumberToObject(value, "size", (double)size_of(name)); cJSON_AddStringToObject(value, "sha256", blob + 7); cJSON_AddNumberToObject(value, "xfer", XFER);
            frame[0] = 6; frame[8] = 1; frame[10] = (uint8_t)XFER; frame[11] = (uint8_t)(XFER >> 8);
            for (int i = 0; i < 4; i++) frame[12 + i] = (uint8_t)(off >> (8 * i));
            if (FAULTS & 2) { FAULTS &= ~2u; if (len) frame[16] ^= 1; }
            send(ws, t + SIM_MS(5), r);
            sim_net_hub_send_at(ws, t + SIM_MS(8), frame, len + 16, true);
            sim_metric_add("backup.mock_downloaded_bytes", len);
            if (off) sim_metric_add("backup.mock_resumed_download", 1);
            sim_host_free(frame); cJSON_Delete(r); return true;
        }
    } else if (!strcmp(method, "backup/commit")) {
        uint32_t size;
        if (valid(sha, true) && path(name, member, identity, sha, ".blob") && num(params, "size", &size) && size_of(name) == size &&
            manifest_complete(name, member, identity) && path(part, member, identity, NULL, "latest.tmp") && path(name, member, identity, NULL, "latest")) {
            char data[128]; int n = snprintf(data, sizeof data, "%s %u\n", sha, size);
            if (n > 0 && (size_t)n < sizeof data && write_file(part, data, (size_t)n, "wb") && rename(part, name) == 0) {
                cJSON_AddBoolToObject(value, "received", true); cJSON_AddBoolToObject(value, "durable", !(FAULTS & 4));
                cJSON_AddStringToObject(value, "snapshot", sha); ok = true;
                sim_metric_add("backup.mock_committed", 1);
            }
        }
    } else if (!strcmp(method, "backup/latest")) {
        if (path(name, member, identity, NULL, "latest")) {
            FILE *f = fopen(name, "rb"); char data[128], hash[65]; unsigned size;
            bool read_ok = f && fgets(data, sizeof data, f) != NULL;
            if (f && fclose(f) != 0) read_ok = false;
            if (read_ok && sscanf(data, "%64s %u", hash, &size) == 2 && valid(hash, true)) {
                cJSON_AddStringToObject(value, "sha256", hash); cJSON_AddNumberToObject(value, "size", size); ok = true;
            }
        }
    }
done:
    if (ok) send(ws, t + SIM_MS(5), r); else error(ws, t, id);
    cJSON_Delete(r); return true;
}
