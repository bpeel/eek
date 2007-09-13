#ifndef _TAPE_BUFFER_H
#define _TAPE_BUFFER_H

#include "stypes.h"

typedef struct _TapeBuffer TapeBuffer;

struct _TapeBuffer
{
  UBYTE *buf;
  int buf_size, buf_length;
  /* Index of next byte to read or written to */
  int buf_pos;
};

TapeBuffer *tape_buffer_new ();
void tape_buffer_free (TapeBuffer *tbuf);
int tape_buffer_get_next_byte (TapeBuffer *tbuf);
void tape_buffer_store_byte (TapeBuffer *tbuf, UBYTE byte);
void tape_buffer_store_high_tone (TapeBuffer *tbuf);
void tape_buffer_rewind (TapeBuffer *tbuf);

#endif /* _TAPE_BUFFER_H */
