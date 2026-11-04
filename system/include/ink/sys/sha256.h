/* SHA-256, HMAC and PBKDF2 (FIPS 180-4, RFC 2104, RFC 8018). Small and slow: for hashing an upgrade package and a PIN. */
#ifndef INK_SYS_SHA256_H
#define INK_SYS_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t buf[64];
    size_t fill;
} ink_sha256_t;

void ink_sha256_init(ink_sha256_t *s);
void ink_sha256_update(ink_sha256_t *s, const void *data, size_t n);
void ink_sha256_final(ink_sha256_t *s, uint8_t out[32]);
void ink_sha256(const void *data, size_t n, uint8_t out[32]);
void ink_hmac_sha256(const void *key, size_t key_len, const void *data, size_t n, uint8_t out[32]);
/** One output block is all a PIN check needs. */
void ink_pbkdf2_sha256(const void *password, size_t pw_len, const void *salt, size_t salt_len, uint32_t iterations, uint8_t out[32]);

#ifdef __cplusplus
}
#endif
#endif
