/* CRC-32: the common reflected one (data store frames), and the MSB-first one Ogg pages use. */
#ifndef INK_SYS_CRC_H
#define INK_SYS_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ink_crc32(uint32_t crc, const void *data, size_t n);     /* start with 0 */
uint32_t ink_crc32_ogg(uint32_t crc, const void *data, size_t n); /* start with 0 */

#ifdef __cplusplus
}
#endif
#endif
