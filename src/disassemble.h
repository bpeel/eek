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

#ifndef _DISASSEMBLE_H
#define _DISASSEMBLE_H

#include <glib/gtypes.h>

#define DISASSEMBLE_MAX_BYTES    3
#define DISASSEMBLE_MAX_MNEMONIC 3
#define DISASSEMBLE_MAX_OPERANDS 7

int disassemble_instruction (guint16 address, const guint8 *bytes,
                             char *mnemonic, char *operands);

#endif /* _DISASSEMBLE_H */
