#ifndef _FRAME_SOURCE_H
#define _FRAME_SOURCE_H

#include <glib.h>

guint frame_source_add (guint frame_time, GSourceFunc function, gpointer data,
			GDestroyNotify notify);

#endif /* _FRAME_SOURCE_H */
