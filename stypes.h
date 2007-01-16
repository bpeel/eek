#ifndef _STYPES_H
#define _STYPES_H

typedef unsigned char  UBYTE;
typedef signed char    SBYTE;
typedef unsigned short UWORD;

/* Assert that these actually are the correct sizes during compile
   time (see http://www.jaggersoft.com/pubs/CVu11_3.html, except this
   is slightly modified because GCC actually allows zero-length
   arrays) */

#define STYPES_ASSERT_SIZE(type, size) \
typedef int stypes_stub_ ## type [(sizeof(type) == (size)) * 2 - 1]

STYPES_ASSERT_SIZE(UBYTE, 1);
STYPES_ASSERT_SIZE(SBYTE, 1);
STYPES_ASSERT_SIZE(UWORD, 2);

#endif /* _STYPES_H */
