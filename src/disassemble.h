#ifndef _DISASSEMBLE_H
#define _DISASSEMBLE_H

#include "stypes.h"

#define DISASSEMBLE_MAX_BYTES    3
#define DISASSEMBLE_MAX_MNEMONIC 3
#define DISASSEMBLE_MAX_OPERANDS 7

int disassemble_instruction (UWORD address, const UBYTE *bytes,
			     char *mnemonic, char *operands);

#endif /* _DISASSEMBLE_H */
