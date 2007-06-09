#include "config.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkhbox.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "eek.h"
#include "electronwidget.h"
#include "electronmanager.h"
#include "cpu.h"
#include "electron.h"
#include "util.h"
#include "debugger.h"

static void
main_window_on_destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  char *os_rom = "roms/os.rom";
  GtkWidget *window, *ewidget, *debugger, *box;
  ElectronManager *eman;

  shortname = util_shortname (argv[0]);

  if (argc >= 2)
    os_rom = argv[1];

  /* Initialise GTK */
  gtk_init (&argc, &argv);
  /* Create the electron */
  eman = electron_manager_new ();
  /* Create the main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (main_window_on_destroy), NULL);
  /* Create a box to pack the electron widget next to the debugger */
  box = gtk_hbox_new (FALSE, 0);
  /* Add an electron widget to it */
  ewidget = electron_widget_new_with_electron (eman);
  gtk_box_pack_start (GTK_BOX (box), ewidget, TRUE, TRUE, 0);
  gtk_widget_show (ewidget);
  /* Add a debugger window */
  debugger = debugger_new_with_electron (eman);
  gtk_box_pack_start (GTK_BOX (box), debugger, FALSE, FALSE, 0);
  gtk_widget_show (debugger);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

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

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
