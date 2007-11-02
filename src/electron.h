#ifndef _ELECTRON_H
#define _ELECTRON_H

#include <stdio.h>

#include "cpu.h"
#include "video.h"
#include "tapebuffer.h"

/* Start address of the current paged rom */
#define ELECTRON_PAGED_ROM_ADDRESS 0x8000
#define ELECTRON_PAGED_ROM_LENGTH  0x4000
#define ELECTRON_PAGED_ROM_COUNT   16
/* Start address of the OS rom */
#define ELECTRON_OS_ROM_ADDRESS    0xC000
#define ELECTRON_OS_ROM_LENGTH     0x4000

#define ELECTRON_CYCLES_PER_SCANLINE 128
#define ELECTRON_SCANLINES_PER_FRAME 312
#define ELECTRON_TIMER_SCANLINE      100
#define ELECTRON_END_SCANLINE        256

#define ELECTRON_TICKS_PER_FRAME   20

/* Page for the basic rom */
#define ELECTRON_BASIC_PAGE    10
/* Page to access the keyboard */
#define ELECTRON_KEYBOARD_PAGE 8

typedef struct _Electron Electron;

#define ELECTRON_MODIFIERS_LINE 13
#define ELECTRON_FUNC_BIT       1
#define ELECTRON_CONTROL_BIT    2
#define ELECTRON_SHIFT_BIT      3

struct _Electron
{
  /* The entire memory space */
  guint8 memory[CPU_RAM_SIZE];

  /* The OS rom */
  guint8 os_rom[ELECTRON_OS_ROM_LENGTH];
  /* The current page */
  guint8 page;
  /* The current paged roms */
  guint8 *paged_roms[ELECTRON_PAGED_ROM_COUNT];
  /* The number of paged roms loaded */
  guint8 pagedc;
  /* The enabled interrupts */
  guint8 ienabled;

  /* The current scanline */
  guint16 scanline;

  /* The state of the sheila registers */
  guint8 sheila[16];
  /* Whether anything has been written to the cassette data shift register */
  guint8 data_shift_has_data : 1;

  /* The state of the keyboard */
  guint8 keyboard[14];

  /* The state of the cpu */
  Cpu cpu;

  /* The state of the display */
  Video video;

  /* Counter to count scanlines so we know when to read the next byte
     from the cassette */
  guint8 cassette_scanline_counter;
  /* Buffer for the tape data */
  TapeBuffer *tape_buffer;
};

/* Which address page represents the sheila */
#define ELECTRON_SHEILA_PAGE 0xFE

Electron *electron_new ();
void electron_free (Electron *electron);
void electron_clear_os_rom (Electron *electron);
int electron_load_os_rom (Electron *electron, FILE *in);
void electron_clear_paged_rom (Electron *electron, int page);
int electron_load_paged_rom (Electron *electron, int page, FILE *in);
void electron_write_to_location (Electron *electron, guint16 location, guint8 v);
guint8 electron_read_from_location (Electron *electron, guint16 location);
int electron_run_frame (Electron *electron);
void electron_step (Electron *electron);
void electron_rewind_cassette (Electron *electron);

#define electron_press_key(electron, line, bit) \
do { (electron)->keyboard[(line)] |= 1 << (bit); } while (0)
#define electron_release_key(electron, line, bit) \
do { (electron)->keyboard[(line)] &= ~(1 << (bit)); } while (0)

#endif /* _ELECTRON_H */
