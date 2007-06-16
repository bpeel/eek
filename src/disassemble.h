#ifndef _DISASSEMBLE_H
#define _DISASSEMBLE_H

#include "stypes.h"

int disassemble_instruction (UWORD address, const UBYTE *bytes,
			     char *mnemonic, char *operands);

#endif /* _DISASSEMBLE_H */
