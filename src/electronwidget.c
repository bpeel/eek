#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwidget.h>
#include <gdk/gdkkeysyms.h>

#include "electronwidget.h"
#include "electron.h"
#include "video.h"

static void electron_widget_class_init (ElectronWidgetClass *klass);
static void electron_widget_init (ElectronWidget *widget);
static void electron_widget_realize (GtkWidget *widget);
static void electron_widget_dispose (GObject *obj);
static gboolean electron_widget_expose (GtkWidget *widget, GdkEventExpose *event);
static void electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void electron_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean electron_widget_key_event (GtkWidget *widget, GdkEventKey *event);
static void electron_widget_on_frame_end (ElectronManager *electron, gpointer user_data);
static gboolean electron_widget_button_press (GtkWidget *widget, GdkEventButton *event);

static gpointer parent_class;

static GdkRgbCmap electron_widget_color_map =
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

typedef struct _ElectronWidgetKey ElectronWidgetKey;

struct _ElectronWidgetKey
{
  int keysym, line, bit, modifiers;
};

static const ElectronWidgetKey electron_widget_keymap[] =
  {
    { GDK_Escape, 13, 0, 0 },
    { GDK_1, 12, 0, 0 },
    { GDK_exclam, 12, 0, 1 },
    { GDK_2, 11, 0, 0 },
    { GDK_quotedbl, 11, 0, 1 },
    { GDK_3, 10, 0, 0 },
    { GDK_numbersign, 10, 0, 1 },
    { GDK_4, 9, 0, 0 },
    { GDK_dollar, 9, 0, 1 },
    { GDK_5, 8, 0, 0 },
    { GDK_percent, 8, 0, 1 },
    { GDK_6, 7, 0, 0 },
    { GDK_ampersand, 7, 0, 1 },
    { GDK_7, 6, 0, 0 },
    { GDK_apostrophe, 6, 0, 1 },
    { GDK_8, 5, 0, 0 },
    { GDK_parenleft, 5, 0, 1 },
    { GDK_9, 4, 0, 0 },
    { GDK_parenright, 4, 0, 1 },
    { GDK_0, 3, 0, 0 },
    { GDK_at, 3, 0, 1 },
    { GDK_minus, 2, 0, 0 },
    { GDK_equal, 2, 0, 1 },
    { GDK_Left, 1, 0, 0 },
    { GDK_asciicircum, 1, 0, 1 },
    { GDK_asciitilde, 1, 0, 2 },
    { GDK_Right, 0, 0, 0 },
    { GDK_bar, 0, 0, 1 },
    { GDK_backslash, 0, 0, 2 },
    { GDK_q, 12, 1, 0 },
    { GDK_Q, 12, 1, 1 },
    { GDK_w, 11, 1, 0 },
    { GDK_W, 11, 1, 1 },
    { GDK_e, 10, 1, 0 },
    { GDK_E, 10, 1, 1 },
    { GDK_r, 9, 1, 0 },
    { GDK_R, 9, 1, 1 },
    { GDK_t, 8, 1, 0 },
    { GDK_T, 8, 1, 1 },
    { GDK_y, 7, 1, 0 },
    { GDK_Y, 7, 1, 1 },
    { GDK_u, 6, 1, 0 },
    { GDK_U, 6, 1, 1 },
    { GDK_i, 5, 1, 0 },
    { GDK_I, 5, 1, 1 },
    { GDK_o, 4, 1, 0 },
    { GDK_O, 4, 1, 1 },
    { GDK_p, 3, 1, 0 },
    { GDK_P, 3, 1, 1 },
    { GDK_Up, 2, 1, 0 },
    { GDK_sterling, 2, 1, 1 },
    { GDK_braceleft, 2, 1, 2 },
    { GDK_Down, 1, 1, 0 },
    { GDK_underscore, 1, 1, 1 },
    { GDK_braceright, 1, 1, 2 },
    { GDK_Tab, 0, 1, 0 },
    { GDK_bracketleft, 0, 1, 1 },
    { GDK_bracketright, 0, 1, 2 },
    { GDK_a, 12, 2, 0 },
    { GDK_A, 12, 2, 1 },
    { GDK_s, 11, 2, 0 },
    { GDK_S, 11, 2, 1 },
    { GDK_d, 10, 2, 0 },
    { GDK_D, 10, 2, 1 },
    { GDK_f, 9, 2, 0 },
    { GDK_F, 9, 2, 1 },
    { GDK_g, 8, 2, 0 },
    { GDK_G, 8, 2, 1 },
    { GDK_h, 7, 2, 0 },
    { GDK_H, 7, 2, 1 },
    { GDK_j, 6, 2, 0 },
    { GDK_J, 6, 2, 1 },
    { GDK_k, 5, 2, 0 },
    { GDK_K, 5, 2, 1 },
    { GDK_l, 4, 2, 0 },
    { GDK_L, 4, 2, 1 },
    { GDK_semicolon, 3, 2, 0 },
    { GDK_plus, 3, 2, 1 },
    { GDK_colon, 2, 2, 0 },
    { GDK_asterisk, 2, 2, 1 },
    { GDK_Return, 1, 2, 0 },
    { GDK_z, 12, 3, 0 },
    { GDK_Z, 12, 3, 1 },
    { GDK_x, 11, 3, 0 },
    { GDK_X, 11, 3, 1 },
    { GDK_c, 10, 3, 0 },
    { GDK_C, 10, 3, 1 },
    { GDK_v, 9, 3, 0 },
    { GDK_V, 9, 3, 1 },
    { GDK_b, 8, 3, 0 },
    { GDK_B, 8, 3, 1 },
    { GDK_n, 7, 3, 0 },
    { GDK_N, 7, 3, 1 },
    { GDK_m, 6, 3, 0 },
    { GDK_M, 6, 3, 1 },
    { GDK_comma, 5, 3, 0 },
    { GDK_less, 5, 3, 1 },
    { GDK_period, 4, 3, 0 },
    { GDK_greater, 4, 3, 1 },
    { GDK_slash, 3, 3, 0 },
    { GDK_question, 3, 3, 1 },
    { GDK_BackSpace, 1, 3, 0 },
    { GDK_space, 0, 3, 0 },
    { -1, -1, -1, -1 }
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

  object_class->dispose = electron_widget_dispose;

  widget_class->realize = electron_widget_realize;
  widget_class->expose_event = electron_widget_expose;
  widget_class->size_request = electron_widget_size_request;
  widget_class->size_allocate = electron_widget_size_allocate;
  widget_class->key_press_event = electron_widget_key_event;
  widget_class->key_release_event = electron_widget_key_event;
  widget_class->button_press_event = electron_widget_button_press;
}

static void
electron_widget_init (ElectronWidget *ewidget)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ewidget), GTK_CAN_FOCUS);

  ewidget->shift_state = 0;
  ewidget->control_state = 0;
  ewidget->alt_state = 0;
  ewidget->key_override = -1;
}

GtkWidget *
electron_widget_new ()
{
  return g_object_new (TYPE_ELECTRON_WIDGET, NULL);
}

GtkWidget *
electron_widget_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret = g_object_new (TYPE_ELECTRON_WIDGET, NULL);

  electron_widget_set_electron (ELECTRON_WIDGET (ret), electron);

  return ret;
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
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes,
				   GDK_WA_X | GDK_WA_Y);

  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gdk_window_set_user_data (widget->window, widget);

  gdk_window_set_background (widget->window, &widget->style->bg[GTK_WIDGET_STATE (widget)]);
}

static void
electron_widget_dispose (GObject *obj)
{
  ElectronWidget *ewidget;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (obj));

  ewidget = ELECTRON_WIDGET (obj);

  electron_widget_set_electron (ewidget, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
electron_widget_paint_video (ElectronWidget *ewidget)
{
  GtkWidget *widget = GTK_WIDGET (ewidget);

  gdk_draw_indexed_image (GDK_DRAWABLE (widget->window),
			  widget->style->fg_gc[widget->state],
			  ewidget->xpos, ewidget->ypos, VIDEO_WIDTH, VIDEO_HEIGHT,
			  GDK_RGB_DITHER_NONE,
			  ewidget->electron->data->video.screen_memory,
			  VIDEO_SCREEN_PITCH,
			  &electron_widget_color_map);
}

static gboolean
electron_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
  ElectronWidget *ewidget;

  g_return_val_if_fail (IS_ELECTRON_WIDGET (widget), FALSE);

  ewidget = ELECTRON_WIDGET (widget);

  /* If we don't have an electron object to display then just draw the background */
  if (ewidget->electron == NULL)
    gdk_window_clear (widget->window);
  else
  {
    /* Clear the area around the display */
    if (ewidget->xpos > 0)
      gdk_window_clear_area (widget->window,
			     0, 0, ewidget->xpos, widget->allocation.height);
    if (ewidget->xpos + VIDEO_WIDTH < widget->allocation.width)
      gdk_window_clear_area (widget->window,
			     ewidget->xpos + VIDEO_WIDTH, 0, widget->allocation.width - ewidget->xpos - VIDEO_WIDTH,
			     widget->allocation.height);
    if (ewidget->ypos > 0)
      gdk_window_clear_area (widget->window,
			     ewidget->xpos, 0, VIDEO_WIDTH, ewidget->ypos);
    if (ewidget->ypos + VIDEO_HEIGHT < widget->allocation.height)
      gdk_window_clear_area (widget->window,
			     ewidget->xpos, ewidget->ypos + VIDEO_HEIGHT, VIDEO_WIDTH,
			     widget->allocation.height - ewidget->ypos - VIDEO_HEIGHT);

    electron_widget_paint_video (ewidget);
  }

  return FALSE;
}

static gboolean
electron_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
  gtk_widget_grab_focus (widget);

  return TRUE;
}

static void
electron_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  requisition->width = VIDEO_WIDTH;
  requisition->height = VIDEO_HEIGHT;
}

static void
electron_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  ElectronWidget *ewidget;

  g_return_if_fail (IS_ELECTRON_WIDGET (widget));

  ewidget = ELECTRON_WIDGET (widget);

  if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  /* Centre the display on the widget */
  ewidget->xpos = widget->allocation.width / 2 - VIDEO_WIDTH / 2;
  ewidget->ypos = widget->allocation.height / 2 - VIDEO_HEIGHT / 2;
}

static gboolean
electron_widget_key_event (GtkWidget *widget, GdkEventKey *event)
{
  ElectronWidget *ewidget;
  int i;
  gboolean ret = TRUE;

  g_return_val_if_fail (IS_ELECTRON_WIDGET (widget), TRUE);

  ewidget = ELECTRON_WIDGET (widget);

  if (event->keyval == GDK_Shift_L)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->shift_state |= 1;
    else
      ewidget->shift_state &= ~1;
  }
  else if (event->keyval == GDK_Shift_R)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->shift_state |= 2;
    else
      ewidget->shift_state &= ~2;
  }
  else if (event->keyval == GDK_Control_L)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->control_state |= 1;
    else
      ewidget->control_state &= ~1;
  }
  else if (event->keyval == GDK_Control_R)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->control_state |= 2;
    else
      ewidget->control_state &= ~2;
  }
  else if (event->keyval == GDK_Alt_L)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->alt_state |= 1;
    else
      ewidget->alt_state &= ~1;
  }
  else if (event->keyval == GDK_Alt_R)
  {
    if (event->type == GDK_KEY_PRESS)
      ewidget->alt_state |= 2;
    else
      ewidget->alt_state &= ~2;
  }
  else if (event->type == GDK_KEY_RELEASE)
  {
    /* The released key may have a different keyval if the shift key
       state has changed since the key was pressed so we should
       compare by the hardware keycode instead */
    if (ewidget->key_override != -1 && ewidget->override_keycode == event->hardware_keycode)
    {
      if (ewidget->electron)
	electron_manager_release_key (ewidget->electron,
				      electron_widget_keymap[ewidget->key_override].line,
				      electron_widget_keymap[ewidget->key_override].bit);
      ewidget->key_override = -1;
    }
    else
      /* If it wasn't the key we are waiting for then pass it on */
      ret = FALSE;
  }
  else
  {
    for (i = 0; electron_widget_keymap[i].keysym != -1; i++)
      if (electron_widget_keymap[i].keysym == event->keyval)
      {
	if (ewidget->electron)
	{
	  if (ewidget->key_override != i && ewidget->key_override != -1)
	    electron_manager_release_key (ewidget->electron,
					  electron_widget_keymap[ewidget->key_override].line,
					  electron_widget_keymap[ewidget->key_override].bit);
	  electron_manager_press_key (ewidget->electron,
				      electron_widget_keymap[i].line,
				      electron_widget_keymap[i].bit);
	}

	ewidget->key_override = i;
	ewidget->override_keycode = event->hardware_keycode;

	break;
      }

    /* If we don't understand the key event then let it propagate further */
    if (electron_widget_keymap[i].keysym == -1)
      ret = FALSE;
  }

  if (ewidget->electron)
  {
    if (ewidget->key_override != -1)
    {
      if (electron_widget_keymap[ewidget->key_override].modifiers & 4)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_FUNC_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_FUNC_BIT);
      if (electron_widget_keymap[ewidget->key_override].modifiers & 2)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_CONTROL_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_CONTROL_BIT);
      if (electron_widget_keymap[ewidget->key_override].modifiers & 1)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_SHIFT_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_SHIFT_BIT);
    }
    else
    {
      if (ewidget->control_state)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_CONTROL_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_CONTROL_BIT);
      if (ewidget->shift_state)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_SHIFT_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_SHIFT_BIT);
      if (ewidget->alt_state)
	electron_manager_press_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_FUNC_BIT);
      else
	electron_manager_release_key (ewidget->electron, ELECTRON_MODIFIERS_LINE, ELECTRON_FUNC_BIT);
    }
  }
  
  return ret;
}

void
electron_widget_set_electron (ElectronWidget *ewidget, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (ewidget != NULL);
  g_return_if_fail (IS_ELECTRON_WIDGET (ewidget));

  if ((oldelectron = ewidget->electron))
  {
    g_signal_handler_disconnect (oldelectron, ewidget->frame_end_handler);

    ewidget->electron = NULL;

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));
    
    g_object_ref (electron);

    ewidget->electron = electron;

    ewidget->frame_end_handler
      = g_signal_connect (electron, "frame-end",
			  G_CALLBACK (electron_widget_on_frame_end), ewidget);
  }
}

static void
electron_widget_on_frame_end (ElectronManager *electron, gpointer user_data)
{
  ElectronWidget *ewidget;

  g_return_if_fail (IS_ELECTRON_WIDGET (user_data));
  ewidget = ELECTRON_WIDGET (user_data);
  g_return_if_fail (ewidget->electron == electron);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (ewidget)))
    electron_widget_paint_video (ewidget);
}
