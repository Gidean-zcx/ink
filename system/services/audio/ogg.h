/* Ogg Opus (RFC 7845): as much as the media service needs. Inside the audio service. */
#ifndef INK_OGG_H
#define INK_OGG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/file.h"

#define OGG_SAMPLES_PER_MS 48 /* the granule position counts 48 kHz samples, whatever the rate (RFC 7845) */
#define OGG_FRAME_MS 60

typedef struct ogg_reader ogg_reader_t;
typedef enum { OGG_OK = 0, OGG_ERR_FILE = -1, OGG_ERR_FORMAT = -2, OGG_ERR_MEMORY = -3 } ogg_err_t;

/** Reads the header pages. *fs_err: what the file service said, when OGG_ERR_FILE. */
int ogg_open(const char *path, ogg_reader_t **out, int *fs_err);
int ogg_open_for(const char *member, const char *path, ogg_reader_t **out, int *fs_err);
void ogg_close(ogg_reader_t *r);
/** The length of the stream (from the last good page). */
uint32_t ogg_duration_ms(const ogg_reader_t *r);
/** The next packet, valid until the next call: 1, 0 at the end (also at a file cut short), < 0 on a read error. */
int ogg_next_packet(ogg_reader_t *r, const uint8_t **data, size_t *len);
/** To the page holding this moment: only a few pages are read. The time reached. */
uint32_t ogg_seek(ogg_reader_t *r, uint32_t ms);
/** Where the next packet is, as time. */
uint32_t ogg_position_ms(const ogg_reader_t *r);

typedef struct ogg_writer ogg_writer_t;
int ogg_create(const char *path, ogg_writer_t **out, int *fs_err);
int ogg_create_for(const char *member, const char *path, ogg_writer_t **out, int *fs_err);
/** 0, or a file service error: then what was written so far stays good. */
int ogg_write_packet(ogg_writer_t *w, const uint8_t *data, size_t len, uint32_t ms);
/** What is written so far is on the card even if the power goes now. */
int ogg_sync(ogg_writer_t *w);
/** The last page, closed. */
int ogg_finish(ogg_writer_t *w);
uint32_t ogg_written_ms(const ogg_writer_t *w);

#endif
