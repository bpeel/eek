#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>
#include <string.h>
#include <errno.h>

#include "electronmanager.h"
#include "electron.h"
#include "framesource.h"
#include "intl.h"

enum
  {
    ELECTRON_MANAGER_FRAME_END_SIGNAL,
    ELECTRON_MANAGER_STARTED_SIGNAL,
    ELECTRON_MANAGER_STOPPED_SIGNAL,
    ELECTRON_MANAGER_ROM_ERROR_SIGNAL,
    ELECTRON_MANAGER_LAST_SIGNAL
  };

static guint electron_manager_signals[ELECTRON_MANAGER_LAST_SIGNAL];

static void electron_manager_finalize (GObject *obj);
static void electron_manager_dispose (GObject *obj);

static gboolean electron_manager_timeout (ElectronManager *eman);
static void electron_manager_on_value_changed (GConfClient *client,
					       const gchar *key,
					       GConfValue *value,
					       ElectronManager *eman);

static gpointer parent_class;

#define ELECTRON_MANAGER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_ELECTRON_MANAGER, ElectronManagerPrivate))
typedef struct _ElectronManagerPrivate ElectronManagerPrivate;

struct _ElectronManagerPrivate
{
  int timeout;
  gboolean added_dir;
  GConfClient *gconf;
  int value_changed_handler;
};

#define ELECTRON_MANAGER_ROMS_CONF_DIR "/apps/eek/roms"

static const struct { const char *key; int page; }
electron_manager_rom_table[] =
  {
    { "os_rom", -1 },
    { "basic_rom", ELECTRON_BASIC_PAGE },
    { "rom_0", 0 }, { "rom_1", 1 }, { "rom_2", 2 }, { "rom_3", 3 },
    { "rom_4", 4 }, { "rom_5", 5 }, { "rom_6", 6 }, { "rom_7", 7 },
    { "rom_12", 12 }, { "rom_13", 13 }, { "rom_14", 14 }, { "rom_15", 15 }
  };
#define ELECTRON_MANAGER_ROM_COUNT (sizeof (electron_manager_rom_table) \
				    / sizeof (electron_manager_rom_table[0]))

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
  electron_manager_signals[ELECTRON_MANAGER_ROM_ERROR_SIGNAL]
    = g_signal_new ("rom-error",
		    G_TYPE_FROM_CLASS (klass),
		    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (ElectronManagerClass, rom_error),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_type_class_add_private (object_class, sizeof (ElectronManagerPrivate));
}

static void
electron_manager_init (ElectronManager *eman)
{
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);
  GError *error = NULL;

  eman->data = electron_new ();
  priv->timeout = 0;

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, ELECTRON_MANAGER_ROMS_CONF_DIR,
			GCONF_CLIENT_PRELOAD_ONELEVEL, &error);

  if (error == NULL)
    priv->added_dir = FALSE;
  else
  {
    g_error_free (error);
    priv->added_dir = TRUE;
  }

  priv->value_changed_handler
    = g_signal_connect (priv->gconf, "value-changed",
			G_CALLBACK (electron_manager_on_value_changed),
			eman);
}

gboolean
electron_manager_is_running (ElectronManager *eman)
{
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);

  g_return_val_if_fail (IS_ELECTRON_MANAGER (eman), FALSE);

  return priv->timeout != 0;
}

void
electron_manager_start (ElectronManager *eman)
{
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);

  g_return_if_fail (IS_ELECTRON_MANAGER (eman));

  if (priv->timeout == 0)
  {
    priv->timeout = frame_source_add (ELECTRON_TICKS_PER_FRAME,
				      (GSourceFunc) electron_manager_timeout,
				      eman, NULL);
    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_STARTED_SIGNAL], 0);
    /* If we're breaking at the current address then skip over one
       instruction. Otherwise when the breakpoint is hit continuing
       the emulation will cause it to break immediatly */
    if (eman->data->cpu.break_type == CPU_BREAK_ADDR
	&& eman->data->cpu.break_address == eman->data->cpu.pc)
      electron_step (eman->data);
  }
}

void
electron_manager_stop (ElectronManager *eman)
{
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);

  g_return_if_fail (IS_ELECTRON_MANAGER (eman));

  if (priv->timeout)
  {
    g_source_remove (priv->timeout);
    priv->timeout = 0;

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
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);

  g_return_val_if_fail (IS_ELECTRON_MANAGER (eman), FALSE);
  g_return_val_if_fail (priv->timeout != 0, FALSE);

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
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);

  electron_manager_stop (eman);

  if (priv->gconf)
  {
    if (priv->added_dir)
      gconf_client_remove_dir (priv->gconf, ELECTRON_MANAGER_ROMS_CONF_DIR, NULL);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

ElectronManager *
electron_manager_new ()
{
  return g_object_new (TYPE_ELECTRON_MANAGER, NULL);
}

static int
electron_manager_update_rom (ElectronManager *eman, int rom_num, GError **error)
{
  GConfValue *value = NULL;
  ElectronManagerPrivate *priv = ELECTRON_MANAGER_GET_PRIVATE (eman);
  int ret = 0;

  if (priv->gconf)
  {
    gchar *full_key = g_strconcat (ELECTRON_MANAGER_ROMS_CONF_DIR,
				   "/", electron_manager_rom_table[rom_num].key,
				   NULL);
    value = gconf_client_get (priv->gconf, full_key, NULL);
    g_free (full_key);
    
    if (value && (value->type != GCONF_VALUE_STRING
		  || gconf_value_get_string (value)[0] == '\0'))
    {
      gconf_value_free (value);
      value = NULL;
    }
  }

  if (value == NULL)
  {
    if (electron_manager_rom_table[rom_num].page == -1)
      electron_clear_os_rom (eman->data);
    else
      electron_clear_paged_rom (eman->data, electron_manager_rom_table[rom_num].page);
  }
  else
  {
    FILE *file;
    gchar *filename;
    GError *conv_error = NULL;

    if ((filename = g_filename_from_utf8 (gconf_value_get_string (value), -1,
					  NULL, NULL, &conv_error)) == NULL)
    {
      g_set_error (error, ELECTRON_MANAGER_ERROR, ELECTRON_MANAGER_ERROR_FILE,
		   _("Failed to convert \"%s\" to correct filename encoding: %s"),
		   gconf_value_get_string (value), conv_error->message);
      g_error_free (conv_error);
      ret = -1;
    }
    else
    {
      if ((file = g_fopen (filename, "rb")) == NULL)
      {
	g_set_error (error, ELECTRON_MANAGER_ERROR, ELECTRON_MANAGER_ERROR_FILE,
		     _("Failed to load \"%s\": %s"), gconf_value_get_string (value),
		     strerror (errno));
	ret = -1;
      }
      else
      {
	int load_ret;

	if (electron_manager_rom_table[rom_num].page == -1)
	  load_ret = electron_load_os_rom (eman->data, file);
	else
	  load_ret = electron_load_paged_rom (eman->data,
					      electron_manager_rom_table[rom_num].page,
					      file);
	if (load_ret == -1)
	{
	  g_set_error (error, ELECTRON_MANAGER_ERROR, ELECTRON_MANAGER_ERROR_FILE,
		       _("Failed to load \"%s\": %s"), gconf_value_get_string (value),
		       ferror (file) ? strerror (errno) : _("ROM file too short"));
	  ret = -1;
	}

	fclose (file);
      }
      
      g_free (filename);
    }

    gconf_value_free (value);
  }

  return ret;
}

GQuark
electron_manager_error_quark ()
{
  return g_quark_from_static_string ("electron_manager_error");
}

static void
electron_manager_on_value_changed (GConfClient *client,
				   const gchar *key,
				   GConfValue *value,
				   ElectronManager *eman)
{
  ElectronManagerPrivate *priv;

  g_return_if_fail (IS_ELECTRON_MANAGER (eman));
  priv = ELECTRON_MANAGER_GET_PRIVATE (eman);
  g_return_if_fail (GCONF_IS_CLIENT (client));
  g_return_if_fail (priv->gconf == client);
  
  if (g_str_has_prefix (key, ELECTRON_MANAGER_ROMS_CONF_DIR)
      && key[sizeof (ELECTRON_MANAGER_ROMS_CONF_DIR) - 1] == '/')
  {
    int rom_num;

    for (rom_num = 0; rom_num < ELECTRON_MANAGER_ROM_COUNT; rom_num++)
      if (!strcmp (key + sizeof (ELECTRON_MANAGER_ROMS_CONF_DIR),
		   electron_manager_rom_table[rom_num].key))
      {
	GError *error = NULL;

	if (electron_manager_update_rom (eman, rom_num, &error) == -1)
	{
	  GList *list = g_list_prepend (NULL, error);
	  g_signal_emit (G_OBJECT (eman),
			 electron_manager_signals[ELECTRON_MANAGER_ROM_ERROR_SIGNAL],
			 0, list);
	  g_error_free (error);
	  g_list_free (list);
	}

	break;
      }
  }
}

void
electron_manager_update_all_roms (ElectronManager *eman)
{
  GList *errors = NULL;
  int rom_num;

  for (rom_num = ELECTRON_MANAGER_ROM_COUNT - 1; rom_num >= 0; rom_num--)
  {
    GError *error = NULL;

    if (electron_manager_update_rom (eman, rom_num, &error) == -1)
      errors = g_list_prepend (errors, error);
  }

  if (errors)
  {
    g_signal_emit (G_OBJECT (eman),
		   electron_manager_signals[ELECTRON_MANAGER_ROM_ERROR_SIGNAL],
		   0, errors);
    g_list_foreach (errors, (GFunc) g_error_free, NULL);
    g_list_free (errors);
  }
}
