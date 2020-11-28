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

#include "streamer.h"

#include <glib.h>
#include <glib-object.h>
#include <signal.h>

#include "electronmanager.h"
#include "electron.h"

/* Time in microseconds between frames */
#define STREAMER_FRAME_TIME (1000000 / STREAMER_FPS)

struct _Streamer
{
  GObject parent;

  ElectronManager *electron;

  /* The video buffer that we are currently in the process of sending,
   * expanded to RGB. This is only expanded just before writing. */
  guint8 *video_buffer;
  /* The amount of video_buffer that has been sent to the streaming
   * process */
  size_t bytes_sent;
  GIOChannel *write_channel;
  GPid pid;
  /* The last monotonic clock time that we started to send a frame */
  guint64 last_frame_time;

  /* While the process is running, we will either be waiting for the
   * write fd to be ready or waiting for the timeout. One of these
   * sources will be set to make that happen.
   */
  guint write_watch;
  guint timeout_handler;
};

static const guint8
streamer_color_map[] =
  {
   0xff, 0xff, 0xff, /* white */
   0x00, 0xff, 0xff, /* cyan */
   0xff, 0x00, 0xff, /* magenta */
   0x00, 0x00, 0xff, /* blue */
   0xff, 0xff, 0x00, /* yellow */
   0x00, 0xff, 0x00, /* green */
   0xff, 0x00, 0x00, /* red */
   0x00, 0x00, 0x00, /* black */
  };

#define STREAMER_VIDEO_BUFFER_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT * 3)

G_DEFINE_TYPE (Streamer, streamer, G_TYPE_OBJECT);

enum
  {
   STREAM_ERROR,
   LAST_SIGNAL
  };

static guint signals[LAST_SIGNAL] = { 0 };

static void streamer_update_sources (Streamer *streamer);

static void
streamer_report_error (Streamer *streamer,
                       const char *message)
{
  streamer_stop_process (streamer);
  g_signal_emit (streamer,
                 signals[STREAM_ERROR],
                 0, /* detail */
                 message);
}

void
streamer_stop_process (Streamer *streamer)
{
  if (streamer->write_watch)
  {
    g_source_remove (streamer->write_watch);
    streamer->write_watch = 0;
  }

  if (streamer->timeout_handler)
  {
    g_source_remove (streamer->timeout_handler);
    streamer->timeout_handler = 0;
  }

  if (streamer->write_channel)
  {
    g_io_channel_unref (streamer->write_channel);
    streamer->write_channel = NULL;
  }

  if (streamer->pid)
  {
    kill (streamer->pid, SIGTERM);
    streamer->pid = 0;
  }
}

gboolean
streamer_start_process (Streamer *streamer,
                        const char *command,
                        GError **error)
{
  gboolean ret;
  GPid pid;
  int write_fd;
  const char *argv[] =
    {
     "/bin/sh",
     "-c",
     command,
     NULL
    };

  if (streamer->write_channel)
    return TRUE;

  ret = g_spawn_async_with_pipes (NULL, /* working directory */
                                  (char **) argv,
                                  NULL, /* envp */
                                  G_SPAWN_DEFAULT,
                                  NULL, /* child_setup */
                                  NULL, /* user_data for child_setup */
                                  &pid,
                                  &write_fd,
                                  NULL, /* stdout */
                                  NULL, /* stderr */
                                  error);

  if (!ret)
    return FALSE;

  streamer->write_channel = g_io_channel_unix_new (write_fd);
  g_io_channel_set_encoding (streamer->write_channel, NULL, NULL);
  g_io_channel_set_buffered (streamer->write_channel, FALSE);
  g_io_channel_set_close_on_unref (streamer->write_channel, TRUE);
  g_io_channel_set_flags (streamer->write_channel, G_IO_FLAG_NONBLOCK, NULL);

  streamer->pid = pid;

  streamer->bytes_sent = 0;
  streamer->last_frame_time = g_get_monotonic_time () - STREAMER_FRAME_TIME;

  streamer_update_sources (streamer);

  return TRUE;
}

static void
streamer_expand_frame (Streamer *streamer)
{
  const guint8 *src = streamer->electron->data->video.screen_memory;
  guint8 *dst = streamer->video_buffer;
  int i;

  for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
    memcpy (dst + i * 3, streamer_color_map + (src[i] & 7) * 3, 3);
}

static gboolean
streamer_on_write_watch (GIOChannel *source,
                         GIOCondition condition,
                         gpointer user_data)
{
  Streamer *streamer = STREAMER (user_data);
  gsize bytes_written;
  GIOStatus status;

  g_return_val_if_fail (IS_STREAMER (user_data), FALSE);
  g_return_val_if_fail (source == streamer->write_channel, FALSE);

  if ((condition & (G_IO_ERR | G_IO_HUP)))
  {
    streamer->write_watch = 0;
    streamer_report_error (streamer, "Error writing to the streamer process");
    return FALSE;
  }

  if (streamer->bytes_sent == 0)
    streamer_expand_frame (streamer);

  status = g_io_channel_write_chars (streamer->write_channel,
                                     (const char *) streamer->video_buffer +
                                     streamer->bytes_sent,
                                     STREAMER_VIDEO_BUFFER_SIZE -
                                     streamer->bytes_sent,
                                     &bytes_written,
                                     NULL);
  switch (status)
  {
    case G_IO_STATUS_ERROR:
    case G_IO_STATUS_EOF:
      streamer->write_watch = 0;
      streamer_report_error (streamer, "Error writing to the streamer process");
      return FALSE;

    case G_IO_STATUS_AGAIN:
    case G_IO_STATUS_NORMAL:
      if (bytes_written == 0)
        return TRUE;

      if (streamer->bytes_sent == 0)
        streamer->last_frame_time += STREAMER_FRAME_TIME;

      streamer->bytes_sent += bytes_written;

      break;
  }

  if (streamer->bytes_sent >= STREAMER_VIDEO_BUFFER_SIZE)
    streamer->bytes_sent = 0;

  streamer_update_sources (streamer);

  return TRUE;
}

static gboolean
streamer_on_timeout (void *user_data)
{
  streamer_update_sources (user_data);

  return G_SOURCE_CONTINUE;
}

Streamer *
streamer_new (void)
{
  return g_object_new (TYPE_STREAMER, NULL);
}

void
streamer_set_electron (Streamer *streamer,
                       ElectronManager *electron)
{
  if (streamer->electron)
  {
    g_object_unref (streamer->electron);
    streamer->electron = NULL;
  }

  if (electron)
    streamer->electron = g_object_ref (electron);
}

static void
streamer_finalize (GObject *obj)
{
  Streamer *streamer = STREAMER (obj);

  streamer_stop_process (streamer);
  streamer_set_electron (streamer, NULL);

  g_free (streamer->video_buffer);

  G_OBJECT_CLASS (streamer_parent_class)->finalize (obj);
}

static void
streamer_class_init (StreamerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = streamer_finalize;

  signals[STREAM_ERROR] =
    g_signal_new ("stream-error",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, /* class_offset */
                  NULL, /* accumulator */
                  NULL, /* accu_data */
                  g_cclosure_marshal_VOID__STRING, /* c_marshaller */
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

static void
streamer_init (Streamer *streamer)
{
  streamer->video_buffer = g_malloc (STREAMER_VIDEO_BUFFER_SIZE);
}

gboolean
streamer_is_running (Streamer *streamer)
{
  return streamer->write_channel != NULL;
}

static void
streamer_update_sources (Streamer *streamer)
{
  guint64 now = g_get_monotonic_time ();

  if (streamer->bytes_sent > 0 ||
      now - streamer->last_frame_time >= STREAMER_FRAME_TIME)
  {
    if (streamer->timeout_handler)
    {
      g_source_remove (streamer->timeout_handler);
      streamer->timeout_handler = 0;
    }

    if (streamer->write_watch == 0)
    {
      streamer->write_watch = g_io_add_watch (streamer->write_channel,
                                              G_IO_OUT,
                                              streamer_on_write_watch,
                                              streamer);
    }
  }
  else
  {
    if (streamer->write_watch)
    {
      g_source_remove (streamer->write_watch);
      streamer->write_watch = 0;
    }

    if (streamer->timeout_handler == 0)
    {
      guint64 delay = ((STREAMER_FRAME_TIME +
                        streamer->last_frame_time -
                        now) / 1000 + 1);
      streamer->timeout_handler = g_timeout_add (delay,
                                                 streamer_on_timeout,
                                                 streamer);
    }
  }
}
