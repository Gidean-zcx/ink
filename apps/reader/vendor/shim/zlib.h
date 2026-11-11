#pragma once
#include <reader_miniz/miniz.h>
/* miniz treats first-call FINISH as one-shot; zlib permits a small output buffer. */
static inline int ink_reader_inflate(mz_streamp stream, int flush) {
    return mz_inflate(stream,flush == Z_FINISH ? Z_NO_FLUSH : flush);
}
#undef inflate
#define inflate ink_reader_inflate
