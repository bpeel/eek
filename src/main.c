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

  electron_manager_update_all_roms (eman);
  cpu_restart (&eman->data->cpu);
  /* Set the emulation to start when the main loop is entered */
  electron_manager_start (eman);

  g_object_unref (eman);

  gtk_widget_show (mainwin);
  gtk_main ();

  return 0;
}
