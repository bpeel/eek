#include "config.h"

#include <gtk/gtkspinbutton.h>
#include <gtk/gtkeditable.h>
#include <string.h>

#include "hexspinbutton.h"
#include "intl.h"

static void hex_spin_button_class_init (HexSpinButtonClass *klass);
static void hex_spin_button_init (HexSpinButton *widget);
static void hex_spin_button_editable_init (GtkEditableClass *iface);

static gint hex_spin_button_input (GtkSpinButton *spin_button,
				   gdouble *new_val);
static gint hex_spin_button_output (GtkSpinButton *spin_button);
static void hex_spin_button_insert_text (GtkEditable *editable,
					 const gchar *new_text,
					 gint new_text_length,
					 gint *position);

static void hex_spin_button_set_property (GObject *object,
					  guint prop_id,
					  const GValue *value,
					  GParamSpec *pspec);
static void hex_spin_button_get_property (GObject *object,
					  guint prop_id,
					  GValue *value,
					  GParamSpec *pspec);

static gpointer parent_class;

enum { PROP_0, PROP_HEX };

GType
hex_spin_button_get_type ()
{
  static GType hex_spin_button_type = 0;

  if (!hex_spin_button_type)
  {
    static const GTypeInfo hex_spin_button_info =
      {
	sizeof (HexSpinButtonClass),
	NULL, NULL,
	(GClassInitFunc) hex_spin_button_class_init,
	NULL, NULL,

	sizeof (HexSpinButton),
	0,
	(GInstanceInitFunc) hex_spin_button_init,
	NULL
      };
    static const GInterfaceInfo editable_info =
      {
	(GInterfaceInitFunc) hex_spin_button_editable_init, /* interface_init */
	NULL, /* interface_finalize */
	NULL /* interface_data */
      };

    hex_spin_button_type = g_type_register_static (GTK_TYPE_SPIN_BUTTON,
						   "HexSpinButton",
						   &hex_spin_button_info, 0);
    g_type_add_interface_static (hex_spin_button_type, GTK_TYPE_EDITABLE,
				 &editable_info);
  }

  return hex_spin_button_type;
}

static void
hex_spin_button_class_init (HexSpinButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSpinButtonClass *spin_button_class = GTK_SPIN_BUTTON_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  spin_button_class->output = hex_spin_button_output;
  spin_button_class->input = hex_spin_button_input;

  object_class->set_property = hex_spin_button_set_property;
  object_class->get_property = hex_spin_button_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
				   PROP_HEX,
				   g_param_spec_boolean ("hex",
							 _("Hex"),
							 _("Whether to display in hexadecimal"),
							 TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
hex_spin_button_editable_init (GtkEditableClass *iface)
{
  iface->insert_text = hex_spin_button_insert_text;
}

static void
hex_spin_button_init (HexSpinButton *hexspin)
{
  hexspin->hex = TRUE;
}

GtkWidget *
hex_spin_button_new ()
{
  return g_object_new (TYPE_HEX_SPIN_BUTTON, NULL);
}

GtkWidget *
hex_spin_button_new_with_range (gint min, gint max, gint step)
{
  GtkWidget *widget = hex_spin_button_new ();
  GtkObject *adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);
  g_object_set (widget, "numeric", TRUE,
		"adjustment", adj,
		"climb-rate", step, NULL);
  return widget;
}

static gboolean
hex_spin_button_convert_hex_string (const char *buf, gint *out)
{
  int len = 0;

  *out = 0;
  
  while (*buf)
  {
    int digit;

    if (*buf >= 'a' && *buf <= 'f')
      digit = *buf - 'a' + 10;
    else if (*buf >= 'A' && *buf <= 'f')
      digit = *buf - 'A' + 10;
    else if (*buf >= '0' && *buf <= '9')
      digit = *buf - '0';
    else
      return FALSE;

    if (len >= 8 || (len == 7 && (*out & 0x0f000000) >= 0x08000000))
      return FALSE;

    *out <<= 4;
    *out |= digit;
    buf++;
    len++;
  }
 
  return len != 0;
}

static gint
hex_spin_button_input (GtkSpinButton *spin_button, gdouble *new_val)
{
  if (HEX_SPIN_BUTTON (spin_button)->hex)
  {
    gint int_val;

    if (hex_spin_button_convert_hex_string (gtk_entry_get_text (GTK_ENTRY (spin_button)), &int_val))
    {
      *new_val = int_val;
      return TRUE;
    }
    else
      return GTK_INPUT_ERROR;
  }
  else
    return FALSE;
}

static gint
hex_spin_button_output (GtkSpinButton *spin_button)
{
  if (HEX_SPIN_BUTTON (spin_button)->hex)
  {
    gchar *buf = g_strdup_printf ("%x", (unsigned int) spin_button->adjustment->value);

    if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
      gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
    g_free (buf);

    return TRUE;
  }
  else
    return FALSE;
}

static void
hex_spin_button_insert_text (GtkEditable *editable,
			     const gchar *new_text,
			     gint new_text_length,
			     gint *position)
{
  /* If the control is in hex and numeric mode then only allow
     hexadecimal characters to be inserted */
  if (HEX_SPIN_BUTTON (editable)->hex
      && gtk_spin_button_get_numeric (GTK_SPIN_BUTTON (editable)))
  {
    int i;
    
    for (i = 0; i < new_text_length; i++)
      if ((new_text[i] < 'a' || new_text[i] > 'f')
	  && (new_text[i] < 'A' || new_text[i] > 'F')
	  && (new_text[i] < '0' || new_text[i] > '9'))
	return;
    /* Call the insert_text method for the parent of GtkSpinButton to
       bypass the non-digit check */
    ((GtkEditableClass *) g_type_interface_peek (g_type_class_peek_parent (parent_class),
						 GTK_TYPE_EDITABLE))
      ->insert_text (editable, new_text, new_text_length, position);
  }
  else
    ((GtkEditableClass *) g_type_interface_peek (parent_class, GTK_TYPE_EDITABLE))
      ->insert_text (editable, new_text, new_text_length, position);
}

void
hex_spin_button_set_hex (HexSpinButton *hexspin, gboolean hex)
{
  g_return_if_fail (IS_HEX_SPIN_BUTTON (hexspin));

  hex = hex ? TRUE : FALSE;

  if (hex != hexspin->hex)
  {
    hexspin->hex = hex;
    g_object_notify (G_OBJECT (hexspin), "hex");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (hexspin),
			       gtk_spin_button_get_value (GTK_SPIN_BUTTON (hexspin)));
  }
}

gboolean
hex_spin_button_get_hex (HexSpinButton *hexspin)
{
  g_return_val_if_fail (IS_HEX_SPIN_BUTTON (hexspin), FALSE);

  return hexspin->hex;
}

static void
hex_spin_button_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
  switch (prop_id)
  {
    case PROP_HEX:
      hex_spin_button_set_hex (HEX_SPIN_BUTTON (object), g_value_get_boolean (value));
      break;

    default:
      g_return_if_reached ();
  }
}

static void
hex_spin_button_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
  switch (prop_id)
  {
    case PROP_HEX:
      g_value_set_boolean (value, HEX_SPIN_BUTTON (object)->hex);
      break;

    default:
      g_return_if_reached ();
  }
}
