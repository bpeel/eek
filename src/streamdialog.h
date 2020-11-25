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

#ifndef _STREAM_DIALOG_H
#define _STREAM_DIALOG_H

#include <gtk/gtk.h>
#include "electronmanager.h"

#define TYPE_STREAM_DIALOG (stream_dialog_get_type ())
#define STREAM_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST \
                            ((obj), TYPE_STREAM_DIALOG, StreamDialog))
#define IS_STREAM_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE \
                               ((obj), TYPE_STREAM_DIALOG))

typedef struct _StreamDialog StreamDialog;
typedef struct _StreamDialogClass StreamDialogClass;

GType stream_dialog_get_type ();
GtkWidget *stream_dialog_new ();
GtkWidget *stream_dialog_new_with_electron (ElectronManager *electron);
void stream_dialog_set_electron (StreamDialog *streamdialog,
                                 ElectronManager *electron);

#endif /* _STREAM_DIALOG_H */
