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

#include <gtk/gtkwidget.h>
#include <gdk/gdkkeysyms.h>

#include "electronwidget.h"
#include "electron.h"
#include "video.h"

static void electron_widget_class_init (ElectronWidgetClass *klass);
static void electron_widget_init (ElectronWidget *widget);
static void electron_widget_realize (GtkWidget *widget);
static void electron_widget_dispose (GObject *obj);
static gboolean electron_widget_expose (GtkWidget *widget, GdkEventExpose *event);
static void electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void electron_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean electron_widget_key_event (GtkWidget *widget, GdkEventKey *event);
static void electron_widget_on_frame_end (ElectronManager *electron, gpointer user_data);
static gboolean electron_widget_button_press (GtkWidget *widget, GdkEventButton *event);

static gpointer parent_class;

static GdkRgbCmap electron_widget_color_map =
  {
    {
      0xffffff, /* white */
      0x00ffff, /* cyan */
      0xff00ff, /* magenta */
      0x0000ff, /* blue */
      0xffff00, /* yellow */
      0x00ff00, /* green */
      0xff0000, /* red */
      0x000000  /* black */
    }, 8
  };

GType
electron_widget_get_type ()
{
  static GType electron_widget_type = 0;

  if (!electron_widget_type)
  {
    static const GTypeInfo electron_widget_info =
      {
        sizeof (ElectronWidgetClass),
        NULL, NULL,
        (GClassInitFunc) electron_widget_class_init,
        NULL, NULL,

        sizeof (ElectronWidget),
        0,
        (GInstanceInitFunc) electron_widget_init,
        NULL
      };

    electron_widget_type = g_type_register_static (GTK_TYPE_WIDGET,
                                                   "ElectronWidget",
                                                   &electron_widget_info, 0);
  }

  return electron_widget_type;
}

static void
electron_widget_class_init (ElectronWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = electron_widget_dispose;

  widget_class->realize = electron_widget_realize;
  widget_class->expose_event = electron_widget_expose;
  widget_class->size_request = electron_widget_size_request;
  widget_class->size_allocate = electron_widget_size_allocate;
  widget_class->key_press_event = electron_widget_key_event;
  widget_class->key_release_event = electron_widget_key_event;
  widget_class->button_press_event = electron_widget_button_press;
}

static void
electron_widget_init (ElectronWidget *ewidget)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ewidget), GTK_CAN_FOCUS);
}

GtkWidget *
electron_widget_new ()
{
  return g_object_new (TYPE_ELECTRON_WIDGET, NULL);
}

GtkWidget *
electron_widget_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret = g_object_new (TYPE_ELECTRON_WIDGET, NULL);

  electron_widget_set_electron (ELECTRON_WIDGET (ret), electron);

  return ret;
}

static void
electron_widget_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* There's no point in double buffering this widget because none of
     the drawing operations overlap so it won't cause flicker */
  gtk_widget_set_double_buffered (widget, FALSE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes,
                                   GDK_WA_X | GDK_WA_Y);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);

  gdk_window_set_background (widget->window, &widget->style->bg[GTK_WIDGET_STATE (widget)]);
}

static void
electron_widget_dispose (GObject *obj)
{
  ElectronWidget *ewidget;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (obj));

  ewidget = ELECTRON_WIDGET (obj);

  electron_widget_set_electron (ewidget, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
electron_widget_paint_video (ElectronWidget *ewidget)
{
  GtkWidget *widget = GTK_WIDGET (ewidget);

  gdk_draw_indexed_image (GDK_DRAWABLE (widget->window),
                          widget->style->fg_gc[widget->state],
                          ewidget->xpos, ewidget->ypos, VIDEO_WIDTH, VIDEO_HEIGHT,
                          GDK_RGB_DITHER_NONE,
                          ewidget->electron->data->video.screen_memory,
                          VIDEO_SCREEN_PITCH,
                          &electron_widget_color_map);
}

static gboolean
electron_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
  ElectronWidget *ewidget;

  g_return_val_if_fail (IS_ELECTRON_WIDGET (widget), FALSE);

  ewidget = ELECTRON_WIDGET (widget);

  /* If we don't have an electron object to display then just draw the background */
  if (ewidget->electron == NULL)
    gdk_window_clear (widget->window);
  else
  {
    /* Clear the area around the display */
    if (ewidget->xpos > 0)
      gdk_window_clear_area (widget->window,
                             0, 0, ewidget->xpos, widget->allocation.height);
    if (ewidget->xpos + VIDEO_WIDTH < widget->allocation.width)
      gdk_window_clear_area (widget->window,
                             ewidget->xpos + VIDEO_WIDTH, 0, widget->allocation.width - ewidget->xpos - VIDEO_WIDTH,
                             widget->allocation.height);
    if (ewidget->ypos > 0)
      gdk_window_clear_area (widget->window,
                             ewidget->xpos, 0, VIDEO_WIDTH, ewidget->ypos);
    if (ewidget->ypos + VIDEO_HEIGHT < widget->allocation.height)
      gdk_window_clear_area (widget->window,
                             ewidget->xpos, ewidget->ypos + VIDEO_HEIGHT, VIDEO_WIDTH,
                             widget->allocation.height - ewidget->ypos - VIDEO_HEIGHT);

    electron_widget_paint_video (ewidget);
  }

  return FALSE;
}

static gboolean
electron_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_grab_focus (widget);

  return TRUE;
}

static void
electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  requisition->width = VIDEO_WIDTH;
  requisition->height = VIDEO_HEIGHT;
}

static void
electron_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  ElectronWidget *ewidget;

  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  ewidget = ELECTRON_WIDGET (widget);

  if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  /* Centre the display on the widget */
  ewidget->xpos = widget->allocation.width / 2 - VIDEO_WIDTH / 2;
  ewidget->ypos = widget->allocation.height / 2 - VIDEO_HEIGHT / 2;
}

static gboolean
keycode_to_line_and_bit (guint16 code, int *line, int *bit)
{
  if (code >= 9 && code <= 20)
  {
    *line = 20 - code + 2;
    *bit = 0;
    return TRUE;
  }
  if (code >= 24 && code <= 33)
  {
    *line = 33 - code + 3;
    *bit = 1;
    return TRUE;
  }
  if (code >= 38 && code <= 48)
  {
    *line = 48 - code + 2;
    *bit = 2;
    return TRUE;
  }
  if (code >= 52 && code <= 61)
  {
    *line = 61 - code + 3;
    *bit = 3;
    return TRUE;
  }
  if (code == 113 || code == 114)
  {
    *line = code & 1;
    *bit = 0;
    return TRUE;
  }
  if (code == 23)
  {
    *line = 0;
    *bit = 1;
    return TRUE;
  }
  if (code == 111)
  {
    *line = 2;
    *bit = 1;
    return TRUE;
  }
  if (code == 116)
  {
    *line = 1;
    *bit = 1;
    return TRUE;
  }
  if (code == 66)
  {
    *line = 0xd;
    *bit = 1;
    return TRUE;
  }
  if (code == 36)
  {
    *line = 1;
    *bit = 2;
    return TRUE;
  }
  if (code == 37 || code == 105)
  {
    *line = 1;
    *bit = 0xd;
    return TRUE;
  }
  if (code == 65)
  {
    *line = 0;
    *bit = 3;
    return TRUE;
  }
  if (code == 22)
  {
    *line = 1;
    *bit = 3;
    return TRUE;
  }
  if (code == 50 || code == 62)
  {
    *line = 0xd;
    *bit = 3;
    return TRUE;
  }

  return FALSE;
}

static gboolean
electron_widget_key_event (GtkWidget *widget, GdkEventKey *event)
{
  ElectronWidget *ewidget;
  int line, bit;

  g_return_val_if_fail (IS_ELECTRON_WIDGET (widget), TRUE);

  ewidget = ELECTRON_WIDGET (widget);

  if (!keycode_to_line_and_bit (event->hardware_keycode, &line, &bit))
    return FALSE;

  if (event->type == GDK_KEY_RELEASE)
    electron_manager_release_key (ewidget->electron, line, bit);
  else
    electron_manager_press_key (ewidget->electron, line, bit);

  return TRUE;
}

void
electron_widget_set_electron (ElectronWidget *ewidget, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (ewidget != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (ewidget));

  if ((oldelectron = ewidget->electron))
  {
    g_signal_handler_disconnect (oldelectron, ewidget->frame_end_handler);

    ewidget->electron = NULL;

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));

    g_object_ref (electron);

    ewidget->electron = electron;

    ewidget->frame_end_handler
      = g_signal_connect (electron, "frame-end",
                          G_CALLBACK (electron_widget_on_frame_end), ewidget);
  }
}

static void
electron_widget_on_frame_end (ElectronManager *electron, gpointer user_data)
{
  ElectronWidget *ewidget;

  g_return_if_fail (IS_ELECTRON_WIDGET (user_data));
  ewidget = ELECTRON_WIDGET (user_data);
  g_return_if_fail (ewidget->electron == electron);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (ewidget)))
    electron_widget_paint_video (ewidget);
}
