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

#include "electronmanager.h"
#include "electron.h"

struct _Streamer
{
  GObject parent;

  ElectronManager *electron;

  /* The video buffer that we are currently in the process of sending,
   * expanded to RGB. This is only expanded just before writing. */
  guint8 *video_buffer;
  /* Number of frames queued in the buffer since the last one was sent */
  gboolean frame_count;
  /* The amount of video_buffer that has been sent to the streaming
   * process */
  size_t bytes_sent;
  /* Source for getting notified when the write fd is ready for writing */
  guint write_watch;
  GIOChannel *write_channel;

  gulong frame_end_handler;
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
#define STREAMER_FRAME_DIVISION 2
#define STREAMER_FPS (1000 / (ELECTRON_TICKS_PER_FRAME * \
                              STREAMER_FRAME_DIVISION))

G_DEFINE_TYPE (Streamer, streamer, G_TYPE_OBJECT);

void
streamer_stop_process (Streamer *streamer)
{
  if (streamer->write_watch)
  {
    g_source_remove (streamer->write_watch);
    streamer->write_watch = 0;
  }

  if (streamer->write_channel)
  {
    g_io_channel_unref (streamer->write_channel);
    streamer->write_channel = NULL;
  }
}

gboolean
streamer_start_process (Streamer *streamer,
                        GError **error)
{
  gboolean ret;
  GPid pid;
  int write_fd;
  char *argv[] =
    {
     "/bin/sh",
     "-c",
     NULL,
     NULL
    };

  if (streamer->write_channel)
    return TRUE;

  argv[2] = g_strdup_printf ("ffmpeg "
                             "-f rawvideo "
                             "-pixel_format rgb24 "
                             "-video_size %ix%i "
                             "-framerate %i "
                             "-i - "
                             "-f pulse -i default "
                             "-filter_complex \"[1]highpass=f=200,"
                             "lowpass=f=3000[a]\" "
                             "-map 0:v -map \"[a]\" "
                             "-ac 1 -ar 44100 "
                             "-vcodec libx264 "
                             "-acodec aac "
                             "-g %i "
                             "-keyint_min %i "
                             "-pix_fmt yuv420p "
                             "-preset ultrafast "
                             "-tune film "
                             "-threads 2 "
                             "-strict normal "
                             "-y "
                             "-f flv "
                             "rtmp://live-cdg.twitch.tv/app/"
                             "<<YOUR_KEY_HERE>>"
                             "?bandwidthtest=true",
                             VIDEO_WIDTH, VIDEO_HEIGHT,
                             STREAMER_FPS,
                             STREAMER_FPS * 2,
                             STREAMER_FPS);

  ret = g_spawn_async_with_pipes (NULL, /* working directory */
                                  argv,
                                  NULL, /* envp */
                                  G_SPAWN_DEFAULT,
                                  NULL, /* child_setup */
                                  NULL, /* user_data for child_setup */
                                  &pid,
                                  &write_fd,
                                  NULL, /* stdout */
                                  NULL, /* stderr */
                                  error);

  g_free (argv[2]);

  if (!ret)
    return FALSE;

  streamer->write_channel = g_io_channel_unix_new (write_fd);
  g_io_channel_set_encoding (streamer->write_channel, NULL, NULL);
  g_io_channel_set_buffered (streamer->write_channel, FALSE);
  g_io_channel_set_close_on_unref (streamer->write_channel, TRUE);
  g_io_channel_set_flags (streamer->write_channel, G_IO_FLAG_NONBLOCK, NULL);

  streamer->bytes_sent = 0;
  streamer->frame_count = 0;

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

  streamer->frame_count = 0;
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
    streamer_stop_process (streamer);
    return FALSE;
  }

  if (streamer->bytes_sent == 0)
  {
    if (streamer->frame_count < STREAMER_FRAME_DIVISION)
    {
      streamer->write_watch = 0;
      return FALSE;
    }

    streamer_expand_frame (streamer);
  }

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
      streamer_stop_process (streamer);
      return FALSE;

    case G_IO_STATUS_AGAIN:
    case G_IO_STATUS_NORMAL:
      if (bytes_written == 0)
      {
        /* This probably shouldn’t happen? */
        if (streamer->bytes_sent == 0)
        {
          /* If we didn’t write anything then start the frame again or
           * bytes_sent will still be zero and it won’t work */
          streamer->frame_count = STREAMER_FRAME_DIVISION;
        }

        return TRUE;
      }

      streamer->bytes_sent += bytes_written;

      break;
  }

  if (streamer->bytes_sent >= STREAMER_VIDEO_BUFFER_SIZE)
  {
    streamer->bytes_sent = 0;

    if (streamer->frame_count < STREAMER_FRAME_DIVISION)
    {
      streamer->write_watch = 0;
      return FALSE;
    }
  }

  return TRUE;
}

static void
streamer_on_frame_end (ElectronManager *electron, gpointer user_data)
{
  Streamer *streamer = STREAMER (user_data);

  g_return_if_fail (IS_STREAMER (user_data));
  g_return_if_fail (streamer->electron == electron);

  if (streamer->write_channel == NULL)
    return;

  streamer->frame_count++;

  if (streamer->frame_count >= STREAMER_FRAME_DIVISION
      && streamer->write_watch == 0)
  {
    streamer->write_watch = g_io_add_watch (streamer->write_channel,
                                            G_IO_OUT,
                                            streamer_on_write_watch,
                                            streamer);
  }
}

Streamer *
streamer_new (ElectronManager *electron)
{
  Streamer *streamer = g_object_new (TYPE_STREAMER, NULL);

  streamer_set_electron (streamer, electron);

  return streamer;
}

void
streamer_set_electron (Streamer *streamer,
                       ElectronManager *electron)
{
  if (streamer->electron)
  {
    g_signal_handler_disconnect (streamer->electron,
                                 streamer->frame_end_handler);
    g_object_unref (streamer->electron);
    streamer->electron = NULL;
  }

  if (electron)
  {
    streamer->electron = g_object_ref (electron);
    streamer->frame_end_handler
      = g_signal_connect (electron, "frame-end",
                          G_CALLBACK (streamer_on_frame_end), streamer);
  }
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
}

static void
streamer_init (Streamer *streamer)
{
  streamer->video_buffer = g_malloc (STREAMER_VIDEO_BUFFER_SIZE);
}
