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

#ifndef _MAIN_WINDOW_H
#define _MAIN_WINDOW_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtkuimanager.h>
#include "electronmanager.h"

#define TYPE_MAIN_WINDOW (main_window_get_type ())
#define MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MAIN_WINDOW, MainWindow))
#define MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MAIN_WINDOW, MainWindowClass))
#define IS_MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MAIN_WINDOW))
#define IS_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MAIN_WINDOW))
#define MAIN_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_MAIN_WINDOW, MainWindowClass))

typedef struct _MainWindow MainWindow;
typedef struct _MainWindowClass MainWindowClass;

struct _MainWindow
{
  GtkWindow parent_object;

  ElectronManager *electron;

  GtkWidget *debugger, *ewidget, *disdialog, *open_dialog, *save_dialog;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;

  guint started, stopped, rom_error, disdialog_destroy;
  guint open_response_handler, save_response_handler;

  gchar *tape_filename;
};

struct _MainWindowClass
{
  GtkWindowClass parent_class;
};

GType main_window_get_type ();
GtkWidget *main_window_new ();
GtkWidget *main_window_new_with_electron (ElectronManager *electron);
void main_window_set_electron (MainWindow *mainwin, ElectronManager *electron);
void main_window_open_tape (MainWindow *mainwin, const gchar *filename);

#endif /* _MAIN_WINDOW_H */
