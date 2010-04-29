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

#ifndef _VIDEO_H
#define _VIDEO_H

#include <glib/gtypes.h>

#define VIDEO_YSCALE       2
#define VIDEO_WIDTH        640
#define VIDEO_SCREEN_PITCH VIDEO_WIDTH
#define VIDEO_HEIGHT       (256 * VIDEO_YSCALE)
#define VIDEO_MEMORY_SIZE  (VIDEO_SCREEN_PITCH * VIDEO_HEIGHT)

#define VIDEO_LOGICAL_COLOR_COUNT 16

typedef struct _Video Video;

struct _Video
{
  const guint8 *memory;
  guint8 screen_memory[VIDEO_MEMORY_SIZE];
  guint16 start_address;
  guint8 logical_colors[VIDEO_LOGICAL_COLOR_COUNT];
  guint8 mode;
};

void video_init (Video *video, const guint8 *memory);
void video_draw_scanline (Video *video, int line);
void video_set_start_address (Video *video, guint16 start);
void video_set_mode (Video *video, guint8 mode);
void video_set_color (Video *video, guint8 logical, guint8 physical);

#endif /* _VIDEO_H */
