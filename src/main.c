#include "config.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "eek.h"
#include "electronwidget.h"
#include "cpu.h"
#include "electron.h"
#include "timer.h"
#include "util.h"

static void
main_window_on_destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  char *os_rom = "roms/os.rom";
  GtkWidget *window, *ewidget;

  shortname = util_shortname (argv[0]);

  if (argc >= 2)
    os_rom = argv[1];

  /* Initialise the timer */
  timer_init ();

  /* Initialise GTK */
  gtk_init (&argc, &argv);
  /* Create the main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (main_window_on_destroy), NULL);
  /* Add an electron widget to it */
  ewidget = electron_widget_new ();
  gtk_container_add (GTK_CONTAINER (window), ewidget);
  gtk_widget_show (ewidget);

  /* Load the two default roms */
  {
    FILE *file;

    if ((file = fopen (os_rom, "rb")) == NULL
	|| electron_load_os_rom (ELECTRON_WIDGET (ewidget)->electron, file) == -1
	|| fclose (file) == EOF
	|| (file = fopen ("roms/basic.rom", "rb")) == NULL
	|| electron_load_paged_rom (ELECTRON_WIDGET (ewidget)->electron, ELECTRON_BASIC_PAGE, file)
	|| fclose (file) == EOF)
    {
      eprintf ("couldn't load rom: %s\n", strerror (errno));
      exit (-1);
    }
  }

  cpu_restart (&ELECTRON_WIDGET (ewidget)->electron->cpu);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
