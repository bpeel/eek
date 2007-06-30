#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkhbox.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "eek.h"
#include "electronmanager.h"
#include "cpu.h"
#include "electron.h"
#include "util.h"
#include "mainwindow.h"

static void
main_window_on_destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  char *os_rom = "roms/os.rom";
  GtkWidget *mainwin;
  ElectronManager *eman;

  shortname = util_shortname (argv[0]);

  if (argc >= 2)
    os_rom = argv[1];

  /* Initialise GTK */
  gtk_init (&argc, &argv);
  /* Create the electron */
  eman = electron_manager_new ();
  /* Create the main window */
  mainwin = main_window_new_with_electron (eman);
  g_signal_connect (G_OBJECT (mainwin), "destroy", G_CALLBACK (main_window_on_destroy), NULL);

  /* Load the two default roms */
  {
    FILE *file;

    if ((file = fopen (os_rom, "rb")) == NULL
	|| electron_load_os_rom (eman->data, file) == -1
	|| fclose (file) == EOF
	|| (file = fopen ("roms/basic.rom", "rb")) == NULL
	|| electron_load_paged_rom (eman->data, ELECTRON_BASIC_PAGE, file)
	|| fclose (file) == EOF)
    {
      eprintf ("couldn't load rom: %s\n", strerror (errno));
      exit (-1);
    }
  }

  cpu_restart (&eman->data->cpu);
  /* Set the emulation to start when the main loop is entered */
  electron_manager_start (eman);

  g_object_unref (eman);

  gtk_widget_show (mainwin);
  gtk_main ();

  return 0;
}
