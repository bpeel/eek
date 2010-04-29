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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkdialog.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>

#include "electronmanager.h"
#include "hexspinbutton.h"
#include "intl.h"

static const struct
{
  const char *name;
  int break_type;
} breakpoint_edit_dialog_break_types[] =
  {
    { N_("Break at address"), CPU_BREAK_ADDR },
    { N_("Break on read from address"), CPU_BREAK_READ },
    { N_("Break on write to address"), CPU_BREAK_WRITE }
  };

#define BREAKPOINT_EDIT_DIALOG_BREAK_TYPE_COUNT \
 (sizeof (breakpoint_edit_dialog_break_types) / sizeof (breakpoint_edit_dialog_break_types[0]))

static void
breakpoint_edit_dialog_set_sensitive (GtkWidget *widget, gpointer data)
{
  if (widget != GTK_WIDGET (data))
    gtk_widget_set_sensitive (widget, TRUE);
}

static void
breakpoint_edit_dialog_set_insensitive (GtkWidget *widget, gpointer data)
{
  if (widget != GTK_WIDGET (data))
    gtk_widget_set_sensitive (widget, FALSE);
}

static void
breakpoint_edit_dialog_update_sensitivity (GtkCheckButton *enabled_checkbox, GtkWidget *table)
{
  g_return_if_fail (GTK_IS_CHECK_BUTTON (enabled_checkbox));
  g_return_if_fail (GTK_IS_WIDGET (table));

  gtk_container_foreach (GTK_CONTAINER (table),
			 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (enabled_checkbox))
			 ? breakpoint_edit_dialog_set_sensitive
			 : breakpoint_edit_dialog_set_insensitive,
			 enabled_checkbox);
}

void
breakpoint_edit_dialog_run (GtkWindow *parent, ElectronManager *electron)
{
  int i;
  gint response;
  GtkWidget *dialog, *label, *enabled_checkbox, *type_combobox, *table;
  GtkAdjustment *address_adj;

  g_return_if_fail (GTK_IS_WINDOW (parent));
  g_return_if_fail (IS_ELECTRON_MANAGER (electron));

  /* Reference the electron manager so that it won't disappear while
     the dialog is running */
  g_object_ref (electron);

  dialog = gtk_dialog_new_with_buttons (_("Edit breakpoint"),
					parent,
					GTK_DIALOG_MODAL,
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					NULL);

  /* Layout the controls in a table */
  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);

  /* Create a checkbox to enable or disable the breakpoint */
  enabled_checkbox = gtk_check_button_new_with_mnemonic (_("_Enable breakpoint"));
  /* Reference it so we can check whether it was enabled after the
     dialog is destroyed */
  g_object_ref_sink (enabled_checkbox);
  g_signal_connect (G_OBJECT (enabled_checkbox), "toggled",
		    G_CALLBACK (breakpoint_edit_dialog_update_sensitivity),
		    table);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enabled_checkbox),
				electron->data->cpu.break_type != CPU_BREAK_NONE);
  gtk_widget_show (enabled_checkbox);
  gtk_table_attach_defaults (GTK_TABLE (table), enabled_checkbox, 0, 2, 0, 1);

  label = gtk_label_new_with_mnemonic (_("_Address:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  /* Create an adjustment for the breakpoint address */
  address_adj = GTK_ADJUSTMENT (gtk_adjustment_new
				(electron->data->cpu.break_type == CPU_BREAK_NONE ? 0.0
				 : (gdouble) electron->data->cpu.break_address,
				 0.0, 65535.0, 1.0, 16.0, 16.0));
  /* Reference it so that it won't go away after the dialog is destroyed */
  g_object_ref_sink (address_adj);

  /* Create a spin control for the address */
  GtkWidget *hexspin = hex_spin_button_new ();
  g_object_set (hexspin, "numeric", TRUE, "adjustment", address_adj, "hex", TRUE,
		"digits", 4, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), hexspin);
  gtk_widget_show (hexspin);
  gtk_table_attach_defaults (GTK_TABLE (table), hexspin, 1, 2, 1, 2);

  label = gtk_label_new_with_mnemonic (_("Break _type:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

  /* Create a combo box for the breakpoint type */
  type_combobox = gtk_combo_box_new_text ();
  /* Reference it so we can check the type after the dialog is destroyed */
  g_object_ref_sink (type_combobox);
  /* Add all of the break point type strings */
  for (i = 0; i < BREAKPOINT_EDIT_DIALOG_BREAK_TYPE_COUNT; i++)
    gtk_combo_box_append_text (GTK_COMBO_BOX (type_combobox),
			       breakpoint_edit_dialog_break_types[i].name);
  /* Select the current breakpoint type */
  for (i = 0; i < BREAKPOINT_EDIT_DIALOG_BREAK_TYPE_COUNT; i++)
    if (breakpoint_edit_dialog_break_types[i].break_type
	== electron->data->cpu.break_type)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (type_combobox), i);
      break;
    }
  /* If no type matches then just assume the first type */
  if (i >= BREAKPOINT_EDIT_DIALOG_BREAK_TYPE_COUNT)
    gtk_combo_box_set_active (GTK_COMBO_BOX (type_combobox), 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), type_combobox);
  gtk_widget_show (type_combobox);
  gtk_table_attach_defaults (GTK_TABLE (table), type_combobox, 1, 2, 2, 3);

  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table,
		      FALSE, FALSE, 0);

  breakpoint_edit_dialog_update_sensitivity (GTK_CHECK_BUTTON (enabled_checkbox), table);

  /* Make the OK button the default button */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
  {
    int break_type;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (enabled_checkbox)))
      break_type = breakpoint_edit_dialog_break_types
	[gtk_combo_box_get_active (GTK_COMBO_BOX (type_combobox))].break_type;
    else
      break_type = CPU_BREAK_NONE;

    cpu_set_break (&electron->data->cpu, break_type,
		   (guint16) gtk_adjustment_get_value (address_adj));
  }

  g_object_unref (type_combobox);
  g_object_unref (address_adj);
  g_object_unref (enabled_checkbox);
  g_object_unref (electron);
}
