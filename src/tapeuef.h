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

#ifndef _TAPE_UEF_H
#define _TAPE_UEF_H

#include <glib.h>
#include "tapebuffer.h"

typedef enum
{
  TAPE_UEF_ERROR_IO,
  TAPE_UEF_ERROR_ZLIB,
  TAPE_UEF_ERROR_INVALID,
  TAPE_UEF_ERROR_NO_ZLIB,
  TAPE_UEF_ERROR_NOT_SUPPORTED
} TapeUEFError;

#define TAPE_UEF_ERROR tape_uef_error_quark ()
GQuark tape_uef_error_quark ();

TapeBuffer *tape_uef_load (FILE *infile, GError **error);
gboolean tape_uef_save (TapeBuffer *buf, gboolean compress,
                        FILE *outfile, GError **error);

#endif /* _TAPE_UEF_H */
