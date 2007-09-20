#ifndef _TAPE_BUFFER_H
#define _TAPE_BUFFER_H

#include <glib/gtypes.h>

typedef struct _TapeBuffer TapeBuffer;

struct _TapeBuffer
{
  guint8 *buf;
  int buf_size, buf_length;
  /* Index of next byte to read or written to */
  int buf_pos;
};

#define TAPE_BUFFER_HIGH_TONE -1
#define TAPE_BUFFER_SILENCE   -2

TapeBuffer *tape_buffer_new ();
void tape_buffer_free (TapeBuffer *tbuf);
int tape_buffer_get_next_byte (TapeBuffer *tbuf);
void tape_buffer_store_byte (TapeBuffer *tbuf, guint8 byte);
void tape_buffer_store_high_tone (TapeBuffer *tbuf);
void tape_buffer_store_repeated_high_tone (TapeBuffer *tbuf, int repeat_count);
void tape_buffer_store_silence (TapeBuffer *tbuf);
void tape_buffer_store_repeated_silence (TapeBuffer *tbuf, int repeat_count);
void tape_buffer_rewind (TapeBuffer *tbuf);
gboolean tape_buffer_is_at_end (TapeBuffer *tbuf);

#endif /* _TAPE_BUFFER_H */
