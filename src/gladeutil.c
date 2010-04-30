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

#include <glade/glade-xml.h>
#include <gtk/gtklabel.h>
#include <glib/gstdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "gladeutil.h"
#include "intl.h"

GladeXML *
glade_util_load (const char *filename, const char *root_widget)
{
  char *full_filename;
  GladeXML *ret;

  /* First check if the file exists in the source directory */
  full_filename = g_build_filename (PACKAGE_SOURCE_DIR, "glade", filename, NULL);
  if (g_access (full_filename, R_OK) == -1)
  {
    /* Otherwise use the glade installation directory */
    g_free (full_filename);
    full_filename = g_build_filename (EEK_GLADE_DIR, filename, NULL);
  }

  ret = glade_xml_new (full_filename, root_widget, NULL);

  g_free (full_filename);

  return ret;
}

gboolean
glade_util_get_widgets (const char *filename, const char *root_widget,
                        GtkWidget **error_widget, ...)
{
  va_list ap;
  const char *name;
  GtkWidget **widget_ret;
  GladeXML *gui;
  gboolean ret = TRUE;

  if ((gui = glade_util_load (filename, root_widget)) == NULL)
  {
    char *full_filename = g_build_filename (EEK_GLADE_DIR, filename, NULL);
    char *error_message = g_strdup_printf (_("There was an error opening the Glade file "
                                             "\"%s\". Please check your installation."),
                                           full_filename);

    *error_widget = gtk_label_new (error_message);
    gtk_label_set_line_wrap (GTK_LABEL (*error_widget), TRUE);

    g_free (error_message);
    g_free (full_filename);

    ret = FALSE;
  }
  else
  {
    va_start (ap, error_widget);

    while ((name = va_arg (ap, const char *)))
    {
      widget_ret = va_arg (ap, GtkWidget **);

      if (((* widget_ret) = glade_xml_get_widget (gui, name)) == NULL)
      {
        GtkWidget *root;
        char *full_filename = g_build_filename (EEK_GLADE_DIR, filename, NULL);
        char *error_message = g_strdup_printf (_("A widget is missing in the file "
                                                 "\"%s\". Please check your installation."),
                                               full_filename);

        *error_widget = gtk_label_new (error_message);
        gtk_label_set_line_wrap (GTK_LABEL (*error_widget), TRUE);

        g_free (error_message);
        g_free (full_filename);

        /* Try to destroy the root widget otherwise it will leak */
        if (root_widget && (root = glade_xml_get_widget (gui, root_widget)))
          gtk_widget_destroy (root);

        ret = FALSE;

        break;
      }
    }

    va_end (ap);

    g_object_unref (gui);
  }

  return ret;
}
