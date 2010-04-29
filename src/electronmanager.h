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

#ifndef _ELECTRON_MANAGER_H
#define _ELECTRON_MANAGER_H

#include <glib.h>
#include <glib-object.h>
#include "electron.h"

#define TYPE_ELECTRON_MANAGER (electron_manager_get_type ())
#define ELECTRON_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                               TYPE_ELECTRON_MANAGER, ElectronManager))
#define ELECTRON_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                       TYPE_ELECTRON_MANAGER, ElectronManagerClass))
#define IS_ELECTRON_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                  TYPE_ELECTRON_MANAGER))
#define IS_ELECTRON_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                          TYPE_ELECTRON_MANAGER))
#define ELECTRON_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         TYPE_ELECTRON_MANAGER, ElectronManagerClass))

typedef struct _ElectronManager ElectronManager;
typedef struct _ElectronManagerClass ElectronManagerClass;

struct _ElectronManager
{
  GObject parent;

  Electron *data;
};

struct _ElectronManagerClass
{
  GObjectClass parent_class;

  void (* frame_end) (ElectronManager *eman);
  void (* started) (ElectronManager *eman);
  void (* stopped) (ElectronManager *eman);
  void (* rom_error) (ElectronManager *eman, GList *error_list);
};

typedef enum
{
  ELECTRON_MANAGER_ERROR_FILE
} ElectronManagerError;

#define ELECTRON_MANAGER_ERROR electron_manager_error_quark ()
GQuark electron_manager_error_quark ();

ElectronManager *electron_manager_new ();
GType electron_manager_get_type ();
void electron_manager_start (ElectronManager *eman);
void electron_manager_stop (ElectronManager *eman);
void electron_manager_step (ElectronManager *eman);
gboolean electron_manager_is_running (ElectronManager *eman);
void electron_manager_update_all_roms (ElectronManager *eman);

#define electron_manager_press_key(eman, line, bit) \
do { electron_press_key ((eman)->data, line, bit); } while (0)
#define electron_manager_release_key(eman, line, bit) \
do { electron_release_key ((eman)->data, line, bit); } while (0)

#endif /* _ELECTRON_MANAGER_H */
