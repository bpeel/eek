#ifndef _ELECTRON_H
#define _ELECTRON_H

#include <stdio.h>

#include "stypes.h"
#include "cpu.h"

/* Start address of the current paged rom */
#define ELECTRON_PAGED_ROM_ADDRESS 0x8000
#define ELECTRON_PAGED_ROM_LENGTH  0x4000
/* Start address of the OS rom */
#define ELECTRON_OS_ROM_ADDRESS    0xC000
#define ELECTRON_OS_ROM_LENGTH     0x4000

/* Page for the basic rom */
#define ELECTRON_BASIC_PAGE    10
/* Page to access the keyboard */
#define ELECTRON_KEYBOARD_PAGE 8

typedef struct _Electron Electron;

struct _Electron
{
  /* The entire memory space */
  UBYTE memory[CPU_RAM_SIZE];

  /* The OS rom */
  UBYTE os_rom[ELECTRON_OS_ROM_LENGTH];
  /* The current page */
  UBYTE page;
  /* The current paged roms */
  UBYTE *paged_roms[16];
  /* The number of paged roms loaded */
  UBYTE pagedc;
  /* The enabled interrupts */
  UBYTE ienabled;

  /* The current scanline */
  UWORD scanline;

  /* The state of the sheila registers */
  UBYTE sheila[16];

  /* The state of the cpu */
  Cpu cpu;
};

/* Which address page represents the sheila */
#define ELECTRON_SHEILA_PAGE 0xFE

void electron_init (Electron *electron);
int electron_load_os_rom (Electron *electron, FILE *in);
int electron_load_paged_rom (Electron *electron, int page, FILE *in);
void electron_write_to_location (Electron *electron, UWORD location, UBYTE v);
UBYTE electron_read_from_location (Electron *electron, UWORD location);
int electron_run (Electron *electron);
void electron_step (Electron *electron);

#endif /* _ELECTRON_H */
