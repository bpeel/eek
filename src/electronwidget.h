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

#ifndef _ELECTRON_WIDGET_H
#define _ELECTRON_WIDGET_H

#include <gtk/gtkwidget.h>
#include "electronmanager.h"

#define TYPE_ELECTRON_WIDGET (electron_widget_get_type ())
#define ELECTRON_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                              TYPE_ELECTRON_WIDGET, ElectronWidget))
#define ELECTRON_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                      TYPE_ELECTRON_WIDGET, ElectronWidgetClass))
#define IS_ELECTRON_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                 TYPE_ELECTRON_WIDGET))
#define IS_ELECTRON_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         TYPE_ELECTRON_WIDGET))
#define ELECTRON_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                        TYPE_ELECTRON_WIDGET, ElectronWidgetClass))

typedef struct _ElectronWidget ElectronWidget;
typedef struct _ElectronWidgetClass ElectronWidgetClass;

typedef enum
{
 ELECTRON_WIDGET_KEYBOARD_TYPE_TEXT,
 ELECTRON_WIDGET_KEYBOARD_TYPE_PHYSICAL,
} ElectronWidgetKeyboardType;

struct _ElectronWidget
{
  GtkWidget parent_object;

  ElectronManager *electron;

  int frame_end_handler;

  /* Position of the main video display. Gets re-centered in the
     widget whenever the widget's size changes */
  int xpos, ypos;

  ElectronWidgetKeyboardType keyboard_type;
};

struct _ElectronWidgetClass
{
  GtkWidgetClass parent_class;
};

GType electron_widget_get_type ();
GtkWidget *electron_widget_new ();
GtkWidget *electron_widget_new_with_electron (ElectronManager *electron);
void electron_widget_set_electron (ElectronWidget *ewidget, ElectronManager *electron);
void electron_widget_set_keyboard_type (ElectronWidget *ewidget,
                                        ElectronWidgetKeyboardType type);

#endif /* _ELECTRON_WIDGET_H */
