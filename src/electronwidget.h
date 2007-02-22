#ifndef _ELECTRON_WIDGET_H
#define _ELECTRON_WIDGET_H

#include <gtk/gtkwidget.h>
#include "electron.h"

#define TYPE_ELECTRON_WIDGET (electron_widget_get_type ())
#define ELECTRON_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ELECTRON_WIDGET, ElectronWidget))
#define ELECTRON_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_ELECTRON_WIDGET, ElectronWidgetClass))
#define IS_ELECTRON_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ELECTRON_WIDGET))
#define IS_ELECTRON_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_ELECTRON_WIDGET))
#define ELECTRON_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_ELECTRON_WIDGET, ElectronWidgetClass))

typedef struct _ElectronWidget ElectronWidget;
typedef struct _ElectronWidgetClass ElectronWidgetClass;

struct _ElectronWidget
{
  GtkWidget parent_object;

  Electron *electron;
  guint timeout;
};

struct _ElectronWidgetClass
{
  GtkWidgetClass parent_class;
};

GType electron_widget_get_type ();
GtkWidget *electron_widget_new ();

#endif /* _ELECTRON_WIDGET_H */
