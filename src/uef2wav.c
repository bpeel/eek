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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tapebuffer.h"
#include "tapeuef.h"

static const char wav_header[] =
  "RIFF" /* chunk id */
  "\0\0\0\0" /* chunk size (filled in later) */
  "WAVE" /* format */
  "fmt " /* sub chunk 1 id */
  "\x10\0\0\0" /* sub chunk 1 size */
  "\1\0" /* pcm format */
  "\1\0" /* num channels */
  "\x44\xac\0\0" /* sample rate (44100Hz) */
  "\x44\xac\0\0" /* byte rate */
  "\1\0" /* block align */
  "\x8\0" /* bits per sample */
  "data" /* sub chunk 2 id */
  "\0\0\0\0" /* sub chunk 2 size (filled in later) */;

#define MAIN_CHUNK_SIZE_OFFSET 4
#define DATA_CHUNK_SIZE_OFFSET (sizeof (wav_header) - 1 - 4)

/* The base rate for the tape data is 1200Hz. The sample rate for the
   audio is 44100Hz. Therefore one cycle at the zero rate takes 36¾
   sound samples. As this is not a whole number the start of each
   cycle we will generate 37 seven samples for the first 3 of 4
   samples, and then 36 for the last */

/* 37 samples of a pregenerated sine wave for a zero bit */
static const guint8 zero_data[37] =
  "\200\213\225\237\250\260\267\274\277\300\277\275\271\263\254\243"
  "\231\217\204\171\156\144\133\123\114\106\102\100\100\102\105\113"
  "\122\132\143\155\170";
/* same for a one bit (two cycles at twice the frequency) */
static const guint8 one_data[37] =
  "\200\225\250\267\277\277\271\254\231\204\156\133\114\102\100\105"
  "\122\143\170\216\242\262\275\300\274\261\241\214\166\142\121\105"
  "\100\103\114\134\160";
/* 37 samples of silence */
static const guint8 silence[37] =
  "\200\200\200\200\200\200\200\200\200\200\200\200\200\200\200\200"
  "\200\200\200\200\200\200\200\200\200\200\200\200\200\200\200\200"
  "\200\200\200\200\200";

typedef struct _Data Data;

struct _Data
{
  FILE *outfile;

  /* Number of cycles that have been written so far */
  int n_cycles;
};

static gboolean
write_samples (Data *data, const guint8 *samples)
{
  int n_samples;
  gboolean ret;

  /* Generate 37 samples 75% of the time and 36 samples the rest of
     the to get an average of 36¾ samples */
  n_samples = (data->n_cycles & 3) ? 37 : 36;

  ret = fwrite (samples, 1, n_samples, data->outfile) == n_samples;

  data->n_cycles++;

  return ret;
}

static gboolean
write_bit (Data *data, gboolean value)
{
  return write_samples (data, value ? one_data : zero_data);
}

static gboolean
tape_buffer_foreach_cb (TapeBufferCallbackType type,
                        int length,
                        const guint8 *bytes,
                        gpointer data_p)
{
  Data *data = data_p;

  switch (type)
  {
    case TAPE_BUFFER_CALLBACK_TYPE_DATA:
      {
        int byte, bit;

        for (byte = 0; byte < length; byte++)
        {
          guint8 byte_value = bytes[byte];

          /* Write a zero bit to mark the start of a byte */
          if (!write_bit (data, FALSE))
            return FALSE;

          /* Write each bit of the byte */
          for (bit = 0; bit < 8; bit++)
          {
            if (!write_bit (data, !!(byte_value & 1)))
              return FALSE;
            byte_value >>= 1;
          }

          /* Write a one bit to mark the end of a byte */
          if (!write_bit (data, TRUE))
            return FALSE;
        }
      }
      break;

    case TAPE_BUFFER_CALLBACK_TYPE_HIGH_TONE:
      {
        int i;
        for (i = length * 10; i > 0; i--)
          if (!write_bit (data, TRUE))
            return FALSE;
      }
      break;

    case TAPE_BUFFER_CALLBACK_TYPE_SILENCE:
      {
        int i;
        for (i = length * 10; i > 0; i--)
          if (!write_samples (data, silence))
            return FALSE;
      }
      break;
  }

  return TRUE;
}

static gboolean
fix_header_size (FILE *outfile)
{
  long int file_size = ftell (outfile);
  guint32 main_chunk_size, data_chunk_size;

  if (file_size == -1)
    return FALSE;

  main_chunk_size = GUINT32_TO_LE (file_size - MAIN_CHUNK_SIZE_OFFSET - 4);
  data_chunk_size = GUINT32_TO_LE (file_size - DATA_CHUNK_SIZE_OFFSET - 4);

  return (!fseek (outfile, MAIN_CHUNK_SIZE_OFFSET, SEEK_SET)
          && fwrite (&main_chunk_size,
                     sizeof (main_chunk_size), 1, outfile) == 1
          && !fseek (outfile, DATA_CHUNK_SIZE_OFFSET, SEEK_SET)
          && fwrite (&data_chunk_size,
                     sizeof (data_chunk_size), 1, outfile) == 1);
}

int
main (int argc, char **argv)
{
  TapeBuffer *tbuf;
  FILE *infile;
  int ret = 0;

  if (argc != 3)
  {
    fprintf (stderr, "usage: %s <input.uef> <output.wav>\n", argv[0]);
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
    Data data;

    data.n_cycles = 0;

    if ((tbuf = tape_uef_load (infile, &error)) == NULL)
    {
      fprintf (stderr, "%s: %s\n", argv[1], error->message);
      g_error_free (error);
      ret = 1;
    }
    else if ((data.outfile = fopen (argv[2], "wb")) == NULL)
    {
      fprintf (stderr, "%s: %s\n", argv[2], strerror (errno));
      ret = 1;
    }
    else
    {
      if (fwrite (wav_header, 1, sizeof (wav_header) - 1, data.outfile)
          != sizeof (wav_header) - 1
          || !tape_buffer_foreach (tbuf, tape_buffer_foreach_cb, &data)
          || !fix_header_size (data.outfile))
        fprintf (stderr, "%s: %s\n", argv[2], strerror (errno));

      fclose (data.outfile);
    }

    fclose (infile);
  }

  return ret;
}
