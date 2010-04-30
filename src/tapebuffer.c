/*
 * eek - An emulator for the Acorn Electron
 * Copyright (C) 2010  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>

#include "tapebuffer.h"

/* Special bytes in the tape data */
#define TAPE_BUFFER_CMD_QUOTE     0xfd /* Next byte should not be interpreted as a command */
#define TAPE_BUFFER_CMD_SILENCE   0xfe /* No data on the tape in this time slice */
#define TAPE_BUFFER_CMD_HIGH_TONE 0xff /* High tone in this time slice */
/* Any byte >= this is a command */
#define TAPE_BUFFER_CMD_FIRST TAPE_BUFFER_CMD_QUOTE

TapeBuffer *
tape_buffer_new ()
{
  TapeBuffer *ret = g_malloc (sizeof (TapeBuffer));

  ret->buf = g_malloc (ret->buf_size = 16);
  ret->buf_length = 0;
  ret->buf_pos = 0;

  return ret;
}

void
tape_buffer_free (TapeBuffer *tbuf)
{
  g_free (tbuf->buf);
  g_free (tbuf);
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
    tbuf->buf = g_realloc (tbuf->buf, tbuf->buf_size = nsize);
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
tape_buffer_store_byte_or_command (TapeBuffer *tbuf, guint8 byte)
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

static void
tape_buffer_store_repeated_byte_or_command (TapeBuffer *tbuf, guint8 byte, int repeat_count)
{
  while (tbuf->buf_pos < tbuf->buf_length && repeat_count > 0)
  {
    tape_buffer_store_byte_or_command (tbuf, byte);
    repeat_count--;
  }
  tape_buffer_ensure_size (tbuf, tbuf->buf_pos + repeat_count);
  memset (tbuf->buf + tbuf->buf_pos, byte, repeat_count);
  tbuf->buf_pos += repeat_count;
  tbuf->buf_length += repeat_count;
}

void
tape_buffer_store_byte (TapeBuffer *tbuf, guint8 byte)
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
tape_buffer_store_repeated_high_tone (TapeBuffer *tbuf, int repeat_count)
{
  tape_buffer_store_repeated_byte_or_command (tbuf, TAPE_BUFFER_CMD_HIGH_TONE, repeat_count);
}

void
tape_buffer_store_silence (TapeBuffer *tbuf)
{
  tape_buffer_store_byte_or_command (tbuf, TAPE_BUFFER_CMD_SILENCE);
}

void
tape_buffer_store_repeated_silence (TapeBuffer *tbuf, int repeat_count)
{
  tape_buffer_store_repeated_byte_or_command (tbuf, TAPE_BUFFER_CMD_SILENCE, repeat_count);
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
