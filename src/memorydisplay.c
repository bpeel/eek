#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwidget.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>

#include "memorydisplay.h"
#include "electronmanager.h"
#include "electron.h"
#include "eekmarshalers.h"

static void memory_display_class_init (MemoryDisplayClass *klass);
static void memory_display_init (MemoryDisplay *memdisplay);
static void memory_display_realize (GtkWidget *widget);
static void memory_display_dispose (GObject *obj);
static gboolean memory_display_expose (GtkWidget *widget, GdkEventExpose *event);

static void memory_display_style_set (GtkWidget *widget, GtkStyle *style);
static void memory_display_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void memory_display_size_allocate (GtkWidget *widget, GtkAllocation *allocation);

static void memory_display_on_started (ElectronManager *electron, MemoryDisplay *memdisplay);
static void memory_display_on_stopped (ElectronManager *electron, MemoryDisplay *memdisplay);
static void memory_display_on_scroll_value_changed (GtkAdjustment *adjustment,
						    MemoryDisplay *memdisplay);

static void memory_display_set_scroll_adjustments (MemoryDisplay *memdisplay,
						   GtkAdjustment *hadjustment,
						   GtkAdjustment *vadjustment);

static void memory_display_move_cursor (MemoryDisplay *memdisplay,
					gint direction);

static void memory_display_update_cursor_adjustment (MemoryDisplay *memdisplay);

static gboolean memory_display_button_press (GtkWidget *widget, GdkEventButton *event);

static gpointer parent_class;

#define GET_HEX_DIGIT(x) ((x) >= 10 ? (x) - 10 + 'A' : (x) + '0')

#define MEMORY_DISPLAY_MINIMUM_ROWS 64

#define MEMORY_DISPLAY_MEM_SIZE 65536

enum {
  MOVE_CURSOR,
  LAST_SIGNAL
};

enum {
  MEMORY_DISPLAY_MOVE_UP,
  MEMORY_DISPLAY_MOVE_DOWN,
  MEMORY_DISPLAY_MOVE_LEFT,
  MEMORY_DISPLAY_MOVE_RIGHT,
  MEMORY_DISPLAY_MOVE_BOL,
  MEMORY_DISPLAY_MOVE_EOL,
  MEMORY_DISPLAY_MOVE_BOF,
  MEMORY_DISPLAY_MOVE_EOF,
  MEMORY_DISPLAY_MOVE_PAGE_DOWN,
  MEMORY_DISPLAY_MOVE_PAGE_UP
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
memory_display_get_type ()
{
  static GType memory_display_type = 0;

  if (!memory_display_type)
  {
    static const GTypeInfo memory_display_info =
      {
	sizeof (MemoryDisplayClass),
	NULL, NULL,
	(GClassInitFunc) memory_display_class_init,
	NULL, NULL,

	sizeof (MemoryDisplay),
	0,
	(GInstanceInitFunc) memory_display_init,
	NULL
      };

    memory_display_type = g_type_register_static (GTK_TYPE_WIDGET,
						  "MemoryDisplay",
						  &memory_display_info, 0);
  }

  return memory_display_type;
}

static void
memory_display_class_init (MemoryDisplayClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = memory_display_dispose;

  widget_class->realize = memory_display_realize;
  widget_class->expose_event = memory_display_expose;
  widget_class->style_set = memory_display_style_set;
  widget_class->size_request = memory_display_size_request;
  widget_class->size_allocate = memory_display_size_allocate;
  widget_class->button_press_event = memory_display_button_press;

  klass->set_scroll_adjustments = memory_display_set_scroll_adjustments;
  klass->move_cursor = memory_display_move_cursor;

  widget_class->set_scroll_adjustments_signal =
    g_signal_new ("set_scroll_adjustments",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (MemoryDisplayClass, set_scroll_adjustments),
		  NULL, NULL,
		  eek_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

  signals[MOVE_CURSOR] = 
    g_signal_new ("move_cursor",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (MemoryDisplayClass, move_cursor),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);

  binding_set = gtk_binding_set_by_class (object_class);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_LEFT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_LEFT);
  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_RIGHT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_RIGHT);
  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_BOL);
  gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_EOL);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, GDK_CONTROL_MASK,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_BOF);
  gtk_binding_entry_add_signal (binding_set, GDK_End, GDK_CONTROL_MASK,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_EOF);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				"move_cursor", 1, G_TYPE_INT, MEMORY_DISPLAY_MOVE_PAGE_UP);
}

static void
memory_display_init (MemoryDisplay *memdisplay)
{
  GtkRcStyle *rc_style;

  GTK_WIDGET_SET_FLAGS (memdisplay, GTK_CAN_FOCUS);

  memdisplay->electron = NULL;
  memdisplay->bytes_per_row = 8;
  memdisplay->row_height = 5;
  memdisplay->disp_type = MEMORY_DISPLAY_TEXT;
  memdisplay->draw_start = 0;
  memdisplay->cur_pos = 0;

  memdisplay->hadjustment = NULL;
  memdisplay->vadjustment = NULL;
  memdisplay->cur_adjustment = NULL;

  /* Switch to a monospaced font unless it's already been overridden
     by the rc scripts */
  rc_style = gtk_widget_get_modifier_style (GTK_WIDGET (memdisplay));
  if (rc_style->font_desc == NULL)
  {
    rc_style->font_desc = pango_font_description_from_string ("monospace");
    gtk_widget_modify_style (GTK_WIDGET (memdisplay), rc_style);
  }

  memory_display_set_cursor_adjustment (memdisplay,
					GTK_ADJUSTMENT (gtk_adjustment_new (0.0f, 0.0f, 0.0f,
									    0.0f, 0.0f, 0.0f)));
}

GtkWidget *
memory_display_new ()
{
  return g_object_new (TYPE_MEMORY_DISPLAY, NULL);
}

GtkWidget *
memory_display_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_MEMORY_DISPLAY, NULL);

  memory_display_set_electron (MEMORY_DISPLAY (ret), electron);

  return ret;
}

static void
memory_display_refresh_row_height (MemoryDisplay *memdisplay)
{
  PangoLayout *layout;
  PangoRectangle logical_rect;

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
  {
    layout = gtk_widget_create_pango_layout (GTK_WIDGET (memdisplay), "0123456789");

    pango_layout_get_extents (layout, NULL, &logical_rect);

    g_object_unref (layout);

    memdisplay->row_height = logical_rect.height / PANGO_SCALE;

    if (memdisplay->vadjustment)
    {
      gint display_height, bytes_per_page;
      size_t scroll_max;

      display_height = GTK_WIDGET (memdisplay)->allocation.height;

      if (memdisplay->row_height == 0
	  || (bytes_per_page = display_height / memdisplay->row_height * memdisplay->bytes_per_row)
	  < memdisplay->bytes_per_row)
	bytes_per_page = memdisplay->bytes_per_row;

      scroll_max = memdisplay->electron ? MEMORY_DISPLAY_MEM_SIZE : 0;
      /* Must be able to show the last line */
      scroll_max = (scroll_max + memdisplay->bytes_per_row - 1) / memdisplay->bytes_per_row
	* memdisplay->bytes_per_row;

      memdisplay->vadjustment->lower = 0.0f;
      memdisplay->vadjustment->upper = scroll_max;
      memdisplay->vadjustment->step_increment = memdisplay->bytes_per_row;
      memdisplay->vadjustment->page_increment = bytes_per_page;
      memdisplay->vadjustment->page_size = bytes_per_page;

      gtk_adjustment_changed (memdisplay->vadjustment);
    }

    if (memdisplay->hadjustment)
    {
      memdisplay->hadjustment->lower = 0.0f;
      memdisplay->hadjustment->upper = 0.0f;
      memdisplay->hadjustment->step_increment = 0.0f;
      memdisplay->hadjustment->page_increment = 0.0f;
      memdisplay->hadjustment->page_size = 0.0f;

      gtk_adjustment_changed (memdisplay->hadjustment);
    }
  }
  else
    memdisplay->row_height = 5;

  memory_display_update_cursor_adjustment (memdisplay);
}

void
memory_display_set_electron (MemoryDisplay *memdisplay, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));

  if ((oldelectron = memdisplay->electron))
  {
    g_signal_handler_disconnect (oldelectron, memdisplay->started_handler);
    g_signal_handler_disconnect (oldelectron, memdisplay->stopped_handler);

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));
    
    g_object_ref (electron);

    memdisplay->started_handler
      = g_signal_connect (electron, "started",
			  G_CALLBACK (memory_display_on_started),
			  memdisplay);
    memdisplay->stopped_handler
      = g_signal_connect (electron, "stopped",
			  G_CALLBACK (memory_display_on_stopped),
			  memdisplay);
  }
    
  memdisplay->electron = electron;

  memory_display_refresh_row_height (memdisplay);
}

static void
memory_display_realize (GtkWidget *widget)
{
  MemoryDisplay *memdisplay;
  GdkWindowAttr attributes;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_MEMORY_DISPLAY (widget));

  memdisplay = MEMORY_DISPLAY (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_EXPOSURE_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes,
				   GDK_WA_X | GDK_WA_Y);

  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (widget->window, widget);

  gdk_window_set_background (widget->window, &widget->style->base[GTK_WIDGET_STATE (widget)]);
  
  memory_display_refresh_row_height (memdisplay);
}

static void
memory_display_dispose (GObject *obj)
{
  MemoryDisplay *memdisplay;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_MEMORY_DISPLAY (obj));

  memdisplay = MEMORY_DISPLAY (obj);

  memory_display_set_electron (memdisplay, NULL);
  gtk_widget_set_scroll_adjustments (GTK_WIDGET (memdisplay), NULL, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static gboolean
memory_display_expose (GtkWidget *widget, GdkEventExpose *event)
{
  MemoryDisplay *memdisplay;

  g_return_val_if_fail (IS_MEMORY_DISPLAY (widget), FALSE);

  memdisplay = MEMORY_DISPLAY (widget);

  if (memdisplay->electron)
  {
    int yp, xp, i, rowmin, rowmax;
    size_t pos, filesize = MEMORY_DISPLAY_MEM_SIZE;
    unsigned char *membuf = (unsigned char *) g_alloca (memdisplay->bytes_per_row);
    char hexbuf[5];
    PangoLayout *layout;
    PangoRectangle logical_rect;
    GdkGC *back_gc, *text_gc;

    hexbuf[2] = ' ';

    layout = gtk_widget_create_pango_layout (GTK_WIDGET (memdisplay), NULL);

    /* Calculate the range of rows that are covered by the exposed area */
    rowmin = event->area.y / memdisplay->row_height;
    rowmax = (event->area.y + event->area.height + memdisplay->row_height - 1)
      / memdisplay->row_height;

    /* If the end of the file lies outside of the exposed area then
       only draw to the end of the exposed area instead */
    if (rowmax * memdisplay->bytes_per_row + memdisplay->draw_start < filesize)
      filesize = rowmax * memdisplay->bytes_per_row + memdisplay->draw_start;

    for (yp = rowmin * memdisplay->row_height,
	   pos = memdisplay->draw_start + rowmin * memdisplay->bytes_per_row;
	 yp < widget->allocation.height && pos < filesize;
	 yp += memdisplay->row_height)
    {
      if (memdisplay->disp_type == MEMORY_DISPLAY_ADDRESS)
      {
	g_snprintf (hexbuf, 5, "%04X", pos);

	pango_layout_set_text (layout, hexbuf, -1);
	
	if (memdisplay->cur_pos >= pos
	    && memdisplay->cur_pos < pos + memdisplay->bytes_per_row)
	{
	  pango_layout_get_extents (layout, NULL, &logical_rect);
	  
	  if (GTK_WIDGET_HAS_FOCUS (memdisplay))
	  {
	    text_gc = widget->style->text_gc[GTK_STATE_SELECTED];
	    back_gc = widget->style->base_gc[GTK_STATE_SELECTED];
	  }
	  else
	  {
	    text_gc = widget->style->text_gc[GTK_STATE_ACTIVE];
	    back_gc = widget->style->base_gc[GTK_STATE_ACTIVE];
	  }

	  gdk_draw_rectangle (widget->window, back_gc, TRUE,
			      logical_rect.x / PANGO_SCALE,
			      yp + logical_rect.y / PANGO_SCALE,
			      logical_rect.width / PANGO_SCALE,
			      logical_rect.height / PANGO_SCALE);
	}
	else
	  text_gc = widget->style->text_gc[widget->state];

	gdk_draw_layout (GDK_DRAWABLE (widget->window), text_gc,
			 0, yp, layout);
	
	pos += memdisplay->bytes_per_row;
      }
      else
      {
	int len = MIN (memdisplay->bytes_per_row, filesize - pos);

	for (i = 0; i < len; i++)
	  membuf[i] = electron_read_from_location (memdisplay->electron->data, pos + i);
	
	if (memdisplay->disp_type == MEMORY_DISPLAY_TEXT)
	{
	  unsigned char *p = membuf;
	  int count = len;

	  while (count-- > 0)
	  {
	    if (*p < 32 || *p >= 127)
	      *p = '.';
	    p++;
	  }

	  if (memdisplay->cur_pos >= pos
	      && memdisplay->cur_pos < pos + memdisplay->bytes_per_row)
	  {
	    int xoff = memdisplay->cur_pos % memdisplay->bytes_per_row;

	    xp = 0;

	    if (GTK_WIDGET_HAS_FOCUS (memdisplay))
	    {
	      text_gc = widget->style->text_gc[GTK_STATE_SELECTED];
	      back_gc = widget->style->base_gc[GTK_STATE_SELECTED];
	    }
	    else
	    {
	      text_gc = widget->style->text_gc[GTK_STATE_ACTIVE];
	      back_gc = widget->style->base_gc[GTK_STATE_ACTIVE];
	    }

	    pango_layout_set_text (layout, (char *) membuf, xoff);
	    pango_layout_get_extents (layout, NULL, &logical_rect);
	    gdk_draw_layout (GDK_DRAWABLE (widget->window),
			     widget->style->text_gc[widget->state],
			     0, yp, layout);
	    xp += logical_rect.width;

	    pango_layout_set_text (layout, (char *) membuf + xoff, 1);
	    pango_layout_get_extents (layout, NULL, &logical_rect);
	    gdk_draw_rectangle (widget->window, back_gc, TRUE,
				(xp + logical_rect.x) / PANGO_SCALE,
				yp + logical_rect.y / PANGO_SCALE,
				logical_rect.width / PANGO_SCALE,
				logical_rect.height / PANGO_SCALE);
	    gdk_draw_layout (GDK_DRAWABLE (widget->window), text_gc,
			     xp / PANGO_SCALE, yp, layout);
	    xp += logical_rect.width;

	    pango_layout_set_text (layout, (char *) membuf + xoff + 1,
				   (memdisplay->cur_pos + memdisplay->bytes_per_row > filesize
				    ? len
				    : memdisplay->bytes_per_row)
				   - (xoff + 1));
	    gdk_draw_layout (GDK_DRAWABLE (widget->window),
			     widget->style->text_gc[widget->state],
			     xp / PANGO_SCALE, yp, layout);
	  }
	  else
	  {
	    pango_layout_set_text (layout, (char *) membuf, len);
	    gdk_draw_layout (GDK_DRAWABLE (widget->window),
			     widget->style->text_gc[widget->state],
			     0, yp, layout);
	  }

	  pos += len;
	}
	else
	  for (i = 0, xp = 0; i < memdisplay->bytes_per_row && pos < filesize;
	       i++, pos++)
	  {
	    hexbuf[0] = GET_HEX_DIGIT (membuf[i] >> 4);
	    hexbuf[1] = GET_HEX_DIGIT (membuf[i] & 0x0f);

	    if (pos == memdisplay->cur_pos)
	    {
	      pango_layout_set_text (layout, hexbuf, 2);
	      pango_layout_get_extents (layout, NULL, &logical_rect);

	      if (GTK_WIDGET_HAS_FOCUS (memdisplay))
	      {
		text_gc = widget->style->text_gc[GTK_STATE_SELECTED];
		back_gc = widget->style->base_gc[GTK_STATE_SELECTED];
	      }
	      else
	      {
		text_gc = widget->style->text_gc[GTK_STATE_ACTIVE];
		back_gc = widget->style->base_gc[GTK_STATE_ACTIVE];
	      }

	      gdk_draw_rectangle (widget->window, back_gc, TRUE,
				  xp + logical_rect.x / PANGO_SCALE,
				  yp + logical_rect.y / PANGO_SCALE,
				  logical_rect.width / PANGO_SCALE,
				  logical_rect.height / PANGO_SCALE);
	    }
	    else
	      text_gc = widget->style->text_gc[widget->state];

	    pango_layout_set_text (layout, hexbuf, 3);
	    pango_layout_get_extents (layout, NULL, &logical_rect);

	    gdk_draw_layout (GDK_DRAWABLE (widget->window), text_gc,
			     xp, yp, layout);
	
	    xp += logical_rect.width / PANGO_SCALE;
	  }
      }
    }

    g_object_unref (layout);
  }

  return FALSE;
}

static void
memory_display_style_set (GtkWidget *widget, GtkStyle *style)
{
  g_return_if_fail (IS_MEMORY_DISPLAY (widget));

  GTK_WIDGET_CLASS (parent_class)->style_set (widget, style);

  memory_display_refresh_row_height (MEMORY_DISPLAY (widget));
}

static void
memory_display_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  MemoryDisplay *memdisplay;

  g_return_if_fail (IS_MEMORY_DISPLAY (widget));

  memdisplay = MEMORY_DISPLAY (widget);

  layout = gtk_widget_create_pango_layout (widget,
					   memdisplay->disp_type == MEMORY_DISPLAY_HEX
					   ? "00 " : "0");
  pango_layout_get_extents (layout, NULL, &logical_rect);
  g_object_unref (layout);

  requisition->width = (memdisplay->disp_type == MEMORY_DISPLAY_ADDRESS ? 4
			: memdisplay->bytes_per_row) * (logical_rect.width / PANGO_SCALE);
  requisition->height = MEMORY_DISPLAY_MINIMUM_ROWS * memdisplay->row_height;
}

static void
memory_display_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  g_return_if_fail (IS_MEMORY_DISPLAY (widget));

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  memory_display_refresh_row_height (MEMORY_DISPLAY (widget));
}

static void
memory_display_on_started (ElectronManager *electron, MemoryDisplay *memdisplay)
{
  g_return_if_fail (IS_ELECTRON_MANAGER (electron));
  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  g_return_if_fail (memdisplay->electron == electron);
}

static void
memory_display_on_stopped (ElectronManager *electron, MemoryDisplay *memdisplay)
{
  g_return_if_fail (IS_ELECTRON_MANAGER (electron));
  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  g_return_if_fail (memdisplay->electron == electron);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
    gdk_window_invalidate_rect (GTK_WIDGET (memdisplay)->window, NULL, FALSE);
}

static void
memory_display_on_scroll_value_changed (GtkAdjustment *adjustment, MemoryDisplay *memdisplay)
{
  size_t oldpos;
  gint dx;
  
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  g_return_if_fail (memdisplay->vadjustment == adjustment);

  oldpos = memdisplay->draw_start;
  memdisplay->draw_start = (size_t) gtk_adjustment_get_value (adjustment)
    / memdisplay->bytes_per_row * memdisplay->bytes_per_row;

  if (oldpos != memdisplay->draw_start
      && GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
    gdk_window_scroll (GTK_WIDGET (memdisplay)->window, 0,
		       dx = ((gint) oldpos - (gint) memdisplay->draw_start)
		       * memdisplay->row_height / memdisplay->bytes_per_row);
}

void
memory_display_set_type (MemoryDisplay *memdisplay, int type)
{
  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  g_return_if_fail (type >= 0 && type < MEMORY_DISPLAY_TYPE_COUNT);

  memdisplay->disp_type = type;

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
  {
    gtk_widget_queue_resize (GTK_WIDGET (memdisplay));
    gdk_window_invalidate_rect (GTK_WIDGET (memdisplay)->window, NULL, FALSE);
  }
}

static void
memory_display_set_scroll_adjustments (MemoryDisplay *memdisplay,
				       GtkAdjustment *hadjustment,
				       GtkAdjustment *vadjustment)
{
  GtkAdjustment *oldadj;
  gint oldhandler;

  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));

  oldadj = memdisplay->hadjustment;
  memdisplay->hadjustment = hadjustment;
  if (hadjustment)
  {
    g_object_ref (hadjustment);
    gtk_object_sink (GTK_OBJECT (hadjustment));
  }
  if (oldadj)
    g_object_unref (oldadj);
  
  oldadj = memdisplay->vadjustment;
  oldhandler = memdisplay->scroll_value_changed_handler;
  memdisplay->vadjustment = vadjustment;
  if (vadjustment)
  {
    g_object_ref (vadjustment);
    gtk_object_sink (GTK_OBJECT (vadjustment));

    memdisplay->scroll_value_changed_handler
      = g_signal_connect (vadjustment, "value_changed",
			  G_CALLBACK (memory_display_on_scroll_value_changed),
			  memdisplay);

    memdisplay->draw_start = gtk_adjustment_get_value (vadjustment);
    if (GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
      gdk_window_invalidate_rect (GTK_WIDGET (memdisplay)->window, NULL, FALSE);
  }
  if (oldadj)
  {
    g_signal_handler_disconnect (oldadj, oldhandler);

    g_object_unref (oldadj);
  }
  
  memory_display_refresh_row_height (memdisplay);
}

static void
memory_display_update_cursor_position (MemoryDisplay *memdisplay, size_t pos)
{
  size_t oldpos = memdisplay->cur_pos;

  memdisplay->cur_pos = pos;

  if (memdisplay->vadjustment)
  {
    size_t clamp_pos;

    clamp_pos = pos - pos % memdisplay->bytes_per_row;
    gtk_adjustment_clamp_page (memdisplay->vadjustment, clamp_pos,
			       clamp_pos + memdisplay->bytes_per_row);
  }

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (memdisplay)))
  {
    GtkAllocation *allocation = &GTK_WIDGET (memdisplay)->allocation;
    GdkRectangle rect;

    /* Convert the positions to a count of the number of rows from the top */
    oldpos /= memdisplay->bytes_per_row;
    pos /= memdisplay->bytes_per_row;

    /* Redraw the row containing the old position */
    rect.x = 0;
    rect.y = (int) (oldpos - memdisplay->draw_start / memdisplay->bytes_per_row)
      * memdisplay->row_height;
    rect.width = allocation->width;
    rect.height = memdisplay->row_height;

    gdk_window_invalidate_rect (GTK_WIDGET (memdisplay)->window, &rect, FALSE);

    /* If the new pos is on another row then redraw that row as well */
    if (pos != oldpos)
    {
      rect.y = (int) (pos - memdisplay->draw_start / memdisplay->bytes_per_row)
	* memdisplay->row_height;
      
      gdk_window_invalidate_rect (GTK_WIDGET (memdisplay)->window, &rect, FALSE);
    }
  }
}

static void
memory_display_on_cursor_value_changed (GtkAdjustment *adjustment, gpointer user_data)
{
  MemoryDisplay *memdisplay;
  
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (IS_MEMORY_DISPLAY (user_data));

  memdisplay = MEMORY_DISPLAY (user_data);
  
  g_return_if_fail (memdisplay->cur_adjustment == adjustment);

  memory_display_update_cursor_position (memdisplay, gtk_adjustment_get_value (adjustment));
}

static void
memory_display_update_cursor_adjustment (MemoryDisplay *memdisplay)
{
  if (memdisplay->cur_adjustment)
  {
    memdisplay->cur_adjustment->lower = 0.0f;
    if (memdisplay->electron)
      memdisplay->cur_adjustment->upper = MEMORY_DISPLAY_MEM_SIZE - 1;
    else
      memdisplay->cur_adjustment->upper = -1;
    memdisplay->cur_adjustment->step_increment = 1;
    memdisplay->cur_adjustment->page_increment = 1;
    memdisplay->cur_adjustment->page_size = 1;
    
    gtk_adjustment_changed (memdisplay->cur_adjustment);
  }
}

void
memory_display_set_cursor_adjustment (MemoryDisplay *memdisplay, GtkAdjustment *adjustment)
{
  GtkAdjustment *oldadj;
  gint oldhandler;

  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  oldadj = memdisplay->cur_adjustment;
  oldhandler = memdisplay->cursor_value_changed_handler;
  memdisplay->cur_adjustment = adjustment;

  if (adjustment)
  {
    g_object_ref (adjustment);
    gtk_object_sink (GTK_OBJECT (adjustment));

    memdisplay->cursor_value_changed_handler
      = g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (memory_display_on_cursor_value_changed),
			  memdisplay);

    memory_display_update_cursor_adjustment (memdisplay);
    memory_display_update_cursor_position (memdisplay, gtk_adjustment_get_value (adjustment));
  }

  if (oldadj)
  {
    g_signal_handler_disconnect (oldadj, oldhandler);

    g_object_unref (oldadj);
  }
}

static void
memory_display_move_cursor (MemoryDisplay *memdisplay, gint direction)
{
  g_return_if_fail (IS_MEMORY_DISPLAY (memdisplay));
  g_return_if_fail (direction == MEMORY_DISPLAY_MOVE_UP
		    || direction == MEMORY_DISPLAY_MOVE_DOWN
		    || direction == MEMORY_DISPLAY_MOVE_LEFT
		    || direction == MEMORY_DISPLAY_MOVE_RIGHT
		    || direction == MEMORY_DISPLAY_MOVE_BOL
		    || direction == MEMORY_DISPLAY_MOVE_EOL
		    || direction == MEMORY_DISPLAY_MOVE_BOF
		    || direction == MEMORY_DISPLAY_MOVE_EOF
		    || direction == MEMORY_DISPLAY_MOVE_PAGE_UP
		    || direction == MEMORY_DISPLAY_MOVE_PAGE_DOWN);

  if (memdisplay->cur_adjustment)
  {
    int val = (int) memdisplay->cur_pos;

    if (direction == MEMORY_DISPLAY_MOVE_UP)
      val -= memdisplay->bytes_per_row;
    else if (direction == MEMORY_DISPLAY_MOVE_DOWN)
      val += memdisplay->bytes_per_row;
    else if (direction == MEMORY_DISPLAY_MOVE_LEFT)
      val--;
    else if (direction == MEMORY_DISPLAY_MOVE_RIGHT)
      val++;
    else if (direction == MEMORY_DISPLAY_MOVE_BOL)
      val -= val % memdisplay->bytes_per_row;
    else if (direction == MEMORY_DISPLAY_MOVE_EOL)
      val += (memdisplay->bytes_per_row - 1) - (val % memdisplay->bytes_per_row);
    else if (direction == MEMORY_DISPLAY_MOVE_BOF)
      val = 0;
    else if (direction == MEMORY_DISPLAY_MOVE_EOF)
      val = memdisplay->cur_adjustment->upper;
    else if (memdisplay->vadjustment)
    {
      gdouble scroll_val;
      gint cur_scroll_offset;

      /* Make sure the cursor is visible before we scroll the display */
      gtk_adjustment_clamp_page (memdisplay->vadjustment, val,
				 val + memdisplay->bytes_per_row);

      scroll_val = gtk_adjustment_get_value (memdisplay->vadjustment);
      /* Record the offset of the cursor from the top of the screen */
      cur_scroll_offset = val - memdisplay->draw_start;

      /* Scroll the display */
      if (direction == MEMORY_DISPLAY_MOVE_PAGE_DOWN)
	scroll_val += memdisplay->vadjustment->page_increment;
      else
	scroll_val -= memdisplay->vadjustment->page_increment;

      scroll_val = CLAMP (scroll_val, memdisplay->vadjustment->lower,
			  memdisplay->vadjustment->upper - memdisplay->vadjustment->page_size);
      
      gtk_adjustment_set_value (memdisplay->vadjustment, scroll_val);

      /* Put the cursor back at the same offset from the top of the screen */
      val = memdisplay->draw_start + cur_scroll_offset;
    }

    gtk_adjustment_set_value (memdisplay->cur_adjustment, val);
  }
}

static gboolean
memory_display_button_press (GtkWidget *widget, GdkEventButton *event)
{
  MemoryDisplay *memdisplay;

  g_return_val_if_fail (IS_MEMORY_DISPLAY (widget), TRUE);

  memdisplay = MEMORY_DISPLAY (widget);

  gtk_widget_grab_focus (widget);

  if (GTK_WIDGET_REALIZED (widget) && memdisplay->cur_adjustment
      && memdisplay->electron)
  {
    size_t addr = memdisplay->draw_start + (int) event->y / memdisplay->row_height
      * memdisplay->bytes_per_row;

    if (addr < MEMORY_DISPLAY_MEM_SIZE)
    {
      PangoLayout *layout;
      PangoRectangle logical_rect;
      unsigned char *membuf = (unsigned char *) g_alloca (memdisplay->bytes_per_row);
      size_t len = MIN (memdisplay->bytes_per_row, MEMORY_DISPLAY_MEM_SIZE - addr);
      int i;

      layout = gtk_widget_create_pango_layout (widget, NULL);

      for (i = 0; i < len; i++)
	membuf[i] = electron_read_from_location (memdisplay->electron->data, addr + i);

      if (memdisplay->disp_type == MEMORY_DISPLAY_HEX)
      {
	char hexbuf[3];
	int xp = 0, i;

	hexbuf[2] = ' ';

	for (i = 0; i < memdisplay->bytes_per_row - 1; i++, addr++)
	{
	  hexbuf[0] = GET_HEX_DIGIT (membuf[i] >> 4);
	  hexbuf[1] = GET_HEX_DIGIT (membuf[i] & 0x0f);
	    
	  pango_layout_set_text (layout, hexbuf, 3);
	  pango_layout_get_extents (layout, NULL, &logical_rect);

	  xp += logical_rect.width / PANGO_SCALE;

	  if (xp > event->x)
	    break;
	}
      }
      else if (memdisplay->disp_type == MEMORY_DISPLAY_TEXT)
      {
	unsigned char *p = membuf;
	int count = len;
	PangoLayoutLine *line;

	while (count-- > 0)
	{
	  if (*p < 32 || *p >= 127)
	    *p = '.';
	  p++;
	}

	pango_layout_set_text (layout, (char *) membuf, len);

	if ((line = pango_layout_get_line (layout, 0)))
	{
	  int offset;

	  if (!pango_layout_line_x_to_index (line, event->x * PANGO_SCALE, &offset, NULL)
	      || offset >= memdisplay->bytes_per_row)
	    offset = memdisplay->bytes_per_row - 1;
	    
	  addr += offset;
	}
      }
      
      g_object_unref (layout);
    }

    gtk_adjustment_set_value (memdisplay->cur_adjustment, addr);
  }  

  return TRUE;
}

