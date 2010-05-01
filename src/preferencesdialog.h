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
typedef struct _PreferencesDialogPrivate PreferencesDialogPrivate;

struct _PreferencesDialog
{
  GtkDialog parent_object;

  PreferencesDialogPrivate *priv;
};

struct _PreferencesDialogClass
{
  GtkDialogClass parent_class;
};

GType preferences_dialog_get_type ();
void preferences_dialog_show (GtkWindow *parent);

#endif /* _PREFERENCES_DIALOG_H */
