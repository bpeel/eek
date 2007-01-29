#ifndef _BYTE_ORDER_H
#define _BYTE_ORDER_H

#include "stypes.h"

#if !defined(BYTE_ORDER_BIG) && !defined(BYTE_ORDER_SMALL)
#error no byte order defined
#endif
#if defined(BYTE_ORDER_BIG) && defined(BYTE_ORDER_SMALL)
#error both byte orders defined
#endif

#define BO_SWAP_WORD(x) ({UWORD _x = (x); (_x << 8) | (_x >> 8);})

#ifdef BYTE_ORDER_SMALL
#define BO_WORD_FROM_LE(x) (x)
#define BO_WORD_FROM_BE(x) BO_SWAP_WORD(x)
#else
#define BO_WORD_FROM_LE(x) BO_SWAP_WORD(x)
#define BO_WORD_FROM_BE(x) (x)
#endif

#ifdef BYTE_ORDER_SMALL
#define BO_WORD_TO_LE(x) (x)
#define BO_WORD_TO_BE(x) BO_SWAP_WORD(x)
#else
#define BO_WORD_TO_LE(x) BO_SWAP_WORD(x)
#define BO_WORD_TO_BE(x) (x)
#endif

#endif /* _BYTE_ORDER_H */
