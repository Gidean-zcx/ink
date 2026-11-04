/*
 * The two application slots, and the bootloader's part in them, as behaviour: what a slot holds and which boots lives in a file
 * next to the flash image, so that a test can run the next boot as another run. The program is the same whichever slot "runs":
 * the first bytes of what was written say which version that stands for ("SIMAPP <code> <version>"), and tests look at what
 * the system does about it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_ota.h"
#include "ink/osal.h"
#include "ink/sys/sha256.h"
#include "sim.h"

#define SLOT_SIZE (2u * 1024u * 1024u)
#define WRITE_US_PER_4K 70000ull /* an erase and a write: the stall is modelled by the storage task's guard */

static struct {
    bool loaded;
    int running, boot;
    bool pending, rolled_back;
    char version[2][24];
    uint32_t code[2];
    /* being written */
    bool writing;
    size_t size, got;
    uint8_t head[64];
    uint64_t resource_bytes;
} O;

static const char *state_path(char *out, size_t cap) {
    if (!g_sim.flash_image) return NULL;
    snprintf(out, cap, "%s.ota", g_sim.flash_image);
    return out;
}

static void save(void *arg) { /* host */
    (void)arg;
    char p[600];
    if (!state_path(p, sizeof p)) return;
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %u %u %s %s\n", O.running, O.boot, O.pending, (unsigned)O.code[0], (unsigned)O.code[1], O.version[0][0] ? O.version[0] : "-", O.version[1][0] ? O.version[1] : "-");
    fclose(f);
}

/* What the bootloader does before anything runs. */
static void load(void *arg) { /* host */
    (void)arg;
    char p[600];
    snprintf(O.version[0], sizeof O.version[0], "1.0.0");
    O.code[0] = 100;
    FILE *f = state_path(p, sizeof p) ? fopen(p, "r") : NULL;
    if (f) {
        int running = 0, boot = 0, pending = 0;
        unsigned c0 = 0, c1 = 0;
        char v0[24], v1[24];
        if (fscanf(f, "%d %d %d %u %u %23s %23s", &running, &boot, &pending, &c0, &c1, v0, v1) == 7) {
            O.running = running;
            O.boot = boot;
            O.code[0] = c0;
            O.code[1] = c1;
            snprintf(O.version[0], sizeof O.version[0], "%s", strcmp(v0, "-") ? v0 : "");
            snprintf(O.version[1], sizeof O.version[1], "%s", strcmp(v1, "-") ? v1 : "");
            if (pending) { /* it was tried and did not say it was good: back */
                O.boot = !running;
                O.running = O.boot;
                O.pending = false;
                O.rolled_back = true;
            } else if (boot != running) { /* the new one, the first time */
                O.running = boot;
                O.pending = true;
            }
        }
        fclose(f);
    }
    save(NULL);
}

static void ensure(void) {
    if (O.loaded) return;
    O.loaded = true;
    vtk_host_call(load, NULL);
}

void hal_ota_info(hal_ota_info_t *out) {
    ensure();
    memset(out, 0, sizeof *out);
    out->slot_size = SLOT_SIZE;
    out->running = O.running;
    out->pending_verify = O.pending;
    out->rolled_back = O.rolled_back;
    snprintf(out->version, sizeof out->version, "%s", O.version[O.running]);
    out->version_code = O.code[O.running];
}

int hal_ota_begin(size_t size) {
    ensure();
    if (size > SLOT_SIZE) return -1;
    O.writing = true;
    O.size = size;
    O.got = 0;
    memset(O.head, 0, sizeof O.head);
    return 0;
}

int hal_ota_write(const void *data, size_t n) {
    if (!O.writing || O.got + n > O.size) return -1;
    if (O.got < sizeof O.head - 1) {
        size_t take = sizeof O.head - 1 - O.got < n ? sizeof O.head - 1 - O.got : n;
        memcpy(O.head + O.got, data, take);
    }
    O.got += n;
    sim_metric_add("ota.bytes_written", (double)n);
    osal_task_delay_us(WRITE_US_PER_4K * (n + 4095) / 4096);
    return 0;
}

int hal_ota_end(void) {
    if (!O.writing || O.got != O.size) return -1;
    unsigned code = 0;
    char version[24] = "";
    if (sscanf((const char *)O.head, "SIMAPP %u %23s", &code, version) != 2) return -1;
    int other = !O.running;
    O.code[other] = code;
    snprintf(O.version[other], sizeof O.version[other], "%s", version);
    O.writing = false;
    vtk_host_call(save, NULL);
    return 0;
}

int hal_ota_set_boot(void) {
    O.boot = !O.running;
    vtk_host_call(save, NULL);
    return 0;
}

int hal_ota_mark_valid(void) {
    ensure();
    O.pending = false;
    O.boot = O.running;
    vtk_host_call(save, NULL);
    sim_metric_add("ota.marked_valid", 1);
    return 0;
}

int hal_ota_write_resource(int kind, size_t offset, const void *data, size_t n) {
    (void)kind;
    (void)offset;
    (void)data;
    O.resource_bytes += n;
    sim_metric_add("ota.resource_bytes", (double)n);
    osal_task_delay_us(WRITE_US_PER_4K * (n + 4095) / 4096);
    return 0;
}

bool hal_crypto_verify_fw(const uint8_t hash[32], const uint8_t *sig, size_t sig_len, uint32_t key_id) {
    static const char KEY[] = "ink-simulator-test-signing-key"; /* a stand-in, not a secret; tools/inkfw has the same */
    uint8_t want[32];
    if (key_id != 1 || sig_len != 64) return false;
    ink_hmac_sha256(KEY, sizeof KEY - 1, hash, 32, want);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= want[i] ^ sig[i];
    for (int i = 32; i < 64; i++) diff |= sig[i]; /* every bit of it counts, as in the real one */
    return diff == 0;
}
