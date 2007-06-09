#ifndef _MEMORY_DISPLAY_H
#define _MEMORY_DISPLAY_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkadjustment.h>
#include "electronmanager.h"

#define TYPE_MEMORY_DISPLAY (memory_display_get_type ())
#define MEMORY_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
			     TYPE_MEMORY_DISPLAY, MemoryDisplay))
#define MEMORY_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
				     TYPE_MEMORY_DISPLAY, MemoryDisplayClass))
#define IS_MEMORY_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MEMORY_DISPLAY))
#define IS_MEMORY_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MEMORY_DISPLAY))
#define MEMORY_DISPLAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
				       TYPE_MEMORY_DISPLAY, MemoryDisplayClass))

typedef struct _MemoryDisplay MemoryDisplay;
typedef struct _MemoryDisplayClass MemoryDisplayClass;

struct _MemoryDisplay
{
  GtkWidget parent_object;

  ElectronManager *electron;
  GtkAdjustment *hadjustment, *vadjustment, *cur_adjustment;

  int started_handler, stopped_handler, scroll_value_changed_handler, cursor_value_changed_handler;

  int bytes_per_row, row_height;

  size_t draw_start, cur_pos;

  enum { MEMORY_DISPLAY_ADDRESS, MEMORY_DISPLAY_TEXT, MEMORY_DISPLAY_HEX,
	 MEMORY_DISPLAY_TYPE_COUNT } disp_type;
};

struct _MemoryDisplayClass
{
  GtkWidgetClass parent_class;

  void  (* set_scroll_adjustments) (MemoryDisplay *memdisplay,
				    GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);
  void  (* move_cursor) (MemoryDisplay *memdisplay, gint direction);
};

GType memory_display_get_type ();
GtkWidget *memory_display_new ();
GtkWidget *memory_display_new_with_electron (ElectronManager *electron);
void memory_display_set_electron (MemoryDisplay *memdisplay, ElectronManager *electron);
void memory_display_set_type (MemoryDisplay *memdisplay, int type);
void memory_display_set_cursor_adjustment (MemoryDisplay *memdisplay, GtkAdjustment *adjustment);

#endif /* _MEMORY_DISPLAY_H */
