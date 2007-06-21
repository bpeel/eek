#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <gtk/gtktable.h>

#include "electronmanager.h"
#include "dismodel.h"

#define TYPE_DEBUGGER (debugger_get_type ())
#define DEBUGGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                       TYPE_DEBUGGER, Debugger))
#define DEBUGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                               TYPE_DEBUGGER, DebuggerClass))
#define IS_DEBUGGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                          TYPE_DEBUGGER))
#define IS_DEBUGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                  TYPE_DEBUGGER))
#define DEBUGGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                 TYPE_DEBUGGER, DebuggerClass))

typedef struct _Debugger Debugger;
typedef struct _DebuggerClass DebuggerClass;

enum { DEBUGGER_REGISTER_A, DEBUGGER_REGISTER_X, DEBUGGER_REGISTER_Y,
       DEBUGGER_REGISTER_S, DEBUGGER_REGISTER_PC, DEBUGGER_REGISTER_P,
       DEBUGGER_REGISTER_COUNT };

struct _Debugger
{
  GtkTable parent_object;

  GtkWidget *register_widgets[DEBUGGER_REGISTER_COUNT];
  GtkWidget *mem_disp;
  DisModel *dis_model;

  ElectronManager *electron;

  guint started, stopped;
};

struct _DebuggerClass
{
  GtkTableClass parent_class;
};

GType debugger_get_type ();
GtkWidget *debugger_new ();
GtkWidget *debugger_new_with_electron (ElectronManager *electron);
void debugger_set_electron (Debugger *debugger, ElectronManager *electron);

#endif /* _DEBUGGER_H */
