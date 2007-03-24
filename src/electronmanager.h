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

  int timeout;
};

struct _ElectronManagerClass
{
  GObjectClass parent_class;

  void (* frame_end) (ElectronManager *filebuf);
};

ElectronManager *electron_manager_new ();
GType electron_manager_get_type ();

#define electron_manager_press_key(eman, line, bit) \
do { electron_press_key ((eman)->data, line, bit); } while (0)
#define electron_manager_release_key(eman, line, bit) \
do { electron_release_key ((eman)->data, line, bit); } while (0)

#endif /* _ELECTRON_MANAGER_H */
