/* CRC-32, cJSON parse, tinf inflate and TLSF malloc/free kernels. */
#include <stdlib.h>

#include "cJSON.h"
#include "k.h"
#include "miniz_tinfl.h"
#include "tinf.h"
#include "tlsf.h"

/* ------------------------------------------------------------------ CRC-32 */
/* Reflected CRC-32 (zlib/IEEE polynomial), byte-wise with a 256-entry table
 * kept in internal SRAM (ESP-IDF's esp_rom_crc32_le uses a ROM table). */
static uint32_t crc_table[256];

static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        crc_table[i] = c;
    }
}

static uint32_t crc32_buf(uint32_t crc, const uint8_t *p, size_t n) {
    crc = ~crc;
    while (n--) crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

void k_crc(void) {
    iss_alloc_reset();
    crc_init();
    const uint32_t SN = 32 * 1024, PN = 256 * 1024;
    uint8_t *s = iss_sram_alloc(SN, 64), *p = iss_psram_alloc(PN, 64);
    k_fill_random(s, SN, 11);
    k_fill_random(p, PN, 12);
    uint32_t c = crc32_buf(0, s, 4096);
    iss_begin("crc32.byte@sram");
    c = crc32_buf(c, s, SN);
    iss_end(SN);
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("crc32.byte@psram");
    c = crc32_buf(c, p, PN);
    iss_end(PN);
    k_sink = c;
}

/* --------------------------------------------------------------- TLSF heap */
static tlsf_t heap_psram;

static void *psram_malloc(size_t n) { return tlsf_malloc(heap_psram, n); }
static void psram_free(void *p) { tlsf_free(heap_psram, p); }

/* --------------------------------------------------------------------- JSON */
/* A ~14 KB news-feed response (kernels/data/sample.json) received into PSRAM;
 * cJSON allocates from a TLSF heap in PSRAM (upstream: SPIRAM_MALLOC_ALWAYSINTERNAL=16,
 * so every cJSON node/string goes to PSRAM). Per byte of JSON text. */
void k_json_parse(void) {
    iss_alloc_reset();
    const size_t POOL = 1024 * 1024;
    heap_psram = tlsf_create_with_pool(iss_psram_alloc(POOL, 64), POOL);
    cJSON_Hooks hooks = {psram_malloc, psram_free};
    cJSON_InitHooks(&hooks);
    size_t len = (size_t)(k_json_end - k_json);
    char *buf = iss_psram_alloc(len + 1, 64);
    memcpy(buf, k_json, len);
    buf[len] = 0;

    cJSON *root = cJSON_ParseWithLength(buf, len); /* warm-up */
    if (!root) {
        k_sink = 0xdead;
        return;
    }
    cJSON_Delete(root);

    iss_begin("json.byte");
    root = cJSON_ParseWithLength(buf, len);
    cJSON_Delete(root);
    iss_end((uint32_t)len);

    iss_begin("json.byte@parse_only");
    root = cJSON_ParseWithLength(buf, len);
    iss_end((uint32_t)len);
    const cJSON *items = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "data"), "items");
    k_sink = (uint32_t)cJSON_GetArraySize(items);
    cJSON_Delete(root);

    iss_cache_cold();
    iss_begin("json.byte#cold");
    root = cJSON_ParseWithLength(buf, len);
    cJSON_Delete(root);
    iss_end((uint32_t)len);
}

/* ------------------------------------------------------------------ inflate */
/* zlib stream of the Chinese sample (8.8 KB -> 17 KB), compressed data and
 * output in PSRAM, adler32 verified. Per output byte.
 *   zlib.inflate_byte       miniz tinfl (table-driven; the ESP32-S31 ROM ships miniz)
 *   zlib.inflate_byte@tinf  tinf (small bit-serial decoder, slower) */
void k_inflate(void) {
    iss_alloc_reset();
    unsigned zlen = (unsigned)(k_text_zlib_end - k_text_zlib);
    unsigned olen_exp = (unsigned)(k_text_zh_end - k_text_zh);
    uint8_t *z = iss_psram_alloc(zlen, 64), *out = iss_psram_alloc(olen_exp + 64, 64);
    memcpy(z, k_text_zlib, zlen);
    const int flags = TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32;

    size_t n = tinfl_decompress_mem_to_mem(out, olen_exp + 64, z, zlen, flags); /* warm-up + check */
    if (n != olen_exp || memcmp(out, k_text_zh, n)) {
        k_sink = 0xbad;
        return;
    }
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("zlib.inflate_byte");
    n = tinfl_decompress_mem_to_mem(out, olen_exp + 64, z, zlen, flags);
    iss_cache_drain();
    iss_end((uint32_t)n);

    tinf_init();
    unsigned olen = olen_exp + 64;
    int rc = tinf_zlib_uncompress(out, &olen, z, zlen); /* warm-up + check */
    if (rc != TINF_OK || olen != olen_exp || memcmp(out, k_text_zh, olen)) {
        k_sink = 0xbad;
        return;
    }
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("zlib.inflate_byte@tinf");
    olen = olen_exp + 64;
    rc = tinf_zlib_uncompress(out, &olen, z, zlen);
    iss_cache_drain();
    iss_end(olen);
    k_sink = (uint32_t)rc ^ out[100];
}

/* --------------------------------------------------------------------- TLSF */
/* malloc/free mix with a UI/network-like size distribution on a pool in
 * internal SRAM (96 KiB, like the internal heap) and in PSRAM (2 MiB). Per op
 * (one malloc or one free). */
static uint32_t tlsf_workload(tlsf_t t, int ops, uint32_t seed, int big) {
    enum { SLOTS = 160 };
    void *slot[SLOTS] = {0};
    uint32_t done = 0;
    for (int i = 0; i < ops; i++) {
        uint32_t r = k_rand(&seed);
        int s = (int)(r % SLOTS);
        if (slot[s]) {
            tlsf_free(t, slot[s]);
            slot[s] = NULL;
        } else {
            uint32_t q = (r >> 8) & 1023, n;
            if (q < 600) n = 16 + (r >> 18) % 48;         /* 60 %: 16-63 B */
            else if (q < 860) n = 64 + (r >> 18) % 448;   /* 25 %: 64-511 B */
            else if (q < 1000) n = 512 + (r >> 18) % 1536; /* 14 %: 0.5-2 KiB */
            else n = big ? 4096 + (r >> 18) % 12288 : 2048; /* 2 %: 4-16 KiB (PSRAM) */
            slot[s] = tlsf_malloc(t, n);
            if (slot[s]) ((uint8_t *)slot[s])[0] = (uint8_t)n;
        }
        done++;
    }
    for (int s = 0; s < SLOTS; s++)
        if (slot[s]) tlsf_free(t, slot[s]);
    return done;
}

void k_tlsf(void) {
    iss_alloc_reset();
    const size_t SP = 96 * 1024, PP = 2 * 1024 * 1024;
    tlsf_t ts = tlsf_create_with_pool(iss_sram_alloc(SP, 64), SP);
    tlsf_t tp = tlsf_create_with_pool(iss_psram_alloc(PP, 64), PP);
    tlsf_workload(ts, 2000, 5, 0); /* warm-up */
    iss_begin("tlsf.op@sram");
    uint32_t n = tlsf_workload(ts, 20000, 6, 0);
    iss_end(n);
    tlsf_workload(tp, 2000, 7, 1);
    iss_begin("tlsf.op@psram");
    n = tlsf_workload(tp, 20000, 8, 1);
    iss_end(n);
    k_sink = (uint32_t)tlsf_check(ts) ^ (uint32_t)tlsf_check(tp);
}
