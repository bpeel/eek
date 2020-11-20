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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "electron.h"
#include "cpu.h"
#include "video.h"
#include "tapebuffer.h"

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

/* Cassette runs at 1200 baud with 2 stop bits per byte so there is
   120 bytes per second of actual data. There are 50 frames per second
   and 312 scanlines per frame so there are 15600 scanlines per
   second. Therefore there are 15600/120 = 130 scanlines per byte */
#define ELECTRON_SCANLINES_PER_CASSETTE_BYTE 130

#define ELECTRON_MODE_OF_BYTE(byte) (((byte) >> 3) & 7)
#define ELECTRON_MODE(electron) ELECTRON_MODE_OF_BYTE((electron)->sheila[0x7])

/* Number of times to scan a queued key before moving on to the next state */
#define ELECTRON_QUEUED_KEYS_SCAN_COUNT 4

guint8 electron_read_from_location (Electron *electron, guint16 address);
void electron_write_to_location (Electron *electron, guint16 address, guint8 val);

typedef struct
{
  gunichar ch;
  ElectronQueuedKey key;
} ElectronKeyMap;

static const ElectronKeyMap
key_map[] =
  {
#define S (1 << ELECTRON_SHIFT_BIT)
#define C (1 << ELECTRON_CONTROL_BIT)
   { 0x1b, { .line = 13, .bit = 0, .modifiers = 0 } },
   { '1', { .line = 12, .bit = 0, .modifiers = 0 } },
   { '!', { .line = 12, .bit = 0, .modifiers = S } },
   { '2', { .line = 11, .bit = 0, .modifiers = 0 } },
   { '"', { .line = 11, .bit = 0, .modifiers = S } },
   { '3', { .line = 10, .bit = 0, .modifiers = 0 } },
   { '#', { .line = 10, .bit = 0, .modifiers = S } },
   { '4', { .line = 9, .bit = 0, .modifiers = 0 } },
   { '$', { .line = 9, .bit = 0, .modifiers = S } },
   { '5', { .line = 8, .bit = 0, .modifiers = 0 } },
   { '%', { .line = 8, .bit = 0, .modifiers = S } },
   { '6', { .line = 7, .bit = 0, .modifiers = 0 } },
   { '&', { .line = 7, .bit = 0, .modifiers = S } },
   { '7', { .line = 6, .bit = 0, .modifiers = 0 } },
   { '\'', { .line = 6, .bit = 0, .modifiers = S } },
   { '8', { .line = 5, .bit = 0, .modifiers = 0 } },
   { '(', { .line = 5, .bit = 0, .modifiers = S } },
   { '9', { .line = 4, .bit = 0, .modifiers = 0 } },
   { ')', { .line = 4, .bit = 0, .modifiers = S } },
   { '0', { .line = 3, .bit = 0, .modifiers = 0 } },
   { '@', { .line = 3, .bit = 0, .modifiers = S } },
   { '-', { .line = 2, .bit = 0, .modifiers = 0 } },
   { '=', { .line = 2, .bit = 0, .modifiers = S } },
   { '^', { .line = 1, .bit = 0, .modifiers = S } },
   { '~', { .line = 1, .bit = 0, .modifiers =C } },
   { '|', { .line = 0, .bit = 0, .modifiers = S } },
   { '\\', { .line = 0, .bit = 0, .modifiers = C } },
   { 'q', { .line = 12, .bit = 1, .modifiers = 0 } },
   { 'Q', { .line = 12, .bit = 1, .modifiers = S } },
   { 'w', { .line = 11, .bit = 1, .modifiers = 0 } },
   { 'W', { .line = 11, .bit = 1, .modifiers = S } },
   { 'e', { .line = 10, .bit = 1, .modifiers = 0 } },
   { 'E', { .line = 10, .bit = 1, .modifiers = S } },
   { 'r', { .line = 9, .bit = 1, .modifiers = 0 } },
   { 'R', { .line = 9, .bit = 1, .modifiers = S } },
   { 't', { .line = 8, .bit = 1, .modifiers = 0 } },
   { 'T', { .line = 8, .bit = 1, .modifiers = S } },
   { 'y', { .line = 7, .bit = 1, .modifiers = 0 } },
   { 'Y', { .line = 7, .bit = 1, .modifiers = S } },
   { 'u', { .line = 6, .bit = 1, .modifiers = 0 } },
   { 'U', { .line = 6, .bit = 1, .modifiers = S } },
   { 'i', { .line = 5, .bit = 1, .modifiers = 0 } },
   { 'I', { .line = 5, .bit = 1, .modifiers = S } },
   { 'o', { .line = 4, .bit = 1, .modifiers = 0 } },
   { 'O', { .line = 4, .bit = 1, .modifiers = S } },
   { 'p', { .line = 3, .bit = 1, .modifiers = 0 } },
   { 'P', { .line = 3, .bit = 1, .modifiers = S } },
   { 0xa3, { .line = 2, .bit = 1, .modifiers = S } },
   { '{', { .line = 2, .bit = 1, .modifiers = C } },
   { '_', { .line = 1, .bit = 1, .modifiers = S } },
   { '}', { .line = 1, .bit = 1, .modifiers = C } },
   { '\t', { .line = 0, .bit = 1, .modifiers = 0 } },
   { '[', { .line = 0, .bit = 1, .modifiers = S } },
   { ']', { .line = 0, .bit = 1, .modifiers = C } },
   { 'a', { .line = 12, .bit = 2, .modifiers = 0 } },
   { 'A', { .line = 12, .bit = 2, .modifiers = S } },
   { 's', { .line = 11, .bit = 2, .modifiers = 0 } },
   { 'S', { .line = 11, .bit = 2, .modifiers = S } },
   { 'd', { .line = 10, .bit = 2, .modifiers = 0 } },
   { 'D', { .line = 10, .bit = 2, .modifiers = S } },
   { 'f', { .line = 9, .bit = 2, .modifiers = 0 } },
   { 'F', { .line = 9, .bit = 2, .modifiers = S } },
   { 'g', { .line = 8, .bit = 2, .modifiers = 0 } },
   { 'G', { .line = 8, .bit = 2, .modifiers = S } },
   { 'h', { .line = 7, .bit = 2, .modifiers = 0 } },
   { 'H', { .line = 7, .bit = 2, .modifiers = S } },
   { 'j', { .line = 6, .bit = 2, .modifiers = 0 } },
   { 'J', { .line = 6, .bit = 2, .modifiers = S } },
   { 'k', { .line = 5, .bit = 2, .modifiers = 0 } },
   { 'K', { .line = 5, .bit = 2, .modifiers = S } },
   { 'l', { .line = 4, .bit = 2, .modifiers = 0 } },
   { 'L', { .line = 4, .bit = 2, .modifiers = S } },
   { ';', { .line = 3, .bit = 2, .modifiers = 0 } },
   { '+', { .line = 3, .bit = 2, .modifiers = S } },
   { ':', { .line = 2, .bit = 2, .modifiers = 0 } },
   { '*', { .line = 2, .bit = 2, .modifiers = S } },
   { '\n', { .line = 1, .bit = 2, .modifiers = 0 } },
   { 'z', { .line = 12, .bit = 3, .modifiers = 0 } },
   { 'Z', { .line = 12, .bit = 3, .modifiers = S } },
   { 'x', { .line = 11, .bit = 3, .modifiers = 0 } },
   { 'X', { .line = 11, .bit = 3, .modifiers = S } },
   { 'c', { .line = 10, .bit = 3, .modifiers = 0 } },
   { 'C', { .line = 10, .bit = 3, .modifiers = S } },
   { 'v', { .line = 9, .bit = 3, .modifiers = 0 } },
   { 'V', { .line = 9, .bit = 3, .modifiers = S } },
   { 'b', { .line = 8, .bit = 3, .modifiers = 0 } },
   { 'B', { .line = 8, .bit = 3, .modifiers = S } },
   { 'n', { .line = 7, .bit = 3, .modifiers = 0 } },
   { 'N', { .line = 7, .bit = 3, .modifiers = S } },
   { 'm', { .line = 6, .bit = 3, .modifiers = 0 } },
   { 'M', { .line = 6, .bit = 3, .modifiers = S } },
   { ',', { .line = 5, .bit = 3, .modifiers = 0 } },
   { '<', { .line = 5, .bit = 3, .modifiers = S } },
   { '.', { .line = 4, .bit = 3, .modifiers = 0 } },
   { '>', { .line = 4, .bit = 3, .modifiers = S } },
   { '/', { .line = 3, .bit = 3, .modifiers = 0 } },
   { '?', { .line = 3, .bit = 3, .modifiers = S } },
   { ' ', { .line = 0, .bit = 3, .modifiers = 0 } },
   { 0 },
#undef S
#undef C
  };

Electron *
electron_new ()
{
  Electron *electron = g_malloc (sizeof (Electron));

  /* Initialise the sheila to zeros */
  memset (electron->sheila, 0, sizeof (electron->sheila));

  /* Set the power on transient bit */
  electron->sheila[0x0] |= ELECTRON_I_POWERON;

  /* We haven't got any paged roms yet */
  memset (electron->paged_roms, 0, sizeof (electron->paged_roms));

  electron->scanline = 0;
  electron->ienabled = 0;
  electron->page = ELECTRON_BASIC_PAGE;
  video_init (&electron->video, electron->memory);
  video_set_start_address (&electron->video, 0x0000);
  video_set_mode (&electron->video, ELECTRON_MODE (electron));

  /* No keys are being pressed */
  memset (electron->keyboard, '\0', 14);

  electron->queued_keys = g_array_new (FALSE, FALSE,
                                       sizeof (ElectronQueuedKey));
  electron->queued_keys_pos = 0;

  /* Allocate tape buffer */
  electron->tape_buffer = tape_buffer_new ();
  electron->cassette_scanline_counter = 0;
  electron->data_shift_has_data = FALSE;

  /* Initialise the cpu */
  cpu_init (&electron->cpu, electron->memory,
            (CpuMemReadFunc) electron_read_from_location,
            (CpuMemWriteFunc) electron_write_to_location, electron);

  return electron;
}

void
electron_add_queued_keys (Electron *electron,
                          size_t n_keys,
                          const ElectronQueuedKey *keys)
{
  if (electron->queued_keys_pos >= electron->queued_keys->len)
  {
    g_array_set_size (electron->queued_keys, 0);
    electron->queued_keys_pos = 0;
    electron->queued_keys_scan_count = 0;
  }

  g_array_append_vals (electron->queued_keys, keys, n_keys);
}

void
electron_type_string (Electron *electron,
                      const char *str)
{
  for (; *str; str = g_utf8_next_char (str))
  {
    gunichar ch = g_utf8_get_char (str);
    gboolean caps_lock = !!(electron->sheila[0x7] & 0x80);
    int i;

    if (caps_lock && ch < 0x80 && g_ascii_isalpha (ch))
      ch ^= 32;

    for (i = 0; key_map[i].ch; i++)
    {
      if (key_map[i].ch == ch)
        electron_add_queued_keys (electron, 1, &key_map[i].key);
    }
  }
}

void
electron_free (Electron *electron)
{
  int i;

  g_array_free (electron->queued_keys, TRUE);

  /* Free all of the paged rom data */
  for (i = 0; i < ELECTRON_PAGED_ROM_COUNT; i++)
    if (electron->paged_roms[i])
      g_free (electron->paged_roms[i]);
  /* Free the cassette buffer */
  tape_buffer_free (electron->tape_buffer);
  /* Free the electron data */
  g_free (electron);
}

static void
electron_generate_interrupt (Electron *electron, int num)
{
  /* Set the event occuring flag */
  electron->sheila[0x0] |= num;

  /* Check if this type of interrupt is enabled */
  if ((electron->ienabled & num))
    /* If so, fire the interrupt */
    cpu_set_irq (&electron->cpu);
}

static void
electron_clear_interrupts (Electron *electron, int mask)
{
  electron->sheila[0x0] &= ~mask;
  /* If no interrupts are on then put the irq line back */
  if ((electron->sheila[0x0] & electron->ienabled) == 0)
    cpu_reset_irq (&electron->cpu);
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
      video_set_start_address (&electron->video, ((electron->sheila[0x3] & 0x3f) << 9)
                               | ((electron->sheila[0x2] & 0xe0) << 1));

    video_draw_scanline (&electron->video, electron->scanline);

    /* If we're on the scanline where the timer interrupt occurs then
       generate that interrupt */
    if (electron->scanline == ELECTRON_TIMER_SCANLINE)
      electron_generate_interrupt (electron, ELECTRON_I_RTC);
  }
  else if (electron->scanline == ELECTRON_END_SCANLINE)
    electron_generate_interrupt (electron, ELECTRON_I_DISPLAY_END);

  /* Is it time for the next byte from the cassette? */
  if (++electron->cassette_scanline_counter >= ELECTRON_SCANLINES_PER_CASSETTE_BYTE)
  {
    electron->cassette_scanline_counter = 0;

    /* Is the cassette motor on? */
    if ((electron->sheila[0x7] & 0x40))
    {
      /* Are we in write mode? */
      if ((electron->sheila[0x7] & 0x06) == 0x04)
      {
        /* Add a high tone if the cassette buffer has no data */
        if (electron->data_shift_has_data)
        {
          tape_buffer_store_byte (electron->tape_buffer, electron->sheila[0x4]);
          electron->data_shift_has_data = FALSE;
          electron_generate_interrupt (electron, ELECTRON_I_TRANSMIT);
        }
        else
          tape_buffer_store_high_tone (electron->tape_buffer);
      }
      else
      {
        int next_byte;

        /* If we've gone past the end of the tape then add silence */
        if (tape_buffer_is_at_end (electron->tape_buffer))
        {
          tape_buffer_store_silence (electron->tape_buffer);
          next_byte = TAPE_BUFFER_SILENCE;
        }
        else
          next_byte = tape_buffer_get_next_byte (electron->tape_buffer);

        /* If it was a high tone then generate the interrupt */
        if (next_byte == TAPE_BUFFER_HIGH_TONE)
          electron_generate_interrupt (electron, ELECTRON_I_HIGH_TONE);
        else
        {
          electron_clear_interrupts (electron, ELECTRON_I_HIGH_TONE);
          /* Put the byte in the buffer if we are in read mode unless
             the tape is silent */
          if (next_byte >= 0 && (electron->sheila[0x7] & 0x06) == 0x00)
          {
            electron->sheila[0x4] = next_byte;
            electron_generate_interrupt (electron, ELECTRON_I_RECEIVE);
          }
        }
      }
    }
  }
}

void
electron_step (Electron *electron)
{
  /* Temporarily disable the breakpoint */
  int old_break_type = electron->cpu.break_type;
  electron->cpu.break_type = CPU_BREAK_NONE;
  /* Execute one instruction */
  cpu_fetch_execute (&electron->cpu, electron->cpu.time + 1);
  /* Restore the breakpoint */
  electron->cpu.break_type = old_break_type;
  /* If we've done a whole scanline's worth of cycles then draw the next scanline */
  if (electron->cpu.time >= ELECTRON_CYCLES_PER_SCANLINE)
    electron_next_scanline (electron);
}

int
electron_run_frame (Electron *electron)
{
  int got_break;

  do
  {
    /* Execute instructions until the next scanline */
    got_break = cpu_fetch_execute (&electron->cpu, ELECTRON_CYCLES_PER_SCANLINE);
    /* Process the next scanline */
    if (electron->cpu.time >= ELECTRON_CYCLES_PER_SCANLINE)
      electron_next_scanline (electron);
  } while (!got_break && electron->scanline != ELECTRON_END_SCANLINE);

  return got_break;
}

void
electron_clear_os_rom (Electron *electron)
{
  memset (electron->os_rom, 0, ELECTRON_OS_ROM_LENGTH);
}

int
electron_load_os_rom (Electron *electron, FILE *in)
{
  if (fread (electron->os_rom, sizeof (guint8), ELECTRON_OS_ROM_LENGTH, in)
      < ELECTRON_OS_ROM_LENGTH)
    return -1;
  else
    return 0;
}

void
electron_clear_paged_rom (Electron *electron, int page)
{
  if (electron->paged_roms[page])
  {
    g_free (electron->paged_roms[page]);
    electron->paged_roms[page] = NULL;
  }
}

int
electron_load_paged_rom (Electron *electron, int page, FILE *in)
{
  guint8 *buf;
  page &= 0x0f;

  if (electron->paged_roms[page])
    buf = electron->paged_roms[page];
  else
    electron->paged_roms[page] = buf
      = g_malloc (ELECTRON_PAGED_ROM_LENGTH * sizeof (guint8));

  if (fread (buf, sizeof (guint8), ELECTRON_PAGED_ROM_LENGTH, in)
      < ELECTRON_PAGED_ROM_LENGTH)
  {
    g_free (buf);
    electron->paged_roms[page] = NULL;
    return -1;
  }

  return 0;
}

static void
electron_update_palette (Electron *electron)
{
  const guint8 *palette = electron->sheila + 0x08;

  switch (ELECTRON_MODE (electron))
  {
    case 0: case 3: case 4: case 6: case 7:
      /* 2 colours */
      video_set_color (&electron->video, 0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (&electron->video, 1, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      break;
    case 1: case 5:
      /* 4 colours */
      video_set_color (&electron->video, 0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (&electron->video, 1, ((palette[1] >> 1) & 1) | ((palette[1] >> 4) & 2) | ((palette[0] >> 3) & 4));
      video_set_color (&electron->video, 2, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      video_set_color (&electron->video, 3, ((palette[1] >> 3) & 1) | ((palette[0] >> 2) & 2) | ((palette[0] >> 5) & 4));
      break;
    case 2:
      /* 16 colours */
      video_set_color (&electron->video, 0, (palette[1] & 1) | ((palette[1] >> 3) & 2) | ((palette[0] >> 2) & 4));
      video_set_color (&electron->video, 1, (palette[7] & 1) | ((palette[7] >> 3) & 2) | ((palette[6] >> 2) & 4));
      video_set_color (&electron->video, 2, ((palette[1] >> 1) & 1) | ((palette[1] >> 4) & 2) | ((palette[0] >> 3) & 4));
      video_set_color (&electron->video, 3, ((palette[7] >> 1) & 1) | ((palette[7] >> 4) & 2) | ((palette[6] >> 3) & 4));
      video_set_color (&electron->video, 4, (palette[3] & 1) | ((palette[3] >> 3) & 2) | ((palette[2] >> 2) & 4));
      video_set_color (&electron->video, 5, (palette[5] & 1) | ((palette[5] >> 3) & 2) | ((palette[4] >> 2) & 4));
      video_set_color (&electron->video, 6, ((palette[3] >> 1) & 1) | ((palette[3] >> 4) & 2) | ((palette[2] >> 3) & 4));
      video_set_color (&electron->video, 7, ((palette[5] >> 1) & 1) | ((palette[5] >> 4) & 2) | ((palette[4] >> 3) & 4));
      video_set_color (&electron->video, 8, ((palette[1] >> 2) & 1) | ((palette[0] >> 1) & 2) | ((palette[0] >> 4) & 4));
      video_set_color (&electron->video, 9, ((palette[7] >> 2) & 1) | ((palette[6] >> 1) & 2) | ((palette[6] >> 4) & 4));
      video_set_color (&electron->video, 10, ((palette[1] >> 3) & 1) | ((palette[0] >> 2) & 2) | ((palette[0] >> 5) & 4));
      video_set_color (&electron->video, 11, ((palette[7] >> 3) & 1) | ((palette[6] >> 2) & 2) | ((palette[6] >> 5) & 4));
      video_set_color (&electron->video, 12, ((palette[3] >> 2) & 1) | ((palette[2] >> 1) & 2) | ((palette[2] >> 4) & 4));
      video_set_color (&electron->video, 13, ((palette[5] >> 2) & 1) | ((palette[4] >> 1) & 2) | ((palette[4] >> 4) & 4));
      video_set_color (&electron->video, 14, ((palette[3] >> 3) & 1) | ((palette[2] >> 2) & 2) | ((palette[2] >> 5) & 4));
      video_set_color (&electron->video, 15, ((palette[5] >> 3) & 1) | ((palette[4] >> 2) & 2) | ((palette[4] >> 5) & 4));
      break;
  }
}

static guint8
read_queued_key (Electron *electron, guint16 location)
{
  const ElectronQueuedKey *key =
    &g_array_index (electron->queued_keys,
                    ElectronQueuedKey,
                    electron->queued_keys_pos);
  guint8 value = 0;
  const int all_lines = (1 << 14) - 1;
  guint16 scanning = ~location & all_lines;

  if (electron->queued_keys_scan_count < ELECTRON_QUEUED_KEYS_SCAN_COUNT)
  {
    if (scanning & (1 << key->line))
      value |= 1 << key->bit;
    if (scanning & (1 << ELECTRON_MODIFIERS_LINE))
      value |= key->modifiers;

    if (scanning == (1 << key->line))
      electron->queued_keys_scan_count++;
  }
  else if (scanning & (1 << key->line))
  {
    if (++electron->queued_keys_scan_count >=
        ELECTRON_QUEUED_KEYS_SCAN_COUNT * 2)
    {
      electron->queued_keys_pos++;
      electron->queued_keys_scan_count = 0;
    }
  }

  return value;
}

guint8
electron_read_from_location (Electron *electron, guint16 location)
{
  /* Check if it's in the paged rom location */
  if (location >= ELECTRON_PAGED_ROM_ADDRESS
      && location < ELECTRON_PAGED_ROM_ADDRESS + ELECTRON_PAGED_ROM_LENGTH)
  {
    guint8 page = electron->page;
    /* Basic and keyboard are available in two locations */
    if ((page & 0x0C) == 0x08)
      page &= 0x0E;
    if (page == ELECTRON_KEYBOARD_PAGE)
    {
      int i, value = 0;

      if (electron->queued_keys_pos < electron->queued_keys->len)
        return read_queued_key (electron, location);

      /* or together all of the locations of the keyboard memory which
         have a '0' in the corresponding address bit */
      for (i = 0; i < 14; i++)
      {
        if ((location & 1) == 0)
          value |= electron->keyboard[i];
        location >>= 1;
      }

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
      case 0x4:
        /* Reading from the tape buffer clears the read interrupt */
        electron_clear_interrupts (electron, ELECTRON_I_RECEIVE);
        /* flow through */
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
electron_write_to_location (Electron *electron, guint16 location, guint8 v)
{
  /* If it's in the sheila page then use that */
  if ((location >> 8) == ELECTRON_SHEILA_PAGE)
    switch (location & 0x0f)
    {
      case 0x0:
        electron->ienabled = v & ~ELECTRON_I_POWERON;
        break;
      case 0x4:
        electron->sheila[0x4] = v;
        /* Writing to the cassette data buffer clears the transmit interrupt */
        electron_clear_interrupts (electron, ELECTRON_I_TRANSMIT);
        /* Write the byte instead of a high tone */
        electron->data_shift_has_data = TRUE;
        break;
      case 0x5:
        {
          int clear_mask = 0;
          /* Set the page number. If the keyboard or basic page is
             currently selected then only pages 8-15 are actually
             honoured */
          if (electron->page < 8 || electron->page > 11 || (v & 0x0f) >= 8)
            electron->page = v & 0x0f;
          /* Clear interrupts */
          if (v & ELECTRON_C_HIGH_TONE)
            clear_mask |= ELECTRON_I_HIGH_TONE;
          if (v & ELECTRON_C_RTC)
            clear_mask |= ELECTRON_I_RTC;
          if (v & ELECTRON_C_DISPLAY_END)
            clear_mask |= ELECTRON_I_DISPLAY_END;
          electron_clear_interrupts (electron, clear_mask);
        }
        break;
      case 0x7:
        {
          guint8 old_value = electron->sheila[location & 0x0f];

          electron->sheila[location & 0x0f] = v;

          /* If the mode has changed then update the palette and set
             the mode */
          if (ELECTRON_MODE_OF_BYTE (old_value) != ELECTRON_MODE_OF_BYTE (v))
          {
            video_set_mode (&electron->video, ELECTRON_MODE (electron));
            electron_update_palette (electron);
          }

          /* Changing to cassette write mode causes it to start
             writing a high tone */
          if ((old_value & 0x06) != 0x04 && (v & 0x06) == 0x04)
          {
            electron->data_shift_has_data = FALSE;
            electron_generate_interrupt (electron, ELECTRON_I_TRANSMIT);
          }
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

void
electron_rewind_cassette (Electron *electron)
{
  tape_buffer_rewind (electron->tape_buffer);
}

void
electron_set_tape_buffer (Electron *electron,
                          TapeBuffer *tbuf)
{
  tape_buffer_free (electron->tape_buffer);
  electron->tape_buffer = tbuf;
}
