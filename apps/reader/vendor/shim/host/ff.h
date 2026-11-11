#pragma once
#include <cstdint>
#include <iconv.h>
using WCHAR = uint16_t;
static inline WCHAR ff_oem2uni(WCHAR c, unsigned) {
    iconv_t cv = iconv_open("UTF-32LE", "GB18030");
    if (cv == reinterpret_cast<iconv_t>(-1)) return 0;
    char bytes[2] = {static_cast<char>(c >> 8), static_cast<char>(c)}, out[4] = {};
    char *ink_encoded_bytes = bytes, *ink_unicode_bytes = out; size_t ni = 2, no = 4;
    size_t r = iconv(cv, &ink_encoded_bytes, &ni, &ink_unicode_bytes, &no); iconv_close(cv);
    return r == static_cast<size_t>(-1) ? 0 : static_cast<unsigned char>(out[0]) | (static_cast<unsigned char>(out[1]) << 8);
}
