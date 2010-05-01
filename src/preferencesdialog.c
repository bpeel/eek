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

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkbutton.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "preferencesdialog.h"
#include "gladeutil.h"
#include "intl.h"

static void preferences_dialog_class_init (PreferencesDialogClass *klass);
static void preferences_dialog_init (PreferencesDialog *prefsdlg);
static void preferences_dialog_dispose (GObject *obj);
static void preferences_dialog_on_response (GtkDialog *dialog, gint res_id);
static void preferences_dialog_widget_notify (PreferencesDialog *prefsdlg, GObject *obj);
static void preferences_dialog_on_file_changed (GtkFileChooser *chooser,
                                                PreferencesDialog *prefsdlg);
static void preferences_dialog_on_clear (GtkButton *button,
                                         PreferencesDialog *prefsdlg);
static void preferences_dialog_on_value_changed (GConfClient *client,
                                                 const gchar *key,
                                                 GConfValue *value,
                                                 PreferencesDialog *prefsdlg);

#define PREFERENCES_DIALOG_PREFIX "/apps/eek/"

static gpointer parent_class;

#define PREFERENCES_DIALOG_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_PREFERENCES_DIALOG, PreferencesDialogPrivate))

enum PreferencesDialogEntryType { PREFERENCES_DIALOG_FILE, PREFERENCES_DIALOG_CLEAR };

struct PreferencesDialogEntry
{
  const char *option_name;
  enum PreferencesDialogEntryType type;
};

static const struct PreferencesDialogEntry preferences_dialog_entries[] =
  {
    { "roms/rom_0", PREFERENCES_DIALOG_FILE },
    { "roms/rom_1", PREFERENCES_DIALOG_FILE },
    { "roms/rom_2", PREFERENCES_DIALOG_FILE },
    { "roms/rom_3", PREFERENCES_DIALOG_FILE },
    { "roms/rom_4", PREFERENCES_DIALOG_FILE },
    { "roms/rom_5", PREFERENCES_DIALOG_FILE },
    { "roms/rom_6", PREFERENCES_DIALOG_FILE },
    { "roms/rom_7", PREFERENCES_DIALOG_FILE },
    { "roms/rom_12", PREFERENCES_DIALOG_FILE },
    { "roms/rom_13", PREFERENCES_DIALOG_FILE },
    { "roms/rom_14", PREFERENCES_DIALOG_FILE },
    { "roms/rom_15", PREFERENCES_DIALOG_FILE },
    { "roms/os_rom", PREFERENCES_DIALOG_FILE },
    { "roms/basic_rom", PREFERENCES_DIALOG_FILE },
    { "roms/rom_0", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_1", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_2", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_3", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_4", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_5", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_6", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_7", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_12", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_13", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_14", PREFERENCES_DIALOG_CLEAR },
    { "roms/rom_15", PREFERENCES_DIALOG_CLEAR },
    { "roms/os_rom", PREFERENCES_DIALOG_CLEAR },
    { "roms/basic_rom", PREFERENCES_DIALOG_CLEAR }
  };

#define PREFERENCES_DIALOG_ENTRY_COUNT (sizeof (preferences_dialog_entries) \
                                        / sizeof (struct PreferencesDialogEntry))

static const char *preferences_dialog_conf_dirs[] =
  {
    "roms",
    NULL
  };

struct _PreferencesDialogPrivate
{
  GConfClient *gconf;
  GtkWidget *widgets[PREFERENCES_DIALOG_ENTRY_COUNT];
  gulong handlers[PREFERENCES_DIALOG_ENTRY_COUNT];
  int value_changed_handler;
  int dirs_added;
};

GType
preferences_dialog_get_type ()
{
  static GType preferences_dialog_type = 0;

  if (!preferences_dialog_type)
  {
    static const GTypeInfo preferences_dialog_info =
      {
        sizeof (PreferencesDialogClass),
        NULL, NULL,
        (GClassInitFunc) preferences_dialog_class_init,
        NULL, NULL,

        sizeof (PreferencesDialog),
        0,
        (GInstanceInitFunc) preferences_dialog_init,
        NULL
      };

    preferences_dialog_type = g_type_register_static (GTK_TYPE_DIALOG,
                                                      "PreferencesDialog",
                                                      &preferences_dialog_info, 0);
  }

  return preferences_dialog_type;
}

static void
preferences_dialog_class_init (PreferencesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = preferences_dialog_dispose;

  g_type_class_add_private (object_class, sizeof (PreferencesDialogPrivate));
}

static void
preferences_dialog_init (PreferencesDialog *prefsdlg)
{
  GtkWidget *notebook, *error_widget;
  PreferencesDialogPrivate *priv = PREFERENCES_DIALOG_GET_PRIVATE (prefsdlg);
  int i;

  prefsdlg->priv = priv;

  gtk_dialog_add_button (GTK_DIALOG (prefsdlg), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_window_set_title (GTK_WINDOW (prefsdlg), _("eek Preferences"));
  gtk_dialog_set_has_separator (GTK_DIALOG (prefsdlg), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (prefsdlg), TRUE);

  g_signal_connect (prefsdlg, "response",
                    G_CALLBACK (preferences_dialog_on_response), NULL);

  priv->gconf = gconf_client_get_default ();

  priv->value_changed_handler
    = g_signal_connect (priv->gconf, "value-changed",
                        G_CALLBACK (preferences_dialog_on_value_changed),
                        prefsdlg);

  priv->dirs_added = 0;
  for (i = 0; preferences_dialog_conf_dirs[i]; i++)
  {
    gchar *dir = g_strconcat (PREFERENCES_DIALOG_PREFIX,
                              preferences_dialog_conf_dirs[i],
                              NULL);
    GError *error = NULL;

    gconf_client_add_dir (priv->gconf, dir, GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
    g_free (dir);

    if (error)
      g_error_free (error);
    else
      priv->dirs_added |= 1 << i;
  }

  if (glade_util_get_widgets ("preferences-dialog.glade", "notebook",
                              &error_widget, "notebook", &notebook,
                              "rom_0_chooser", priv->widgets,
                              "rom_1_chooser", priv->widgets + 1,
                              "rom_2_chooser", priv->widgets + 2,
                              "rom_3_chooser", priv->widgets + 3,
                              "rom_4_chooser", priv->widgets + 4,
                              "rom_5_chooser", priv->widgets + 5,
                              "rom_6_chooser", priv->widgets + 6,
                              "rom_7_chooser", priv->widgets + 7,
                              "rom_12_chooser", priv->widgets + 8,
                              "rom_13_chooser", priv->widgets + 9,
                              "rom_14_chooser", priv->widgets + 10,
                              "rom_15_chooser", priv->widgets + 11,
                              "os_rom_chooser", priv->widgets + 12,
                              "basic_rom_chooser", priv->widgets + 13,
                              "clear_rom_0", priv->widgets + 14,
                              "clear_rom_1", priv->widgets + 15,
                              "clear_rom_2", priv->widgets + 16,
                              "clear_rom_3", priv->widgets + 17,
                              "clear_rom_4", priv->widgets + 18,
                              "clear_rom_5", priv->widgets + 19,
                              "clear_rom_6", priv->widgets + 20,
                              "clear_rom_7", priv->widgets + 21,
                              "clear_rom_12", priv->widgets + 22,
                              "clear_rom_13", priv->widgets + 23,
                              "clear_rom_14", priv->widgets + 24,
                              "clear_rom_15", priv->widgets + 25,
                              "clear_os_rom", priv->widgets + 26,
                              "clear_basic_rom", priv->widgets + 27,
                              NULL))
  {
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefsdlg)->vbox), notebook,
                        TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
    gtk_widget_show (notebook);

    for (i = 0; i < PREFERENCES_DIALOG_ENTRY_COUNT; i++)
    {
      gchar *key = g_strconcat (PREFERENCES_DIALOG_PREFIX,
                                preferences_dialog_entries[i].option_name,
                                NULL);

      g_object_weak_ref (G_OBJECT (priv->widgets[i]),
                         (GWeakNotify) preferences_dialog_widget_notify,
                         prefsdlg);

      if (!gconf_client_key_is_writable (priv->gconf, key, NULL))
        gtk_widget_set_sensitive (priv->widgets[i], FALSE);

      switch (preferences_dialog_entries[i].type)
      {
        case PREFERENCES_DIALOG_FILE:
          if (GTK_IS_FILE_CHOOSER (priv->widgets[i]))
          {
            GConfValue *value;
            gchar *filename;
            const gchar *filename_utf8;

            if ((value = gconf_client_get (priv->gconf, key, NULL)))
            {
              if (value->type == GCONF_VALUE_STRING)
              {
                filename_utf8 = gconf_value_get_string (value);
                if (*filename_utf8
                    && (filename = g_filename_from_utf8 (filename_utf8,
                                                         -1, NULL, NULL, NULL)))
                {
                  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->widgets[i]),
                                                 filename);
                  g_free (filename);
                }
              }
              gconf_value_free (value);
            }

            priv->handlers[i] = g_signal_connect (priv->widgets[i],
                                                  "selection-changed",
                                                  G_CALLBACK (preferences_dialog_on_file_changed),
                                                  prefsdlg);
          }
          else
            priv->handlers[i] = 0;
          break;

        case PREFERENCES_DIALOG_CLEAR:
          if (GTK_IS_BUTTON (priv->widgets[i]))
            priv->handlers[i] = g_signal_connect (priv->widgets[i],
                                                  "clicked",
                                                  G_CALLBACK (preferences_dialog_on_clear),
                                                  prefsdlg);
          else
            priv->handlers[i] = 0;
          break;
      }

      g_free (key);
    }
  }
  else
  {
    memset (priv->widgets, 0, sizeof (GtkWidget *) * PREFERENCES_DIALOG_ENTRY_COUNT);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefsdlg)->vbox), error_widget,
                        TRUE, TRUE, 12);
    gtk_widget_show (error_widget);
  }
}

static void
preferences_dialog_dispose (GObject *obj)
{
  PreferencesDialogPrivate *priv = PREFERENCES_DIALOG (obj)->priv;
  int i;

  g_return_if_fail (IS_PREFERENCES_DIALOG (obj));

  if (priv->gconf)
  {
    for (i = 0; preferences_dialog_conf_dirs[i]; i++)
      if ((priv->dirs_added & (1 << i)))
      {
        gchar *dir = g_strconcat (PREFERENCES_DIALOG_PREFIX,
                                  preferences_dialog_conf_dirs[i],
                                  NULL);
        gconf_client_remove_dir (priv->gconf, dir, NULL);
        g_free (dir);
      }

    g_signal_handler_disconnect (priv->gconf, priv->value_changed_handler);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  for (i = 0; i < PREFERENCES_DIALOG_ENTRY_COUNT; i++)
    if (priv->widgets[i])
    {
      if (priv->handlers[i])
        g_signal_handler_disconnect (priv->widgets[i], priv->handlers[i]);
      g_object_weak_unref (G_OBJECT (priv->widgets[i]),
                           (GWeakNotify) preferences_dialog_widget_notify,
                           obj);
      priv->widgets[i] = NULL;
    }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

void
preferences_dialog_show (GtkWindow *parent)
{
  static PreferencesDialog *prefsdlg = NULL;

  g_return_if_fail (GTK_IS_WINDOW (parent));

  /* We only ever want to display a single preferences dialog, so if
     we've already got one then just reparent it */
  if (prefsdlg == NULL)
  {
    prefsdlg = PREFERENCES_DIALOG (g_object_new (TYPE_PREFERENCES_DIALOG, NULL));

    g_signal_connect (prefsdlg, "destroy", G_CALLBACK (gtk_widget_destroyed), &prefsdlg);
  }

  gtk_window_set_transient_for (GTK_WINDOW (prefsdlg), parent);

  gtk_window_present (GTK_WINDOW (prefsdlg));
}

static void
preferences_dialog_on_response (GtkDialog *dialog, gint res_id)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
preferences_dialog_widget_notify (PreferencesDialog *prefsdlg, GObject *obj)
{
  PreferencesDialogPrivate *priv = prefsdlg->priv;
  int i;

  for (i = 0; i < PREFERENCES_DIALOG_ENTRY_COUNT; i++)
    if (priv->widgets[i] == (GtkWidget *) obj)
    {
      priv->widgets[i] = NULL;
      break;
    }
}

static void
preferences_dialog_on_file_changed (GtkFileChooser *chooser,
                                    PreferencesDialog *prefsdlg)
{
  PreferencesDialogPrivate *priv = prefsdlg->priv;
  int widget_num;

  for (widget_num = 0; widget_num < PREFERENCES_DIALOG_ENTRY_COUNT; widget_num++)
    if (priv->widgets[widget_num] == (GtkWidget *) chooser)
    {
      if (priv->gconf)
      {
        gchar *filename_local;

        if ((filename_local = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
                                                             (priv->widgets[widget_num]))))
        {
          gchar *filename;
          GConfValue *value = gconf_value_new (GCONF_VALUE_STRING);
          gchar *option_name = g_strconcat ("/apps/eek/",
                                            preferences_dialog_entries[widget_num].option_name,
                                            NULL);

          /* There appears to be a bug in GtkFileChooserButton so that
             it sends an extra selection-changed signal during
             initialisation. We can skip skip it by ignoring signals
             when there is no file selected */
          if ((filename = g_filename_to_utf8 (filename_local, -1, NULL, NULL, NULL)) == NULL)
            gconf_value_set_string (value, "");
          else
          {
            gconf_value_set_string (value, filename);
            g_free (filename);
          }

          gconf_client_set (priv->gconf, option_name, value, NULL);

          g_free (option_name);
          gconf_value_free (value);
          g_free (filename_local);
        }
      }

      break;
    }
}

static void
preferences_dialog_on_clear (GtkButton *button,
                             PreferencesDialog *prefsdlg)
{
  PreferencesDialogPrivate *priv = prefsdlg->priv;
  int widget_num;

  for (widget_num = 0; widget_num < PREFERENCES_DIALOG_ENTRY_COUNT; widget_num++)
    if (priv->widgets[widget_num] == (GtkWidget *) button)
    {
      if (priv->gconf)
      {
        GConfValue *value = gconf_value_new (GCONF_VALUE_STRING);
        gchar *option_name = g_strconcat (PREFERENCES_DIALOG_PREFIX,
                                          preferences_dialog_entries[widget_num].option_name,
                                          NULL);
        gconf_value_set_string (value, "");
        gconf_client_set (priv->gconf, option_name, value, NULL);
        g_free (option_name);
        gconf_value_free (value);
      }

      break;
    }
}

static void
preferences_dialog_on_value_changed (GConfClient *client,
                                     const gchar *key,
                                     GConfValue *value,
                                     PreferencesDialog *prefsdlg)
{
  PreferencesDialogPrivate *priv;

  g_return_if_fail (IS_PREFERENCES_DIALOG (prefsdlg));
  priv = prefsdlg->priv;
  g_return_if_fail (GCONF_IS_CLIENT (client));
  g_return_if_fail (priv->gconf == client);

  if (g_str_has_prefix (key, PREFERENCES_DIALOG_PREFIX))
  {
    int i;

    for (i = 0; i < PREFERENCES_DIALOG_ENTRY_COUNT; i++)
      if (priv->widgets[i]
          && !strcmp (preferences_dialog_entries[i].option_name,
                      key + sizeof (PREFERENCES_DIALOG_PREFIX) - 1))
      {
        const struct PreferencesDialogEntry *entry = preferences_dialog_entries + i;

        /* Temporarily block the handler for this widget so that it
           won't get stuck in an infinite loop */
        if (priv->handlers[i])
          g_signal_handler_block (priv->widgets[i], priv->handlers[i]);

        /* Recheck whether we have write permission to this key */
        gtk_widget_set_sensitive (priv->widgets[i],
                                  gconf_client_key_is_writable (priv->gconf, key, NULL));

        switch (entry->type)
        {
          case PREFERENCES_DIALOG_FILE:
            if (value->type == GCONF_VALUE_STRING
                && GTK_IS_FILE_CHOOSER (priv->widgets[i]))
            {
              const gchar *value_str = gconf_value_get_string (value);

              /* If the string is empty then deselect the file */
              if (*value_str == 0)
                gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->widgets[i]));
              else
              {
                gchar *filename = g_filename_from_utf8 (value_str, -1,
                                                        NULL, NULL, NULL);
                if (filename)
                {
                  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->widgets[i]),
                                                 filename);
                  g_free (filename);
                }
              }
            }
            break;

          case PREFERENCES_DIALOG_CLEAR:
            break;
        }

        if (priv->handlers[i])
          g_signal_handler_unblock (priv->widgets[i], priv->handlers[i]);
      }
  }
}
