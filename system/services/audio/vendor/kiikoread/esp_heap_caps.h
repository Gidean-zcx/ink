#include "codec_io.h"
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#define heap_caps_malloc(n, caps) ink_music_malloc(n)
