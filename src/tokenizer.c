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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>

#include "tokenizer.h"

#define CONDITIONAL_FLAG 1
#define MIDDLE_FLAG 2
#define START_FLAG 4
#define FN_FLAG 8
#define LINE_FLAG 16
#define REM_FLAG 32
#define PSEUDO_FLAG 64

typedef struct
{
  const char *name;
  guint8 value;
  guint8 flags;
} Token;

static const Token
tokens[] =
  {
#define L LINE_FLAG
#define M MIDDLE_FLAG
#define C CONDITIONAL_FLAG
#define R REM_FLAG
#define S START_FLAG
#define P PSEUDO_FLAG
#define F FN_FLAG
   /* Sorted by length of name so that it will prefer the longer tokens */
   { .name = "ENVELOPE", .value = 0xE2, .flags = M },
   { .name = "RENUMBER", .value = 0xCC, .flags = L },
   { .name = "STRING$(", .value = 0xC4, .flags = 0 },
   { .name = "ENDPROC", .value = 0xE1, .flags = C },
   { .name = "OPENOUT", .value = 0xAE, .flags = 0 },
   { .name = "RESTORE", .value = 0xF7, .flags = L|M },
   { .name = "RIGHT$(", .value = 0xC2, .flags = 0 },
   { .name = "COLOUR", .value = 0xFB, .flags = M },
   { .name = "DELETE", .value = 0xC7, .flags = L },
   { .name = "INKEY$", .value = 0xBF, .flags = 0 },
   { .name = "INSTR(", .value = 0xA7, .flags = 0 },
   { .name = "LEFT$(", .value = 0xC0, .flags = 0 },
   { .name = "OPENIN", .value = 0x8E, .flags = 0 },
   { .name = "OPENO.", .value = 0xAE, .flags = 0 },
   { .name = "OPENUP", .value = 0xAD, .flags = 0 },
   { .name = "POINT(", .value = 0xB0, .flags = 0 },
   { .name = "REPEAT", .value = 0xF5, .flags = 0 },
   { .name = "REPORT", .value = 0xF6, .flags = C },
   { .name = "RETURN", .value = 0xF8, .flags = C },
   { .name = "ADVAL", .value = 0x96, .flags = 0 },
   { .name = "CHAIN", .value = 0xD7, .flags = M },
   { .name = "CLEAR", .value = 0xD8, .flags = C },
   { .name = "CLOSE", .value = 0xD9, .flags = M|C },
   { .name = "COUNT", .value = 0x9C, .flags = C },
   { .name = "ERROR", .value = 0x85, .flags = S },
   { .name = "FALSE", .value = 0xA3, .flags = C },
   { .name = "GOSUB", .value = 0xE4, .flags = L|M },
   { .name = "HIMEM", .value = 0x93, .flags = P|M|C },
   { .name = "INKEY", .value = 0xA6, .flags = 0 },
   { .name = "INPUT", .value = 0xE8, .flags = M },
   { .name = "LOCAL", .value = 0xEA, .flags = M },
   { .name = "LOMEM", .value = 0x92, .flags = P|M|C },
   { .name = "MID$(", .value = 0xC1, .flags = 0 },
   { .name = "OSCLI", .value = 0xFF, .flags = M },
   { .name = "PRINT", .value = 0xF1, .flags = M },
   { .name = "REPO.", .value = 0xF6, .flags = C },
   { .name = "SOUND", .value = 0xD4, .flags = M },
   { .name = "STRI.", .value = 0xC4, .flags = 0 },
   { .name = "TRACE", .value = 0xFC, .flags = L|M },
   { .name = "UNTIL", .value = 0xFD, .flags = M },
   { .name = "WIDTH", .value = 0xFE, .flags = M },
   { .name = "AUTO", .value = 0xC6, .flags = L },
   { .name = "BGET", .value = 0x9A, .flags = C },
   { .name = "BPUT", .value = 0xD5, .flags = M|C },
   { .name = "CALL", .value = 0xD6, .flags = M },
   { .name = "CHR$", .value = 0xBD, .flags = 0 },
   { .name = "CHR.", .value = 0xBD, .flags = 0 },
   { .name = "CLO.", .value = 0xD9, .flags = M|C },
   { .name = "COU.", .value = 0x9C, .flags = C },
   { .name = "DATA", .value = 0xDC, .flags = R },
   { .name = "DEL.", .value = 0xC7, .flags = L },
   { .name = "DRAW", .value = 0xDF, .flags = M },
   { .name = "ELSE", .value = 0x8B, .flags = L|S },
   { .name = "ENV.", .value = 0xE2, .flags = M },
   { .name = "ERR.", .value = 0x85, .flags = S },
   { .name = "EVAL", .value = 0xA0, .flags = 0 },
   { .name = "GCOL", .value = 0xE6, .flags = M },
   { .name = "GET$", .value = 0xBE, .flags = 0 },
   { .name = "GOS.", .value = 0xE4, .flags = L|M },
   { .name = "GOTO", .value = 0xE5, .flags = L|M },
   { .name = "INK.", .value = 0xBF, .flags = 0 },
   { .name = "INS.", .value = 0xA7, .flags = 0 },
   { .name = "LINE", .value = 0x86, .flags = 0 },
   { .name = "LIN.", .value = 0x86, .flags = 0 },
   { .name = "LIST", .value = 0xC9, .flags = L },
   { .name = "LOAD", .value = 0xC8, .flags = M },
   { .name = "LOC.", .value = 0xEA, .flags = M },
   { .name = "LOM.", .value = 0x92, .flags = P|M|C },
   { .name = "MODE", .value = 0xEB, .flags = M },
   { .name = "MOVE", .value = 0xEC, .flags = M },
   { .name = "MOV.", .value = 0xEC, .flags = M },
   { .name = "NEXT", .value = 0xED, .flags = M },
   { .name = "PAGE", .value = 0x90, .flags = P|M|C },
   { .name = "PLOT", .value = 0xF0, .flags = M },
   { .name = "PROC", .value = 0xF2, .flags = F|M },
   { .name = "PRO.", .value = 0xF2, .flags = F|M },
   { .name = "READ", .value = 0xF3, .flags = M },
   { .name = "REN.", .value = 0xCC, .flags = L },
   { .name = "REP.", .value = 0xF5, .flags = 0 },
   { .name = "RES.", .value = 0xF7, .flags = L|M },
   { .name = "SAVE", .value = 0xCD, .flags = M },
   { .name = "STEP", .value = 0x88, .flags = 0 },
   { .name = "STOP", .value = 0xFA, .flags = C },
   { .name = "STO.", .value = 0xFA, .flags = C },
   { .name = "STR$", .value = 0xC3, .flags = 0 },
   { .name = "STR.", .value = 0xC3, .flags = 0 },
   { .name = "TAB(", .value = 0x8A, .flags = 0 },
   { .name = "THEN", .value = 0x8C, .flags = L|S },
   { .name = "TIME", .value = 0x91, .flags = P|M|C },
   { .name = "TRUE", .value = 0xB9, .flags = C },
   { .name = "VPOS", .value = 0xBC, .flags = C },
   { .name = "ABS", .value = 0x94, .flags = 0 },
   { .name = "ACS", .value = 0x95, .flags = 0 },
   { .name = "AD.", .value = 0x96, .flags = 0 },
   { .name = "AND", .value = 0x80, .flags = 0 },
   { .name = "ASC", .value = 0x97, .flags = 0 },
   { .name = "ASN", .value = 0x98, .flags = 0 },
   { .name = "ATN", .value = 0x99, .flags = 0 },
   { .name = "AU.", .value = 0xC6, .flags = L },
   { .name = "BP.", .value = 0xD5, .flags = M|C },
   { .name = "CA.", .value = 0xD6, .flags = M },
   { .name = "CH.", .value = 0xD7, .flags = M },
   { .name = "CLG", .value = 0xDA, .flags = C },
   { .name = "CLS", .value = 0xDB, .flags = C },
   { .name = "CL.", .value = 0xD8, .flags = C },
   { .name = "COS", .value = 0x9B, .flags = 0 },
   { .name = "DEF", .value = 0xDD, .flags = 0 },
   { .name = "DEG", .value = 0x9D, .flags = 0 },
   { .name = "DIM", .value = 0xDE, .flags = M },
   { .name = "DIV", .value = 0x81, .flags = 0 },
   { .name = "DR.", .value = 0xDF, .flags = M },
   { .name = "EL.", .value = 0x8B, .flags = L|S },
   { .name = "END", .value = 0xE0, .flags = C },
   { .name = "EOF", .value = 0xC5, .flags = C },
   { .name = "EOR", .value = 0x82, .flags = 0 },
   { .name = "ERL", .value = 0x9E, .flags = C },
   { .name = "ERR", .value = 0x9F, .flags = C },
   { .name = "EV.", .value = 0xA0, .flags = 0 },
   { .name = "EXP", .value = 0xA1, .flags = 0 },
   { .name = "EXT", .value = 0xA2, .flags = C },
   { .name = "FA.", .value = 0xA3, .flags = C },
   { .name = "FOR", .value = 0xE3, .flags = M },
   { .name = "GC.", .value = 0xE6, .flags = M },
   { .name = "GET", .value = 0xA5, .flags = 0 },
   { .name = "GE.", .value = 0xBE, .flags = 0 },
   { .name = "INT", .value = 0xA8, .flags = 0 },
   { .name = "LEN", .value = 0xA9, .flags = 0 },
   { .name = "LET", .value = 0xE9, .flags = S },
   { .name = "LE.", .value = 0xC0, .flags = 0 },
   { .name = "LOG", .value = 0xAB, .flags = 0 },
   { .name = "LO.", .value = 0xC8, .flags = M },
   { .name = "MOD", .value = 0x83, .flags = 0 },
   { .name = "MO.", .value = 0xEB, .flags = M },
   { .name = "NEW", .value = 0xCA, .flags = C },
   { .name = "NOT", .value = 0xAC, .flags = 0 },
   { .name = "OFF", .value = 0x87, .flags = 0 },
   { .name = "OLD", .value = 0xCB, .flags = C },
   { .name = "OP.", .value = 0x8E, .flags = 0 },
   { .name = "OS.", .value = 0xFF, .flags = M },
   { .name = "PA.", .value = 0x90, .flags = P|M|C },
   { .name = "PL.", .value = 0xF0, .flags = M },
   { .name = "POS", .value = 0xB1, .flags = C },
   { .name = "PO.", .value = 0xB0, .flags = 0 },
   { .name = "PTR", .value = 0x8F, .flags = P|M|C },
   { .name = "PT.", .value = 0x8F, .flags = P|M|C },
   { .name = "PUT", .value = 0xCE, .flags = 0 },
   { .name = "RAD", .value = 0xB2, .flags = 0 },
   { .name = "REM", .value = 0xF4, .flags = R },
   { .name = "RI.", .value = 0xC2, .flags = 0 },
   { .name = "RND", .value = 0xB3, .flags = C },
   { .name = "RUN", .value = 0xF9, .flags = C },
   { .name = "SA.", .value = 0xCD, .flags = M },
   { .name = "SGN", .value = 0xB4, .flags = 0 },
   { .name = "SIN", .value = 0xB5, .flags = 0 },
   { .name = "SO.", .value = 0xD4, .flags = M },
   { .name = "SPC", .value = 0x89, .flags = 0 },
   { .name = "SQR", .value = 0xB6, .flags = 0 },
   { .name = "TAN", .value = 0xB7, .flags = 0 },
   { .name = "TH.", .value = 0x8C, .flags = L|S },
   { .name = "TI.", .value = 0x91, .flags = P|M|C },
   { .name = "TR.", .value = 0xFC, .flags = L|M },
   { .name = "USR", .value = 0xBA, .flags = 0 },
   { .name = "VAL", .value = 0xBB, .flags = 0 },
   { .name = "VDU", .value = 0xEF, .flags = M },
   { .name = "VP.", .value = 0xBC, .flags = C },
   { .name = "A.", .value = 0x80, .flags = 0 },
   { .name = "B.", .value = 0x9A, .flags = C },
   { .name = "C.", .value = 0xFB, .flags = M },
   { .name = "D.", .value = 0xDC, .flags = R },
   { .name = "E.", .value = 0xE1, .flags = C },
   { .name = "FN", .value = 0xA4, .flags = F },
   { .name = "F.", .value = 0xE3, .flags = M },
   { .name = "G.", .value = 0xE5, .flags = L|M },
   { .name = "H.", .value = 0x93, .flags = P|M|C },
   { .name = "IF", .value = 0xE7, .flags = M },
   { .name = "I.", .value = 0xE8, .flags = M },
   { .name = "LN", .value = 0xAA, .flags = 0 },
   { .name = "L.", .value = 0xC9, .flags = L },
   { .name = "M.", .value = 0xC1, .flags = 0 },
   { .name = "N.", .value = 0xED, .flags = M },
   { .name = "ON", .value = 0xEE, .flags = M },
   { .name = "OR", .value = 0x84, .flags = 0 },
   { .name = "O.", .value = 0xCB, .flags = C },
   { .name = "PI", .value = 0xAF, .flags = C },
   { .name = "P.", .value = 0xF1, .flags = M },
   { .name = "R.", .value = 0xF8, .flags = C },
   { .name = "S.", .value = 0x88, .flags = 0 },
   { .name = "TO", .value = 0xB8, .flags = 0 },
   { .name = "T.", .value = 0xB7, .flags = 0 },
   { .name = "U.", .value = 0xFD, .flags = M },
   { .name = "V.", .value = 0xEF, .flags = M },
   { .name = "W.", .value = 0xFE, .flags = M },
#undef L
#undef M
#undef C
#undef R
#undef S
#undef P
#undef F
  };

static const Token *
find_token (const char *src)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (tokens); i++)
  {
    if (g_str_has_prefix (src, tokens[i].name))
    {
      if ((tokens[i].flags & CONDITIONAL_FLAG))
      {
        size_t len = strlen (tokens[i].name);
        if (g_ascii_isalnum (src[len]))
          return NULL;
      }

      return tokens + i;
    }
  }

  return NULL;
}

static const char *
tokenize_line_numbers (const char *src,
                       GString *out)
{
  int line_num;
  guint8 bytes[4] = { 0x8d };

  while (TRUE)
  {
    if (*src == ',' || *src == ' ')
    {
      g_string_append_c (out, *src);
      src++;
      continue;
    }

    if (!g_ascii_isdigit (*src))
      break;

    line_num = 0;
    do
    {
      line_num = (line_num * 10) + *src - '0';
      src++;
    } while (g_ascii_isdigit (*src));

    bytes[1] = ((((line_num & 0x00c0) >> 2) |
                 ((line_num & 0xc000) >> 12)) ^ 0x54);
    bytes[2] = (line_num & 0x3f) | 0x40;
    bytes[3] = ((line_num >> 8) & 0x3f) | 0x40;

    g_string_append_len (out, (char *) bytes, sizeof bytes);
  }

  return src;
}

void
tokenize_line (const char *src,
               GString *out)
{
  gboolean start = TRUE;

  while (*src)
  {
    if (g_ascii_isalpha (*src))
    {
      const Token *token = find_token (src);

      if (token)
      {
        guint8 value = token->value;

        if ((token->flags & PSEUDO_FLAG) && start)
          value += 0x40;

        g_string_append_c (out, value);
        src += strlen (token->name);

        if ((token->flags & MIDDLE_FLAG))
          start = FALSE;
        if ((token->flags & START_FLAG))
          start = TRUE;
        if ((token->flags & FN_FLAG))
        {
          while (g_ascii_isalnum (*src))
            g_string_append_c (out, *(src++));
        }
        if ((token->flags & REM_FLAG))
        {
          g_string_append (out, src);
          break;
        }
        if ((token->flags & LINE_FLAG))
          src = tokenize_line_numbers (src, out);
      }
      else
      {
        start = FALSE;

        while (g_ascii_isalnum (*src))
          g_string_append_c (out, *(src++));
      }
    }
    else if (*src == '&')
    {
      start = FALSE;

      do
        g_string_append_c (out, *(src++));
      while (g_ascii_isxdigit (*src));
    }
    else if (*src == '"')
    {
      start = FALSE;

      const char *end = strchr (src + 1, '"');
      if (end)
      {
        g_string_append_len (out, src, end - src + 1);
        src = end + 1;
      }
      else
      {
        g_string_append (out, src);
        break;
      }
    }
    else if (*src == '*' && start)
    {
      g_string_append (out, src);
      break;
    }
    else
    {
      if (*src == ':')
        start = TRUE;
      else if (!g_ascii_isspace (*src))
        start = FALSE;

      g_string_append_c (out, *(src++));
    }
  }
}

GString *
tokenize_program (const char *program)
{
  GString *out = g_string_new (NULL);
  GString *line = g_string_new (NULL);
  int line_number = 0;
  guint16 line_number_bin;
  size_t line_start;

  while (TRUE)
  {
    const char *end;

    while (*program && g_ascii_isspace (*program))
      program++;

    if (*program == 0)
      break;

    end = strchr (program, '\n');

    if (end == NULL)
      end = program + strlen (program);

    while (end > program && g_ascii_isspace (end[-1]))
      end--;

    if (g_ascii_isdigit (*program))
    {
      line_number = 0;

      do
        line_number = (line_number * 10) + *(program++) - '0';
      while (g_ascii_isdigit (*program));
    }
    else
    {
      line_number++;
    }

    line_start = out->len;
    g_string_append_c (out, 0x0d);
    line_number_bin = GUINT16_TO_BE (line_number);
    g_string_append_len (out,
                         (char *) &line_number_bin,
                         sizeof line_number_bin);
    /* Stub for the length */
    g_string_append_c (out, ' ');

    g_string_set_size (line, 0);
    g_string_append_len (line, program, end - program);
    tokenize_line (line->str, out);

    if (out->len - line_start > 255)
      g_string_set_size (out, line_start + 255);

    out->str[line_start + 3] = out->len - line_start;

    program = end;
  }

  g_string_free (line, TRUE);

  g_string_append (out, "\x0d\xff");

  return out;
}

static const Token *
find_token_by_value (guint8 value)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (tokens); i++)
  {
    if (tokens[i].value == value)
      return tokens + i;
  }

  return NULL;
}

static void
detokenize_line (size_t length,
                 const guint8 *line,
                 GString *source)
{
  const guint8 *end = line + length;
  int byte1, line_num;

  for (; line < end; line++)
  {
    if (*line == 0x8d)
    {
      if (line + 4 > end)
        break;

      byte1 = line[1] ^ 0x14;
      line_num = (((byte1 & 0x30) << 2)
                  | ((byte1 & 0x0c) << 12)
                  | (line[2] & 0x3f)
                  | ((line[3] & 0x3f) << 8));
      g_string_append_printf (source, "%i", line_num);

      line += 3;

      continue;
    }

    if (*line >= 0x80)
    {
      const Token *token = find_token_by_value (*line);

      if (token)
      {
        g_string_append (source, token->name);
        continue;
      }
    }

    g_string_append_c (source, *line);
  }
}

GString *
detokenize_program (size_t length,
                    const guint8 *program)
{
  GString *source = g_string_new (NULL);
  const guint8 *end = program + length;

  while (program + 4 <= end
         && program[0] == 0x0d
         && program[1] != 0xff)
  {
    guint8 length = program[3];
    int line_num = (program[1] << 8) | program[2];

    if (length < 4 || program + length > end)
      break;

    g_string_append_printf (source, "%i", line_num);

    detokenize_line (length - 4, program + 4, source);

    g_string_append_c (source, '\n');

    program += length;
  }

  return source;
}
