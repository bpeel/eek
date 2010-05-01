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

#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkhbox.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "electronmanager.h"
#include "cpu.h"
#include "electron.h"
#include "mainwindow.h"

static void
main_window_on_destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *mainwin;
  ElectronManager *eman;

  /* Initialise GTK */
  gtk_init (&argc, &argv);
  /* Create the electron */
  eman = electron_manager_new ();
  /* Create the main window */
  mainwin = main_window_new_with_electron (eman);
  g_signal_connect (G_OBJECT (mainwin), "destroy", G_CALLBACK (main_window_on_destroy), NULL);

  if (argc > 1)
    main_window_open_tape (MAIN_WINDOW (mainwin), argv[1]);

  electron_manager_update_all_roms (eman);
  cpu_restart (&eman->data->cpu);
  /* Set the emulation to start when the main loop is entered */
  electron_manager_start (eman);

  g_object_unref (eman);

  gtk_widget_show (mainwin);
  gtk_main ();

  return 0;
}
