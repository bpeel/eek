#include "config.h"

#include <stdio.h>

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

static const UWORD electron_mode_start_addresses[] =
  { 0x3000, 0x3000, 0x3000, 0x4000, 0x5800, 0x5800, 0x6000 };

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
  video_set_start_address (electron_mode_start_addresses[ELECTRON_MODE (electron)]);
  video_set_mode (ELECTRON_MODE (electron));

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
    video_draw_scanline (electron->scanline);

  /* If we're on the scanline where the timer interrupt occurs then
     generate that interrupt */
  if (electron->scanline == ELECTRON_TIMER_SCANLINE)
    electron_generate_interrupt (electron, ELECTRON_I_RTC);
  if (electron->scanline == ELECTRON_END_SCANLINE)
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
	then += 20;
	while (!quit && timer_ticks () < then)
	  quit = events_check (electron);
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
  char *buf;
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

UBYTE
electron_read_from_location (Electron *electron, UWORD location)
{
  /* Check if it's in the paged rom location */
  if (location >= ELECTRON_PAGED_ROM_ADDRESS
      && location < ELECTRON_PAGED_ROM_ADDRESS + ELECTRON_OS_ROM_LENGTH)
  {
    UBYTE page = electron->page;
    /* Basic and keyboard are available in two locations */
    if ((page & 0x0C) == 0x08)
      page &= 0x0E;
    if (electron->paged_roms[page])
      return (electron->paged_roms[page])[location - ELECTRON_PAGED_ROM_ADDRESS];
  }
  /* If it's in the sheila page then return from that location */
  else if ((location >> 8) == ELECTRON_SHEILA_PAGE)
    switch (location & 0x0f)
    {
      case 0x0:
	{
	  /* bit 7 is always set */
	  int r = electron->sheila[0x0] | 0x80;
	  /* the master irq bit is set if an enabled interrupt is occuring */
	  if ((electron->ienabled & electron->sheila[0x0]))
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
	electron->ienabled = v;
	break;
      case 0x2:
      case 0x3:
	electron->sheila[location & 0x0f] = v;
	if (electron->sheila[0x3] == 0 && electron->sheila[0x2] == 0)
	  video_set_start_address (electron_mode_start_addresses
				   [ELECTRON_MODE (electron)]);
	else
	  video_set_start_address (((electron->sheila[0x3] & 0x3f) << 9)
				   | ((electron->sheila[0x2] & 0xe0) << 1));
	break;
      case 0x5:
	/* Set the page number */
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
	electron->sheila[location & 0x07] = v;
	video_set_mode (ELECTRON_MODE (electron));
	break;
      default:
	electron->sheila[location & 0x0f] = v;
	break;
    }
  /* Otherwise if it's in memory use that */
  else if (location < CPU_RAM_SIZE)
    electron->memory[location] = v;
}
