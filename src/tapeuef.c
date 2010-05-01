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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "tapeuef.h"
#include "tapebuffer.h"
#include "intl.h"

/* Supported version of UEF format files */
#define TAPE_UEF_MINOR 6
#define TAPE_UEF_MAJOR 0

#define TAPE_UEF_CYCLES_PER_TIME_UNIT  10
#define TAPE_UEF_TIME_UNITS_PER_SECOND 120

/* Chunk IDs */
#define TAPE_UEF_START_STOP_BIT_DATA     0x0100
#define TAPE_UEF_CARRIER_TONE            0x0110
#define TAPE_UEF_CARRIER_TONE_WITH_DUMMY 0x0111
#define TAPE_UEF_INTEGER_GAP             0x0112
#define TAPE_UEF_FLOATING_POINT_GAP      0x0116

#define TAPE_UEF_FLOAT_SIZE 4

/* Stream object with a virtual function so that the same code can be
   used to read from compressed or uncompressed UEF files */
typedef struct _TapeUEFStream TapeUEFStream;
struct _TapeUEFStream
{
  void *data;
  /* Read the given number of bytes. Returns -1 if there was an error
     and sets the GError, otherwise returns the number of bytes
     actually read which may be less than length if the end of the
     file is reached */
  gssize (* read) (TapeUEFStream *stream, gsize length,
                   gchar *buf, GError **error);
};

static const char tape_uef_magic[10] = "UEF File!\0";

/* Magic bytes found at the start of zlib compressed data. Not
   bracketed with ifdef HAVE_ZLIB so that we can still detect
   compressed files and report an error even if zlib support is not
   compiled in */
static const char tape_uef_zlib_magic[2] = "\037\213";

/* Extra stream data for reading from gzipped files */
#ifdef HAVE_ZLIB

#define TAPE_UEF_ZLIB_IN_BUF_SIZE  128
#define TAPE_UEF_ZLIB_OUT_BUF_SIZE 128

typedef struct _TapeUEFZlibData TapeUEFZlibData;

struct _TapeUEFZlibData
{
  /* The underlying stream to read compressed data from */
  TapeUEFStream *child_stream;
  /* The state of the zlib decompressor */
  z_stream strm;
  int reached_end_of_input, reached_end_of_output;
  /* Next byte of decompressed data to send upstream */
  Bytef *next_out_byte;
  /* The compressed data buffer */
  Bytef in_buf[TAPE_UEF_ZLIB_IN_BUF_SIZE];
  /* The decompressed data buffer */
  Bytef out_buf[TAPE_UEF_ZLIB_OUT_BUF_SIZE];
};
#endif

static gssize
tape_uef_stream_read_from_file (TapeUEFStream *stream, gsize length,
                                   gchar *buf, GError **error)
{
  size_t got;
  gssize ret;

  if ((got = fread (buf, 1, length, (FILE *) stream->data)) < length
      && ferror ((FILE *) stream->data))
  {
    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_IO,
                 _("Read error: %s"), g_strerror (errno));
    ret = -1;
  }
  else
    ret = got;

  return ret;
}

#ifdef HAVE_ZLIB
static gssize
tape_uef_stream_read_from_zlib (TapeUEFStream *stream, gsize length,
                                gchar *buf, GError **error)
{
  gssize got = 0, read_got;
  TapeUEFZlibData *zlib_data = (TapeUEFZlibData *) stream->data;
  int zret;

  while (TRUE)
  {
    /* If there's some decompressed data then read from that first */
    size_t to_copy = zlib_data->strm.next_out - zlib_data->next_out_byte;
    if (to_copy > length)
      to_copy = length;
    memcpy (buf, zlib_data->next_out_byte, to_copy);
    buf += to_copy;
    length -= to_copy;
    got += to_copy;
    zlib_data->next_out_byte += to_copy;

    /* Stop if we've filled the request or we've already exhausted the
       compressed stream */
    if (length <= 0 || zlib_data->reached_end_of_output)
      break;

    /* If we make it here then all of the output bytes have been
       consumed so we can start from the beginning of the buffer */
    zlib_data->next_out_byte = zlib_data->out_buf;
    zlib_data->strm.next_out = zlib_data->out_buf;
    zlib_data->strm.avail_out = TAPE_UEF_ZLIB_OUT_BUF_SIZE;

    /* Fill the input buffer unless we've already reached the end in a
       previous call */
    if (zlib_data->reached_end_of_input)
      read_got = 0;
    else if ((read_got = zlib_data->child_stream->read (zlib_data->child_stream,
                                                        TAPE_UEF_ZLIB_IN_BUF_SIZE
                                                        - zlib_data->strm.avail_in,
                                                        (gchar *) zlib_data->in_buf
                                                        + zlib_data->strm.avail_in,
                                                        error)) == -1)
      return -1;
    if (read_got < TAPE_UEF_ZLIB_IN_BUF_SIZE - zlib_data->strm.avail_in)
      zlib_data->reached_end_of_input = TRUE;
    zlib_data->strm.avail_in += read_got;

    /* Decompress the next chunk of data */
    if ((zret = inflate (&zlib_data->strm, 0)) == Z_STREAM_END)
      zlib_data->reached_end_of_output = TRUE;
    else if (zret != Z_OK)
    {
      if (zlib_data->strm.msg)
        g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_ZLIB,
                     _("Decompression error: %s"),
                     zlib_data->strm.msg);
      else
        g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_ZLIB,
                     _("Decompression error"));
      return -1;
    }

    /* Move any remaining compressed data to the beginning of the buffer */
    memmove (zlib_data->in_buf, zlib_data->strm.next_in, zlib_data->strm.avail_in);
    zlib_data->strm.next_in = zlib_data->in_buf;
  }

  return got;
}
#endif

static gboolean
tape_uef_skip_bytes (TapeUEFStream *stream, gsize length, GError **error)
{
  gssize got;
  gchar buf[128];
  int to_copy;

  while (length > 0)
  {
    to_copy = length > sizeof (buf) ? sizeof (buf) : length;
    if ((got = stream->read (stream, to_copy, buf, error)) == -1)
      return FALSE;
    else if (got < to_copy)
    {
      g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                   _("Invalid UEF file"));
      return FALSE;
    }
    else
      length -= to_copy;
  }

  return TRUE;
}

static float
tape_uef_extract_float (const void *buf)
{
  /* Extract a float from the portable UEF format. Algorithm is based
     on the code in the UEF specification */
  const guchar *bufb = (const guchar *) buf;
  int mantissa = bufb[0] | (bufb[1] << 8) | (((bufb[2] & 0x7f) | 0x80) << 16);
  float result = ldexp (mantissa, -23.0);
  int exponent = (((bufb[2] & 0x80) >> 7) | ((bufb[3] & 0x7f) << 1)) - 127;
  result = ldexp (result, exponent);
  return (bufb[3] & 0x80) ? -result : result;
}

static TapeBuffer *
tape_uef_do_load_from_stream (TapeUEFStream *stream, GError **error)
{
  gchar buf[512];
  gssize got;
  TapeBuffer *ret;

  /* Check the version number of the file format */
  if ((got = stream->read (stream, 2, buf, error)) == -1)
    ret = NULL;
  else if (got < 2)
  {
    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                 _("Invalid UEF file"));
    ret = NULL;
  }
  else if (buf[0] != TAPE_UEF_MINOR || buf[1] != TAPE_UEF_MAJOR)
  {
    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_NOT_SUPPORTED,
                 _("UEF format version not supported"));
    ret = NULL;
  }
  else
  {
    ret = tape_buffer_new ();

    while (ret)
    {
      /* Read the 6-byte chunk header */
      if ((got = stream->read (stream, 6, buf, error)) == -1)
      {
        ret = NULL;
        break;
      }
      else if (got == 0)
        /* Reached the end of the file */
        break;
      else if (got < 6)
      {
        /* Read some data and then reached the end of the file but the
           data was not large enough to extract a chunk header */
        g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                     _("Invalid UEF file"));
        ret = NULL;
        break;
      }
      else
      {
        guint16 chunk_id = GUINT16_FROM_LE (*(guint16 *) buf);
        guint32 chunk_len = GUINT32_FROM_LE (*(guint32 *) (buf + 2));

        switch (chunk_id)
        {
          case TAPE_UEF_START_STOP_BIT_DATA:
            while (chunk_len > 0)
            {
              guint32 to_copy = chunk_len > sizeof (buf) ? sizeof (buf) : chunk_len, i;

              if ((got = stream->read (stream, to_copy, buf, error)) == -1)
              {
                tape_buffer_free (ret);
                ret = NULL;
                break;
              }
              else if (got < to_copy)
              {
                tape_buffer_free (ret);
                g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                             _("UEF file is invalid"));
                ret = NULL;
                break;
              }
              else
                for (i = 0; i < to_copy; i++)
                  tape_buffer_store_byte (ret, buf[i]);

              chunk_len -= to_copy;
            }
            break;

          case TAPE_UEF_CARRIER_TONE:
            if (chunk_len < 2)
            {
              g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                           _("Invalid UEF file"));
              tape_buffer_free (ret);
              ret = NULL;
            }
            else
            {
              guint16 cycle_count;

              if ((got = stream->read (stream, sizeof (guint16),
                                       (gchar *) &cycle_count, error)) == -1)
              {
                tape_buffer_free (ret);
                ret = NULL;
              }
              else if (got < sizeof (guint16))
              {
                g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                             _("Invalid UEF file"));
                tape_buffer_free (ret);
                ret = NULL;
              }
              else
              {
                cycle_count = GUINT16_FROM_LE (cycle_count);
                tape_buffer_store_repeated_high_tone (ret, (cycle_count
                                                             + TAPE_UEF_CYCLES_PER_TIME_UNIT - 1)
                                                      / TAPE_UEF_CYCLES_PER_TIME_UNIT);
                if (!tape_uef_skip_bytes (stream, chunk_len - sizeof (guint16), error))
                {
                  tape_buffer_free (ret);
                  ret = NULL;
                }
              }
            }
            break;

          case TAPE_UEF_CARRIER_TONE_WITH_DUMMY:
            if (chunk_len < 4)
            {
              g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                           _("Invalid UEF file"));
              tape_buffer_free (ret);
              ret = NULL;
            }
            else
            {
              guint16 cycle_count[2];

              if ((got = stream->read (stream, sizeof (cycle_count),
                                       (gchar *) &cycle_count, error)) == -1)
              {
                tape_buffer_free (ret);
                ret = NULL;
              }
              else if (got < sizeof (cycle_count))
              {
                g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                             _("Invalid UEF file"));
                tape_buffer_free (ret);
                ret = NULL;
              }
              else
              {
                cycle_count[0] = GUINT16_FROM_LE (cycle_count[0]);
                cycle_count[1] = GUINT16_FROM_LE (cycle_count[1]);
                tape_buffer_store_repeated_high_tone (ret, (cycle_count[0]
                                                             + TAPE_UEF_CYCLES_PER_TIME_UNIT - 1)
                                                      / TAPE_UEF_CYCLES_PER_TIME_UNIT);
                tape_buffer_store_byte (ret, 0xaa);
                tape_buffer_store_repeated_high_tone (ret, (cycle_count[1]
                                                             + TAPE_UEF_CYCLES_PER_TIME_UNIT - 1)
                                                      / TAPE_UEF_CYCLES_PER_TIME_UNIT);
                if (!tape_uef_skip_bytes (stream, chunk_len - sizeof (guint16), error))
                {
                  tape_buffer_free (ret);
                  ret = NULL;
                }
              }
            }
            break;

          case TAPE_UEF_INTEGER_GAP:
            if (chunk_len < 2)
            {
              g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                           _("Invalid UEF file"));
              tape_buffer_free (ret);
              ret = NULL;
            }
            else
            {
              guint16 cycle_count;

              if ((got = stream->read (stream, sizeof (guint16),
                                       (gchar *) &cycle_count, error)) == -1)
              {
                tape_buffer_free (ret);
                ret = NULL;
              }
              else if (got < sizeof (guint16))
              {
                g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                             _("Invalid UEF file"));
                tape_buffer_free (ret);
                ret = NULL;
              }
              else
              {
                cycle_count = GUINT16_FROM_LE (cycle_count);
                tape_buffer_store_repeated_silence (ret, (cycle_count
                                                           + TAPE_UEF_CYCLES_PER_TIME_UNIT - 1)
                                                    / TAPE_UEF_CYCLES_PER_TIME_UNIT);
                if (!tape_uef_skip_bytes (stream, chunk_len - sizeof (guint16), error))
                {
                  tape_buffer_free (ret);
                  ret = NULL;
                }
              }
            }
            break;

          case TAPE_UEF_FLOATING_POINT_GAP:
            if (chunk_len < TAPE_UEF_FLOAT_SIZE)
            {
              g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                           _("Invalid UEF file"));
              tape_buffer_free (ret);
              ret = NULL;
            }
            else
            {
              gchar time_a[TAPE_UEF_FLOAT_SIZE];

              if ((got = stream->read (stream, TAPE_UEF_FLOAT_SIZE,
                                       time_a, error)) == -1)
              {
                tape_buffer_free (ret);
                ret = NULL;
              }
              else if (got < TAPE_UEF_FLOAT_SIZE)
              {
                g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                             _("Invalid UEF file"));
                tape_buffer_free (ret);
                ret = NULL;
              }
              else
              {
                float time_f = tape_uef_extract_float (time_a);
                tape_buffer_store_repeated_silence (ret, time_f
                                                    * TAPE_UEF_TIME_UNITS_PER_SECOND);
                if (!tape_uef_skip_bytes (stream, chunk_len - sizeof (guint16), error))
                {
                  tape_buffer_free (ret);
                  ret = NULL;
                }
              }
            }
            break;

          default:
            g_warning ("Unsupported chunk id %04x", chunk_id);
            if (!tape_uef_skip_bytes (stream, chunk_len, error))
            {
              tape_buffer_free (ret);
              ret = NULL;
            }
            break;
        }
      }
    }
  }

  return ret;
}

static TapeBuffer *
tape_uef_load_from_stream (TapeUEFStream *stream, GError **error)
{
  char buf[sizeof (tape_uef_magic)];
  gssize got;
  TapeBuffer *ret;

  /* Read the first few bytes so we can detect zlib compressed data */
  if ((got = stream->read (stream, sizeof (tape_uef_zlib_magic), buf, error)) == -1)
    ret = NULL;
  else if (got < sizeof (tape_uef_zlib_magic))
  {
    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                 _("Invalid UEF file"));
    ret = NULL;
  }
  else if (!memcmp (buf, tape_uef_zlib_magic, sizeof (tape_uef_zlib_magic)))
  {
#ifdef HAVE_ZLIB

    TapeUEFStream zlib_stream;
    TapeUEFZlibData *zlib_data;

    /* Wrap the child stream into a new zlib reading stream and
       recursively restart the loading */
    zlib_data = g_malloc (sizeof (TapeUEFZlibData));
    zlib_data->child_stream = stream;
    zlib_data->reached_end_of_input = FALSE;
    zlib_data->reached_end_of_output = FALSE;
    memcpy (zlib_data->in_buf, tape_uef_zlib_magic, sizeof (tape_uef_zlib_magic));
    memset (&zlib_data->strm, '\0', sizeof (z_stream));
    zlib_data->strm.next_in = zlib_data->in_buf;
    zlib_data->strm.avail_in = sizeof (tape_uef_zlib_magic);
    zlib_data->strm.next_out = zlib_data->out_buf;
    zlib_data->strm.avail_out = TAPE_UEF_ZLIB_OUT_BUF_SIZE;
    zlib_data->next_out_byte = zlib_data->out_buf;

    /* Initialize the zlib decompressor. The '15' is the maximum
       window size (the default) and the '+ 16' tells it to accept
       gzip format instead of zlib format */
    if (inflateInit2 (&zlib_data->strm, 15 + 16) != Z_OK)
    {
      if (zlib_data->strm.msg)
        g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_ZLIB,
                     _("Error initialising zlib: %s"),
                     zlib_data->strm.msg);
      else
        g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_ZLIB,
                     _("Error initialising zlib"));
      ret = NULL;
    }
    else
    {
      zlib_stream.data = zlib_data;
      zlib_stream.read = tape_uef_stream_read_from_zlib;
      ret = tape_uef_load_from_stream (&zlib_stream, error);
      inflateEnd (&zlib_data->strm);
    }

    g_free (zlib_data);

#else /* HAVE_ZLIB */

    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_NO_ZLIB,
                 _("UEF file is compressed but zlib support "
                   "is not compiled into this executable"));
    ret = NULL;

#endif
  }
  else if ((got = stream->read (stream, sizeof (tape_uef_magic)
                                - sizeof (tape_uef_zlib_magic),
                                buf + sizeof (tape_uef_zlib_magic),
                                error)) == -1)
    ret = NULL;
  else if (got < sizeof (tape_uef_magic) - sizeof (tape_uef_zlib_magic)
           || memcmp (buf, tape_uef_magic,
                      sizeof (tape_uef_magic)))
  {
    g_set_error (error, TAPE_UEF_ERROR, TAPE_UEF_ERROR_INVALID,
                 _("Invalid UEF file"));
    ret = NULL;
  }
  else if ((ret = tape_uef_do_load_from_stream (stream, error)) != NULL)
    tape_buffer_rewind (ret);

  return ret;
}

TapeBuffer *
tape_uef_load (FILE *infile, GError **error)
{
  TapeUEFStream stream;

  /* Wrap the FILE* object into a TapeUEFStream */
  stream.data = (void *) infile;
  stream.read = tape_uef_stream_read_from_file;
  /* Use the stream loader */
  return tape_uef_load_from_stream (&stream, error);
}

GQuark
tape_uef_error_quark ()
{
  return g_quark_from_static_string ("tape_uef_error");
}
