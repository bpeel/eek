#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "electronmanager.h"
#include "electron.h"
#include "framesource.h"

enum
  {
    ELECTRON_MANAGER_FRAME_END_SIGNAL,
    ELECTRON_MANAGER_STARTED_SIGNAL,
    ELECTRON_MANAGER_STOPPED_SIGNAL,
    ELECTRON_MANAGER_LAST_SIGNAL
  };

static guint electron_manager_signals[ELECTRON_MANAGER_LAST_SIGNAL];

static void electron_manager_finalize (GObject *obj);
static void electron_manager_dispose (GObject *obj);

static gboolean electron_manager_timeout (ElectronManager *eman);

static gpointer parent_class;

static void
electron_manager_class_init (ElectronManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = electron_manager_finalize;
  object_class->dispose = electron_manager_dispose;

  electron_manager_signals[ELECTRON_MANAGER_FRAME_END_SIGNAL]
    = g_signal_new ("frame-end",
		    G_TYPE_FROM_CLASS (klass),
		    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (ElectronManagerClass, frame_end),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
  electron_manager_signals[ELECTRON_MANAGER_STARTED_SIGNAL]
    = g_signal_new ("started",
		    G_TYPE_FROM_CLASS (klass),
		    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (ElectronManagerClass, started),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
  electron_manager_signals[ELECTRON_MANAGER_STOPPED_SIGNAL]
    = g_signal_new ("stopped",
		    G_TYPE_FROM_CLASS (klass),
		    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (ElectronManagerClass, stopped),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
}

static void
electron_manager_init (ElectronManager *eman)
{
  eman->data = electron_new ();
  eman->timeout = 0;
}

gboolean
electron_manager_is_running (ElectronManager *eman)
{
  g_return_val_if_fail (IS_ELECTRON_MANAGER (eman), FALSE);

  return eman->timeout != 0;
}

void
electron_manager_start (ElectronManager *eman)
{
  g_return_if_fail (IS_ELECTRON_MANAGER (eman));

  if (eman->timeout == 0)
  {
    eman->timeout = frame_source_add (ELECTRON_TICKS_PER_FRAME,
				      (GSourceFunc) electron_manager_timeout,
				      eman, NULL);
    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_STARTED_SIGNAL], 0);
  }
}

void
electron_manager_stop (ElectronManager *eman)
{
  g_return_if_fail (IS_ELECTRON_MANAGER (eman));

  if (eman->timeout)
  {
    g_source_remove (eman->timeout);
    eman->timeout = 0;

    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_STOPPED_SIGNAL], 0);
  }
}

void
electron_manager_step (ElectronManager *eman)
{
  int last_scanline;

  g_return_if_fail (IS_ELECTRON_MANAGER (eman));

  electron_manager_stop (eman);

  g_signal_emit (G_OBJECT (eman),
		 electron_manager_signals[ELECTRON_MANAGER_STARTED_SIGNAL], 0);

  last_scanline = eman->data->scanline;
  electron_step (eman->data);

  /* Check if we've reached the end of a frame */
  if (last_scanline != eman->data->scanline && eman->data->scanline == ELECTRON_END_SCANLINE)
    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_FRAME_END_SIGNAL], 0);

  g_signal_emit (G_OBJECT (eman),
		 electron_manager_signals[ELECTRON_MANAGER_STOPPED_SIGNAL], 0);
}

static gboolean
electron_manager_timeout (ElectronManager *eman)
{
  g_return_val_if_fail (IS_ELECTRON_MANAGER (eman), FALSE);
  g_return_val_if_fail (eman->timeout != 0, FALSE);

  if (electron_run_frame (eman->data))
    /* Breakpoint was hit, so stop the electron */
    electron_manager_stop (eman);
  else
    /* Otherwise we've done a whole frame so emit the frame end signal */
    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_FRAME_END_SIGNAL], 0);

  return TRUE;
}

GType
electron_manager_get_type ()
{
  static GType electron_manager_type = 0;

  if (electron_manager_type == 0)
  {
    static const GTypeInfo electron_manager_info =
      {
	sizeof (ElectronManagerClass), /* size of class structure */
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) electron_manager_class_init, /* class struct init func */
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (ElectronManager), /* size of the object structure */
	0, /* n_preallocs */
	(GInstanceInitFunc) electron_manager_init /* constructor */
      };

    electron_manager_type = g_type_register_static (G_TYPE_OBJECT, "ElectronManager",
						    &electron_manager_info, 0);
  }

  return electron_manager_type;
}

static void
electron_manager_finalize (GObject *obj)
{
  ElectronManager *eman = ELECTRON_MANAGER (obj);

  electron_free (eman->data);

  /* Chain up */
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
electron_manager_dispose (GObject *obj)
{
  ElectronManager *eman = ELECTRON_MANAGER (obj);

  electron_manager_stop (eman);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

ElectronManager *
electron_manager_new ()
{
  return g_object_new (TYPE_ELECTRON_MANAGER, NULL);
}
