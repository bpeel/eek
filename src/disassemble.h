#ifndef _DISASSEMBLE_H
#define _DISASSEMBLE_H

#include <glib/gtypes.h>

#define DISASSEMBLE_MAX_BYTES    3
#define DISASSEMBLE_MAX_MNEMONIC 3
#define DISASSEMBLE_MAX_OPERANDS 7

int disassemble_instruction (guint16 address, const guint8 *bytes,
			     char *mnemonic, char *operands);

#endif /* _DISASSEMBLE_H */
