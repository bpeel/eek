/*
 * eek - An emulator for the Acorn Electron
 * Copyright (C) 2020  Neil Roberts
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

#ifndef _STREAMER_H
#define _STREAMER_H

#include <glib.h>
#include <glib-object.h>
#include "electronmanager.h"

#define TYPE_STREAMER (streamer_get_type ())
#define STREAMER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                   TYPE_STREAMER, Streamer))
#define IS_STREAMER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                      TYPE_STREAMER))

G_DECLARE_FINAL_TYPE (Streamer, streamer, streamer, STREAMER, GObject);

#define STREAMER_FRAME_DIVISION 2
#define STREAMER_FPS (1000 / (ELECTRON_TICKS_PER_FRAME * \
                              STREAMER_FRAME_DIVISION))

Streamer *streamer_new (void);
void streamer_set_electron (Streamer *streamer,
                            ElectronManager *emanager);
gboolean streamer_start_process (Streamer *streamer,
                                 const char *command,
                                 GError **error);
void streamer_stop_process (Streamer *streamer);

#endif /* _STREAMER_H */
