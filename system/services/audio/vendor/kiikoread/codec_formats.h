#ifndef INK_FORMATS_H
#define INK_FORMATS_H
#include <stddef.h>
#include <stdint.h>
typedef struct ink_format ink_format_t;
int ink_format_open_for(const char *member, const char *path, ink_format_t **out, int *fs_err);
void ink_format_close(ink_format_t *r);
uint32_t ink_format_duration_ms(const ink_format_t *r);
uint32_t ink_format_position_ms(const ink_format_t *r);
uint32_t ink_format_seek(ink_format_t *r, uint32_t ms);
int ink_format_next_packet(ink_format_t *r, const uint8_t **data, size_t *len);
size_t ink_format_pcm(ink_format_t *r, int16_t *pcm, size_t frames);
unsigned ink_format_rate(const ink_format_t *r);
unsigned ink_format_channels(const ink_format_t *r);
#endif
