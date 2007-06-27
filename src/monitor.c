#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "eek.h"
#include "electron.h"
#include "monitor.h"
#include "stypes.h"

static const char *electron_flags = "NV-BDIZC";

#define MONITOR_INSTRUCTION_COUNT 56

#define MONITOR_IMPLIED    0
#define MONITOR_IMMEDIATE  1
#define MONITOR_ABSOLUTE   2
#define MONITOR_ZERO_PAGE  3
#define MONITOR_ACCUM      4
#define MONITOR_PRE_IND_X  5
#define MONITOR_POS_IND_Y  6
#define MONITOR_Z_IND_X    7
#define MONITOR_ABS_IND_X  8
#define MONITOR_ABS_IND_Y  9
#define MONITOR_RELATIVE   10
#define MONITOR_INDIRECT   11
#define MONITOR_Z_IND_Y    12
#define MONITOR_MODE_COUNT 13

#define MONITOR_DUMP_BYTES 16
#define MONITOR_DUMP_LINES 16

struct MonitorInstruction
{
  char mnemonic[4];
  UBYTE opcodes[MONITOR_MODE_COUNT];
};

static const struct MonitorInstruction 
monitor_instructions[MONITOR_INSTRUCTION_COUNT];
static const int monitor_bytes_per_mode[];

char *
monitor_get_flags (int flags)
{
  static char flagstr[9];
  int i;

  flagstr[8] = '\0';

  for (i = 0; i < 8; i++)
    flagstr[i] = (flags & (1 << (7 - i))) ? electron_flags[i] : '*';

  return flagstr;
}

char *
monitor_disassemble (Electron *electron, UWORD *addr)
{
  UWORD address = *addr;
  UBYTE code = electron_read_from_location (electron, address);
  static char dstr[30];
  char *p;
  int i, op, mode;

  /* Look for the instruction in the array */
  for (op = 0; op < MONITOR_INSTRUCTION_COUNT; op++)
  {
    for (mode = 0; mode < MONITOR_MODE_COUNT; mode++)
      if (monitor_instructions[op].opcodes[mode] == code)
	break;
    if (mode < MONITOR_MODE_COUNT)
      break;
  }

  if (op < MONITOR_INSTRUCTION_COUNT)
  {
    UBYTE byte = electron_read_from_location (electron, address + 1);
    UWORD word = (electron_read_from_location (electron, address + 2) << 8) | byte;

    *addr += monitor_bytes_per_mode[mode] + 1;

    p = dstr;
    for (i = 0; i <= monitor_bytes_per_mode[mode]; i++)
      p += sprintf (p, "%02X ", electron_read_from_location (electron, address + i));
    memset (p, ' ', (3 - i) * 3);
    p += (3 - i) * 3;

    strcpy (p, monitor_instructions[op].mnemonic);

    p += 3;

    switch (mode)
    {
      case MONITOR_IMPLIED:
	break;
      case MONITOR_IMMEDIATE:
	p += sprintf (p, " #&%02X", byte);
	break;
      case MONITOR_ABSOLUTE:
	p += sprintf (p, " &%04X", word);
	break;
      case MONITOR_ZERO_PAGE:
	p += sprintf (p, " &%02X", byte);
	break;
      case MONITOR_ACCUM:
	strcpy (p, " a");
	p += 2;
	break;
      case MONITOR_PRE_IND_X:
	p += sprintf (p, " (&%02X, X)", byte);
	break;
      case MONITOR_POS_IND_Y:
	p += sprintf (p, " (&%02X), Y", byte);
	break;
      case MONITOR_Z_IND_X:
	p += sprintf (p, " &%02X, X", byte);
	break;
      case MONITOR_ABS_IND_X:
	p += sprintf (p, " &%04X, X", word);
	break;
      case MONITOR_ABS_IND_Y:
	p += sprintf (p, " &%04X, Y", word);
	break;
      case MONITOR_RELATIVE:
	p += sprintf (p, " &%04X", address + 2 + (SBYTE) byte);
	break;
      case MONITOR_INDIRECT:
	p += sprintf (p, " (&%04X)", word);
	break;
      case MONITOR_Z_IND_Y:
	p += sprintf (p, " &%02X, Y", byte);
	break;
    }
  }
  else
  {
    sprintf (dstr, "%02X       ???", electron_read_from_location (electron, address));
    *addr += 1;
  }

  return dstr;
}

void
monitor_print_state (Electron *electron)
{
  UWORD addr = electron->cpu.pc;
  printf ("A=%02X X=%02X Y=%02X S=01%02X P=%s PC=%04X %s\n",
	  electron->cpu.a, electron->cpu.x, electron->cpu.y, electron->cpu.s,
	  monitor_get_flags (electron->cpu.p), electron->cpu.pc,
	  monitor_disassemble (electron, &addr));
}

void
monitor_dump (Electron *electron, UWORD address)
{
  int line, i;
  UBYTE c;

  for (line = 0; line < MONITOR_DUMP_LINES; line++)
  {
    printf ("%04X ", address);
    for (i = 0; i < MONITOR_DUMP_BYTES; i++)
      printf ("%02X ", electron_read_from_location (electron, address + i));
    for (i = 0; i < MONITOR_DUMP_BYTES; i++)
      fputc ((c = electron_read_from_location (electron, address++)) < 128
	     && c >= 32 ? c : '.', stdout);
    fputc ('\n', stdout);
  }
}

void
monitor_dump_disassemble (Electron *electron, UWORD address)
{
  int i;
  for (i = 0; i < MONITOR_DUMP_LINES; i++)
  {
    printf ("%04X ", address);
    /* seperate calls to printf because we don't know what order it
       will evaluate the args in */
    printf ("%s\n", monitor_disassemble (electron, &address));
  }
}

int
monitor (Electron *electron)
{
  char line[128];
  char *p;
  int break_type = electron->cpu.break_type;

  electron->cpu.break_type = CPU_BREAK_NONE;

  while (TRUE)
  {
    monitor_print_state (electron);

    if (fgets (line, sizeof (line) / sizeof (char), stdin) == NULL)
      return -1;
    
    for (p = line; isspace (*p); p++);

    switch (*p)
    {
      case 'D':
	monitor_dump (electron, strtoul (p + 1, NULL, 0));
	break;
      case 'd':
	monitor_dump_disassemble (electron, strtoul (p + 1, NULL, 0));
	break;
      case 'c':
	electron->cpu.break_type = break_type;
	return -2;
      case 'b':
	break_type = CPU_BREAK_ADDR;
	cpu_set_break (&electron->cpu, CPU_BREAK_ADDR, strtoul (p + 1, NULL, 0));
	break;
      case 'r':
	break_type = CPU_BREAK_READ;
	cpu_set_break (&electron->cpu, CPU_BREAK_READ, strtoul (p + 1, NULL, 0));
	break;
      case 'w':
	break_type = CPU_BREAK_WRITE;
	cpu_set_break (&electron->cpu, CPU_BREAK_WRITE, strtoul (p + 1, NULL, 0));
	break;
      case 'B':
	electron->cpu.break_type = CPU_BREAK_NONE;
	break_type = CPU_BREAK_NONE;
	break;
      case 0:
      case 'n': /* step */
	/* Execute one instruction */
	electron_step (electron);
	break;
    }

  }

  return 0;
}

static const struct MonitorInstruction 
monitor_instructions[MONITOR_INSTRUCTION_COUNT] =
  {
    { "BRK", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "ADC", { 0x00, 0x69, 0x6d, 0x65, 0x00, 0x61, 0x71,
	       0x75, 0x7d, 0x79, 0x00, 0x00, 0x00 } },
    { "AND", { 0x00, 0x29, 0x2d, 0x25, 0x00, 0x21, 0x31,
	       0x35, 0x3d, 0x39, 0x00, 0x00, 0x00 } },
    { "ASL", { 0x00, 0x00, 0x0e, 0x06, 0x0a, 0x00, 0x00,
	       0x16, 0x1e, 0x00, 0x00, 0x00, 0x00 } },
    { "BCC", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x90, 0x00, 0x00 } },
    { "BCS", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0xb0, 0x00, 0x00 } },
    { "BEQ", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0xf0, 0x00, 0x00 } },
    { "BIT", { 0x00, 0x00, 0x2c, 0x24, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "BMI", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x30, 0x00, 0x00 } },
    { "BNE", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0xd0, 0x00, 0x00 } },
    { "BPL", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x10, 0x00, 0x00 } },
    { "BVC", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x50, 0x00, 0x00 } },
    { "BVS", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x70, 0x00, 0x00 } },
    { "CLC", { 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x18, 0x00, 0x00 } },
    { "CLD", { 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0xd8, 0x00, 0x00 } },
    { "CLI", { 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x58, 0x00, 0x00 } },
    { "CLV", { 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0xb8, 0x00, 0x00 } },
    { "CMP", { 0x00, 0xc9, 0xcd, 0xc5, 0x00, 0xc1, 0xd1,
	       0xd5, 0xdd, 0xd9, 0x00, 0x00, 0x00 } },
    { "CPX", { 0x00, 0xe0, 0xec, 0xe4, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "CPY", { 0x00, 0xc0, 0xcc, 0xc4, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "DEC", { 0x00, 0x00, 0xce, 0xc6, 0x00, 0x00, 0x00,
	       0xd6, 0xde, 0x00, 0x00, 0x00, 0x00 } },
    { "DEX", { 0xca, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "DEY", { 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "EOR", { 0x00, 0x49, 0x4d, 0x45, 0x00, 0x41, 0x51,
	       0x55, 0x5d, 0x59, 0x00, 0x00, 0x00 } },
    { "INC", { 0x00, 0x00, 0xee, 0xe6, 0x00, 0x00, 0x00,
	       0xf6, 0xfe, 0x00, 0x00, 0x00, 0x00 } },
    { "INX", { 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "INY", { 0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "JMP", { 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x6c, 0x00 } },
    { "JSR", { 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "LDA", { 0x00, 0xa9, 0xad, 0xa5, 0x00, 0xa1, 0xb1,
	       0xb5, 0xbd, 0xb9, 0x00, 0x00, 0x00 } },
    { "LDX", { 0x00, 0xa2, 0xae, 0xa6, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0xbe, 0x00, 0x00, 0xb6 } },
    { "LDY", { 0x00, 0xa0, 0xac, 0xa4, 0x00, 0x00, 0x00,
	       0xb4, 0xbc, 0x00, 0x00, 0x00, 0x00 } },
    { "LSR", { 0x00, 0x00, 0x4e, 0x46, 0x4a, 0x00, 0x00,
	       0x56, 0x5e, 0x00, 0x00, 0x00, 0x00 } },
    { "NOP", { 0xea, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "ORA", { 0x00, 0x09, 0x0d, 0x05, 0x00, 0x01, 0x11,
	       0x15, 0x1d, 0x19, 0x00, 0x00, 0x00 } },
    { "PHA", { 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "PHP", { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "PLA", { 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "PLP", { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "ROL", { 0x00, 0x00, 0x2e, 0x26, 0x2a, 0x00, 0x00,
	       0x36, 0x3e, 0x00, 0x00, 0x00, 0x00 } },
    { "ROR", { 0x00, 0x00, 0x6e, 0x66, 0x6a, 0x00, 0x00,
	       0x76, 0x7e, 0x00, 0x00, 0x00, 0x00 } },
    { "RTI", { 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "RTS", { 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "SBC", { 0x00, 0xe9, 0xed, 0xe5, 0x00, 0xe1, 0xf1,
	       0xf5, 0xfd, 0xf9, 0x00, 0x00, 0x00 } },
    { "SEC", { 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "SED", { 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "SEI", { 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "STA", { 0x00, 0x00, 0x8d, 0x85, 0x00, 0x81, 0x91,
	       0x95, 0x9d, 0x99, 0x00, 0x00, 0x00 } },
    { "STX", { 0x00, 0x00, 0x8e, 0x86, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x96 } },
    { "STY", { 0x00, 0x00, 0x8c, 0x84, 0x00, 0x00, 0x00,
	       0x94, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TAX", { 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TAY", { 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TSX", { 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TXA", { 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TXS", { 0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "TYA", { 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
  };

static const int monitor_bytes_per_mode[MONITOR_MODE_COUNT] = 
  { 0, 1, 2, 1, 0, 1, 1, 1, 2, 2, 1, 2, 1 };
