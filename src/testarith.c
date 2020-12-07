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

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <errno.h>
#include <string.h>

#include "cpu.h"

#define EXEC_ADDRESS 0x1000
#define IGNORE_FLAGS (4 | 16 | 32)

static guint8
read_func (void *data, guint16 address)
{
  return 0xff;
}

static void
write_func (void *data, guint16 address, guint8 val)
{
}

static void
print_flags (FILE *out, guint8 p)
{
  static const char flags[] = "CZIDBUVN";
  int i;

  for (i = 0; i < 8; i++)
    fputc ((p & (1 << i)) ? flags[i] : '-', out);
}

static gboolean
test_instruction (Cpu *cpu,
                  gboolean subtract,
                  gboolean decimal,
                  gboolean carry,
                  guint8 a,
                  guint8 b,
                  const guint8 *real_values)
{
  if (decimal)
    cpu->memory[EXEC_ADDRESS] = 0xf8;
  else
    cpu->memory[EXEC_ADDRESS] = 0xd8;

  if (carry)
    cpu->memory[EXEC_ADDRESS + 1] = 0x38;
  else
    cpu->memory[EXEC_ADDRESS + 1] = 0x18;

  cpu->memory[EXEC_ADDRESS + 2] = subtract ? 0xe9 : 0x69;
  cpu->memory[EXEC_ADDRESS + 3] = b;

  cpu->time = 0;
  cpu->a = a;
  cpu->pc = EXEC_ADDRESS;

  cpu_fetch_execute (cpu, 5);

  if (cpu->a != real_values[0]
      || (cpu->p & ~IGNORE_FLAGS) != (real_values[1] & ~IGNORE_FLAGS))
  {
    fprintf (stderr,
             "%c%c A=&%02x : %s #&%02x -> &%02x ",
             decimal ? 'D' : '-',
             carry ? 'C' : '-',
             a,
             subtract ? "SBC" : "ADC",
             b,
             cpu->a);
    print_flags (stderr, cpu->p);
    fprintf (stderr,
             " (should be: &%02x ",
             real_values[0]);
    print_flags (stderr, real_values[1]);
    fputs (")\n", stderr);

    return FALSE;
  }
  else
  {
    return TRUE;
  }
}

int
main (int argc, char **argv)
{
  int subtract, a, b, carry, decimal;
  Cpu cpu;
  guint8 *memory = g_malloc (CPU_RAM_SIZE);
  int ret = EXIT_SUCCESS;
  FILE *data_file;
  char *data_filename =
    g_build_filename (PACKAGE_SOURCE_DIR, "src", "testarith.data", NULL);

  data_file = fopen (data_filename, "rb");
  if (data_file == NULL)
  {
    fprintf (stderr, "%s: %s\n", data_filename, strerror (errno));
    return EXIT_FAILURE;
  }

  g_free (data_filename);

  cpu_init (&cpu,
            memory,
            read_func,
            write_func,
            NULL /* memory_data */);

  for (subtract = 0; subtract < 2; subtract++)
  {
    for (decimal = 0; decimal < 2; decimal++)
    {
      for (carry = 0; carry < 2; carry++)
      {
        for (b = 0; b < 256; b++)
        {
          for (a = 0; a < 256; a++)
          {
            guint8 real_values[2];

            if (fread (real_values, 1, 2, data_file) != 2)
            {
              fprintf (stderr, "error reading test data\n");
              ret = EXIT_FAILURE;
              goto done;
            }

            if (!test_instruction (&cpu,
                                   subtract,
                                   decimal,
                                   carry,
                                   a, b,
                                   real_values))
              ret = EXIT_FAILURE;
          }
        }
      }
    }
  }

 done:
  g_free (memory);
  fclose (data_file);

  return ret;
}
