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

#ifndef _DIS_MODEL_H
#define _DIS_MODEL_H

#include <glib.h>
#include <glib-object.h>

#include "electronmanager.h"
#include "disassemble.h"

#define TYPE_DIS_MODEL (dis_model_get_type ())
#define DIS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DIS_MODEL, DisModel))
#define DIS_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_DIS_MODEL, DisModelClass))
#define IS_DIS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DIS_MODEL))
#define IS_DIS_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_DIS_MODEL))
#define DIS_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_DIS_MODEL, DisModelClass))

typedef struct _DisModel DisModel;
typedef struct _DisModelClass DisModelClass;
typedef struct _DisModelRowData DisModelRowData;

#define DIS_MODEL_ROW_COUNT 16

enum
  {
    DIS_MODEL_COL_ADDRESS,
    DIS_MODEL_COL_BYTES,
    DIS_MODEL_COL_MNEMONIC,
    DIS_MODEL_COL_OPERANDS,
    DIS_MODEL_COL_BOLD_IF_CURRENT,
    DIS_MODEL_COL_COUNT
  };

struct _DisModelRowData
{
  guint16 address;
  guint8 num_bytes;
  guint8 bytes[DISASSEMBLE_MAX_BYTES];
  char mnemonic[DISASSEMBLE_MAX_MNEMONIC + 1];
  char operands[DISASSEMBLE_MAX_OPERANDS + 1];
  gboolean current : 1;
};

struct _DisModel
{
  GObject parent;

  guint16 address;
  int iter_stamp;
  ElectronManager *electron;
  guint stopped_handler;

  DisModelRowData rows[DIS_MODEL_ROW_COUNT];
};

struct _DisModelClass
{
  GObjectClass parent_class;
};

DisModel *dis_model_new ();
DisModel *dis_model_new_with_electron (ElectronManager *electron);
void dis_model_set_electron (DisModel *model, ElectronManager *electron);
GType dis_model_get_type ();

#endif /* _DIS_MODEL_H */
