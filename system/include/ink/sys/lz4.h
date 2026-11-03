/*
 * LZ4 block format (lossless, very fast to decode): the remote screen's text
 * and interface regions (D-53). The S31 has no compression hardware; this is
 * the project's own small implementation of the public block format. The
 * decoder never writes or reads outside its buffers and rejects bad input.
 */
#ifndef INK_SYS_LZ4_H
#define INK_SYS_LZ4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Decode one block into dst: bytes written, or -1 (malformed, or larger than cap). */
int ink_lz4_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
/** Worst-case encoded size of n bytes. */
size_t ink_lz4_bound(size_t n);
/** Bytes of work memory the encoder needs (its hash table). */
#define INK_LZ4_WORK (16 * 1024)
/** Encode one block (greedy), with INK_LZ4_WORK bytes of the caller's work memory (4-byte aligned).
 *  Bytes written, or -1 if cap is too small. */
int ink_lz4_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, void *work);

#ifdef __cplusplus
}
#endif
#endif
