#include "config.h"

#include <gtk/gtkwidget.h>

#include "electronwidget.h"
#include "electron.h"
#include "video.h"
#include "framesource.h"

static void electron_widget_class_init (ElectronWidgetClass *klass);
static void electron_widget_init (ElectronWidget *widget);
static void electron_widget_realize (GtkWidget *widget);
static void electron_widget_finalize (GObject *obj);
static void electron_widget_dispose (GObject *obj);
static gboolean electron_widget_expose (GtkWidget *widget, GdkEventExpose *event);
static void electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition);

static gboolean electron_widget_timeout (ElectronWidget *ewidget);

static gpointer parent_class;

GdkRgbCmap electron_widget_color_map =
  {
    {
      0xffffff, /* white */
      0x00ffff, /* cyan */
      0xff00ff, /* magenta */
      0x0000ff, /* blue */
      0xffff00, /* yellow */
      0x00ff00, /* green */
      0xff0000, /* red */
      0x000000  /* black */
    }, 8
  };

GType
electron_widget_get_type ()
{
  static GType electron_widget_type = 0;

  if (!electron_widget_type)
  {
    static const GTypeInfo electron_widget_info =
      {
	sizeof (ElectronWidgetClass),
	NULL, NULL,
	(GClassInitFunc) electron_widget_class_init,
	NULL, NULL,

	sizeof (ElectronWidget),
	0,
	(GInstanceInitFunc) electron_widget_init,
	NULL
      };

    electron_widget_type = g_type_register_static (GTK_TYPE_WIDGET,
						   "ElectronWidget",
						   &electron_widget_info, 0);
  }

  return electron_widget_type;
}

static void
electron_widget_class_init (ElectronWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = electron_widget_finalize;
  object_class->dispose = electron_widget_dispose;

  widget_class->realize = electron_widget_realize;
  widget_class->expose_event = electron_widget_expose;
  widget_class->size_request = electron_widget_size_request;
}

static void
electron_widget_init (ElectronWidget *ewidget)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ewidget), GTK_CAN_FOCUS);

  ewidget->electron = electron_new ();

  ewidget->timeout = frame_source_add (ELECTRON_TICKS_PER_FRAME,
				       (GSourceFunc) electron_widget_timeout,
				       ewidget, NULL);
}

GtkWidget *
electron_widget_new ()
{
  return g_object_new (TYPE_ELECTRON_WIDGET, NULL);
}

static void
electron_widget_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* There's no point in double buffering this widget because none of
     the drawing operations overlap so it won't cause flicker */
  gtk_widget_set_double_buffered (widget, FALSE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes,
				   GDK_WA_X | GDK_WA_Y);

  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (widget->window, widget);

  gdk_window_set_background (widget->window, &widget->style->bg[GTK_WIDGET_STATE (widget)]);
}

static void
electron_widget_finalize (GObject *obj)
{
  ElectronWidget *ewidget;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (obj));

  ewidget = ELECTRON_WIDGET (obj);
  electron_free (ewidget->electron);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
electron_widget_dispose (GObject *obj)
{
  ElectronWidget *ewidget;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (obj));

  ewidget = ELECTRON_WIDGET (obj);

  if (ewidget->timeout)
  {
    g_source_remove (ewidget->timeout);
    ewidget->timeout = 0;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static gboolean
electron_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
  ElectronWidget *ewidget;
  int xpos, ypos;

  g_return_val_if_fail (IS_ELECTRON_WIDGET (widget), FALSE);

  ewidget = ELECTRON_WIDGET (widget);

  /* Centre the display on the widget */
  xpos = widget->allocation.width / 2 - VIDEO_WIDTH / 2;
  ypos = widget->allocation.height / 2 - VIDEO_HEIGHT / 2;

  /* Clear the area around the display */
  if (xpos > 0)
    gdk_window_clear_area (widget->window,
			   0, 0, xpos, widget->allocation.height);
  if (xpos + VIDEO_WIDTH < widget->allocation.height)
    gdk_window_clear_area (widget->window,
			   xpos + VIDEO_WIDTH, 0, widget->allocation.width - xpos - VIDEO_WIDTH,
			   widget->allocation.height);
  if (ypos > 0)
    gdk_window_clear_area (widget->window,
			   xpos, 0, VIDEO_WIDTH, ypos);
  if (ypos + VIDEO_HEIGHT < widget->allocation.height)
    gdk_window_clear_area (widget->window,
			   xpos, ypos + VIDEO_HEIGHT, VIDEO_WIDTH,
			   widget->allocation.height - ypos - VIDEO_HEIGHT);
			   
  gdk_draw_indexed_image (GDK_DRAWABLE (widget->window),
			  widget->style->fg_gc[widget->state],
			  xpos, ypos, VIDEO_WIDTH, VIDEO_HEIGHT,
			  GDK_RGB_DITHER_NONE,
			  ewidget->electron->video.screen_memory,
			  VIDEO_SCREEN_PITCH,
			  &electron_widget_color_map);

  return FALSE;
}

static void
electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  requisition->width = VIDEO_WIDTH;
  requisition->height = VIDEO_HEIGHT;
}

static gboolean
electron_widget_timeout (ElectronWidget *ewidget)
{
  g_return_val_if_fail (IS_ELECTRON_WIDGET (ewidget), FALSE);

  electron_run_frame (ewidget->electron);
  if (GTK_WIDGET_REALIZED (GTK_WIDGET (ewidget)))
    gdk_window_invalidate_rect (GTK_WIDGET (ewidget)->window, NULL, FALSE);

  return TRUE;
}
