#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h>

extern char *shortname;

void eprintf (const char *template, ...);
void *xmalloc (size_t size);
char *util_shortname (char *fullname);
#define xfree(x) free (x)
 
#endif /* _UTIL_H */
