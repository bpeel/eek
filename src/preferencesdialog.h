#ifndef _PREFERENCES_DIALOG_H
#define _PREFERENCES_DIALOG_H

#include <gtk/gtkdialog.h>

#define TYPE_PREFERENCES_DIALOG (preferences_dialog_get_type ())
#define PREFERENCES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                 TYPE_PREFERENCES_DIALOG, PreferencesDialog))
#define PREFERENCES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST \
                                         ((klass), TYPE_PREFERENCES_DIALOG, PreferencesDialogClass))
#define IS_PREFERENCES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                    TYPE_PREFERENCES_DIALOG))
#define IS_PREFERENCES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                            TYPE_PREFERENCES_DIALOG))
#define PREFERENCES_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           TYPE_PREFERENCES_DIALOG, PreferencesDialogClass))

typedef struct _PreferencesDialog PreferencesDialog;
typedef struct _PreferencesDialogClass PreferencesDialogClass;

struct _PreferencesDialog
{
  GtkDialog parent_object;
};

struct _PreferencesDialogClass
{
  GtkDialogClass parent_class;
};

GType preferences_dialog_get_type ();
void preferences_dialog_show (GtkWindow *parent);

#endif /* _PREFERENCES_DIALOG_H */
