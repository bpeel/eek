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

#ifndef _TAPE_BUFFER_H
#define _TAPE_BUFFER_H

#include <glib/gtypes.h>

typedef struct _TapeBuffer TapeBuffer;

#define TAPE_BUFFER_HIGH_TONE -1
#define TAPE_BUFFER_SILENCE   -2

typedef enum
{
  TAPE_BUFFER_CALLBACK_TYPE_DATA,
  TAPE_BUFFER_CALLBACK_TYPE_HIGH_TONE,
  TAPE_BUFFER_CALLBACK_TYPE_SILENCE
} TapeBufferCallbackType;

typedef gboolean (* TapeBufferCallback) (TapeBufferCallbackType type,
                                         int length,
                                         const guint8 *bytes,
                                         gpointer data);

TapeBuffer *tape_buffer_new ();
void tape_buffer_free (TapeBuffer *tbuf);
int tape_buffer_get_next_byte (TapeBuffer *tbuf);
void tape_buffer_store_byte (TapeBuffer *tbuf, guint8 byte);
void tape_buffer_store_high_tone (TapeBuffer *tbuf);
void tape_buffer_store_repeated_high_tone (TapeBuffer *tbuf, int repeat_count);
void tape_buffer_store_silence (TapeBuffer *tbuf);
void tape_buffer_store_repeated_silence (TapeBuffer *tbuf, int repeat_count);
void tape_buffer_rewind (TapeBuffer *tbuf);
gboolean tape_buffer_is_at_end (TapeBuffer *tbuf);
gboolean tape_buffer_foreach (TapeBuffer *tbuf,
                              TapeBufferCallback callback,
                              gpointer data);

#endif /* _TAPE_BUFFER_H */
