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
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gtk/gtktreemodel.h>
#include <pango/pango-font.h>
#include <string.h>

#include "dismodel.h"
#include "electron.h"
#include "electronmanager.h"
#include "disassemble.h"

static gpointer parent_class;

static void dis_model_dispose (GObject *object);
static GtkTreeModelFlags dis_model_get_flags (GtkTreeModel *tree_model);
static gint dis_model_get_n_columns (GtkTreeModel *tree_model);
static GType dis_model_get_column_type (GtkTreeModel *tree_model, gint index);
static gboolean dis_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter_r,
                                    GtkTreePath *path);
static GtkTreePath *dis_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter);
static void dis_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter,
                                 gint column, GValue *value);
static gboolean dis_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean dis_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter,
                                         GtkTreeIter *parent);
static gboolean dis_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gint dis_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean dis_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter,
                                          GtkTreeIter *parent, gint n);
static gboolean dis_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter,
                                       GtkTreeIter *child);

static void dis_model_refresh_rows (DisModel *model);
static void dis_model_on_stopped (ElectronManager *electron, DisModel *model);

static void
dis_model_class_init (DisModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = dis_model_dispose;
}

static void
dis_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = dis_model_get_flags;
  iface->get_n_columns = dis_model_get_n_columns;
  iface->get_column_type = dis_model_get_column_type;
  iface->get_iter = dis_model_get_iter;
  iface->get_path = dis_model_get_path;
  iface->get_value = dis_model_get_value;
  iface->iter_next = dis_model_iter_next;
  iface->iter_children = dis_model_iter_children;
  iface->iter_has_child = dis_model_iter_has_child;
  iface->iter_n_children = dis_model_iter_n_children;
  iface->iter_nth_child = dis_model_iter_nth_child;
  iface->iter_parent = dis_model_iter_parent;
}

static void
dis_model_init (DisModel *model)
{
  model->address = 0;
  model->iter_stamp = 0;
  model->electron = NULL;
  memset (model->rows, sizeof (DisModelRowData) * DIS_MODEL_ROW_COUNT, 0);
}

DisModel *
dis_model_new ()
{
  return g_object_new (TYPE_DIS_MODEL, NULL);
}

DisModel *
dis_model_new_with_electron (ElectronManager *electron)
{
  DisModel *model = dis_model_new ();
  dis_model_set_electron (model, electron);
  return model;
}

GType
dis_model_get_type ()
{
  static GType dis_model_type = 0;

  if (dis_model_type == 0)
  {
    static const GTypeInfo dis_model_info =
      {
        sizeof (DisModelClass), /* size of class structure */
        NULL, /* base_init */
        NULL, /* base_finalize */
        (GClassInitFunc) dis_model_class_init, /* class struct init func */
        NULL, /* class_finalize */
        NULL, /* class_data */
        sizeof (DisModel), /* size of the object structure */
        0, /* n_preallocs */
        (GInstanceInitFunc) dis_model_init /* constructor */
      };

    static const GInterfaceInfo tree_model_info =
      {
        (GInterfaceInitFunc) dis_model_tree_model_init,
        NULL,
        NULL
      };

    dis_model_type = g_type_register_static (G_TYPE_OBJECT, "DisModel",
                                             &dis_model_info, 0);

    g_type_add_interface_static (dis_model_type,
                                 GTK_TYPE_TREE_MODEL,
                                 &tree_model_info);
  }

  return dis_model_type;
}

void
dis_model_set_electron (DisModel *model, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (IS_DIS_MODEL (model));
  g_return_if_fail (electron == NULL || IS_ELECTRON_MANAGER (electron));

  if ((oldelectron = model->electron))
  {
    g_signal_handler_disconnect (oldelectron, model->stopped_handler);

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_object_ref (electron);

    model->stopped_handler
      = g_signal_connect (electron, "stopped",
                          G_CALLBACK (dis_model_on_stopped),
                          model);
    model->address = electron->data->cpu.pc;
  }

  model->electron = electron;

  dis_model_refresh_rows (model);
}

static void
dis_model_on_stopped (ElectronManager *electron, DisModel *model)
{
  guint16 pc;
  int row;

  g_return_if_fail (IS_DIS_MODEL (model));
  g_return_if_fail (IS_ELECTRON_MANAGER (electron));
  g_return_if_fail (model->electron == electron);

  /* Check if we're already displaying the next instruction */
  pc = electron->data->cpu.pc;
  for (row = 0; row < DIS_MODEL_ROW_COUNT; row++)
    if (model->rows[row].address == pc)
      break;
  if (row < DIS_MODEL_ROW_COUNT)
    /* Make sure the current address is in the top quarter of the display */
    model->address = model->rows[row >= DIS_MODEL_ROW_COUNT / 4
                                 ? row - DIS_MODEL_ROW_COUNT / 4 : 0].address;
  else
    /* We aren't already displaying the address so just start from
       there */
    model->address = pc;

  dis_model_refresh_rows (model);
}

static void
dis_model_refresh_rows (DisModel *model)
{
  int row_num, got_bytes = 0;
  DisModelRowData row;
  guint16 address = model->address;
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_first ();

  for (row_num = 0; row_num < DIS_MODEL_ROW_COUNT; row_num++)
  {
    if (model->electron == NULL)
    {
      row.address = 0;
      row.num_bytes = 0;
      row.mnemonic[0] = '\0';
      row.operands[0] = '\0';
      row.current = FALSE;
    }
    else
    {
      while (got_bytes < DISASSEMBLE_MAX_BYTES)
      {
        row.bytes[got_bytes] = electron_read_from_location (model->electron->data,
                                                            address + got_bytes);
        got_bytes++;
      }
      row.address = address;
      row.num_bytes = disassemble_instruction (address, row.bytes, row.mnemonic, row.operands);
      row.current = model->electron->data->cpu.pc == address ? TRUE : FALSE;
    }

    /* Only fire the changed signal if the row is actually different */
    if (row.address != model->rows[row_num].address
        || row.num_bytes != model->rows[row_num].num_bytes
        || memcmp (row.bytes, model->rows[row_num].bytes, row.num_bytes)
        || row.current != model->rows[row_num].current)
    {
      model->rows[row_num] = row;
      gtk_tree_path_get_indices (path)[0] = row_num;
      iter.user_data = GINT_TO_POINTER (row_num);
      iter.stamp = model->iter_stamp;
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
    }

    memmove (row.bytes, row.bytes + row.num_bytes, got_bytes = DISASSEMBLE_MAX_BYTES - row.num_bytes);
    address += row.num_bytes;
  }

  gtk_tree_path_free (path);
}

static void
dis_model_dispose (GObject *object)
{
  g_return_if_fail (IS_DIS_MODEL (object));

  /* Get rid of the electron */
  dis_model_set_electron (DIS_MODEL (object), NULL);

  /* Chain up */
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GtkTreeModelFlags
dis_model_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (IS_DIS_MODEL (tree_model), 0);

  /* Iterators persist and the model is flat */
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
dis_model_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (IS_DIS_MODEL (tree_model), 0);

  return DIS_MODEL_COL_COUNT;
}

static GType
dis_model_get_column_type (GtkTreeModel *tree_model, gint index)
{
  g_return_val_if_fail (IS_DIS_MODEL (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (index >= 0 && index < DIS_MODEL_COL_COUNT, G_TYPE_INVALID);

  return index == DIS_MODEL_COL_BOLD_IF_CURRENT ? G_TYPE_INT : G_TYPE_STRING;
}

static gboolean
dis_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
  gint pos;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  if (gtk_tree_path_get_depth (path) != 1 || (pos = gtk_tree_path_get_indices (path)[0]) < 0
      || pos >= DIS_MODEL_ROW_COUNT)
    return FALSE;
  else
  {
    DisModel *model = DIS_MODEL (tree_model);

    iter->stamp = model->iter_stamp;
    iter->user_data = GINT_TO_POINTER (pos);

    return TRUE;
  }
}

static GtkTreePath *
dis_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), NULL);
  model = DIS_MODEL (tree_model);
  g_return_val_if_fail (model->iter_stamp == iter->stamp, NULL);
  g_return_val_if_fail (GPOINTER_TO_INT (iter->user_data) >= 0
                        && GPOINTER_TO_INT (iter->user_data) < DIS_MODEL_ROW_COUNT, NULL);
  return gtk_tree_path_new_from_indices (GPOINTER_TO_INT (iter->user_data), -1);
}

static void
dis_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter,
                     gint column, GValue *value)
{
  DisModel *model;
  int row;
  char buf[DISASSEMBLE_MAX_BYTES * 3 + 1];

  g_return_if_fail (IS_DIS_MODEL (tree_model));

  model = DIS_MODEL (tree_model);
  row = GPOINTER_TO_INT (iter->user_data);

  g_return_if_fail (model->iter_stamp == iter->stamp);
  g_return_if_fail (row >= 0 && row < DIS_MODEL_ROW_COUNT);

  if (model->electron == NULL)
    buf[0] = '\0';
  else
    switch (column)
    {
      case DIS_MODEL_COL_ADDRESS:
        g_sprintf (buf, "%04X", model->rows[row].address);
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, buf);
        break;

      case DIS_MODEL_COL_BYTES:
        {
          char *p = buf;
          int i;

          if (model->rows[row].num_bytes < 1)
            buf[0] = '\0';
          else
          {
            for (i = 0; i < model->rows[row].num_bytes; i++)
            {
              g_sprintf (p, "%02X ", model->rows[row].bytes[i]);
              p += 3;
            }
            *(p - 1) = '\0';
          }

          g_value_init (value, G_TYPE_STRING);
          g_value_set_string (value, buf);
        }
        break;

      case DIS_MODEL_COL_MNEMONIC:
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, model->rows[row].mnemonic);
        break;

      case DIS_MODEL_COL_OPERANDS:
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, model->rows[row].operands);
        break;

      case DIS_MODEL_COL_BOLD_IF_CURRENT:
        g_value_init (value, G_TYPE_INT);
        g_value_set_int (value, model->rows[row].current
                         ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
        break;

      default:
        g_return_if_reached ();
    }
}

static gboolean
dis_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  DisModel *model;
  int row;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (model->iter_stamp == iter->stamp, FALSE);
  row = GPOINTER_TO_INT (iter->user_data);
  g_return_val_if_fail (row >= 0 && row < DIS_MODEL_ROW_COUNT, FALSE);

  if (row >= DIS_MODEL_ROW_COUNT - 1)
  {
    iter->user_data = GINT_TO_POINTER (-1);
    return FALSE;
  }
  else
  {
    iter->user_data = GINT_TO_POINTER (GPOINTER_TO_INT (iter->user_data) + 1);
    return TRUE;
  }
}

static gboolean
dis_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (parent == NULL
                        || (model->iter_stamp == parent->stamp
                            && GPOINTER_TO_INT (parent->user_data) >= 0
                            && GPOINTER_TO_INT (parent->user_data) < DIS_MODEL_ROW_COUNT), FALSE);

  if (parent)
  {
    iter->user_data = GINT_TO_POINTER (-1);
    return FALSE;
  }
  else
  {
    iter->stamp = model->iter_stamp;
    iter->user_data = GINT_TO_POINTER (0);
    return TRUE;
  }
}

static gboolean
dis_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (model->iter_stamp == iter->stamp, FALSE);
  g_return_val_if_fail (GPOINTER_TO_INT (iter->user_data) >= 0
                        && GPOINTER_TO_INT (iter->user_data) < DIS_MODEL_ROW_COUNT, FALSE);

  return FALSE;
}

static gint
dis_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), 0);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (iter == NULL
                        || (model->iter_stamp == iter->stamp
                            && GPOINTER_TO_INT (iter->user_data) >= 0
                            && GPOINTER_TO_INT (iter->user_data) < DIS_MODEL_ROW_COUNT), FALSE);

  if (iter == NULL)
    return DIS_MODEL_ROW_COUNT;
  else
    return 0;
}

static gboolean
dis_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter,
                          GtkTreeIter *parent, gint n)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (parent == NULL
                        || (model->iter_stamp == parent->stamp
                            && GPOINTER_TO_INT (parent->user_data) >= 0
                            && GPOINTER_TO_INT (parent->user_data) < DIS_MODEL_ROW_COUNT), FALSE);

  model = DIS_MODEL (tree_model);

  if (parent != NULL || n < 0 || n >= DIS_MODEL_ROW_COUNT)
  {
    iter->user_data = GINT_TO_POINTER (-1);
    return FALSE;
  }
  else
  {
    iter->stamp = model->iter_stamp;
    iter->user_data = GINT_TO_POINTER (n);
    return TRUE;
  }
}

static gboolean
dis_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
  DisModel *model;

  g_return_val_if_fail (IS_DIS_MODEL (tree_model), FALSE);

  model = DIS_MODEL (tree_model);

  g_return_val_if_fail (model->iter_stamp == child->stamp, FALSE);
  g_return_val_if_fail (GPOINTER_TO_INT (child->user_data) >= 0
                        && GPOINTER_TO_INT (child->user_data) < DIS_MODEL_ROW_COUNT, FALSE);

  iter->user_data = GINT_TO_POINTER (-1);
  return FALSE;
}
