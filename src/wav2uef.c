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

#define THRESHOLD 0.8f

typedef enum
{
 /* We’re waiting for the first high peak to start processing */
 WAVE_STATE_AWAITING_HIGH,
 /* We’re waiting for the low peak to determine the frequency */
 WAVE_STATE_AWAITING_LOW,
 /* We’ve determined that this is a high tone and we’re waiting to
  * skip the high peak of the second half of it */
 WAVE_STATE_SKIPPING_HIGH,
 /* Skipping the low peak of the second half of a high tone */
 WAVE_STATE_SKIPPING_LOW,
} WaveState;

typedef enum
{
 DATA_STATE_AWAITING_START,
 DATA_STATE_READING_BITS,
 DATA_STATE_AWAITING_STOP,
} DataState;

typedef struct
{
  TapeBuffer *tbuf;
  FILE *infile;
  gboolean had_format;
  int n_channels;
  int sample_rate;
  int bits_per_sample;

  float channel_sum;
  int n_channels_read;

  WaveState wave_state;
  /* Number of samples processed at the point we encountered a high peak */
  size_t high_time;
  /* Count of the number of samples that we have processed so far */
  size_t sample_count;

  DataState data_state;
  int n_bits_read;
  int n_high_tones;
  guint8 byte;
} Data;

static guint16
read_guint16 (guint8 *buf)
{
  guint16 val;

  memcpy (&val, buf, sizeof val);

  return GUINT16_FROM_LE (val);
}

static guint32
read_guint32 (guint8 *buf)
{
  guint32 val;

  memcpy (&val, buf, sizeof val);

  return GUINT32_FROM_LE (val);
}

static gboolean
read_or_error (FILE *infile, size_t length, guint8 *buf)
{
  size_t got = fread (buf, 1, length, infile);

  if (got != length)
  {
    fprintf (stderr, "WAV file invalid\n");
    return FALSE;
  }

  return TRUE;
}

static gboolean
skip_chunk (Data *data, guint32 chunk_size)
{
  guint8 buf[128];
  int ret;

  ret = fseek (data->infile, chunk_size, SEEK_CUR);

  if (ret == 0)
    return TRUE;

  if (errno != EBADF)
  {
    fprintf (stderr, "seek failed: %s\n", strerror (errno));
    return FALSE;
  }

  /* Try reading all the data */
  while (chunk_size > 0)
  {
    size_t to_read = MIN (sizeof buf, chunk_size);
    if (!read_or_error (data->infile, to_read, buf))
      return FALSE;
    chunk_size -= to_read;
  }

  return TRUE;
}

static gboolean
handle_format_chunk (Data *data, guint32 chunk_size)
{
  guint8 buf[16];

  if (chunk_size < sizeof buf)
  {
    fprintf (stderr, "fmt chunk invalid\n");
    return FALSE;
  }

  if (!read_or_error (data->infile, sizeof buf, buf))
    return FALSE;

  if (memcmp (buf, "\x1\x0", 2))
  {
    fprintf (stderr, "WAV file not in PCM format\n");
    return FALSE;
  }

  data->n_channels = read_guint16 (buf + 2);
  if (data->n_channels < 1)
  {
    fprintf (stderr, "Invalid number of channels\n");
    return FALSE;
  }

  data->sample_rate = read_guint32 (buf + 4);
  if (data->sample_rate < 4200)
  {
    fprintf (stderr, "Invalid sample rate\n");
    return FALSE;
  }

  data->bits_per_sample = read_guint16 (buf + 14);
  if (data->bits_per_sample != 8
      && data->bits_per_sample != 16
      && data->bits_per_sample != 32)
  {
    fprintf (stderr, "Invalid bits per sample\n");
    return FALSE;
  }

  data->had_format = TRUE;

  return skip_chunk (data, chunk_size - sizeof buf);
}

static void
handle_low_tone (Data *data)
{
  switch (data->data_state)
  {
    case DATA_STATE_AWAITING_START:
      data->data_state = DATA_STATE_READING_BITS;
      data->n_bits_read = 0;
      break;
    case DATA_STATE_READING_BITS:
      data->byte >>= 1;
      if (++data->n_bits_read >= 8)
        data->data_state = DATA_STATE_AWAITING_STOP;
      break;
    case DATA_STATE_AWAITING_STOP:
      /* Invalid stop bit, ignore the byte? */
      data->data_state = DATA_STATE_AWAITING_START;
      data->n_high_tones = 0;
      break;
  }
}

static void
handle_high_tone (Data *data)
{
  switch (data->data_state)
  {
    case DATA_STATE_AWAITING_START:
      if (++data->n_high_tones >= 10)
      {
        tape_buffer_store_high_tone (data->tbuf);
        data->n_high_tones = 0;
      }
      break;
    case DATA_STATE_READING_BITS:
      data->byte = (data->byte >> 1) | 0x80;
      if (++data->n_bits_read >= 8)
        data->data_state = DATA_STATE_AWAITING_STOP;
      break;
    case DATA_STATE_AWAITING_STOP:
      tape_buffer_store_byte (data->tbuf, data->byte);
      data->data_state = DATA_STATE_AWAITING_START;
      data->n_high_tones = 0;
      break;
  }
}

static gboolean
is_high_frequency (Data *data)
{
  /* When this function is hit, the sampling has gone from high to
   * low, which means we have found half of a full wave. The high tone
   * is 2400Hz and the low tone is 1200Hz. For it to be a high
   * frequency the time since the high peak should be closer to
   * 0.5/2400 than 0.5/1200. If we take the average of those times we
   * have 0.75/2400 seconds. As integers the fraction is 1/3200 */

  size_t samples_elapsed = data->sample_count - data->high_time;

  return samples_elapsed < data->sample_rate / 3200;
}

static void
handle_sample (Data *data, float sample)
{
  data->channel_sum += sample;

  if (++data->n_channels_read < data->n_channels)
    return;

  sample = data->channel_sum / data->n_channels;
  data->n_channels_read = 0;
  data->channel_sum = 0.0f;

  if (sample > THRESHOLD)
  {
    switch (data->wave_state)
    {
      case WAVE_STATE_AWAITING_HIGH:
        data->high_time = data->sample_count;
        data->wave_state = WAVE_STATE_AWAITING_LOW;
        break;
      case WAVE_STATE_SKIPPING_HIGH:
        data->high_time = data->sample_count;
        data->wave_state = WAVE_STATE_SKIPPING_LOW;
        break;
      default:
        break;
    }
  }
  else if (sample < -THRESHOLD)
  {
    /* If we’ve waiting for the more than twice the length of the low
     * frequency then assume the high peak was just noise and abandon
     * it. */
    if (data->sample_count - data->high_time >= data->sample_rate / 600)
      data->wave_state = WAVE_STATE_AWAITING_HIGH;

    switch (data->wave_state)
    {
      case WAVE_STATE_AWAITING_LOW:
        if (is_high_frequency (data))
        {
          data->wave_state = WAVE_STATE_SKIPPING_HIGH;
        }
        else
        {
          handle_low_tone (data);
          data->wave_state = WAVE_STATE_AWAITING_HIGH;
        }
        break;
      case WAVE_STATE_SKIPPING_LOW:
        if (is_high_frequency (data))
          handle_high_tone (data);
        else
          handle_low_tone (data);

        data->wave_state = WAVE_STATE_AWAITING_HIGH;
        break;
      default:
        break;
    }
  }

  data->sample_count++;
}

static gboolean
handle_8bit_samples (Data *data, guint32 chunk_size)
{
  guint8 buf[128];
  gboolean odd = !!(chunk_size & 1);

  while (chunk_size > 0)
  {
    size_t to_read = MIN (sizeof buf, chunk_size);
    unsigned i;

    if (!read_or_error (data->infile, to_read, buf))
      return FALSE;

    for (i = 0; i < to_read; i++)
      handle_sample (data, ((int) buf[i] - 0x80) / 128.0f);

    chunk_size -= to_read;
  }

  if (odd)
    return skip_chunk (data, 1);

  return TRUE;
}

static gboolean
handle_16bit_samples (Data *data, guint32 chunk_size)
{
  gint16 buf[64];

  while (chunk_size > 0)
  {
    size_t to_read = MIN (sizeof buf, chunk_size);
    unsigned i;

    if (!read_or_error (data->infile, to_read, (guint8 *) buf))
      return FALSE;

    for (i = 0; i < to_read / sizeof buf[0]; i++)
      handle_sample (data, buf[i] / 32768.0f);

    chunk_size -= to_read;
  }

  return TRUE;
}

static gboolean
handle_32bit_samples (Data *data, guint32 chunk_size)
{
  gint32 buf[64];

  while (chunk_size > 0)
  {
    size_t to_read = MIN (sizeof buf, chunk_size);
    unsigned i;

    if (!read_or_error (data->infile, to_read, (guint8 *) buf))
      return FALSE;

    for (i = 0; i < to_read / sizeof buf[0]; i++)
      handle_sample (data, buf[i] / 2147483648.0f);

    chunk_size -= to_read;
  }

  return TRUE;
}

static gboolean
handle_data_chunk (Data *data, guint32 chunk_size)
{
  if (!data->had_format)
  {
    fprintf (stderr, "WAV file missing fmt chunk\n");
    return FALSE;
  }

  if (chunk_size & (data->bits_per_sample / 8 - 1))
  {
    fprintf (stderr, "Invalid data chunk size\n");
    return FALSE;
  }

  switch (data->bits_per_sample)
  {
    case 8:
      return handle_8bit_samples (data, chunk_size);
    case 16:
      return handle_16bit_samples (data, chunk_size);
    case 32:
      return handle_32bit_samples (data, chunk_size);
  }

  g_assert_not_reached ();
}

static TapeBuffer *
wav_to_tape_buffer (FILE *infile)
{
  Data data =
    {
     .tbuf = tape_buffer_new (),
     .infile = infile,
     .had_format = FALSE,
    };
  guint32 file_size, file_pos;
  guint8 buf[12];

  if (!read_or_error (infile, 12, buf))
    goto error;

  if (memcmp (buf, "RIFF", 4)
      || memcmp (buf + 8, "WAVE", 4))
    goto error_invalid;

  file_size = read_guint32 (buf + 4);
  file_pos = 4;

  while (file_pos < file_size)
  {
    guint32 chunk_size;

    if (file_size - file_pos < 8)
      goto error_invalid;

    if (!read_or_error (infile, 8, buf))
      goto error;

    chunk_size = read_guint32 (buf + 4);

    if (file_pos + chunk_size + 8 > file_size)
      goto error_invalid;

    if (!memcmp (buf, "fmt ", 4))
    {
      if (!handle_format_chunk (&data, chunk_size))
        goto error;
    }
    else if (!memcmp (buf, "data", 4))
    {
      if (!handle_data_chunk (&data, chunk_size))
        goto error;
    }
    else
    {
      if (!skip_chunk (&data, (chunk_size + 1) & ~(guint32) 1))
        goto error;
    }

    file_pos += chunk_size + 8;
  }

  return data.tbuf;

 error_invalid:
  fprintf (stderr, "WAV file invalid\n");
 error:
  tape_buffer_free (data.tbuf);
  return NULL;
}

int
main (int argc, char **argv)
{
  TapeBuffer *tbuf;
  FILE *infile, *outfile;
  int ret = 0;

  if (argc != 3)
  {
    fprintf (stderr, "usage: %s <input.wav> <output.uef>\n", argv[0]);
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

    if ((tbuf = wav_to_tape_buffer (infile)) == NULL)
    {
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
        fprintf (stderr, "%s: %s\n", argv[2], error->message);
        g_error_free (error);
        ret = 1;
      }

      fclose (outfile);
    }

    fclose (infile);
  }

  return ret;
}
