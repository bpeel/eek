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

#ifndef _DIS_DIALOG_H
#define _DIS_DIALOG_H

#include <gtk/gtkdialog.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtktextbuffer.h>
#include "electronmanager.h"

#define TYPE_DIS_DIALOG (dis_dialog_get_type ())
#define DIS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DIS_DIALOG, DisDialog))
#define DIS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_DIS_DIALOG, DisDialogClass))
#define IS_DIS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DIS_DIALOG))
#define IS_DIS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_DIS_DIALOG))
#define DIS_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_DIS_DIALOG, DisDialogClass))

typedef struct _DisDialog DisDialog;
typedef struct _DisDialogClass DisDialogClass;

struct _DisDialog
{
  GtkDialog parent_object;

  ElectronManager *electron;

  GtkAdjustment *address_adj, *lines_adj;
  GtkTextBuffer *text_buffer;
};

struct _DisDialogClass
{
  GtkDialogClass parent_class;
};

GType dis_dialog_get_type ();
GtkWidget *dis_dialog_new ();
GtkWidget *dis_dialog_new_with_electron (ElectronManager *electron);
void dis_dialog_set_electron (DisDialog *disdialog, ElectronManager *electron);

#endif /* _DIS_DIALOG_H */
