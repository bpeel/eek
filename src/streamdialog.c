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

#include "streamdialog.h"

#include <gtk/gtk.h>

#include "streamer.h"
#include "electronmanager.h"
#include "video.h"
#include "intl.h"

struct _StreamDialog
{
  GtkDialog parent_object;

  Streamer *streamer;
  GtkTextBuffer *command_buffer;
  GtkWidget *start_stop_button;
};

struct _StreamDialogClass
{
  GtkDialogClass parent_class;
};

static gpointer parent_class;

static void
stream_dialog_update_start_stop_button (StreamDialog *streamdialog)
{
  const char *label;

  if (streamer_is_running (streamdialog->streamer))
    label = "Stop";
  else
    label = "Start";

  gtk_button_set_label (GTK_BUTTON (streamdialog->start_stop_button), label);
}

void
stream_dialog_on_start_stop (GtkButton *button,
                             StreamDialog *streamdialog)
{
  g_return_if_fail (IS_STREAM_DIALOG (streamdialog));

  if (streamer_is_running (streamdialog->streamer))
  {
    streamer_stop_process (streamdialog->streamer);
  }
  else
  {
    GError *error = NULL;
    char *command;
    GtkTextIter start, end;

    gtk_text_buffer_get_bounds (streamdialog->command_buffer, &start, &end);
    command = gtk_text_buffer_get_text (streamdialog->command_buffer,
                                        &start, &end,
                                        FALSE);

    if (!streamer_start_process (streamdialog->streamer, command, &error))
    {
      GtkWidget *dialog =
        gtk_message_dialog_new (GTK_WINDOW (streamdialog),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR,
                                GTK_BUTTONS_CLOSE,
                                _("Streaming failed"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      g_signal_connect_swapped (dialog, "response",
                                G_CALLBACK (gtk_widget_destroy),
                                dialog);
      gtk_widget_show (dialog);
      g_error_free (error);
    }

    g_free (command);
  }

  stream_dialog_update_start_stop_button (streamdialog);
}

static void
stream_dialog_init (StreamDialog *streamdialog)
{
  GtkWidget *command_view, *scrolled_win;
  char *command;

  streamdialog->streamer = streamer_new ();

  gtk_window_set_title (GTK_WINDOW (streamdialog), _("Streaming"));

  command = g_strdup_printf ("ffmpeg \\\n"
                             "-f rawvideo "
                             "-pixel_format rgb24 \\\n"
                             "-video_size %ix%i "
                             "-framerate %i \\\n"
                             "-i - \\\n"
                             "-f pulse -i default \\\n"
                             "-filter_complex \"[1]highpass=f=200,"
                             "lowpass=f=3000[a]\" \\\n"
                             "-map 0:v -map \"[a]\" \\\n"
                             "-ac 1 -ar 44100 \\\n"
                             "-vcodec libx264 -acodec aac \\\n"
                             "-g %i "
                             "-keyint_min %i \\\n"
                             "-pix_fmt yuv420p \\\n"
                             "-preset ultrafast "
                             "-tune film \\\n"
                             "-threads 2 "
                             "-strict normal \\\n"
                             "-y \\\n"
                             "-f flv \\\n"
                             "rtmp://live-cdg.twitch.tv/app/"
                             "<<YOUR_KEY_HERE>>"
                             "?bandwidthtest=true",
                             VIDEO_WIDTH, VIDEO_HEIGHT,
                             STREAMER_FPS,
                             STREAMER_FPS * 2,
                             STREAMER_FPS);

  streamdialog->command_buffer = gtk_text_buffer_new (NULL);
  g_object_ref_sink (streamdialog->command_buffer);
  gtk_text_buffer_set_text (streamdialog->command_buffer, command, -1);

  g_free (command);

  streamdialog->start_stop_button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (streamdialog)->vbox),
                      streamdialog->start_stop_button,
                      FALSE, /* expand */
                      FALSE, /* fill */
                      0 /* padding */);
  g_signal_connect (G_OBJECT (streamdialog->start_stop_button),
                    "clicked",
                    G_CALLBACK (stream_dialog_on_start_stop),
                    streamdialog);
  stream_dialog_update_start_stop_button (streamdialog);
  gtk_widget_show (streamdialog->start_stop_button);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  command_view = gtk_text_view_new_with_buffer (streamdialog->command_buffer);
  gtk_widget_show (command_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), command_view);
  gtk_widget_show (scrolled_win);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (streamdialog)->vbox),
                      scrolled_win,
                      TRUE, /* expand */
                      TRUE, /* fill */
                      0 /* padding */);

  gtk_dialog_add_button (GTK_DIALOG (streamdialog),
                         GTK_STOCK_CLOSE,
                         GTK_RESPONSE_CLOSE);
}

GtkWidget *
stream_dialog_new ()
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_STREAM_DIALOG, NULL);

  return ret;
}

GtkWidget *
stream_dialog_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_STREAM_DIALOG, NULL);

  stream_dialog_set_electron (STREAM_DIALOG (ret), electron);

  return ret;
}

void
stream_dialog_set_electron (StreamDialog *streamdialog,
                            ElectronManager *electron)
{
  streamer_set_electron (streamdialog->streamer, electron);
}

static void
stream_dialog_dispose (GObject *obj)
{
  StreamDialog *streamdialog;

  g_return_if_fail (IS_STREAM_DIALOG (obj));

  streamdialog = STREAM_DIALOG (obj);

  if (streamdialog->streamer)
  {
    g_object_unref (streamdialog->streamer);
    streamdialog->streamer = NULL;
  }

  if (streamdialog->command_buffer)
  {
    g_object_unref (streamdialog->command_buffer);
    streamdialog->command_buffer = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
stream_dialog_class_init (StreamDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = stream_dialog_dispose;
}

GType
stream_dialog_get_type ()
{
  static GType stream_dialog_type = 0;

  if (!stream_dialog_type)
  {
    static const GTypeInfo stream_dialog_info =
      {
       sizeof (StreamDialogClass),
       NULL, NULL,
       (GClassInitFunc) stream_dialog_class_init,
       NULL, NULL,

       sizeof (StreamDialog),
       0,
       (GInstanceInitFunc) stream_dialog_init,
       NULL
      };

    stream_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
                                                 "StreamDialog",
                                                 &stream_dialog_info, 0);
  }

  return stream_dialog_type;
}
