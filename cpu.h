#ifndef _CPU_H
#define _CPU_H

#include "stypes.h"

typedef struct _Cpu Cpu;

typedef unsigned int cycles_t;

/* Defines a type to point to a function to perform a particular
   instruction */
typedef void (*CpuOpcodeFunc) (void);

/* Defines a type to point to a function that gets data using one of
   the addressing modes */
typedef UBYTE (*CpuReadFunc) (void);
/* Defines a type to point to a function that writes data using one of
   the addressing modes */
typedef void (*CpuWriteFunc) (UBYTE v);
/* Defines a type to point to a function that gets an address using
   one of the addressing modes */
typedef UWORD (*CpuAddressFunc) (void);

/* Defines a function that reads from a memory location */
typedef UBYTE (*CpuMemReadFunc) (void *data, UWORD address);
/* Defines a function that write to a memory location */
typedef void (*CpuMemWriteFunc) (void *data, UWORD address, UBYTE val);

#define CPU_START_VECTOR 0xFFFC
#define CPU_IRQ_VECTOR   0xFFFE
#define CPU_NMI_ADDRESS  0x0D00

/* Structure to keep track of the state of the CPU */
struct _Cpu
{
  /* The main registers */
  UBYTE a, x, y;

  /* The status register */
  UBYTE p;

  /* The stack pointer */
  UBYTE s;

  /* The program counter */
  UWORD pc;

  /* This should point to 32k of ram */
  UBYTE *memory;
  /* These two functions are used to access other addresses. They are
     pointers so that the code for the 6502 can be completly
     isolated */
  CpuMemReadFunc read_func;
  CpuMemWriteFunc write_func;
  /* This is the data to pass to the two functions above */
  void *memory_data;

  /* The current instruction */
  UBYTE instruction;

  /* The number of cycles executed since started */
  cycles_t time;

  /* Whether an interrupt is being requested */
  int irq : 1;
  /* whether a non-maskable interrupt is being requested */
  int nmi : 1;

  int got_break : 1;
  enum { CPU_BREAK_NONE, CPU_BREAK_ADDR } break_type;
  UWORD break_address;
};

/* Macros that define the accessible memory */
#define CPU_ADDRESS_SIZE 65536
#define CPU_RAM_SIZE     32768

void cpu_init (Cpu *cpu, UBYTE *memory, 
	       CpuMemReadFunc read_func, CpuMemWriteFunc write_func,
	       void *memory_data);
int cpu_fetch_execute (Cpu *cpu, cycles_t target_time);

void cpu_set_irq (Cpu *cpu);
void cpu_reset_irq (Cpu *cpu);
void cpu_cause_nmi (Cpu *cpu);
void cpu_restart (Cpu *cpu);
void cpu_set_break (Cpu *cpu, int break_type, UWORD address);

#endif /* _CPU_H */
