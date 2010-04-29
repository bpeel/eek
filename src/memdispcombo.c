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

#include <gtk/gtkhpaned.h>

#include "memdispcombo.h"
#include "memorydisplay.h"
#include "electronmanager.h"
#include "eekmarshalers.h"

static void mem_disp_combo_class_init (MemDispComboClass *klass);
static void mem_disp_combo_init (MemDispCombo *memdispcombo);

static void mem_disp_combo_set_scroll_adjustments (MemDispCombo *memdispcombo,
						   GtkAdjustment *hadjustment,
						   GtkAdjustment *vadjustment);

GType
mem_disp_combo_get_type ()
{
  static GType mem_disp_combo_type = 0;

  if (!mem_disp_combo_type)
  {
    static const GTypeInfo mem_disp_combo_info =
      {
	sizeof (MemDispComboClass),
	NULL, NULL,
	(GClassInitFunc) mem_disp_combo_class_init,
	NULL, NULL,

	sizeof (MemDispCombo),
	0,
	(GInstanceInitFunc) mem_disp_combo_init,
	NULL
      };

    mem_disp_combo_type = g_type_register_static (GTK_TYPE_HPANED,
						  "MemDispCombo",
						  &mem_disp_combo_info, 0);
  }
  
  return mem_disp_combo_type;
}

static void
mem_disp_combo_class_init (MemDispComboClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  klass->set_scroll_adjustments = mem_disp_combo_set_scroll_adjustments;

  widget_class->set_scroll_adjustments_signal =
    g_signal_new ("set_scroll_adjustments",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (MemDispComboClass, set_scroll_adjustments),
		  NULL, NULL,
		  eek_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void
mem_disp_combo_init (MemDispCombo *memdispcombo)
{
  GtkWidget *left_split;
  GtkAdjustment *cur_adjustment;

  left_split = gtk_hpaned_new ();

  memdispcombo->address_display = memory_display_new ();
  memory_display_set_type (MEMORY_DISPLAY (memdispcombo->address_display), MEMORY_DISPLAY_ADDRESS);
  gtk_widget_show (memdispcombo->address_display);

  gtk_paned_add1 (GTK_PANED (left_split), memdispcombo->address_display);

  memdispcombo->binary_display = memory_display_new ();
  memory_display_set_type (MEMORY_DISPLAY (memdispcombo->binary_display), MEMORY_DISPLAY_HEX);
  gtk_widget_show (memdispcombo->binary_display);

  gtk_paned_add2 (GTK_PANED (left_split), memdispcombo->binary_display);

  gtk_widget_show (left_split);

  gtk_paned_add1 (GTK_PANED (memdispcombo), left_split);

  memdispcombo->text_display = memory_display_new ();
  memory_display_set_type (MEMORY_DISPLAY (memdispcombo->text_display), MEMORY_DISPLAY_TEXT);
  gtk_widget_show (memdispcombo->text_display);
  
  gtk_paned_add2 (GTK_PANED (memdispcombo), memdispcombo->text_display);

  cur_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
  mem_disp_combo_set_cursor_adjustment (memdispcombo, cur_adjustment);
}

void
mem_disp_combo_set_cursor_adjustment (MemDispCombo *memdispcombo, GtkAdjustment *cur_adjustment)
{
  g_return_if_fail (IS_MEM_DISP_COMBO (memdispcombo));
  g_return_if_fail (cur_adjustment == NULL || GTK_IS_ADJUSTMENT (cur_adjustment));

  memory_display_set_cursor_adjustment (MEMORY_DISPLAY (memdispcombo->address_display), cur_adjustment);
  memory_display_set_cursor_adjustment (MEMORY_DISPLAY (memdispcombo->binary_display), cur_adjustment);
  memory_display_set_cursor_adjustment (MEMORY_DISPLAY (memdispcombo->text_display), cur_adjustment);
}

GtkWidget *
mem_disp_combo_new ()
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_MEM_DISP_COMBO, NULL);

  return ret;
}

GtkWidget *
mem_disp_combo_new_with_electron (ElectronManager *electron)				  
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_MEM_DISP_COMBO, NULL);

  mem_disp_combo_set_electron (MEM_DISP_COMBO (ret), electron);

  return ret;
}

void
mem_disp_combo_set_electron (MemDispCombo *memdispcombo, ElectronManager *electron)
{
  g_return_if_fail (IS_MEM_DISP_COMBO (memdispcombo));

  memory_display_set_electron (MEMORY_DISPLAY (memdispcombo->address_display), electron);
  memory_display_set_electron (MEMORY_DISPLAY (memdispcombo->binary_display), electron);
  memory_display_set_electron (MEMORY_DISPLAY (memdispcombo->text_display), electron);
}

static void
mem_disp_combo_set_scroll_adjustments (MemDispCombo *memdispcombo,
				       GtkAdjustment *hadjustment,
				       GtkAdjustment *vadjustment)
{
  g_return_if_fail (IS_MEM_DISP_COMBO (memdispcombo));

  gtk_widget_set_scroll_adjustments (memdispcombo->address_display,
				     hadjustment, vadjustment);
  gtk_widget_set_scroll_adjustments (memdispcombo->binary_display,
				     hadjustment, vadjustment);
  gtk_widget_set_scroll_adjustments (memdispcombo->text_display,
				     hadjustment, vadjustment);
}
