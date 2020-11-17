/*
 * eek - An emulator for the Acorn Electron
 * Copyright (C) 2020  Neil Roberts
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tapebuffer.h"
#include "tapeuef.h"

#define TAPE_FILENAME "FILE"
#define TAPE_LOAD_ADDRESS 0x0e00
#define TAPE_EXEC_ADDRESS 0x0000

/* The start of each file has 5.1 seconds of high tone */
#define FILE_START_TONE_LENGTH ((51 * 1200 / 10) / 10)
/* The start of each subsequent block has has 0.9 seconds of high tone */
#define BLOCK_START_TONE_LENGTH ((9 * 1200 / 10) / 10)
/* The end of the file has a 5.3-second tone */
#define FILE_END_TONE_LENGTH ((53 * 1200 / 10) / 10)

typedef struct
{
  TapeBuffer *tbuf;
  guint16 crc;
} Writer;

static void
add_bytes (Writer *writer,
           int length,
           const guint8 *bytes)
{
  while (length-- > 0)
  {
    guint8 b = *(bytes++);
    int i;

    tape_buffer_store_byte (writer->tbuf, b);

    writer->crc ^= ((guint16) b) << 8;

    for (i = 0; i < 8; i++)
    {
      if (writer->crc & 0x8000)
        writer->crc = ((writer->crc ^ 0x0810) << 1) | 1;
      else
        writer->crc <<= 1;
    }
  }
}

static void
add_uint32 (Writer *writer,
            guint32 value)
{
  value = GUINT32_TO_LE (value);
  add_bytes (writer, sizeof value, (const guint8 *) &value);
}

static void
add_uint16 (Writer *writer,
            guint16 value)
{
  value = GUINT16_TO_LE (value);
  add_bytes (writer, sizeof value, (const guint8 *) &value);
}

static void
add_uint8 (Writer *writer,
           guint8 value)
{
  add_bytes (writer, sizeof value, (const guint8 *) &value);
}

static void
add_crc (Writer *writer)
{
  guint16 value = GUINT16_TO_BE (writer->crc);
  add_bytes (writer, sizeof value, (const guint8 *) &value);
}

static TapeBuffer *
file_to_tape_buffer (FILE *infile, GError **error)
{
  Writer writer = { .tbuf = tape_buffer_new (), .crc = 0 };
  size_t got;
  guint8 buf[256];
  int block_num = 0;
  guint8 block_flags;

  tape_buffer_store_repeated_high_tone (writer.tbuf, FILE_START_TONE_LENGTH);

  do {
    got = fread(buf, 1, sizeof buf, infile);

    if (block_num > 0)
    {
      tape_buffer_store_repeated_high_tone (writer.tbuf,
                                            BLOCK_START_TONE_LENGTH);
    }

    /* Synchronisation byte */
    add_uint8 (&writer, 0x2a);

    writer.crc = 0;

    add_bytes (&writer,
               strlen (TAPE_FILENAME) + 1,
               (const guint8 *) TAPE_FILENAME);
    add_uint32 (&writer, TAPE_LOAD_ADDRESS);
    add_uint32 (&writer, TAPE_EXEC_ADDRESS);
    add_uint16 (&writer, block_num);
    add_uint16 (&writer, got);

    block_flags = 0;

    if (got < sizeof buf)
      block_flags |= 0x80;
    if (got == 0)
      block_flags |= 0x40;

    add_uint8 (&writer, block_flags);

    /* Address of next file */
    add_uint32 (&writer, 0);

    add_crc (&writer);

    writer.crc = 0;

    add_bytes (&writer, got, buf);

    add_crc (&writer);

    block_num++;
  } while (got >= sizeof buf);

  return writer.tbuf;
}

int
main (int argc, char **argv)
{
  FILE *infile;
  int ret = 0;

  if (argc != 3)
  {
    fprintf (stderr, "usage: %s <input> <output.uef>\n", argv[0]);
    ret = 1;
  }
  else if ((infile = fopen (argv[1], "rb")) == NULL)
  {
    fprintf (stderr, "%s: %s\n", argv[1], strerror (errno));
    ret = 1;
  }
  else
  {
    GError *error = NULL;
    TapeBuffer *tbuf;
    FILE *outfile;

    tbuf = tape_buffer_new ();

    if ((tbuf = file_to_tape_buffer (infile, &error)) == NULL)
    {
      fprintf (stderr, "%s: %s\n", argv[1], error->message);
      g_error_free (error);
      ret = 1;
    }
    else if ((outfile = fopen (argv[2], "wb")) == NULL)
    {
      fprintf (stderr, "%s: %s\n", argv[2], strerror (errno));
      ret = 1;
    }
    else
    {
      if (!tape_uef_save (tbuf, TRUE /* compress */, outfile, &error))
      {
        fprintf (stderr, "%s: %s\n", argv[2], strerror (errno));
        ret = 1;
      }

      fclose (outfile);
    }

    tape_buffer_free (tbuf);

    fclose (infile);
  }

  return ret;
}
