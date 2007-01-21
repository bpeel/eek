#include "config.h"

#include <stdio.h>
#include <string.h>

#include "eek.h"
#include "cpu.h"
#include "byteorder.h"

/* Macros to check the flags */
#define CPU_FLAG_N 128
#define CPU_FLAG_V 64
#define CPU_FLAG_U 32
#define CPU_FLAG_B 16
#define CPU_FLAG_D 8
#define CPU_FLAG_I 4
#define CPU_FLAG_Z 2
#define CPU_FLAG_C 1
#define CPU_CHECK_FLAG(f) (cpu_state.p & (f))
#define CPU_SET_FLAG(f, v) \
 do { if ((v)) cpu_state.p |= (f); else cpu_state.p &= ~(f); } while (0)
#define CPU_IS_N() CPU_CHECK_FLAG (CPU_FLAG_N)
#define CPU_IS_V() CPU_CHECK_FLAG (CPU_FLAG_V)
#define CPU_IS_U() CPU_CHECK_FLAG (CPU_FLAG_U)
#define CPU_IS_B() CPU_CHECK_FLAG (CPU_FLAG_B)
#define CPU_IS_D() CPU_CHECK_FLAG (CPU_FLAG_D)
#define CPU_IS_I() CPU_CHECK_FLAG (CPU_FLAG_I)
#define CPU_IS_Z() CPU_CHECK_FLAG (CPU_FLAG_Z)
#define CPU_IS_C() CPU_CHECK_FLAG (CPU_FLAG_C)
#define CPU_SET_N(v) CPU_SET_FLAG (CPU_FLAG_N, v)
#define CPU_SET_V(v) CPU_SET_FLAG (CPU_FLAG_V, v)
#define CPU_SET_U(v) CPU_SET_FLAG (CPU_FLAG_U, v)
#define CPU_SET_B(v) CPU_SET_FLAG (CPU_FLAG_B, v)
#define CPU_SET_D(v) CPU_SET_FLAG (CPU_FLAG_D, v)
#define CPU_SET_I(v) CPU_SET_FLAG (CPU_FLAG_I, v)
#define CPU_SET_Z(v) CPU_SET_FLAG (CPU_FLAG_Z, v)
#define CPU_SET_C(v) CPU_SET_FLAG (CPU_FLAG_C, v)

/* The entire state of the cpu gets copied into this struct before a
   fetch execute cycle so that we can have the speed of accessing
   global variables but still be able to emulate more than one cpu if
   need be. */
static Cpu cpu_state;

/* Macros to operate on the cpu's memory */
#define CPU_WRITE(addr, v) \
                            do { UWORD _taddr = (addr); \
                                 UBYTE _v = (v); \
                                 if (cpu_state.break_type == CPU_BREAK_WRITE \
                                     && cpu_state.break_address == _taddr) \
                                   cpu_state.got_break = 1; \
                                 if (_taddr < CPU_RAM_SIZE) \
                                   cpu_state.memory[_taddr] = _v; \
                                 else cpu_state.write_func (cpu_state.memory_data, \
                                                         _taddr, _v); } while (0)
/* Zero page macro should be faster because we don't need to test if
   the address is in RAM */
#define CPU_WRITE_ZERO(addr, v) \
                            do { UWORD _taddr = (addr); \
                                 UBYTE _v = (v); \
                                 if (cpu_state.break_type == CPU_BREAK_WRITE \
                                     && cpu_state.break_address == _taddr) \
                                   cpu_state.got_break = 1; \
                                 cpu_state.memory[_taddr] = _v; } while (0)
#define CPU_WRITE_WORD(addr, v) \
                            do { UWORD _taddr = (addr); \
                                 UWORD _v = (v); \
                                 if (cpu_state.break_type == CPU_BREAK_WRITE \
                                     && (cpu_state.break_address == _taddr \
                                         || cpu_state.break_address == _taddr + 1)) \
                                   cpu_state.got_break = 1; \
                                 if (_taddr < CPU_RAM_SIZE - 1) \
                                  (*(UWORD *) (cpu_state.memory + (_taddr))) \
                                   = BO_WORD_TO_LE (_v); \
                                 else \
                                 { cpu_state.write_func (cpu_state.memory_data, _taddr, _v); \
                                   cpu_state.write_func (cpu_state.memory_data, \
                                   _taddr + 1, _v >> 8); \
                                 } } while (0)
#define CPU_READ(addr)        ({ UWORD _taddr = (addr); \
                               if (cpu_state.break_type == CPU_BREAK_READ \
                                   && cpu_state.break_address == _taddr) \
                                 cpu_state.got_break = 1; \
                               _taddr < CPU_RAM_SIZE ? cpu_state.memory[_taddr] \
                               : cpu_state.read_func (cpu_state.memory_data, _taddr); })
/* Zero page macro should be faster because we don't need to test if
   the address is in RAM */
#define CPU_READ_ZERO(addr)   ({ UWORD _taddr = (addr); \
                               if (cpu_state.break_type == CPU_BREAK_READ \
                                   && cpu_state.break_address == _taddr) \
                                 cpu_state.got_break = 1; \
                               cpu_state.memory[_taddr]; })
#define CPU_READ_WORD(addr) \
                            ({ UWORD _taddr = (addr); \
                               if (cpu_state.break_type == CPU_BREAK_READ \
                                   && (cpu_state.break_address == _taddr \
                                       || cpu_state.break_address == _taddr + 1)) \
                                 cpu_state.got_break = 1; \
                               (_taddr < CPU_RAM_SIZE - 1) \
                               ? (BO_WORD_FROM_LE (*(UWORD *) (cpu_state.memory + (_taddr)))) \
  			       : (cpu_state.read_func (cpu_state.memory_data, _taddr) \
                                  | (cpu_state.read_func (cpu_state.memory_data, \
                                                          _taddr + 1) << 8)); })
#define CPU_FETCH()        (CPU_READ (cpu_state.pc++))
#define CPU_PUSH(v)        CPU_WRITE (cpu_state.s-- | 0x100, (v))
#define CPU_PUSH_WORD(w) \
                            do { UWORD _w = w; CPU_PUSH (_w >> 8); /* high byte */ \
                                 CPU_PUSH (_w);                   /* low byte */ \
                            } while (0)
#define CPU_PUSH_PC()       CPU_PUSH_WORD (cpu_state.pc)
#define CPU_POP()           (CPU_READ (++cpu_state.s | 0x100))

#define CPU_DATA_FOR_OP(op) \
                            (cpu_addressing_modes[((op) >> 2) & 7] ())
#define CPU_WRITE_FOR_OP(op, v) \
                            (cpu_addressing_modes_write[((op) >> 2) & 7] (v))
#define CPU_ADDR_FOR_OP(op) \
                            (cpu_addressing_modes_addresses[((op) >> 3) & 3] ())

static const CpuOpcodeFunc cpu_jumpblock[];
static const CpuReadFunc cpu_addressing_modes[];
static const CpuWriteFunc cpu_addressing_modes_write[];
static const CpuAddressFunc cpu_addressing_modes_addresses[];

/* Initialise the cpu struct */
void
cpu_init (Cpu *cpu, UBYTE *memory, 
	  CpuMemReadFunc read_func, CpuMemWriteFunc write_func,
	  void *memory_data)
{
  /* Initialise the memory access */
  cpu->memory = memory;
  cpu->read_func = read_func;
  cpu->write_func = write_func;
  cpu->memory_data = memory_data;

  cpu->break_type = CPU_BREAK_NONE;

  cpu_restart (cpu);
}

void
cpu_restart (Cpu *cpu)
{
  /* Copy the cpu state */
  memcpy (&cpu_state, cpu, sizeof (Cpu));

  /* Clear the registers */
  cpu_state.x = cpu_state.y = cpu_state.a = 0;
  /* Disable interrupts */
  cpu_state.p = CPU_FLAG_I;

  /* We haven't counted any clock cycles yet */
  cpu_state.time = 0;

  /* Start stack at top */
  cpu_state.s = 0xff;

  /* Read start location from memory */
  cpu_state.pc = CPU_READ_WORD (CPU_START_VECTOR);

  /* No interrupts yet */
  cpu_state.irq = 0;
  cpu_state.nmi = 0;

  cpu_state.got_break = FALSE;

  /* Put the cpu state back */
  memcpy (cpu, &cpu_state, sizeof (Cpu));
}

/* Execute one instruction from the memory */
int
cpu_fetch_execute (Cpu *cpu, cycles_t target_time)
{
  /* Copy the cpu state */
  memcpy (&cpu_state, cpu, sizeof (Cpu));

  while (cpu_state.time < target_time
	 /* Check for a break */
	 && !cpu_state.got_break)
  {
    /* Check for interrupts */
    if (cpu_state.nmi)
    {
      cpu_state.time += 7;
      /* Store pc - 1 */
      CPU_PUSH_WORD (cpu_state.pc - 1);
      /* Store flags */
      CPU_PUSH (cpu_state.p);
      /* Disable interrupts */
      CPU_SET_I (TRUE);
      /* Jump to nmi handling routine */
      cpu_state.pc = CPU_READ_WORD (CPU_NMI_VECTOR);
      /* Clear the nmi flag */
      cpu_state.nmi = FALSE;
    }
    else if (cpu_state.irq && !CPU_IS_I ())
    {
      cpu_state.time += 7;
    
      /* Store pc - 1 */
      CPU_PUSH_WORD (cpu_state.pc - 1);
      /* Store the flags */
      CPU_PUSH (cpu_state.p);
      /* Disable interrupts */
      CPU_SET_I (TRUE);
      /* Jump to irq handling routine */
      cpu_state.pc = CPU_READ_WORD (CPU_IRQ_VECTOR);
    }
    else
    {
      if (cpu_state.break_type == CPU_BREAK_ADDR
	  && cpu_state.break_address == cpu_state.pc)
	cpu_state.got_break = TRUE;
      else
	/* Use the jumpblock to call the function that is being pointed to
	   by the program counter. Also store the current instruction */
	cpu_jumpblock[cpu_state.instruction = CPU_FETCH ()] ();
    }
  }
  
  /* Put the cpu state back */
  memcpy (cpu, &cpu_state, sizeof (Cpu));

  if (cpu_state.got_break)
  {
    cpu->got_break = 0;
    return 1;
  }
  else
    return 0;
}

void
cpu_set_break (Cpu *cpu, int break_type, UWORD address)
{
  cpu->break_address = address;
  cpu->break_type = break_type;
  cpu->got_break = FALSE;
}

void
cpu_set_irq (Cpu *cpu)
{
  cpu_state.irq = TRUE;
  cpu->irq = TRUE;
}

void
cpu_reset_irq (Cpu *cpu)
{
  cpu_state.irq = FALSE;
  cpu->irq = FALSE;
}

void
cpu_cause_nmi (Cpu *cpu)
{
  cpu->nmi = TRUE;
}

void
cpu_op_undefined (void)
{
  /* Count two instruction cycles */
  cpu_state.time += 2;

  fprintf (stderr, "Undefined instruction %02X\n", cpu_state.instruction);
}

void
cpu_op_adc (void)
{
  UBYTE oa = cpu_state.a, ov = CPU_DATA_FOR_OP (cpu_state.instruction);
  int v = ov + oa;
  if (CPU_IS_C ())
    v++;
  if (CPU_IS_D () && (v & 0x0f) > 10)
    v += 6;
  cpu_state.a = v;
  CPU_SET_C (v > 255);
  CPU_SET_Z (!cpu_state.a); /* a could be different from v */
  CPU_SET_N (cpu_state.a & 128);
  CPU_SET_V (((oa & 0x7f) + (ov & 0x7f)) & 128);
}

void
cpu_op_sbc (void)
{
  UBYTE oa = cpu_state.a, ov = CPU_DATA_FOR_OP (cpu_state.instruction);
  int v = oa - ov;
  if (!CPU_IS_C ())
    v--;
  if (CPU_IS_D () && (v & 0x0f) > 10)
    v -= 6;
  cpu_state.a = v;
  CPU_SET_C (v >= 0);
  CPU_SET_Z (!cpu_state.a); /* a could be different from v */
  CPU_SET_N (cpu_state.a & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_cmp (void)
{
  UBYTE oa = cpu_state.a, ov = CPU_DATA_FOR_OP (cpu_state.instruction);
  int v = oa - ov;
  UBYTE na = v;

  CPU_SET_C (v >= 0);
  CPU_SET_Z (!na); /* a could be different from v */
  CPU_SET_N (na & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_cpx_immediate (void)
{
  UBYTE oa = cpu_state.x, ov = CPU_FETCH ();
  int v = oa - ov;
  UBYTE na = v;

  cpu_state.time += 2;
  CPU_SET_C (v >= 0);
  CPU_SET_Z (!na); /* a could be different from v */
  CPU_SET_N (na & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_cpx (void)
{
  UBYTE oa = cpu_state.x, ov = CPU_DATA_FOR_OP (cpu_state.instruction);
  int v = oa - ov;
  UBYTE na = v;

  CPU_SET_C (v >= 0);
  CPU_SET_Z (!na); /* a could be different from v */
  CPU_SET_N (na & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_cpy_immediate (void)
{
  UBYTE oa = cpu_state.y, ov = CPU_FETCH ();
  int v = oa - ov;
  UBYTE na = v;

  cpu_state.time += 2;
  CPU_SET_C (v >= 0);
  CPU_SET_Z (!na); /* a could be different from v */
  CPU_SET_N (na & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_cpy (void)
{
  UBYTE oa = cpu_state.y, ov = CPU_DATA_FOR_OP (cpu_state.instruction);
  int v = oa - ov;
  UBYTE na = v;

  CPU_SET_C (v >= 0);
  CPU_SET_Z (!na); /* a could be different from v */
  CPU_SET_N (na & 128);
  CPU_SET_V (((oa & 0x7f) - (ov & 0x7f)) & 128);
}

void
cpu_op_eor (void)
{
  UBYTE v = cpu_state.a ^= CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_ora (void)
{
  UBYTE v = cpu_state.a |= CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_and (void)
{
  UBYTE v = cpu_state.a &= CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_bit (void)
{
  UBYTE v = CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_N (v & 0x80);
  CPU_SET_V (v & 0x40);
  CPU_SET_Z (!(v & cpu_state.a));
}

void
cpu_op_lda (void)
{
  UBYTE v = cpu_state.a = CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_ldy_immediate (void)
{
  /* This instruction doesn't match the addressing mode pattern for
     some reason */
  UBYTE v = cpu_state.y = CPU_FETCH ();
  cpu_state.time += 2;
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_ldx_immediate (void)
{
  /* This instruction doesn't match the addressing mode pattern for
     some reason */
  UBYTE v = cpu_state.x = CPU_FETCH ();
  cpu_state.time += 2;
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_ldx (void)
{
  UBYTE v;
  /* The addressing modes seems to use x instead of y here somehow, so
     we can compensate by copying it across */
  cpu_state.x = cpu_state.y;
  v = cpu_state.x = CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_ldy (void)
{
  UBYTE v = cpu_state.y = CPU_DATA_FOR_OP (cpu_state.instruction);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_sta (void)
{
  CPU_WRITE_FOR_OP (cpu_state.instruction, cpu_state.a);
}

void
cpu_op_stx (void)
{
  /* According to the datasheet this instruction uses y for zero page
     indexing instead of x which makes sense, but the book says
     otherwise. I trust the datasheet so we have to copy it over */
  UBYTE t = cpu_state.x;
  cpu_state.x = cpu_state.y;
  CPU_WRITE_FOR_OP (cpu_state.instruction, t);
  cpu_state.x = t;
}

void
cpu_op_sty (void)
{
  CPU_WRITE_FOR_OP (cpu_state.instruction, cpu_state.y);
}

void
cpu_op_inc (void)
{
  UBYTE v;
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  CPU_WRITE (addr, v = CPU_READ (addr) + 1);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_dec (void)
{
  UBYTE v;
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  CPU_WRITE (addr, v = CPU_READ (addr) - 1);
  CPU_SET_Z (!v);
  CPU_SET_N (v & 0x80);
}

void
cpu_op_asl (void)
{
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  UBYTE v = CPU_READ (addr);
  
  CPU_SET_C (v & 0x80);
  v <<= 1;
  CPU_SET_N (v & 0x80);
  CPU_SET_Z (!v);

  CPU_WRITE (addr, v);
}

void
cpu_op_asl_a (void)
{
  cpu_state.time += 2;
  CPU_SET_C (cpu_state.a & 0x80);
  cpu_state.a <<= 1;
  CPU_SET_N (cpu_state.a & 0x80);
  CPU_SET_Z (!cpu_state.a);
}

void
cpu_op_lsr (void)
{
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  UBYTE v = CPU_READ (addr);
  
  CPU_SET_C (v & 0x01);
  v >>= 1;
  CPU_SET_N (FALSE);
  CPU_SET_Z (!v);

  CPU_WRITE (addr, v);
}

void
cpu_op_lsr_a (void)
{
  cpu_state.time += 2;
  CPU_SET_C (cpu_state.a & 0x01);
  cpu_state.a >>= 1;
  CPU_SET_N (FALSE);
  CPU_SET_Z (!cpu_state.a);
}

void
cpu_op_rol (void)
{
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  UBYTE v = CPU_READ (addr);
  int oc = CPU_IS_C ();
  
  CPU_SET_C (v & 0x80);
  v <<= 1;
  if (oc)
    v |= 1;
  CPU_SET_N (v & 0x80);
  CPU_SET_Z (!v);

  CPU_WRITE (addr, v);
}

void
cpu_op_rol_a (void)
{
  int oc = CPU_IS_C ();
  cpu_state.time += 2;
  CPU_SET_C (cpu_state.a & 0x80);
  cpu_state.a <<= 1;
  if (oc)
    cpu_state.a |= 1;
  CPU_SET_N (cpu_state.a & 0x80);
  CPU_SET_Z (!cpu_state.a);
}

void
cpu_op_ror (void)
{
  UWORD addr = CPU_ADDR_FOR_OP (cpu_state.instruction);
  UBYTE v = CPU_READ (addr);
  int oc = CPU_IS_C ();
  
  CPU_SET_C (v & 0x01);
  v >>= 1;
  if (oc)
    v |= 0x80;
  CPU_SET_N (oc);
  CPU_SET_Z (!v);

  CPU_WRITE (addr, v);
}

void
cpu_op_ror_a (void)
{
  int oc = CPU_IS_C ();
  cpu_state.time += 2;
  CPU_SET_C (cpu_state.a & 0x01);
  cpu_state.a >>= 1;
  if (oc)
    cpu_state.a |= 0x80;
  CPU_SET_N (oc);
  CPU_SET_Z (!cpu_state.a);
}

void
cpu_op_clc (void)
{
  cpu_state.time += 2;
  CPU_SET_C (FALSE);
}

void
cpu_op_cld (void)
{
  cpu_state.time += 2;
  CPU_SET_D (FALSE);
}

void
cpu_op_cli (void)
{
  cpu_state.time += 2;
  CPU_SET_I (FALSE);
}

void
cpu_op_clv (void)
{
  cpu_state.time += 2;
  CPU_SET_V (FALSE);
}

void
cpu_op_sec (void)
{
  cpu_state.time += 2;
  CPU_SET_C (TRUE);
}

void
cpu_op_sed (void)
{
  cpu_state.time += 2;
  CPU_SET_D (TRUE);
}

void
cpu_op_sei (void)
{
  cpu_state.time += 2;
  CPU_SET_I (TRUE);
}

void 
cpu_op_inx (void)
{
  cpu_state.time += 2;
  cpu_state.x++;
  CPU_SET_N (cpu_state.x & 0x80);
  CPU_SET_Z (!cpu_state.x);
}

void 
cpu_op_iny (void)
{
  cpu_state.time += 2;
  cpu_state.y++;
  CPU_SET_N (cpu_state.y & 0x80);
  CPU_SET_Z (!cpu_state.y);
}

void 
cpu_op_dex (void)
{
  cpu_state.time += 2;
  cpu_state.x--;
  CPU_SET_N (cpu_state.x & 0x80);
  CPU_SET_Z (!cpu_state.x);
}

void 
cpu_op_dey (void)
{
  cpu_state.time += 2;
  cpu_state.y--;
  CPU_SET_N (cpu_state.y & 0x80);
  CPU_SET_Z (!cpu_state.y);
}

static const int cpu_branch_tests[4] = { CPU_FLAG_N,
					 CPU_FLAG_V,
					 CPU_FLAG_C,
					 CPU_FLAG_Z };

void
cpu_op_branch (void)
{
  SBYTE offset = CPU_FETCH ();
  UWORD new_addr;
  int ins;
  int t = cpu_state.p & cpu_branch_tests[(ins = cpu_state.instruction) >> 6];
  
  if ((ins & 0x20) == 0)
    t = !t;
  if (t)
  {
    new_addr = cpu_state.pc + offset;
    if ((new_addr & 0xFF00) != (cpu_state.pc & 0xFF00))
      cpu_state.time += 4;
    else
      cpu_state.time += 3;
    cpu_state.pc = new_addr;
  }
  else
    cpu_state.time += 2;
}

void
cpu_op_nop (void)
{
  /* Count two instruction cycles */
  cpu_state.time += 2;
}

void
cpu_op_jmp (void)
{
  int al = CPU_FETCH ();
  int ah = CPU_FETCH ();
  cpu_state.time += 3;
  cpu_state.pc = (ah << 8) | al;
}

void
cpu_op_jmp_ind (void)
{
  int al = CPU_FETCH ();
  int ah = CPU_FETCH ();
  cpu_state.time += 5;
  cpu_state.pc = CPU_READ_WORD ((ah << 8) | al);
}

void
cpu_op_jsr (void)
{
  int al = CPU_FETCH ();

  cpu_state.time += 6;

  /* Save the pc before getting the high byte */
  CPU_PUSH_PC ();

  /* Jump to the new address */
  cpu_state.pc = (CPU_FETCH () << 8) | al;
}

void
cpu_op_brk (void)
{
  /* Push the program counter */
  CPU_PUSH_WORD (cpu_state.pc);

  /* Push the flags */
  CPU_PUSH (cpu_state.p);

  /* Set the break flag */
  CPU_SET_B (TRUE);

  /* Jump to the address in the interrupt vector */
  cpu_state.pc = CPU_READ_WORD (CPU_IRQ_VECTOR);

  cpu_state.time += 7;
}

void
cpu_op_rts (void)
{
  int al = CPU_POP ();
  cpu_state.time += 6;
  cpu_state.pc = ((CPU_POP () << 8) | al) + 1;
}

void
cpu_op_rti (void)
{
  int al;
  cpu_state.time += 6;
  cpu_state.p = CPU_POP ();
  al = CPU_POP ();
  cpu_state.pc = ((CPU_POP () << 8) | al) + 1;
}

void
cpu_op_pha (void)
{
  cpu_state.time += 3;
  CPU_PUSH (cpu_state.a);
}

void
cpu_op_php (void)
{
  cpu_state.time += 3;
  CPU_PUSH (cpu_state.p);
}

void
cpu_op_pla (void)
{
  cpu_state.time += 4;
  cpu_state.a = CPU_POP ();
  CPU_SET_Z (!cpu_state.a);
  CPU_SET_N (cpu_state.a & 0x80);
}

void
cpu_op_plp (void)
{
  cpu_state.time += 4;
  cpu_state.p = CPU_POP ();
}

void
cpu_op_tax (void)
{
  cpu_state.time += 2;
  /* Copy a to x and set z flag */
  CPU_SET_Z (!(cpu_state.x = cpu_state.a));
  CPU_SET_N (cpu_state.x & 0x80);
}

void
cpu_op_tay (void)
{
  cpu_state.time += 2;
  /* Copy a to y and set z flag */
  CPU_SET_Z (!(cpu_state.y = cpu_state.a));
  CPU_SET_N (cpu_state.y & 0x80);
}

void
cpu_op_tsx (void)
{
  cpu_state.time += 2;
  /* Copy s to x and set z flag */
  CPU_SET_Z (!(cpu_state.x = cpu_state.s));
  CPU_SET_N (cpu_state.x & 0x80);
}

void
cpu_op_txs (void)
{
  cpu_state.time += 2;
  /* Copy x to s and set z flag */
  CPU_SET_Z (!(cpu_state.s = cpu_state.x));
  CPU_SET_N (cpu_state.s & 0x80);
}

void
cpu_op_txa (void)
{
  cpu_state.time += 2;
  /* Copy x to a and set z flag */
  CPU_SET_Z (!(cpu_state.a = cpu_state.x));
  CPU_SET_N (cpu_state.a & 0x80);
}

void
cpu_op_tya (void)
{
  cpu_state.time += 2;
  /* Copy y to a and set z flag */
  CPU_SET_Z (!(cpu_state.a = cpu_state.y));
  CPU_SET_N (cpu_state.a & 0x80);
}

/* Functions to get values using the different addressing modes */
UBYTE
cpu_get_immediate (void)
{
  cpu_state.time += 2;
  return CPU_FETCH ();
}

UBYTE
cpu_get_zero_page (void)
{
  cpu_state.time += 3;
  return CPU_READ_ZERO (CPU_FETCH ());
}

UBYTE
cpu_get_absolute (void)
{
  int al = CPU_FETCH ();
  cpu_state.time += 4;
  return CPU_READ (al | (CPU_FETCH () << 8));
}

UBYTE
cpu_get_zero_indexed_x (void)
{
  cpu_state.time += 4;
  return CPU_READ_ZERO ((CPU_FETCH () + cpu_state.x) & 0xff);
}

UBYTE
cpu_get_absolute_indexed_x (void)
{
  int al = CPU_FETCH () + cpu_state.x;
  int ah = CPU_FETCH ();

  if (al >= 0x100)
  {
    /* count an extra cycle when it goes over the page boundary */
    cpu_state.time += 5; 
    return CPU_READ ((ah << 8) + al);
  }
  else
  {
    cpu_state.time += 4;
    return CPU_READ ((ah << 8) | al);
  }
}

UBYTE
cpu_get_absolute_indexed_y (void)
{
  int al = CPU_FETCH () + cpu_state.y;
  int ah = CPU_FETCH ();

  if (al >= 0x100)
  {
    cpu_state.time += 5; /* count an extra cycle when it goes over the page
		       boundary */
    return CPU_READ ((ah << 8) + al);
  }
  else
  {
    cpu_state.time += 4;
    return CPU_READ ((ah << 8) | al);
  }
}

UBYTE
cpu_get_pre_indexed_x (void)
{
  UBYTE za = CPU_FETCH () + cpu_state.x;
  int al = CPU_READ (za++);
  int ah = CPU_READ (za);
  cpu_state.time += 6;
  return CPU_READ ((ah << 8) | al);  
}

UBYTE
cpu_get_post_indexed_y (void)
{
  UBYTE za = CPU_FETCH ();
  int al = CPU_READ (za++) + cpu_state.y;
  int ah = CPU_READ (za);
  if (al >= 0x100)
  {
    /* count an extra cycle when it goes over the page boundary */
    cpu_state.time += 6;
    return CPU_READ ((ah << 8) + al);
  }
  else
  {
    cpu_state.time += 5;
    return CPU_READ ((ah << 8) | al);
  }
}

/* Functions to write using the eight addressing modes */
void
cpu_write_zero_page (UBYTE v)
{
  cpu_state.time += 3;
  CPU_WRITE_ZERO (CPU_FETCH (), v);
}

void
cpu_write_absolute (UBYTE v)
{
  int al = CPU_FETCH ();
  cpu_state.time += 4;
  CPU_WRITE (al | (CPU_FETCH () << 8), v);
}

void
cpu_write_zero_indexed_x (UBYTE v)
{
  cpu_state.time += 4;
  CPU_WRITE_ZERO ((CPU_FETCH () + cpu_state.x) & 0xff, v);
}

void
cpu_write_absolute_indexed_x (UBYTE v)
{
  int al = CPU_FETCH () + cpu_state.x;
  int ah = CPU_FETCH ();

  if (al >= 0x100)
  {
    /* count an extra cycle when it goes over the page boundary */
    cpu_state.time += 5; 
    CPU_WRITE ((ah << 8) + al, v);
  }
  else
  {
    cpu_state.time += 4;
    CPU_WRITE ((ah << 8) | al, v);
  }
}

void
cpu_write_absolute_indexed_y (UBYTE v)
{
  int al = CPU_FETCH () + cpu_state.y;
  int ah = CPU_FETCH ();

  if (al >= 0x100)
  {
    /* count an extra cycle when it goes over the page boundary */
    cpu_state.time += 5;
    CPU_WRITE ((ah << 8) + al, v);
  }
  else
  {
    cpu_state.time += 4;
    CPU_WRITE ((ah << 8) | al, v);
  }
}

void
cpu_write_pre_indexed_x (UBYTE v)
{
  UBYTE za = CPU_FETCH () + cpu_state.x;
  int al = CPU_READ (za++);
  int ah = CPU_READ (za);
  cpu_state.time += 6;
  CPU_WRITE ((ah << 8) | al, v);
}

void
cpu_write_post_indexed_y (UBYTE v)
{
  UBYTE za = CPU_FETCH ();
  int al = CPU_READ (za++) + cpu_state.y;
  int ah = CPU_READ (za);
  if (al >= 0x100)
  {
    /* count an extra cycle when it goes over the page boundary */
    cpu_state.time += 6;
    CPU_WRITE ((ah << 8) + al, v);
  }
  else
  {
    cpu_state.time += 5;
    CPU_WRITE ((ah << 8) | al, v);
  }
}

/* Functions to calculate an address using the addressing modes.
   Counts the clock cycles for doing some read write operation on the
   address */
UWORD
cpu_get_zero_page_a (void)
{
  cpu_state.time += 5;
  return CPU_FETCH ();
}

UWORD
cpu_get_absolute_a (void)
{
  int al = CPU_FETCH ();
  cpu_state.time += 6;
  return al | (CPU_FETCH () << 8);
}

UWORD
cpu_get_zero_indexed_x_a (void)
{
  cpu_state.time += 6;
  return (CPU_FETCH () + cpu_state.x) & 0xff;
}

UWORD
cpu_get_absolute_indexed_x_a (void)
{
  int al = CPU_FETCH ();
  int ah = CPU_FETCH ();

  cpu_state.time += 7;

  return (ah << 8) + al + cpu_state.x;
}

/* Jumpblocks for the 8 different addressing modes */
static const CpuReadFunc cpu_addressing_modes[8] =
  {
    cpu_get_pre_indexed_x,
    cpu_get_zero_page,
    cpu_get_immediate,
    cpu_get_absolute,
    cpu_get_post_indexed_y,
    cpu_get_zero_indexed_x,
    cpu_get_absolute_indexed_y,
    cpu_get_absolute_indexed_x
  };
static const CpuWriteFunc cpu_addressing_modes_write[8] =
  {
    cpu_write_pre_indexed_x,
    cpu_write_zero_page,
    NULL,
    cpu_write_absolute,
    cpu_write_post_indexed_y,
    cpu_write_zero_indexed_x,
    cpu_write_absolute_indexed_y,
    cpu_write_absolute_indexed_x
  };
static const CpuAddressFunc cpu_addressing_modes_addresses[4] =
  {
    cpu_get_zero_page_a,
    cpu_get_absolute_a,
    cpu_get_zero_indexed_x_a,
    cpu_get_absolute_indexed_x_a
  };

static const CpuOpcodeFunc cpu_jumpblock[256] =
  {
    cpu_op_brk,           /* 00 */
    cpu_op_ora,           /* 01 */
    cpu_op_undefined,     /* 02 */
    cpu_op_undefined,     /* 03 */
    cpu_op_undefined,     /* 04 */
    cpu_op_ora,           /* 05 */
    cpu_op_asl,           /* 06 */
    cpu_op_undefined,     /* 07 */
    cpu_op_php,           /* 08 */
    cpu_op_ora,           /* 09 */
    cpu_op_asl_a,         /* 0A */
    cpu_op_undefined,     /* 0B */
    cpu_op_undefined,     /* 0C */
    cpu_op_ora,           /* 0D */
    cpu_op_asl,           /* 0E */
    cpu_op_undefined,     /* 0F */
    cpu_op_branch,        /* 10 */
    cpu_op_ora,           /* 11 */
    cpu_op_undefined,     /* 12 */
    cpu_op_undefined,     /* 13 */
    cpu_op_undefined,     /* 14 */
    cpu_op_ora,           /* 15 */
    cpu_op_asl,           /* 16 */
    cpu_op_undefined,     /* 17 */
    cpu_op_clc,           /* 18 */
    cpu_op_ora,           /* 19 */
    cpu_op_undefined,     /* 1A */
    cpu_op_undefined,     /* 1B */
    cpu_op_undefined,     /* 1C */
    cpu_op_ora,           /* 1D */
    cpu_op_asl,           /* 1E */
    cpu_op_undefined,     /* 1F */
    cpu_op_jsr,           /* 20 */
    cpu_op_and,           /* 21 */
    cpu_op_undefined,     /* 22 */
    cpu_op_undefined,     /* 23 */
    cpu_op_bit,           /* 24 */
    cpu_op_and,           /* 25 */
    cpu_op_rol,           /* 26 */
    cpu_op_undefined,     /* 27 */
    cpu_op_plp,           /* 28 */
    cpu_op_and,           /* 29 */
    cpu_op_rol_a,         /* 2A */
    cpu_op_undefined,     /* 2B */
    cpu_op_bit,           /* 2C */
    cpu_op_and,           /* 2D */
    cpu_op_rol,           /* 2E */
    cpu_op_undefined,     /* 2F */
    cpu_op_branch,        /* 30 */
    cpu_op_and,           /* 31 */
    cpu_op_undefined,     /* 32 */
    cpu_op_undefined,     /* 33 */
    cpu_op_undefined,     /* 34 */
    cpu_op_and,           /* 35 */
    cpu_op_rol,           /* 36 */
    cpu_op_undefined,     /* 37 */
    cpu_op_sec,           /* 38 */
    cpu_op_and,           /* 39 */
    cpu_op_undefined,     /* 3A */
    cpu_op_undefined,     /* 3B */
    cpu_op_undefined,     /* 3C */
    cpu_op_and,           /* 3D */
    cpu_op_rol,           /* 3E */
    cpu_op_undefined,     /* 3F */
    cpu_op_rti,           /* 40 */
    cpu_op_eor,           /* 41 */
    cpu_op_undefined,     /* 42 */
    cpu_op_undefined,     /* 43 */
    cpu_op_undefined,     /* 44 */
    cpu_op_eor,           /* 45 */
    cpu_op_lsr,           /* 46 */
    cpu_op_undefined,     /* 47 */
    cpu_op_pha,           /* 48 */
    cpu_op_eor,           /* 49 */
    cpu_op_lsr_a,         /* 4A */
    cpu_op_undefined,     /* 4B */
    cpu_op_jmp,           /* 4C */
    cpu_op_eor,           /* 4D */
    cpu_op_lsr,           /* 4E */
    cpu_op_undefined,     /* 4F */
    cpu_op_branch,        /* 50 */
    cpu_op_eor,           /* 51 */
    cpu_op_undefined,     /* 52 */
    cpu_op_undefined,     /* 53 */
    cpu_op_undefined,     /* 54 */
    cpu_op_eor,           /* 55 */
    cpu_op_lsr,           /* 56 */
    cpu_op_undefined,     /* 57 */
    cpu_op_cli,           /* 58 */
    cpu_op_eor,           /* 59 */
    cpu_op_undefined,     /* 5A */
    cpu_op_undefined,     /* 5B */
    cpu_op_undefined,     /* 5C */
    cpu_op_eor,           /* 5D */
    cpu_op_lsr,           /* 5E */
    cpu_op_undefined,     /* 5F */
    cpu_op_rts,           /* 60 */
    cpu_op_adc,           /* 61 */
    cpu_op_undefined,     /* 62 */
    cpu_op_undefined,     /* 63 */
    cpu_op_undefined,     /* 64 */
    cpu_op_adc,           /* 65 */
    cpu_op_ror,           /* 66 */
    cpu_op_undefined,     /* 67 */
    cpu_op_pla,           /* 68 */
    cpu_op_adc,           /* 69 */
    cpu_op_ror_a,         /* 6A */
    cpu_op_undefined,     /* 6B */
    cpu_op_jmp_ind,       /* 6C */
    cpu_op_adc,           /* 6D */
    cpu_op_ror,           /* 6E */
    cpu_op_undefined,     /* 6F */
    cpu_op_branch,        /* 70 */
    cpu_op_adc,           /* 71 */
    cpu_op_undefined,     /* 72 */
    cpu_op_undefined,     /* 73 */
    cpu_op_undefined,     /* 74 */
    cpu_op_adc,           /* 75 */
    cpu_op_ror,           /* 76 */
    cpu_op_undefined,     /* 77 */
    cpu_op_sei,           /* 78 */
    cpu_op_adc,           /* 79 */
    cpu_op_undefined,     /* 7A */
    cpu_op_undefined,     /* 7B */
    cpu_op_undefined,     /* 7C */
    cpu_op_adc,           /* 7D */
    cpu_op_ror,           /* 7E */
    cpu_op_undefined,     /* 7F */
    cpu_op_undefined,     /* 80 */
    cpu_op_sta,           /* 81 */
    cpu_op_undefined,     /* 82 */
    cpu_op_undefined,     /* 83 */
    cpu_op_sty,           /* 84 */
    cpu_op_sta,           /* 85 */
    cpu_op_stx,           /* 86 */
    cpu_op_undefined,     /* 87 */
    cpu_op_dey,           /* 88 */
    cpu_op_undefined,     /* 89 */
    cpu_op_txa,           /* 8A */
    cpu_op_undefined,     /* 8B */
    cpu_op_sty,           /* 8C */
    cpu_op_sta,           /* 8D */
    cpu_op_stx,           /* 8E */
    cpu_op_undefined,     /* 8F */
    cpu_op_branch,        /* 90 */
    cpu_op_sta,           /* 91 */
    cpu_op_undefined,     /* 92 */
    cpu_op_undefined,     /* 93 */
    cpu_op_sty,           /* 94 */
    cpu_op_sta,           /* 95 */
    cpu_op_stx,           /* 96 */
    cpu_op_undefined,     /* 97 */
    cpu_op_tya,           /* 98 */
    cpu_op_sta,           /* 99 */
    cpu_op_txs,           /* 9A */
    cpu_op_undefined,     /* 9B */
    cpu_op_undefined,     /* 9C */
    cpu_op_sta,           /* 9D */
    cpu_op_undefined,     /* 9E */
    cpu_op_undefined,     /* 9F */
    cpu_op_ldy_immediate, /* A0 */
    cpu_op_lda,           /* A1 */
    cpu_op_ldx_immediate, /* A2 */
    cpu_op_undefined,     /* A3 */
    cpu_op_ldy,           /* A4 */
    cpu_op_lda,           /* A5 */
    cpu_op_ldx,           /* A6 */
    cpu_op_undefined,     /* A7 */
    cpu_op_tay,           /* A8 */
    cpu_op_lda,           /* A9 */
    cpu_op_tax,           /* AA */
    cpu_op_undefined,     /* AB */
    cpu_op_ldy,           /* AC */
    cpu_op_lda,           /* AD */
    cpu_op_ldx,           /* AE */
    cpu_op_undefined,     /* AF */
    cpu_op_branch,        /* B0 */
    cpu_op_lda,           /* B1 */
    cpu_op_undefined,     /* B2 */
    cpu_op_undefined,     /* B3 */
    cpu_op_ldy,           /* B4 */
    cpu_op_lda,           /* B5 */
    cpu_op_ldx,           /* B6 */
    cpu_op_undefined,     /* B7 */
    cpu_op_clv,           /* B8 */
    cpu_op_lda,           /* B9 */
    cpu_op_tsx,           /* BA */
    cpu_op_undefined,     /* BB */
    cpu_op_ldy,           /* BC */
    cpu_op_lda,           /* BD */
    cpu_op_ldx,           /* BE */
    cpu_op_undefined,     /* BF */
    cpu_op_cpy_immediate, /* C0 */
    cpu_op_cmp,           /* C1 */
    cpu_op_undefined,     /* C2 */
    cpu_op_undefined,     /* C3 */
    cpu_op_cpy,           /* C4 */
    cpu_op_cmp,           /* C5 */
    cpu_op_dec,           /* C6 */
    cpu_op_undefined,     /* C7 */
    cpu_op_iny,           /* C8 */
    cpu_op_cmp,           /* C9 */
    cpu_op_dex,           /* CA */
    cpu_op_undefined,     /* CB */
    cpu_op_cpy,           /* CC */
    cpu_op_cmp,           /* CD */
    cpu_op_dec,           /* CE */
    cpu_op_undefined,     /* CF */
    cpu_op_branch,        /* D0 */
    cpu_op_cmp,           /* D1 */
    cpu_op_undefined,     /* D2 */
    cpu_op_undefined,     /* D3 */
    cpu_op_undefined,     /* D4 */
    cpu_op_cmp,           /* D5 */
    cpu_op_dec,           /* D6 */
    cpu_op_undefined,     /* D7 */
    cpu_op_cld,           /* D8 */
    cpu_op_cmp,           /* D9 */
    cpu_op_undefined,     /* DA */
    cpu_op_undefined,     /* DB */
    cpu_op_undefined,     /* DC */
    cpu_op_cmp,           /* DD */
    cpu_op_dec,           /* DE */
    cpu_op_undefined,     /* DF */
    cpu_op_cpx_immediate, /* E0 */
    cpu_op_sbc,           /* E1 */
    cpu_op_undefined,     /* E2 */
    cpu_op_undefined,     /* E3 */
    cpu_op_cpx,           /* E4 */
    cpu_op_sbc,           /* E5 */
    cpu_op_inc,           /* E6 */
    cpu_op_undefined,     /* E7 */
    cpu_op_inx,           /* E8 */
    cpu_op_sbc,           /* E9 */
    cpu_op_undefined,     /* EA */
    cpu_op_undefined,     /* EB */
    cpu_op_cpx,           /* EC */
    cpu_op_sbc,           /* ED */
    cpu_op_inc,           /* EE */
    cpu_op_undefined,     /* EF */
    cpu_op_branch,        /* F0 */
    cpu_op_sbc,           /* F1 */
    cpu_op_undefined,     /* F2 */
    cpu_op_undefined,     /* F3 */
    cpu_op_undefined,     /* F4 */
    cpu_op_sbc,           /* F5 */
    cpu_op_inc,           /* F6 */
    cpu_op_undefined,     /* F7 */
    cpu_op_sed,           /* F8 */
    cpu_op_sbc,           /* F9 */
    cpu_op_undefined,     /* FA */
    cpu_op_undefined,     /* FB */
    cpu_op_undefined,     /* FC */
    cpu_op_sbc,           /* FD */
    cpu_op_inc,           /* FE */
    cpu_op_undefined,     /* FF */
  };
