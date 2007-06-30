#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkaboutdialog.h>

#include "aboutdialog.h"
#include "intl.h"

static const char *about_dialog_authors[] = { "Neil Roberts <bpeeluk@yahoo.co.uk>", NULL };

void
about_dialog_show ()
{
  gtk_show_about_dialog (NULL,
			 "authors", about_dialog_authors,
			 "comments", _("An Acorn Electron Emulator"),
			 "copyright", _("Copyright (C) 2007 Neil Roberts"),
			 "name", _("Eek"),
			 "translator-credits", _("translator-credits"),
			 "version", VERSION,
			 NULL);
}
