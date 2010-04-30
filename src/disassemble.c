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

#include "disassemble.h"

typedef int (* DisassembleModeFunc) (guint16 address, const guint8 *bytes, char *operands);

static int
disassemble_implied (guint16 address, const guint8 *bytes, char *operands)
{
  *operands = '\0';
  return 1;
}

static int
disassemble_accumulator (guint16 address, const guint8 *bytes, char *operands)
{
  *(operands++) = 'A';
  *operands = '\0';
  return 1;
}

static int
disassemble_absolute (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X%02X", bytes[2], bytes[1]);
  return 3;
}

static int
disassemble_absolute_x (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X%02X,X", bytes[2], bytes[1]);
  return 3;
}

static int
disassemble_absolute_y (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X%02X,Y", bytes[2], bytes[1]);
  return 3;
}

static int
disassemble_ind_zero_page_x (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "(&%02X,X)", bytes[1]);
  return 2;
}

static int
disassemble_zero_page (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X", bytes[1]);
  return 2;
}

static int
disassemble_ind_zero_page_y (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "(&%02X),Y", bytes[1]);
  return 2;
}

static int
disassemble_zero_page_x (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X,X", bytes[1]);
  return 2;
}

static int
disassemble_zero_page_y (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%02X,Y", bytes[1]);
  return 2;
}

static int
disassemble_immediate (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "#&%02X", bytes[1]);
  return 2;
}

static int
disassemble_pc_relative (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "&%04X", ((int) address + (gint8) bytes[1] + 2) & 0xffff);
  return 2;
}

static int
disassemble_indirect (guint16 address, const guint8 *bytes, char *operands)
{
  sprintf (operands, "(&%02X%02X)", bytes[2], bytes[1]);
  return 3;
}

int
disassemble_instruction (guint16 address, const guint8 *bytes, char *mnemonic, char *operands)
{
  /* Opcodes with bottom two bits set to 01 */
  if ((bytes[0] & 0x03) == 0x01)
  {
    static const DisassembleModeFunc mode_table[8] =
      { disassemble_ind_zero_page_x, disassemble_zero_page, disassemble_immediate,
        disassemble_absolute, disassemble_ind_zero_page_y, disassemble_zero_page_x,
        disassemble_absolute_y, disassemble_absolute_x };
    static const char mnemonic_table[8 * 4] =
      "ORA\0AND\0EOR\0ADC\0STA\0LDA\0CMP\0SBC";

    /* STA immediate is invalid */
    if (bytes[0] == 0x89)
    {
      memcpy (mnemonic, "???", 4);
      return disassemble_implied (address, bytes, operands);
    }
    else
    {
      memcpy (mnemonic, mnemonic_table + (bytes[0] >> 5) * 4, 4);
      return (* mode_table[(bytes[0] & 0x1c) >> 2]) (address, bytes, operands);
    }
  }
  /* Opcodes with bottom two bits set to 10 */
  else if ((bytes[0] & 0x03) == 0x02)
  {
    /* STX or LDX with zero page, y or absolute, y */
    if ((bytes[0] & 0xd7) == 0x96)
    {
      /* STX absolute, y does not work */
      if (bytes[0] == 0x9e)
      {
        memcpy (mnemonic, "???", 4);
        return disassemble_implied (address, bytes, operands);
      }
      else
      {
        memcpy (mnemonic, (bytes[0] & 0x20) ? "LDX" : "STX", 4);
        return (* ((bytes[0] & 0x08) ? disassemble_absolute_y : disassemble_zero_page_y))
          (address, bytes, operands);
      }
    }
    /* Single-byte instructions in gaps of accumulator mode and missing mode 110 */
    else if ((bytes[0] & 0x8f) == 0x8a)
    {
      static const char mnemonic_table[] = "TXA\0TXS\0TAX\0TSX\0DEX\0???\0NOP\0???";
      memcpy (mnemonic, mnemonic_table + ((bytes[0] >> 4) & 0x07) * 4, 4);
      return disassemble_implied (address, bytes, operands);
    }
    /* Modes 110 and 100 are invalid */
    else if ((bytes[0] & 0x14) == 0x10
             /* Immediate mode is only valid for LDX */
             || ((bytes[0] & 0x1c) == 0x00 && bytes[0] != 0xa2))
    {
      memcpy (mnemonic, "???", 4);
      return disassemble_implied (address, bytes, operands);
    }
    else
    {
      static const char mnemonic_table[] = "ASL\0ROL\0LSR\0ROR\0STX\0LDX\0DEC\0INC";
      static const DisassembleModeFunc mode_table[8] =
        { disassemble_immediate, disassemble_zero_page, disassemble_accumulator,
          disassemble_absolute, NULL, disassemble_zero_page_x, NULL,
          disassemble_absolute_x };
      memcpy (mnemonic, mnemonic_table + (bytes[0] >> 5) * 4, 4);
      return (* mode_table[(bytes[0] >> 2) & 0x07]) (address, bytes, operands);
    }
  }
  /* Opcodes with bottom two bits set to 00 */
  else if ((bytes[0] & 0x03) == 0x00)
  {
    /* Branch instructions (fills missing mode 100) */
    if ((bytes[0] & 0x1f) == 0x10)
    {
      const char mnemonic_table[] = "BPL\0BMI\0BVC\0BVS\0BCC\0BCS\0BNE\0BEQ";
      memcpy (mnemonic, mnemonic_table + (bytes[0] >> 5) * 4, 4);
      return disassemble_pc_relative (address, bytes, operands);
    }
    /* Single-byte instructions in missing modes 010 and 110 */
    else if ((bytes[0] & 0x0c) == 0x08)
    {
      const char mnemonic_table[] = "PHP\0CLC\0PLP\0SEC\0PHA\0CLI\0PLA\0SEI\0"
        "DEY\0TYA\0TAY\0CLV\0INY\0CLD\0INX\0SED";
      memcpy (mnemonic, mnemonic_table + (bytes[0] >> 4) * 4, 4);
      return disassemble_implied (address, bytes, operands);
    }
    /* BRK */
    else if (bytes[0] == 0x00)
    {
      memcpy (mnemonic, "BRK", 4);
      return disassemble_implied (address, bytes, operands);
    }
    /* JSR abs */
    else if (bytes[0] == 0x20)
    {
      memcpy (mnemonic, "JSR", 4);
      return disassemble_absolute (address, bytes, operands);
    }
    /* RTI */
    else if (bytes[0] == 0x40)
    {
      memcpy (mnemonic, "RTI", 4);
      return disassemble_implied (address, bytes, operands);
    }
    /* RTS */
    else if (bytes[0] == 0x60)
    {
      memcpy (mnemonic, "RTS", 4);
      return disassemble_implied (address, bytes, operands);
    }
    /* JMP indirect */
    else if (bytes[0] == 0x6c)
    {
      memcpy (mnemonic, "JMP", 4);
      return disassemble_indirect (address, bytes, operands);
    }
    /* There is no opcode 000 */
    else if ((bytes[0] & 0xe0) == 0x00
             /* No other address modes for JMP indirect or JMP absolute */
             || ((bytes[0] & 0xc0) == 0x40 && bytes[0] != 0x4c)
             /* No zero, x or abs, x for BIT, CPY or CPX */
             || (((bytes[0] & 0x80) >> 1) == (bytes[0] & 0x40) && (bytes[0] & 0x10))
             /* No implied or abs, x for STY */
             || (((bytes[0] & 0xe0) == 0x80) && (bytes[0] == 0x80 || bytes[0] == 0x9c)))
    {
      memcpy (mnemonic, "???", 4);
      return disassemble_implied (address, bytes, operands);
    }
    else
    {
      static const char mnemonic_table[] = "\0\0\0\0BIT\0JMP\0\0\0\0\0STY\0LDY\0CPY\0CPX";
      static const DisassembleModeFunc mode_table[8] =
        { disassemble_immediate, disassemble_zero_page, NULL, disassemble_absolute,
          NULL, disassemble_zero_page_x, NULL, disassemble_absolute_x };

      memcpy (mnemonic, mnemonic_table + (bytes[0] >> 5) * 4, 4);
      return (* mode_table[(bytes[0] & 0x1c) >> 2]) (address, bytes, operands);
    }
  }
  /* No instructions have the bottom two bits set to 11 */
  else
  {
    memcpy (mnemonic, "???", 4);
    return disassemble_implied (address, bytes, operands);
  }
}
