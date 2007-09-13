#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "tapebuffer.h"
#include "util.h"

/* Special bytes in the tape data */
#define TAPE_BUFFER_QUOTE     0xfe /* Next byte should not be interpreted as a command */
#define TAPE_BUFFER_HIGH_TONE 0xff /* Next byte is a high tone */

TapeBuffer *
tape_buffer_new ()
{
  TapeBuffer *ret = xmalloc (sizeof (TapeBuffer));

  ret->buf = xmalloc (ret->buf_size = 16);
  ret->buf_length = 0;
  ret->buf_pos = 0;

  return ret;
}

void
tape_buffer_free (TapeBuffer *tbuf)
{
  xfree (tbuf->buf);
  xfree (tbuf);
}

static void
tape_buffer_ensure_size (TapeBuffer *tbuf, int size)
{
  if (size > tbuf->buf_size)
  {
    int nsize = tbuf->buf_size;
    do
      nsize *= 2;
    while (size > nsize);
    tbuf->buf = xrealloc (tbuf->buf, nsize);
  }
}

int
tape_buffer_get_next_byte (TapeBuffer *tbuf)
{
  int ret;

  /* Return the next byte, or -1 for a high tone */
  if (tbuf->buf_pos >= tbuf->buf_length)
    ret = -1;
  else if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_QUOTE)
  {
    ret = tbuf->buf[tbuf->buf_pos + 1];
    tbuf->buf_pos += 2;
  }
  else if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_HIGH_TONE)
  {
    ret = -1;
    tbuf->buf_pos++;
  }
  else
  {
    ret = tbuf->buf[tbuf->buf_pos];
    tbuf->buf_pos++;
  }

  return ret;
}

void
tape_buffer_store_byte (TapeBuffer *tbuf, UBYTE byte)
{
  if (tbuf->buf_pos >= tbuf->buf_length)
  {
    if (byte >= TAPE_BUFFER_QUOTE)
    {
      tape_buffer_ensure_size (tbuf, tbuf->buf_pos + 2);
      tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_QUOTE;
      tbuf->buf[tbuf->buf_pos++] = byte;
    }
    else
    {
      tape_buffer_ensure_size (tbuf, tbuf->buf_pos + 1);
      tbuf->buf[tbuf->buf_pos++] = byte;
    }
    tbuf->buf_length = tbuf->buf_pos;
  }
  else if (byte >= TAPE_BUFFER_QUOTE)
  {
    if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_QUOTE)
    {
      tbuf->buf[tbuf->buf_pos + 1] = byte;
      tbuf->buf_pos += 2;
    }
    else
    {
      tape_buffer_ensure_size (tbuf, tbuf->buf_length + 1);
      tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_QUOTE;
      memmove (tbuf->buf + tbuf->buf_pos + 1,
	       tbuf->buf + tbuf->buf_pos,
	       tbuf->buf_length - tbuf->buf_pos);
      tbuf->buf[tbuf->buf_pos++] = byte;
      tbuf->buf_length++;
    }
  }
  else
  {
    if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_QUOTE)
    {
      tbuf->buf[tbuf->buf_pos++] = byte;
      memmove (tbuf->buf + tbuf->buf_pos,
	       tbuf->buf + tbuf->buf_pos + 1,
	       tbuf->buf_length - tbuf->buf_pos);
      tbuf->buf_length--;
    }
    else
      tbuf->buf[tbuf->buf_pos++] = byte;
  }
}

void
tape_buffer_store_high_tone (TapeBuffer *tbuf)
{
  if (tbuf->buf_pos >= tbuf->buf_length)
  {
    tape_buffer_ensure_size (tbuf, tbuf->buf_pos + 1);
    tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_HIGH_TONE;
    tbuf->buf_length = tbuf->buf_pos;
  }
  else
  {
    if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_QUOTE)
    {
      tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_HIGH_TONE;
      memmove (tbuf->buf + tbuf->buf_pos,
	       tbuf->buf + tbuf->buf_pos + 1,
	       tbuf->buf_length - tbuf->buf_pos);
      tbuf->buf_length--;
    }
    else
      tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_HIGH_TONE;
  }
}

void
tape_buffer_rewind (TapeBuffer *tbuf)
{
  tbuf->buf_pos = 0;
}
