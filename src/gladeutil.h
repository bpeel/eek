#ifndef _GLADE_UTIL_H
#define _GLADE_UTIL_H

#include <glade/glade-xml.h>

GladeXML *glade_util_load (const char *filename, const char *root_widget);
gboolean glade_util_get_widgets (const char *filename, const char *root_widget,
				 GtkWidget **error_widget, ...) G_GNUC_NULL_TERMINATED;

#endif /* _GLADE_UTIL_H */
