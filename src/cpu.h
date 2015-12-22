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

#ifndef _CPU_H
#define _CPU_H

#include <glib.h>

typedef struct _Cpu Cpu;

typedef unsigned int cycles_t;

/* Defines a type to point to a function to perform a particular
   instruction */
typedef void (*CpuOpcodeFunc) (void);

/* Defines a type to point to a function that gets data using one of
   the addressing modes */
typedef guint8 (*CpuReadFunc) (void);
/* Defines a type to point to a function that writes data using one of
   the addressing modes */
typedef void (*CpuWriteFunc) (guint8 v);
/* Defines a type to point to a function that gets an address using
   one of the addressing modes */
typedef guint16 (*CpuAddressFunc) (void);

/* Defines a function that reads from a memory location */
typedef guint8 (*CpuMemReadFunc) (void *data, guint16 address);
/* Defines a function that write to a memory location */
typedef void (*CpuMemWriteFunc) (void *data, guint16 address, guint8 val);

#define CPU_START_VECTOR 0xFFFC
#define CPU_IRQ_VECTOR   0xFFFE
#define CPU_NMI_VECTOR   0xFFFA

/* Structure to keep track of the state of the CPU */
struct _Cpu
{
  /* The main registers */
  guint8 a, x, y;

  /* The status register */
  guint8 p;

  /* The stack pointer */
  guint8 s;

  /* The program counter */
  guint16 pc;

  /* This should point to 32k of ram */
  guint8 *memory;
  /* These two functions are used to access other addresses. They are
     pointers so that the code for the 6502 can be completly
     isolated */
  CpuMemReadFunc read_func;
  CpuMemWriteFunc write_func;
  /* This is the data to pass to the two functions above */
  void *memory_data;

  /* The current instruction */
  guint8 instruction;

  /* The number of cycles executed since started */
  cycles_t time;

  /* Whether an interrupt is being requested */
  int irq : 1;
  /* whether a non-maskable interrupt is being requested */
  int nmi : 1;

  int got_break : 1;
  enum { CPU_BREAK_NONE, CPU_BREAK_ADDR, CPU_BREAK_WRITE, CPU_BREAK_READ } break_type;
  guint16 break_address;
};

/* Macros that define the accessible memory */
#define CPU_ADDRESS_SIZE 65536
#define CPU_RAM_SIZE     32768

void cpu_init (Cpu *cpu, guint8 *memory,
               CpuMemReadFunc read_func, CpuMemWriteFunc write_func,
               void *memory_data);
int cpu_fetch_execute (Cpu *cpu, cycles_t target_time);

void cpu_set_irq (Cpu *cpu);
void cpu_reset_irq (Cpu *cpu);
void cpu_cause_nmi (Cpu *cpu);
void cpu_restart (Cpu *cpu);
void cpu_set_break (Cpu *cpu, int break_type, guint16 address);

#endif /* _CPU_H */
