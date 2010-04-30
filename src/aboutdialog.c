/*
 * eek - An emulator for the Acorn Electron
 * Copyright (C) 2010  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
                         "website", "http://www.busydoingnothing.co.uk/eek/",
                         "version", VERSION,
                         NULL);
}
