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
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <string.h>

#include "disdialog.h"
#include "hexspinbutton.h"
#include "electronmanager.h"
#include "disassemble.h"
#include "intl.h"

typedef struct _DisDialogAction DisDialogAction;

/* Replacement for GtkActionEntry because there doesn't seem to be a
   nice way to set the short label in it */
struct _DisDialogAction
{
  const gchar *name, *stock_id, *label, *short_label, *accelerator, *tooltip;
  gboolean toggle;
  GCallback callback;
};

static void dis_dialog_class_init (DisDialogClass *klass);
static void dis_dialog_init (DisDialog *disdialog);
static void dis_dialog_dispose (GObject *obj);
static void dis_dialog_on_apply (DisDialog *disdialog);

static gpointer parent_class;

GType
dis_dialog_get_type ()
{
  static GType dis_dialog_type = 0;

  if (!dis_dialog_type)
  {
    static const GTypeInfo dis_dialog_info =
      {
        sizeof (DisDialogClass),
        NULL, NULL,
        (GClassInitFunc) dis_dialog_class_init,
        NULL, NULL,

        sizeof (DisDialog),
        0,
        (GInstanceInitFunc) dis_dialog_init,
        NULL
      };

    dis_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
                                              "DisDialog",
                                              &dis_dialog_info, 0);
  }

  return dis_dialog_type;
}

static void
dis_dialog_class_init (DisDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = dis_dialog_dispose;
}

static void
dis_dialog_init (DisDialog *disdialog)
{
  GtkWidget *table, *label, *address_spin, *lines_spin, *text_view;
  GtkWidget *scrolled_win, *apply_button;

  disdialog->electron = NULL;

  gtk_window_set_title (GTK_WINDOW (disdialog), _("Disassembler"));
  gtk_dialog_add_button (GTK_DIALOG (disdialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  disdialog->address_adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 65535.0,
                                                               1.0, 16.0, 16.0));
  g_object_ref_sink (disdialog->address_adj);
  disdialog->lines_adj = GTK_ADJUSTMENT (gtk_adjustment_new (64.0, 0.0, 65535.0,
                                                             1.0, 16.0, 16.0));
  g_object_ref_sink (disdialog->lines_adj);

  table = gtk_table_new (2, 5, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);

  label = gtk_label_new_with_mnemonic (_("A_ddress:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);

  address_spin = hex_spin_button_new ();
  g_object_set (address_spin, "numeric", TRUE, "adjustment", disdialog->address_adj, "hex", TRUE,
                "digits", 4, NULL);
  g_signal_connect_object (G_OBJECT (address_spin), "activate",
                           G_CALLBACK (dis_dialog_on_apply),
                           disdialog, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), address_spin);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);

  gtk_widget_show (address_spin);
  gtk_table_attach (GTK_TABLE (table), address_spin, 1, 2, 0, 1,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  label = gtk_label_new_with_mnemonic (_("_Lines:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);

  lines_spin = gtk_spin_button_new (disdialog->lines_adj, 1.0, 0);
  g_signal_connect_object (G_OBJECT (lines_spin), "activate",
                           G_CALLBACK (dis_dialog_on_apply),
                           disdialog, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (lines_spin), TRUE);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), lines_spin);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);

  gtk_widget_show (lines_spin);
  gtk_table_attach (GTK_TABLE (table), lines_spin, 3, 4, 0, 1,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  apply_button = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  g_signal_connect_object (G_OBJECT (apply_button), "clicked",
                           G_CALLBACK (dis_dialog_on_apply),
                           disdialog, G_CONNECT_SWAPPED);
  gtk_widget_show (apply_button);
  gtk_table_attach (GTK_TABLE (table), apply_button, 4, 5, 0, 1,
                    GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  disdialog->text_buffer = gtk_text_buffer_new (NULL);
  g_object_ref_sink (disdialog->text_buffer);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  text_view = gtk_text_view_new_with_buffer (disdialog->text_buffer);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_widget_show (text_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), text_view);
  gtk_widget_show (scrolled_win);
  gtk_table_attach_defaults (GTK_TABLE (table), scrolled_win, 0, 5, 1, 2);

  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (disdialog)->vbox), table, TRUE, TRUE, 0);
}

GtkWidget *
dis_dialog_new ()
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_DIS_DIALOG, NULL);

  return ret;
}

GtkWidget *
dis_dialog_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_DIS_DIALOG, NULL);

  dis_dialog_set_electron (DIS_DIALOG (ret), electron);

  return ret;
}

void
dis_dialog_set_electron (DisDialog *disdialog, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (disdialog != NULL);
  g_return_if_fail (IS_DIS_DIALOG (disdialog));

  if ((oldelectron = disdialog->electron))
  {
    disdialog->electron = NULL;

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));

    g_object_ref (electron);

    disdialog->electron = electron;
  }
}

static void
dis_dialog_on_apply (DisDialog *disdialog)
{
  g_return_if_fail (IS_DIS_DIALOG (disdialog));

  if (disdialog->electron && disdialog->address_adj && disdialog->lines_adj
      && disdialog->text_buffer)
  {
    guint16 address = (guint16) gtk_adjustment_get_value (disdialog->address_adj);
    int lines = (int) gtk_adjustment_get_value (disdialog->lines_adj);
    int got_bytes = 0, num_bytes, i;
    GString *strbuf = g_string_new ("");
    guint8 bytes[DISASSEMBLE_MAX_BYTES];
    char mnemonic[DISASSEMBLE_MAX_MNEMONIC + 1];
    char operands[DISASSEMBLE_MAX_OPERANDS + 1];
    GtkTextIter start, end;
    GtkTextTag *tag;

    while (lines-- > 0)
    {
      /* Fill the buffer so that we have at least DISASSEMBLE_MAX_BYTES bytes */
      while (got_bytes < DISASSEMBLE_MAX_BYTES)
      {
        bytes[got_bytes] = electron_read_from_location (disdialog->electron->data,
                                                        address + got_bytes);
        got_bytes++;
      }

      /* Disassemble the bytes */
      num_bytes = disassemble_instruction (address, bytes, mnemonic, operands);

      /* Add the address */
      g_string_append_printf (strbuf, "%04X ", (int) address);

      /* Add the bytes */
      for (i = 0; i < num_bytes; i++)
        g_string_append_printf (strbuf, "%02X ", bytes[i]);
      /* Pad so that the mnemonic will align */
      for (i = num_bytes; i < DISASSEMBLE_MAX_BYTES; i++)
        g_string_append (strbuf, "   ");

      /* Add the mnemonic */
      g_string_append (strbuf, mnemonic);
      /* Pad */
      for (i = strlen (mnemonic); i < DISASSEMBLE_MAX_MNEMONIC + 1; i++)
        g_string_append_c (strbuf, ' ');

      /* Add the operands */
      g_string_append (strbuf, operands);

      /* Terminate the line */
      g_string_append_c (strbuf, '\n');

      /* Move the unused bytes to the beginning */
      memmove (bytes, bytes + num_bytes, got_bytes = DISASSEMBLE_MAX_BYTES - num_bytes);
      /* Step to the next address */
      address += num_bytes;
    }

    /* Replace the text in the buffer */
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (disdialog->text_buffer), strbuf->str, strbuf->len);

    /* Get iterators to covert the entire range */
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (disdialog->text_buffer), &start);
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (disdialog->text_buffer), &end);

    /* Remove all of the old tags */
    gtk_text_buffer_remove_all_tags (GTK_TEXT_BUFFER (disdialog->text_buffer), &start, &end);

    /* Make all of the text monospaced */
    tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (disdialog->text_buffer),
                                      NULL, "family", "monospace", NULL);
    gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (disdialog->text_buffer),
                               tag, &start, &end);

    g_string_free (strbuf, TRUE);
  }
}

static void
dis_dialog_dispose (GObject *obj)
{
  DisDialog *disdialog;

  g_return_if_fail (IS_DIS_DIALOG (obj));

  disdialog = DIS_DIALOG (obj);

  dis_dialog_set_electron (disdialog, NULL);

  if (disdialog->address_adj)
  {
    g_object_unref (disdialog->address_adj);
    disdialog->address_adj = NULL;
  }

  if (disdialog->lines_adj)
  {
    g_object_unref (disdialog->lines_adj);
    disdialog->lines_adj = NULL;
  }

  if (disdialog->text_buffer)
  {
    g_object_unref (disdialog->text_buffer);
    disdialog->text_buffer = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}
