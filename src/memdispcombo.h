#ifndef _MEM_DISP_COMBO_H
#define _MEM_DISP_COMBO_H

#include <gtk/gtkhpaned.h>
#include <gtk/gtkadjustment.h>
#include "electronmanager.h"

#define TYPE_MEM_DISP_COMBO (mem_disp_combo_get_type ())
#define MEM_DISP_COMBO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                             TYPE_MEM_DISP_COMBO, MemDispCombo))
#define MEM_DISP_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                     TYPE_MEM_DISP_COMBO, MemDispComboClass))
#define IS_MEM_DISP_COMBO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MEM_DISP_COMBO))
#define IS_MEM_DISP_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_MEM_DISP_COMBO))
#define MEM_DISP_COMBO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                       TYPE_MEM_DISP_COMBO, MemDispComboClass))

typedef struct _MemDispCombo MemDispCombo;
typedef struct _MemDispComboClass MemDispComboClass;

struct _MemDispCombo
{
  GtkHPaned parent_object;

  GtkWidget *address_display, *binary_display, *text_display;
};

struct _MemDispComboClass
{
  GtkHPanedClass parent_class;

  void  (* set_scroll_adjustments) (MemDispCombo *hexdispcombo,
				    GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);
};

GType mem_disp_combo_get_type ();
GtkWidget *mem_disp_combo_new ();
GtkWidget *mem_disp_combo_new_with_electron (ElectronManager *electron);
void mem_disp_combo_set_electron (MemDispCombo *hexdisplay, ElectronManager *electron);
void mem_disp_combo_set_cursor_adjustment (MemDispCombo *memdispcombo,
					   GtkAdjustment *cur_adjustment);

#endif /* _MEM_DISP_COMBO_H */
