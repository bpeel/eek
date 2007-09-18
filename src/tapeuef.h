#ifndef _TAPE_UEF_H
#define _TAPE_UEF_H

#include <glib.h>
#include "tapebuffer.h"

typedef enum
{
  TAPE_UEF_ERROR_IO,
  TAPE_UEF_ERROR_ZLIB,
  TAPE_UEF_ERROR_INVALID,
  TAPE_UEF_ERROR_NO_ZLIB,
  TAPE_UEF_ERROR_NOT_SUPPORTED
} TapeUEFError;

#define TAPE_UEF_ERROR tape_uef_error_quark ()
GQuark tape_uef_error_quark ();

TapeBuffer *tape_uef_load (FILE *infile, GError **error);

#endif /* _TAPE_UEF_H */
