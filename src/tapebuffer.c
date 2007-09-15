#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "tapebuffer.h"
#include "util.h"

/* Special bytes in the tape data */
#define TAPE_BUFFER_CMD_QUOTE     0xfd /* Next byte should not be interpreted as a command */
#define TAPE_BUFFER_CMD_SILENCE   0xfe /* No data on the tape in this time slice */
#define TAPE_BUFFER_CMD_HIGH_TONE 0xff /* High tone in this time slice */
/* Any byte >= this is a command */
#define TAPE_BUFFER_CMD_FIRST TAPE_BUFFER_CMD_QUOTE

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
    tbuf->buf = xrealloc (tbuf->buf, tbuf->buf_size = nsize);
  }
}

int
tape_buffer_get_next_byte (TapeBuffer *tbuf)
{
  int ret;

  /* Return the next byte, or a marker for special commands */
  if (tbuf->buf_pos >= tbuf->buf_length)
    /* If we've gone over the end of the tape then return silence */
    ret = TAPE_BUFFER_SILENCE;
  else if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_CMD_QUOTE)
  {
    ret = tbuf->buf[tbuf->buf_pos + 1];
    tbuf->buf_pos += 2;
  }
  else if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_CMD_HIGH_TONE)
  {
    ret = TAPE_BUFFER_HIGH_TONE;
    tbuf->buf_pos++;
  }
  else if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_CMD_SILENCE)
  {
    ret = TAPE_BUFFER_SILENCE;
    tbuf->buf_pos++;
  }
  else
  {
    ret = tbuf->buf[tbuf->buf_pos];
    tbuf->buf_pos++;
  }

  return ret;
}

static void
tape_buffer_store_byte_or_command (TapeBuffer *tbuf, UBYTE byte)
{
  if (tbuf->buf_pos >= tbuf->buf_length)
  {
    tape_buffer_ensure_size (tbuf, tbuf->buf_pos + 1);
    tbuf->buf[tbuf->buf_pos++] = byte;
    tbuf->buf_length = tbuf->buf_pos;
  }
  else
  {
    if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_CMD_QUOTE)
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
tape_buffer_store_byte (TapeBuffer *tbuf, UBYTE byte)
{
  if (byte >= TAPE_BUFFER_CMD_FIRST)
  {
    if (tbuf->buf_pos >= tbuf->buf_length)
    {
      tape_buffer_ensure_size (tbuf, tbuf->buf_pos + 2);
      tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_CMD_QUOTE;
      tbuf->buf[tbuf->buf_pos++] = byte;
      tbuf->buf_length = tbuf->buf_pos;
    }
    else
    {
      if (tbuf->buf[tbuf->buf_pos] == TAPE_BUFFER_CMD_QUOTE)
      {
	tbuf->buf[tbuf->buf_pos + 1] = byte;
	tbuf->buf_pos += 2;
      }
      else
      {
	tape_buffer_ensure_size (tbuf, tbuf->buf_length + 1);
	tbuf->buf[tbuf->buf_pos++] = TAPE_BUFFER_CMD_QUOTE;
	memmove (tbuf->buf + tbuf->buf_pos + 1,
		 tbuf->buf + tbuf->buf_pos,
		 tbuf->buf_length - tbuf->buf_pos);
	tbuf->buf[tbuf->buf_pos++] = byte;
	tbuf->buf_length++;
      }
    }
  }
  else
    tape_buffer_store_byte_or_command (tbuf, byte);
}

void
tape_buffer_store_high_tone (TapeBuffer *tbuf)
{
  tape_buffer_store_byte_or_command (tbuf, TAPE_BUFFER_CMD_HIGH_TONE);
}

void
tape_buffer_store_silence (TapeBuffer *tbuf)
{
  tape_buffer_store_byte_or_command (tbuf, TAPE_BUFFER_CMD_SILENCE);
}

void
tape_buffer_rewind (TapeBuffer *tbuf)
{
  tbuf->buf_pos = 0;
}

gboolean
tape_buffer_is_at_end (TapeBuffer *tbuf)
{
  return tbuf->buf_pos >= tbuf->buf_length;
}
