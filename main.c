#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "eek.h"
#include "cpu.h"
#include "electron.h"
#include "timer.h"
#include "util.h"
#include "monitor.h"
#include "video.h"

/* The state of the electron */
Electron main_electron;

int
main (int argc, char **argv)
{
  unsigned int last_time;
  int do_monitor = TRUE;
  char *os_rom = "roms/os.rom";

  shortname = util_shortname (argv[0]);

  if (argc >= 2)
    os_rom = argv[1];

  /* Initialise the electron */
  electron_init (&main_electron);

  /* Load the two default roms */
  {
    FILE *file;

    if ((file = fopen (os_rom, "rb")) == NULL
	|| electron_load_os_rom (&main_electron, file) == -1
	|| fclose (file) == EOF
	|| (file = fopen ("roms/basic.rom", "rb")) == NULL
	|| electron_load_paged_rom (&main_electron, ELECTRON_BASIC_PAGE, file)
	|| fclose (file) == EOF)
    {
      eprintf ("couldn't load rom: %s\n", strerror (errno));
      exit (-1);
    }
  }

  last_time = timer_ticks ();

  cpu_restart (&main_electron.cpu);

  if (video_init (main_electron.memory, 0) == -1)
  {
    eprintf ("couldn't initalise video\n");
    exit (-1);
  }

  if (!do_monitor || monitor (&main_electron) == -2)
    electron_run (&main_electron);

  video_quit ();

  return 0;
}
