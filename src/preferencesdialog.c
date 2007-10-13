#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkstock.h>

#include "preferencesdialog.h"
#include "gladeutil.h"
#include "intl.h"

static void preferences_dialog_class_init (PreferencesDialogClass *klass);
static void preferences_dialog_init (PreferencesDialog *prefsdlg);
static void preferences_dialog_dispose (GObject *obj);
static void preferences_dialog_on_response (GtkDialog *dialog, gint res_id);

static gpointer parent_class;

GType
preferences_dialog_get_type ()
{
  static GType preferences_dialog_type = 0;

  if (!preferences_dialog_type)
  {
    static const GTypeInfo preferences_dialog_info =
      {
	sizeof (PreferencesDialogClass),
	NULL, NULL,
	(GClassInitFunc) preferences_dialog_class_init,
	NULL, NULL,

	sizeof (PreferencesDialog),
	0,
	(GInstanceInitFunc) preferences_dialog_init,
	NULL
      };

    preferences_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
						      "PreferencesDialog",
						      &preferences_dialog_info, 0);
  }

  return preferences_dialog_type;
}

static void
preferences_dialog_class_init (PreferencesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = preferences_dialog_dispose;
}

static void
preferences_dialog_init (PreferencesDialog *prefsdlg)
{
  GtkWidget *notebook, *error_widget;

  gtk_dialog_add_button (GTK_DIALOG (prefsdlg), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_window_set_title (GTK_WINDOW (prefsdlg), _("eek Preferences"));
  gtk_dialog_set_has_separator (GTK_DIALOG (prefsdlg), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (prefsdlg), TRUE);

  g_signal_connect (prefsdlg, "response",
		    G_CALLBACK (preferences_dialog_on_response), NULL);

  if (glade_util_get_widgets ("preferences-dialog.glade", "notebook",
			      &error_widget, "notebook", &notebook, NULL))
  {
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefsdlg)->vbox), notebook,
			TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
    gtk_widget_show (notebook);
  }
  else
  {
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefsdlg)->vbox), error_widget,
			TRUE, TRUE, 12);
    gtk_widget_show (error_widget);
  }
}

static void
preferences_dialog_dispose (GObject *obj)
{
  PreferencesDialog *prefsdlg;

  g_return_if_fail (IS_PREFERENCES_DIALOG (obj));

  prefsdlg = PREFERENCES_DIALOG (obj);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

void
preferences_dialog_show (GtkWindow *parent)
{
  static PreferencesDialog *prefsdlg = NULL;

  g_return_if_fail (GTK_IS_WINDOW (parent));
  
  /* We only ever want to display a single preferences dialog, so if
     we've already got one then just reparent it */
  if (prefsdlg == NULL)
  {
    prefsdlg = PREFERENCES_DIALOG (g_object_new (TYPE_PREFERENCES_DIALOG, NULL));
    
    g_signal_connect (prefsdlg, "destroy", G_CALLBACK (gtk_widget_destroyed), &prefsdlg);
  }

  gtk_window_set_transient_for (GTK_WINDOW (prefsdlg), parent);

  gtk_window_present (GTK_WINDOW (prefsdlg));
}

static void
preferences_dialog_on_response (GtkDialog *dialog, gint res_id)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}
