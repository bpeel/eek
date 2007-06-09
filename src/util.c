#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "eek.h"
#include "util.h"

char *shortname = "";

void
eprintf (const char *template, ...)
{
  va_list ap;
     
  fprintf (stderr, "%s: ", shortname);
  va_start (ap, template);
  vfprintf (stderr, template, ap);
  va_end (ap);
}

void *
xmalloc (size_t size)
{
  void *ret;

  if ((ret = malloc (size)) == NULL)
  {
    eprintf ("virtual memory exhausted\n");
    exit (1);
  } 

  return ret;
}

void *
xrealloc (void *buf, size_t size)
{
  void *ret;

  if ((ret = realloc (buf, size)) == NULL)
  {
    eprintf ("virtual memory exhausted\n");
    exit (1);
  } 

  return ret;
}

char *
util_shortname (char *fullname)
{
  int length = strlen (fullname), i;

  /* Look for a '/' starting from the end */
  for (i = length; i >= 0; i--)
    if (fullname[i] == '/')
      /* If we find one, return a pointer to the next character, possibly '\0' */
      return &fullname[i + 1];

  /* Otherwise return the whole filename */
  return fullname;
}
