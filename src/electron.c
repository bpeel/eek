#include "config.h"

#include <stdio.h>
#include <string.h>

#include "eek.h"
#include "electron.h"
#include "cpu.h"
#include "util.h"
#include "timer.h"
#include "events.h"
#include "video.h"
#include "monitor.h"

#define ELECTRON_CYCLES_PER_SCANLINE 128
#define ELECTRON_SCANLINES_PER_FRAME 312
#define ELECTRON_TIMER_SCANLINE      100
#define ELECTRON_END_SCANLINE        256

#define ELECTRON_I_MASTER      1
#define ELECTRON_I_POWERON     2
#define ELECTRON_I_DISPLAY_END 4
#define ELECTRON_I_RTC         8
#define ELECTRON_I_RECEIVE     16
#define ELECTRON_I_TRANSMIT    32
#define ELECTRON_I_HIGH_TONE   64

#define ELECTRON_C_HIGH_TONE   64
#define ELECTRON_C_RTC         32
#define ELECTRON_C_DISPLAY_END 16

#define ELECTRON_TICKS_PER_FRAME 20

#define ELECTRON_MODE(electron) (((electron)->sheila[0x7] >> 3) & 7)

UBYTE electron_read_from_location (Electron *electron, UWORD address);
void electron_write_to_location (Electron *electron, UWORD address, UBYTE val);

void
electron_init (Electron *electron)
{
  /* Initialise the sheila to zeros */
  memset (electron->sheila, 0, sizeof (electron->sheila));

  /* Set the power on transient bit */
  electron->sheila[0x0] |= ELECTRON_I_POWERON;

  /* We haven't got any paged roms yet */
  memset (electron->paged_roms, 0, sizeof (electron->paged_roms));

  electron->scanline = 0;
  electron->ienabled = 0;
  electron->page = ELECTRON_BASIC_PAGE;
  video_set_start_address (0x0000);
  video_set_mode (ELECTRON_MODE (electron));

  /* No keys are being pressed */
  memset (electron->keyboard, '\0', 14);

  /* Initialise the cpu */
  cpu_init (&electron->cpu, electron->memory,
	    (CpuMemReadFunc) electron_read_from_location, 
	    (CpuMemWriteFunc) electron_write_to_location, electron);
}

void
electron_generate_interrupt (Electron *electron, int num)
{
  /* Set the event occuring flag */
  electron->sheila[0x0] |= num;

  /* Check if this type of interrupt is enabled */
  if ((electron->ienabled & num))
    /* If so, fire the interrupt */
    cpu_set_irq (&electron->cpu);
}

void
electron_next_scanline (Electron *electron)
{
  electron->cpu.time -= ELECTRON_CYCLES_PER_SCANLINE;

  if (++electron->scanline > ELECTRON_SCANLINES_PER_FRAME)
    electron->scanline = 0;

  if (electron->scanline < ELECTRON_END_SCANLINE)
  {
    if (electron->scanline == 0)
      /* The video start address is only recognised at the start of
	 each frame */
      video_set_start_address (((electron->sheila[0x3] & 0x3f) << 9)
			       | ((electron->sheila[0x2] & 0xe0) << 1));

    video_draw_scanline (electron->scanline);

    /* If we're on the scanline where the timer interrupt occurs then
       generate that interrupt */
    if (electron->scanline == ELECTRON_TIMER_SCANLINE)
      electron_generate_interrupt (electron, ELECTRON_I_RTC);
  }
  else if (electron->scanline == ELECTRON_END_SCANLINE)
  {
    electron_generate_interrupt (electron, ELECTRON_I_DISPLAY_END);
    /* We've drawn a whole screen so show it on the display */
    video_update ();
  }
}

void
electron_step (Electron *electron)
{
  /* Execute one instruction */
  cpu_fetch_execute (&electron->cpu, electron->cpu.time + 1);
  /* If we've done a whole scanline's worth of cycles then draw the next scanline */
  if (electron->cpu.time >= ELECTRON_CYCLES_PER_SCANLINE)
    electron_next_scanline (electron);
}

int
electron_run (Electron *electron)
{
  unsigned int then = timer_ticks ();
  int quit = 0;
  
  while (!quit)
  {
    /* Execute instructions until the next scanline */
    if (cpu_fetch_execute (&electron->cpu, ELECTRON_CYCLES_PER_SCANLINE))
      monitor (electron);
    else
    {
      /* Process the next scanline */
      electron_next_scanline (electron);

      if ((quit = events_check (electron)))
	break;
    
      /* Sync to 50Hz video refresh rate */
      if (electron->scanline == 0)
      {
	unsigned int now;

	then += ELECTRON_TICKS_PER_FRAME;

	do
	{
	  now = timer_ticks ();
	  /* If time has gone backwards or we would wait more then the
	     time to scan one frame then something has gone wrong so
	     we should just start the timing again */
	  if (then < now || then - now > ELECTRON_TICKS_PER_FRAME)
	    then = now;
	  else
	    timer_sleep (then - now);
	}
	while (now < then);
      }
    }
  }

  return quit;
}

int
electron_load_os_rom (Electron *electron, FILE *in)
{
  if (fread (electron->os_rom, sizeof (UBYTE), ELECTRON_OS_ROM_LENGTH, in)
      < ELECTRON_OS_ROM_LENGTH)
    return -1;
  else
    return 0;
}

int
electron_load_paged_rom (Electron *electron, int page, FILE *in)
{
  UBYTE *buf;
  page &= 0x0f;

  if (electron->paged_roms[page])
    buf = electron->paged_roms[page];
  else
    electron->paged_roms[page] = buf 
      = xmalloc (ELECTRON_PAGED_ROM_LENGTH * sizeof (UBYTE));

  if (fread (buf, sizeof (UBYTE), ELECTRON_PAGED_ROM_LENGTH, in)
      < ELECTRON_PAGED_ROM_LENGTH)
  {
    xfree (buf);
    electron->paged_roms[page] = NULL;
    return -1;
  }

  return 0;
}

static void
electron_update_palette (Electron *electron)
{
  const UBYTE *palette = electron->sheila + 0x08;

  switch (ELECTRON_MODE (electron))
  {
    case 0: case 3: case 4: case 6: case 7:
      /* 2 colours */
      video_set_color (0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (1, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      break;
    case 1: case 5:
      /* 4 colours */
      video_set_color (0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (1, ((palette[1] >> 1) & 1) | ((palette[1] >> 4) & 2) | ((palette[0] >> 3) & 4));
      video_set_color (2, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      video_set_color (3, ((palette[1] >> 3) & 1) | ((palette[0] >> 2) & 2) | ((palette[0] >> 5) & 4));
      break;
    case 2:
      /* 16 colours */
      video_set_color (0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (1, (palette[7] & 1) | ((palette[7] >> 3) & 2) | ((palette[6] >> 2) & 4));
      video_set_color (2, ((palette[1] >> 1) & 1) | ((palette[1] >> 4) & 2) | ((palette[0] >> 3) & 4));
      video_set_color (3, ((palette[7] >> 1) & 1) | ((palette[7] >> 4) & 2) | ((palette[6] >> 3) & 4));
      video_set_color (4, (palette[3] & 1) | ((palette[3] >> 3) & 2) | ((palette[2] >> 2) & 4));
      video_set_color (5, (palette[5] & 1) | ((palette[5] >> 3) & 2) | ((palette[4] >> 2) & 4));
      video_set_color (6, ((palette[3] >> 1) & 1) | ((palette[3] >> 4) & 2) | ((palette[2] >> 3) & 4));
      video_set_color (7, ((palette[5] >> 1) & 1) | ((palette[5] >> 4) & 2) | ((palette[4] >> 3) & 4));
      video_set_color (8, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      video_set_color (9, ((palette[7] >> 2) & 1) | ((palette[6] >> 1) & 2) | ((palette[6] >> 4) & 4));
      video_set_color (10, ((palette[1] >> 3) & 1) | ((palette[0] >> 2) & 2) | ((palette[0] >> 5) & 4));
      video_set_color (11, ((palette[7] >> 3) & 1) | ((palette[6] >> 2) & 2) | ((palette[6] >> 5) & 4));
      video_set_color (12, ((palette[3] >> 2) & 1) | ((palette[2] >> 1) & 2) | ((palette[2] >> 4) & 4));
      video_set_color (13, ((palette[5] >> 2) & 1) | ((palette[4] >> 1) & 2) | ((palette[4] >> 4) & 4));
      video_set_color (14, ((palette[3] >> 3) & 1) | ((palette[2] >> 2) & 2) | ((palette[2] >> 5) & 4));
      video_set_color (15, ((palette[5] >> 3) & 1) | ((palette[4] >> 2) & 2) | ((palette[4] >> 5) & 4));
      break;
  }
}

UBYTE
electron_read_from_location (Electron *electron, UWORD location)
{
  /* Check if it's in the paged rom location */
  if (location >= ELECTRON_PAGED_ROM_ADDRESS
      && location < ELECTRON_PAGED_ROM_ADDRESS + ELECTRON_PAGED_ROM_LENGTH)
  {
    UBYTE page = electron->page;
    /* Basic and keyboard are available in two locations */
    if ((page & 0x0C) == 0x08)
      page &= 0x0E;
    if (page == ELECTRON_KEYBOARD_PAGE)
    {
      int i, value = 0;

      /* or together all of the locations of the keyboard memory which
	 have a '0' in the corresponding address bit */
      for (i = 0; i < 14; i++)
      {
	if ((location & 1) == 0)
	  value |= electron->keyboard[i];
	location >>= 1;
      }

      fflush (stdout);

      return value;
    }
    else if (electron->paged_roms[page])
      return (electron->paged_roms[page])[location - ELECTRON_PAGED_ROM_ADDRESS];
    else
    /* Return 0 if there is no rom at this page */
      return 0x00;
  }
  /* If it's in the sheila page then return from that location */
  else if ((location >> 8) == ELECTRON_SHEILA_PAGE)
    switch (location & 0x0f)
    {
      case 0x0:
	{
	  /* bit 7 is always set */
	  int r = (electron->sheila[0x0] | 0x80) & ~ELECTRON_I_MASTER;
	  /* the master irq bit is set if an enabled interrupt is occuring */
	  if ((electron->ienabled & electron->sheila[0x0] & ~(ELECTRON_I_MASTER | ELECTRON_I_POWERON | 0x80)))
	    r |= ELECTRON_I_MASTER;
	  /* the power on bit is only set the first time it is read */
	  electron->sheila[0x0] &= ~ELECTRON_I_POWERON;
	  return r;
	}
      case 0x1: case 0x2: case 0x3: case 0x5: case 0x6: case 0x7:
      case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd:
      case 0xe: case 0xf:
	/* write only locations read from the underlying ROM */
	return electron->os_rom[location - ELECTRON_OS_ROM_ADDRESS];
      default:
	return electron->sheila[location & 0x0f];
    }
  /* Check if it's in the OS rom */
  else if (location >= ELECTRON_OS_ROM_ADDRESS 
	   && location < ELECTRON_OS_ROM_ADDRESS + ELECTRON_OS_ROM_LENGTH)
    return electron->os_rom[location - ELECTRON_OS_ROM_ADDRESS];
  /* Otherwise if it's in memory return from there */
  else if (location < CPU_RAM_SIZE)
    return electron->memory[location];

  /* If we get here we don't know what the location is for so just
     return a random byte */
  return 0xff;
}

void
electron_write_to_location (Electron *electron, UWORD location, UBYTE v)
{
  /* If it's in the sheila page then use that */
  if ((location >> 8) == ELECTRON_SHEILA_PAGE)
    switch (location & 0x0f)
    {
      case 0x0:
	electron->ienabled = v & ~ELECTRON_I_POWERON;
	break;
      case 0x5:
	/* Set the page number. If the keyboard or basic page is
	   currently selected then only pages 8-15 are actually
	   honoured */
	if (electron->page < 8 || electron->page > 11 || (v & 0x0f) >= 8)
	  electron->page = v & 0x0f;
	/* Clear interrupts */
	if (v & ELECTRON_C_HIGH_TONE)
	  electron->sheila[0x0] &= ~ELECTRON_I_HIGH_TONE;
	if (v & ELECTRON_C_RTC)
	  electron->sheila[0x0] &= ~ELECTRON_I_RTC;
	if (v & ELECTRON_C_DISPLAY_END)
	  electron->sheila[0x0] &= ~ELECTRON_I_DISPLAY_END;
	/* If no interrupts are on then put the irq line back */
	if ((electron->sheila[0x0] & electron->ienabled) == 0)
	  cpu_reset_irq (&electron->cpu);
	break;
      case 0x7:
	if (electron->sheila[location & 0x0f] != v)
	{
	  electron->sheila[location & 0x0f] = v;
	  video_set_mode (ELECTRON_MODE (electron));
	  electron_update_palette (electron);
	}
	break;
      default:
	electron->sheila[location & 0x0f] = v;
	if ((location & 0x0f) >= 0x08)
	  electron_update_palette (electron);
	break;
    }
  /* Otherwise if it's in memory use that */
  else if (location < CPU_RAM_SIZE)
    electron->memory[location] = v;
}
