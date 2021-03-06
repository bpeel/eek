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

#include "video.h"

void
video_init (Video *video, const guint8 *memory)
{
  video->memory = memory;
  video->mode = 0;
  video->start_address = 0x4000;
  memset (video->logical_colors, '\0', VIDEO_LOGICAL_COLOR_COUNT);
}

void
video_set_mode (Video *video, guint8 mode)
{
  video->mode = mode & 7;
}

void
video_set_start_address (Video *video, guint16 start)
{
  video->start_address = start;
}

void
video_set_color (Video *video, guint8 logical, guint8 physical)
{
  video->logical_colors[logical] = physical;
}

void
video_draw_scanline (Video *video, int line)
{
  int i, j, c, v;
  unsigned char *p;
  guint16 a;

  p = video->screen_memory + VIDEO_SCREEN_PITCH * line * VIDEO_YSCALE;

  switch (video->mode)
  {
    case 0:
      /* 0x280 bytes per 8 lines */
      a = (line / 8) * 0x280 + (line % 8) + (video->start_address ? video->start_address : 0x3000);
      for (i = 0; i < 80; i++)
      {
        if (a >= 0x8000)
          a = (a + 0x3000) & 0x7fff;
        c = video->memory[a];
        for (j = 0; j < 8; j++)
          *(p++) = video->logical_colors[((c <<= 1) & 0x100) ? 1 : 0];
        a += 8;
      }
      break;
    case 1:
      /* 0x280 bytes per 8 lines */
      a = (line / 8) * 0x280 + (line % 8) + (video->start_address ? video->start_address : 0x3000);
      for (i = 0; i < 80; i++)
      {
        if (a >= 0x8000)
          a = (a + 0x3000) & 0x7fff;
        c = video->memory[a];
        for (j = 0; j < 4; j++)
        {
          *(p++) = v = video->logical_colors[((c >> 6) & 2) | ((c >> 3) & 1)];
          *(p++) = v;
          c <<= 1;
        }
        a += 8;
      }
      break;
    case 2:
      /* 0x280 bytes per 8 lines */
      a = (line / 8) * 0x280 + (line % 8) + (video->start_address ? video->start_address : 0x3000);
      for (i = 0; i < 80; i++)
      {
        if (a >= 0x8000)
          a = (a + 0x3000) & 0x7fff;
        c = video->memory[a];
        for (j = 0; j < 2; j++)
        {
          *(p++) = v = video->logical_colors[((c >> 4) & 8)
                                            | ((c >> 3) & 4)
                                            | ((c >> 2) & 2)
                                            | ((c >> 1) & 1)];
          *(p++) = v;
          *(p++) = v;
          *(p++) = v;
          c <<= 1;
        }
        a += 8;
      }
      break;
    case 3:
      /* the last two out of every 10 lines and the last six lines of
         the screen are blank */
      if (line % 10 >= 8 || line >= 250)
        memset (p, 0x7, VIDEO_WIDTH);
      else
      {
        /* 0x280 bytes per 10 lines */
        a = (line / 10) * 0x280 + (line % 10) + (video->start_address ? video->start_address : 0x4000);
        for (i = 0; i < 80; i++)
        {
          if (a >= 0x8000)
            a = (a + 0x4000) & 0x7fff;
          c = video->memory[a];
          for (j = 0; j < 8; j++)
            *(p++) = video->logical_colors[((c <<= 1) & 0x100) ? 1 : 0];
          a += 8;
        }
      }
      break;
    default:
      /* mode 7 becomes mode 4 */
    case 4:
      /* 0x140 bytes per 8 lines */
      a = (line / 8) * 0x140 + (line % 8) + (video->start_address ? video->start_address : 0x5800);
      for (i = 0; i < 40; i++)
      {
        if (a >= 0x8000)
          a = (a + 0x5800) & 0x7fff;
        c = video->memory[a];
        for (j = 0; j < 8; j++)
        {
          *(p++) = v = video->logical_colors[((c <<= 1) & 0x100) ? 1 : 0];
          *(p++) = v;
        }
        a += 8;
      }
      break;
    case 5:
      /* 0x140 bytes per 8 lines */
      a = (line / 8) * 0x140 + (line % 8) + (video->start_address ? video->start_address : 0x5800);
      for (i = 0; i < 40; i++)
      {
        if (a >= 0x8000)
          a = (a + 0x5800) & 0x7fff;
        c = video->memory[a];
        for (j = 0; j < 4; j++)
        {
          *(p++) = v = video->logical_colors[((c >> 6) & 2) | ((c >> 3) & 1)];
          *(p++) = v;
          *(p++) = v;
          *(p++) = v;
          c <<= 1;
        }
        a += 8;
      }
      break;
    case 6:
      /* the last two out of every 10 lines and the last six lines of
         the screen are blank */
      if (line % 10 >= 8 || line >= 250)
        memset (p, 0x7, VIDEO_WIDTH);
      else
      {
        /* 0x140 bytes per 10 lines */
        a = (line / 10) * 0x140 + (line % 10) + (video->start_address ? video->start_address : 0x6000);
        for (i = 0; i < 40; i++)
        {
          if (a >= 0x8000)
            a = (a + 0x6000) & 0x7fff;
          c = video->memory[a];
          for (j = 0; j < 8; j++)
          {
            *(p++) = v = video->logical_colors[((c <<= 1) & 0x100) ? 1 : 0];;
            *(p++) = v;
          }
          a += 8;
        }
      }
      break;
  }

  /* Copy the scanline a few times */
  for (i = 1; i < VIDEO_YSCALE; i++)
    memcpy (video->screen_memory + VIDEO_SCREEN_PITCH * line * VIDEO_YSCALE
            + VIDEO_SCREEN_PITCH * i,
            video->screen_memory + VIDEO_SCREEN_PITCH * line * VIDEO_YSCALE,
            VIDEO_WIDTH);
}
