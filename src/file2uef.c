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
#include "tokenizer.h"

/* The start of each file has 5.1 seconds of high tone */
#define FILE_START_TONE_LENGTH ((51 * 1200 / 10) / 10)
/* The start of each subsequent block has has 0.9 seconds of high tone */
#define BLOCK_START_TONE_LENGTH ((9 * 1200 / 10) / 10)
/* The end of the file has a 5.3-second tone */
#define FILE_END_TONE_LENGTH ((53 * 1200 / 10) / 10)

static char *option_output_file = NULL;
static int option_load_address = 0x0e00;
static int option_exec_address = 0x0000;
static GList *option_files = NULL;
static char *option_tapename = NULL;
static gboolean option_tokenize = FALSE;

typedef struct
{
  TapeBuffer *tbuf;
  guint16 crc;
} Writer;

typedef struct
{
  char *filename;
  char *tapename;
  int load_address;
  int exec_address;
  gboolean tokenize;
} File;

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

static void
write_block (Writer *writer,
             const File *file,
             int block_num,
             size_t size,
             const guint8 *buf)
{
  guint8 block_flags;

  if (block_num > 0)
  {
    tape_buffer_store_repeated_high_tone (writer->tbuf,
                                          BLOCK_START_TONE_LENGTH);
  }

  /* Synchronisation byte */
  add_uint8 (writer, 0x2a);

  writer->crc = 0;

  add_bytes (writer,
             strlen (file->tapename) + 1,
             (const guint8 *) file->tapename);
  add_uint32 (writer, file->load_address);
  add_uint32 (writer, file->exec_address);
  add_uint16 (writer, block_num);
  add_uint16 (writer, size);

  block_flags = 0;

  if (size < 256)
    block_flags |= 0x80;
  if (size == 0)
    block_flags |= 0x40;

  add_uint8 (writer, block_flags);

  /* Address of next file */
  add_uint32 (writer, 0);

  add_crc (writer);

  writer->crc = 0;

  add_bytes (writer, size, buf);

  add_crc (writer);
}

static gboolean
add_file_to_tape_buffer (const File *file,
                         TapeBuffer *tbuf,
                         GError **error)
{
  Writer writer = { .tbuf = tbuf, .crc = 0 };
  int block_num = 0;

  tape_buffer_store_repeated_high_tone (writer.tbuf, FILE_START_TONE_LENGTH);

  if (file->tokenize)
  {
    gchar *contents;
    size_t length;
    GString *tokens;
    size_t pos = 0;

    if (!g_file_get_contents (file->filename, &contents, &length, error))
      return FALSE;

    tokens = tokenize_program (contents);

    g_free (contents);

    while (pos < tokens->len)
    {
      size_t to_write = MIN (256, tokens->len - pos);

      write_block (&writer,
                   file,
                   block_num++,
                   to_write,
                   (guint8 *) tokens->str + pos);

      pos += to_write;
    }

    if (tokens->len % 256 == 0)
      write_block (&writer, file, block_num++, 0, NULL);

    g_string_free (tokens, TRUE);
  }
  else
  {
    size_t got;
    guint8 buf[256];
    FILE *infile;

    infile = fopen (file->filename, "rb");
    if (infile == NULL)
    {
      g_set_error_literal (error,
                           G_FILE_ERROR,
                           g_file_error_from_errno (errno),
                           strerror (errno));
      return FALSE;
    }

    do {
      got = fread(buf, 1, sizeof buf, infile);

      write_block (&writer, file, block_num++, got, buf);
    } while (got >= sizeof buf);

    fclose (infile);
  }

  return TRUE;
}

static gboolean
option_filename_cb(const gchar *option_name,
                   const gchar *value,
                   gpointer data,
                   GError **error)
{
  File *file = g_malloc (sizeof *file);

  file->filename = g_strdup (value);
  file->load_address = option_load_address;
  file->exec_address = option_exec_address;
  file->tapename = g_strdup (option_tapename ? option_tapename : value);
  file->tokenize = option_tokenize;

  option_tapename = NULL;
  option_tokenize = FALSE;

  option_files = g_list_prepend (option_files, file);

  return TRUE;
}

static void
free_file(gpointer data, gpointer user_data)
{
  File *file = data;

  g_free (file->tapename);
  g_free (file->filename);
  g_free (file);
}

static GOptionEntry
options[] =
  {
    {
      G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK,
      &option_filename_cb, "File to add", "file"
    },
    {
      "load", 'l', 0, G_OPTION_ARG_INT, &option_load_address,
      "Load address of subsequent files", "address"
    },
    {
      "exec", 'e', 0, G_OPTION_ARG_INT, &option_exec_address,
      "Exec address of subsequent files", "address"
    },
    {
      "name", 'n', 0, G_OPTION_ARG_STRING, &option_tapename,
      "Internal filename of next file", "name"
    },
    {
     "tokenize", 't', 0, G_OPTION_ARG_NONE, &option_tokenize,
     "Tokenize the next file BASIC", NULL
    },
    {
      "output", 'o', 0, G_OPTION_ARG_FILENAME, &option_output_file,
      "UEF file to output", "file"
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

static gboolean
process_arguments (int *argc, char ***argv,
                   GError **error)
{
  GOptionContext *context;
  gboolean ret;
  GOptionGroup *group;

  group = g_option_group_new (NULL, /* name */
                              NULL, /* description */
                              NULL, /* help_description */
                              NULL, /* user_data */
                              NULL /* destroy notify */);
  g_option_group_add_entries (group, options);
  context = g_option_context_new ("- Combine files into a UEF cassette");
  g_option_context_set_main_group (context, group);
  ret = g_option_context_parse (context, argc, argv, error);
  g_option_context_free (context);

  if (ret && *argc > 1)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                   "Unknown option '%s'", (* argv)[1]);
      ret = FALSE;
    }

  return ret;
}

int
main (int argc, char **argv)
{
  int ret = 0;
  GError *error = NULL;

  if (!process_arguments (&argc, &argv, &error))
  {
    fprintf (stderr, "%s\n", error->message);
    g_error_free (error);
    ret = 1;
  }
  else if (option_files == NULL || option_output_file == NULL)
  {
    fprintf (stderr, "usage: %s -o <output.uef> <input>..\n", argv[0]);
    ret = 1;
  }
  else
  {
    TapeBuffer *tbuf = tape_buffer_new ();
    GList *l;
    FILE *outfile;

    option_files = g_list_reverse (option_files);

    for (l = option_files; l; l = l->next)
    {
      const File *file = l->data;

      if (!add_file_to_tape_buffer (file, tbuf, &error))
      {
        fprintf (stderr, "%s: %s\n", file->filename, error->message);
        g_error_free (error);
        ret = 1;
        goto skip_save;
      }
    }

    if ((outfile = fopen (option_output_file, "wb")) == NULL)
    {
      fprintf (stderr, "%s: %s\n", option_output_file, strerror (errno));
      ret = 1;
    }
    else
    {
      if (!tape_uef_save (tbuf, TRUE /* compress */, outfile, &error))
      {
        fprintf (stderr, "%s: %s\n", option_output_file, strerror (errno));
        ret = 1;
      }

      fclose (outfile);
    }

  skip_save:
    tape_buffer_free (tbuf);
  }

  g_list_foreach (option_files, free_file, NULL);

  return ret;
}
