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

#include <glib.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkadjustment.h>
#include <pango/pango-attributes.h>

#include "debugger.h"
#include "electron.h"
#include "electronmanager.h"
#include "memdispcombo.h"
#include "hexspinbutton.h"

static void debugger_class_init (DebuggerClass *klass);
static void debugger_init (Debugger *widget);
static void debugger_dispose (GObject *obj);
static void debugger_register_notify (gpointer data, GObject *obj);
static void debugger_mem_disp_notify (gpointer data, GObject *obj);
static void debugger_dis_model_notify (gpointer data, GObject *obj);
static void debugger_update (Debugger *debugger);

static gpointer parent_class;

static const char *const debugger_register_names[DEBUGGER_REGISTER_COUNT] =
  { "A", "X", "Y", "S", "PC", "P" };
static const char debugger_flag_names[] = "CZIDB-VN";

static const gint debugger_disassembler_columns[] =
  { DIS_MODEL_COL_ADDRESS, DIS_MODEL_COL_BYTES, DIS_MODEL_COL_MNEMONIC,
    DIS_MODEL_COL_OPERANDS, -1 };

GType
debugger_get_type ()
{
  static GType debugger_type = 0;

  if (!debugger_type)
  {
    static const GTypeInfo debugger_info =
      {
        sizeof (DebuggerClass),
        NULL, NULL,
        (GClassInitFunc) debugger_class_init,
        NULL, NULL,

        sizeof (Debugger),
        0,
        (GInstanceInitFunc) debugger_init,
        NULL
      };

    debugger_type = g_type_register_static (GTK_TYPE_TABLE,
                                            "Debugger",
                                            &debugger_info, 0);
  }

  return debugger_type;
}

static void
debugger_class_init (DebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = debugger_dispose;
}

static void
debugger_init (Debugger *debugger)
{
  int i;
  GtkWidget *label, *scrolled_win, *dis_table, *separator, *hexspin;
  GtkAdjustment *addr_adj;
  PangoAttrList *attr_list;
  PangoAttribute *attr;

  /* Make an attribute list to set all of the labels to monospaced */
  attr_list = pango_attr_list_new ();
  attr = pango_attr_family_new ("monospace");
  attr->start_index = 0;
  attr->end_index = 1000;
  pango_attr_list_insert (attr_list, attr);

  gtk_table_resize (GTK_TABLE (debugger), DEBUGGER_REGISTER_COUNT + 5, 2);
  gtk_table_set_homogeneous (GTK_TABLE (debugger), FALSE);

  /* Add labels for each of the registers */
  for (i = 0; i < DEBUGGER_REGISTER_COUNT; i++)
  {
    label = gtk_label_new (debugger_register_names[i]);
    gtk_label_set_attributes (GTK_LABEL (label), attr_list);
    gtk_table_attach (GTK_TABLE (debugger), label, 0, 1, i, i + 1,
                      GTK_FILL, 0, 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
    gtk_widget_show (label);

    label = debugger->register_widgets[i] = gtk_label_new ("");
    gtk_label_set_attributes (GTK_LABEL (label), attr_list);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
    g_object_weak_ref (G_OBJECT (label), debugger_register_notify, debugger);
    gtk_table_attach (GTK_TABLE (debugger), label, 1, 2, i, i + 1,
                      GTK_FILL, 0, 8, 0);
    gtk_widget_show (label);
  }

  pango_attr_list_unref (attr_list);

  separator = gtk_hseparator_new ();
  gtk_widget_show (separator);
  gtk_table_attach (GTK_TABLE (debugger), separator, 1, 2, DEBUGGER_REGISTER_COUNT,
                    DEBUGGER_REGISTER_COUNT + 1, 0, 0, 0, 0);

  /* Add a table to show the disassembly */
  debugger->dis_model = dis_model_new ();
  g_object_weak_ref (G_OBJECT (debugger->dis_model), debugger_dis_model_notify, debugger);
  dis_table = gtk_tree_view_new_with_model (GTK_TREE_MODEL (debugger->dis_model));
  g_object_unref (debugger->dis_model);
  for (i = 0; debugger_disassembler_columns[i] != -1; i++)
  {
    GtkCellRenderer *cell_renderer = gtk_cell_renderer_text_new ();
    g_object_set (cell_renderer, "weight-set", TRUE, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dis_table), -1, "",
                                                 cell_renderer,
                                                 "text", debugger_disassembler_columns[i],
                                                 "weight", DIS_MODEL_COL_BOLD_IF_CURRENT,
                                                 NULL);
  }
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dis_table), FALSE);
  gtk_widget_show (dis_table);
  gtk_table_attach (GTK_TABLE (debugger), dis_table, 0, 2,
                    DEBUGGER_REGISTER_COUNT + 1, DEBUGGER_REGISTER_COUNT + 2,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  separator = gtk_hseparator_new ();
  gtk_widget_show (separator);
  gtk_table_attach (GTK_TABLE (debugger), separator, 1, 2, DEBUGGER_REGISTER_COUNT + 2,
                    DEBUGGER_REGISTER_COUNT + 3, 0, 0, 0, 0);

  /* Add a hex scroll button to display the address for the memory
     display widget */
  addr_adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 65535.0, 1.0, 10.0, 10.0));
  hexspin = hex_spin_button_new ();
  g_object_set (hexspin, "numeric", TRUE, "adjustment", addr_adj, "hex", TRUE,
                "digits", 4, NULL);
  gtk_widget_show (hexspin);
  gtk_table_attach (GTK_TABLE (debugger), hexspin, 0, 2,
                    DEBUGGER_REGISTER_COUNT + 3, DEBUGGER_REGISTER_COUNT + 4,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  /* Add a memory display widget */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  debugger->mem_disp = mem_disp_combo_new ();
  g_object_weak_ref (G_OBJECT (debugger->mem_disp), debugger_mem_disp_notify, debugger);
  mem_disp_combo_set_cursor_adjustment (MEM_DISP_COMBO (DEBUGGER (debugger)->mem_disp), addr_adj);
  gtk_widget_show (debugger->mem_disp);
  gtk_container_add (GTK_CONTAINER (scrolled_win), debugger->mem_disp);
  gtk_widget_show (scrolled_win);
  gtk_table_attach (GTK_TABLE (debugger), scrolled_win, 0, 2,
                    DEBUGGER_REGISTER_COUNT + 4, DEBUGGER_REGISTER_COUNT + 5,
                    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
}

GtkWidget *
debugger_new ()
{
  return g_object_new (TYPE_DEBUGGER, NULL);
}

GtkWidget *
debugger_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret = g_object_new (TYPE_DEBUGGER, NULL);

  debugger_set_electron (DEBUGGER (ret), electron);

  return ret;
}

static void
debugger_dispose (GObject *obj)
{
  Debugger *debugger;
  int i;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (IS_DEBUGGER (obj));

  debugger = DEBUGGER (obj);

  for (i = 0; i < DEBUGGER_REGISTER_COUNT; i++)
    if (debugger->register_widgets[i])
    {
      g_object_weak_unref (G_OBJECT (debugger->register_widgets[i]),
                           debugger_register_notify, debugger);
      debugger->register_widgets[i] = NULL;
    }

  if (debugger->mem_disp)
  {
    g_object_weak_unref (G_OBJECT (debugger->mem_disp),
                         debugger_mem_disp_notify, debugger);
    debugger->mem_disp = NULL;
  }

  if (debugger->dis_model)
  {
    g_object_weak_unref (G_OBJECT (debugger->dis_model),
                         debugger_dis_model_notify, debugger);
    debugger->dis_model = NULL;
  }

  debugger_set_electron (debugger, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

void
debugger_set_electron (Debugger *debugger, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (debugger != NULL);
  g_return_if_fail (IS_DEBUGGER (debugger));

  if ((oldelectron = debugger->electron))
  {
    g_signal_handler_disconnect (oldelectron, debugger->started);
    g_signal_handler_disconnect (oldelectron, debugger->stopped);

    debugger->electron = NULL;

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));

    g_object_ref (electron);

    debugger->electron = electron;

    debugger->started
      = g_signal_connect_swapped (electron, "started",
                                  G_CALLBACK (debugger_update), debugger);
    debugger->stopped
      = g_signal_connect_swapped (electron, "stopped",
                                  G_CALLBACK (debugger_update), debugger);
  }

  if (debugger->mem_disp)
    mem_disp_combo_set_electron (MEM_DISP_COMBO (debugger->mem_disp), electron);

  if (debugger->dis_model)
    dis_model_set_electron (debugger->dis_model, electron);

  debugger_update (debugger);
}

static void
debugger_register_notify (gpointer data, GObject *obj)
{
  int i;
  Debugger *debugger;

  g_return_if_fail (IS_DEBUGGER (data));

  debugger = DEBUGGER (data);

  for (i = 0; i < DEBUGGER_REGISTER_COUNT; i++)
    if (debugger->register_widgets[i] == GTK_WIDGET (obj))
    {
      debugger->register_widgets[i] = NULL;
      return;
    }

  g_assert_not_reached ();
}

static void
debugger_mem_disp_notify (gpointer data, GObject *obj)
{
  Debugger *debugger;

  g_return_if_fail (IS_DEBUGGER (data));

  debugger = DEBUGGER (data);

  g_return_if_fail (obj == G_OBJECT (debugger->mem_disp));

  debugger->mem_disp = NULL;
}

static void
debugger_dis_model_notify (gpointer data, GObject *obj)
{
  Debugger *debugger;

  g_return_if_fail (IS_DEBUGGER (data));

  debugger = DEBUGGER (data);

  g_return_if_fail (obj == G_OBJECT (debugger->dis_model));

  debugger->dis_model = NULL;
}

static void
debugger_update (Debugger *debugger)
{
  int i;

  g_return_if_fail (IS_DEBUGGER (debugger));

  if (debugger->electron == NULL || electron_manager_is_running (debugger->electron))
  {
    for (i = 0; i < DEBUGGER_REGISTER_COUNT; i++)
      if (debugger->register_widgets[i])
        gtk_label_set_text (GTK_LABEL (debugger->register_widgets[i]), "");
  }
  else
  {
    char txt_buf[9], *p;
    Cpu *cpu = &debugger->electron->data->cpu;

    g_snprintf (txt_buf, sizeof (txt_buf), "%02X", cpu->a);
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_A]), txt_buf);
    g_snprintf (txt_buf, sizeof (txt_buf), "%02X", cpu->x);
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_X]), txt_buf);
    g_snprintf (txt_buf, sizeof (txt_buf), "%02X", cpu->y);
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_Y]), txt_buf);
    g_snprintf (txt_buf, sizeof (txt_buf), "1%02X", cpu->s);
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_S]), txt_buf);
    g_snprintf (txt_buf, sizeof (txt_buf), "%04X", cpu->pc);
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_PC]), txt_buf);

    p = txt_buf;
    for (i = 7; i >= 0; i--)
      *(p++) = (cpu->p & (1 << i)) ? debugger_flag_names[i] : '*';
    *p = '\0';
    gtk_label_set_text (GTK_LABEL (debugger->register_widgets[DEBUGGER_REGISTER_P]), txt_buf);
  }
}
