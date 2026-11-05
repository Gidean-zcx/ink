/* CRC-32 (see ink/sys/crc.h), four bits at a time: small tables, in flash on the device. */
#include "ink/osal.h"
#include "ink/sys/crc.h"

static const uint32_t REFLECTED[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t ink_crc32(uint32_t crc, const void *data, size_t n) {
    const uint8_t *p = data;
    crc = ~crc;
    for (size_t i = 0; i < n; i++) {
        crc = REFLECTED[(crc ^ p[i]) & 15] ^ (crc >> 4);
        crc = REFLECTED[(crc ^ (p[i] >> 4)) & 15] ^ (crc >> 4);
    }
    osal_cost_units("crc32.byte", n);
    return ~crc;
}

static const uint32_t MSB[16] = {
    0x00000000u, 0x04C11DB7u, 0x09823B6Eu, 0x0D4326D9u, 0x130476DCu, 0x17C56B6Bu, 0x1A864DB2u, 0x1E475005u,
    0x2608EDB8u, 0x22C9F00Fu, 0x2F8AD6D6u, 0x2B4BCB61u, 0x350C9B64u, 0x31CD86D3u, 0x3C8EA00Au, 0x384FBDBDu,
};

uint32_t ink_crc32_ogg(uint32_t crc, const void *data, size_t n) {
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) {
        crc = MSB[((crc >> 28) ^ (p[i] >> 4)) & 15] ^ (crc << 4);
        crc = MSB[((crc >> 28) ^ p[i]) & 15] ^ (crc << 4);
    }
    osal_cost_units("crc32.byte", n);
    return crc;
}
