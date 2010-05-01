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

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkfilechooserdialog.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mainwindow.h"
#include "electronmanager.h"
#include "electronwidget.h"
#include "debugger.h"
#include "aboutdialog.h"
#include "intl.h"
#include "breakpointeditdialog.h"
#include "disdialog.h"
#include "preferencesdialog.h"
#include "tapeuef.h"

typedef struct _MainWindowAction MainWindowAction;

/* Replacement for GtkActionEntry because there doesn't seem to be a
   nice way to set the short label in it */
struct _MainWindowAction
{
  const gchar *name, *stock_id, *label, *short_label, *accelerator, *tooltip;
  gboolean toggle;
  GCallback callback;
};

static void main_window_class_init (MainWindowClass *klass);
static void main_window_init (MainWindow *mainwin);
static void main_window_dispose (GObject *obj);
static void main_window_finalize (GObject *obj);
static void main_window_electron_widget_notify (gpointer data, GObject *obj);
static void main_window_debugger_notify (gpointer data, GObject *obj);
static gboolean main_window_delete_event (GtkWidget *widget,
                                          GdkEventAny *event);

static void main_window_on_new (GtkAction *action, MainWindow *mainwin);
static void main_window_on_open (GtkAction *action, MainWindow *mainwin);
static void main_window_on_save (GtkAction *action, MainWindow *mainwin);
static void main_window_on_save_as (GtkAction *action, MainWindow *mainwin);
static void main_window_on_rewind (GtkAction *action, MainWindow *mainwin);
static void main_window_on_quit (GtkAction *action, MainWindow *mainwin);
static void main_window_on_toggle_full_speed (GtkAction *action,
                                              MainWindow *mainwin);
static void main_window_on_preferences (GtkAction *action, MainWindow *mainwin);
static void main_window_on_about (GtkAction *action, MainWindow *mainwin);
static void main_window_on_run (GtkAction *action, MainWindow *mainwin);
static void main_window_on_step (GtkAction *action, MainWindow *mainwin);
static void main_window_on_break (GtkAction *action, MainWindow *mainwin);
static void main_window_on_reset (GtkAction *action, MainWindow *mainwin);
static void main_window_on_edit_breakpoint (GtkAction *action, MainWindow *mainwin);
static void main_window_on_disassembler (GtkAction *action, MainWindow *mainwin);

static void main_window_update_debug_actions (MainWindow *mainwin);
static void main_window_on_rom_error (MainWindow *mainwin, GList *errors,
                                      ElectronManager *eman);

static void main_window_on_toggle_toolbar (GtkAction *action, MainWindow *mainwin);
static void main_window_on_toggle_debugger (GtkAction *action, MainWindow *mainwin);

static void main_window_forget_dis_dialog (MainWindow *mainwin);

static gpointer parent_class;

static const MainWindowAction main_window_actions[] =
  {
    { "ActionTapeMenu", NULL, N_("Menu|_Tape"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionEditMenu", NULL, N_("Menu|_Edit"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionViewMenu", NULL, N_("Menu|_View"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionDebugMenu", NULL, N_("Menu|_Debug"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionHelpMenu", NULL, N_("Menu|_Help"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionNew", GTK_STOCK_NEW, N_("MenuTape|_New"), NULL,
      NULL, N_("Clear the tape data"), FALSE, G_CALLBACK (main_window_on_new) },
    { "ActionOpen", GTK_STOCK_OPEN, N_("MenuTape|_Open"), NULL,
      NULL, N_("Open the tape data from a file"), FALSE,
      G_CALLBACK (main_window_on_open) },
    { "ActionSave", GTK_STOCK_SAVE, N_("MenuTape|_Save"), NULL,
      NULL, N_("Save the tape data to a file"), FALSE,
      G_CALLBACK (main_window_on_save) },
    { "ActionSaveAs", GTK_STOCK_SAVE_AS, N_("MenuTape|Save _As..."), NULL,
      NULL, N_("Save the tape data to a different file"), FALSE,
      G_CALLBACK (main_window_on_save_as) },
    { "ActionRewind", NULL, N_("MenuTape|Rewind"), NULL, "<Control>R",
      N_("Rewind the tape"), FALSE, G_CALLBACK (main_window_on_rewind) },
    { "ActionQuit", GTK_STOCK_QUIT, N_("MenuTape|_Quit"), NULL,
      NULL, N_("Quit the program"), FALSE, G_CALLBACK (main_window_on_quit) },
    { "ActionToggleFullSpeed", NULL, N_("MenuView|Run _full speed"), NULL,
      NULL, N_("When enabled, run full speed otherwise "
               "sync to an accurate speed"), TRUE,
      G_CALLBACK (main_window_on_toggle_full_speed) },
    { "ActionPreferences", GTK_STOCK_PREFERENCES, N_("MenuEdit|_Preferences"), NULL,
      NULL, N_("Configure the application"), FALSE, G_CALLBACK (main_window_on_preferences) },
    { "ActionToggleToolbar", NULL, N_("MenuView|_Toolbar"), NULL,
      NULL, N_("Display or hide the toolbar"), TRUE,
      G_CALLBACK (main_window_on_toggle_toolbar) },
    { "ActionToggleDebugger", NULL, N_("MenuView|_Debugger"), NULL,
      NULL, N_("Display or hide the debugger controls"), TRUE,
      G_CALLBACK (main_window_on_toggle_debugger) },
    { "ActionRun", NULL, N_("MenuDebug|_Run"), NULL,
      "F5", N_("Start the emulator"), FALSE, G_CALLBACK (main_window_on_run) },
    { "ActionStep", NULL, N_("MenuDebug|_Step"), NULL,
      "F11", N_("Single-step one instruction"), FALSE, G_CALLBACK (main_window_on_step) },
    { "ActionBreak", NULL, N_("MenuDebug|_Break"), NULL,
      "F9", N_("Stop the emulator"), FALSE, G_CALLBACK (main_window_on_break) },
    { "ActionReset", NULL, N_("MenuDebug|R_eset"), NULL,
      NULL, N_("Reset the emulator to location in the vector at $FFFC"), FALSE,
      G_CALLBACK (main_window_on_reset) },
    { "ActionEditBreakpoint", NULL, N_("MenuDebug|Edit brea_kpoint..."), NULL,
      NULL, N_("Set or remove a breakpoint"), FALSE,
      G_CALLBACK (main_window_on_edit_breakpoint) },
    { "ActionDisassembler", NULL, N_("MenuDebug|_Disassembler..."), NULL,
      NULL, N_("Show the diassembler dialog"), FALSE,
      G_CALLBACK (main_window_on_disassembler) },
    { "ActionAbout", GTK_STOCK_ABOUT, N_("MenuHelp|_About"), NULL,
      NULL, N_("Display the about box"), FALSE, G_CALLBACK (main_window_on_about) }
  };

static const char main_window_ui_definition[] =
"<ui>\n"
" <menubar name=\"MenuBar\">\n"
"  <menu name=\"TapeMenu\" action=\"ActionTapeMenu\">\n"
"   <menuitem name=\"New\" action=\"ActionNew\" />\n"
"   <menuitem name=\"Open\" action=\"ActionOpen\" />\n"
"   <separator />\n"
"   <menuitem name=\"Save\" action=\"ActionSave\" />\n"
"   <menuitem name=\"SaveAs\" action=\"ActionSaveAs\" />\n"
"   <separator />\n"
"   <menuitem name=\"Rewind\" action=\"ActionRewind\" />\n"
"   <separator />\n"
"   <menuitem name=\"Quit\" action=\"ActionQuit\" />\n"
"  </menu>\n"
"  <menu name=\"EditMenu\" action=\"ActionEditMenu\">\n"
"   <menuitem name=\"ToggleFullSpeed\" action=\"ActionToggleFullSpeed\" />\n"
"   <menuitem name=\"Preferences\" action=\"ActionPreferences\" />\n"
"  </menu>\n"
"  <menu name=\"ViewMenu\" action=\"ActionViewMenu\">\n"
"   <menuitem name=\"ToggleToolbar\" action=\"ActionToggleToolbar\" />\n"
"  </menu>\n"
"  <menu name=\"ViewMenu\" action=\"ActionViewMenu\">\n"
"   <menuitem name=\"ToggleDebugger\" action=\"ActionToggleDebugger\" />\n"
"  </menu>\n"
"  <menu name=\"DebugMenu\" action=\"ActionDebugMenu\">\n"
"   <menuitem name=\"Run\" action=\"ActionRun\" />\n"
"   <menuitem name=\"Step\" action=\"ActionStep\" />\n"
"   <menuitem name=\"Break\" action=\"ActionBreak\" />\n"
"   <menuitem name=\"Reset\" action=\"ActionReset\" />\n"
"   <separator />\n"
"   <menuitem name=\"EditBreakpoint\" action=\"ActionEditBreakpoint\" />\n"
"   <menuitem name=\"Disassembler\" action=\"ActionDisassembler\" />\n"
"  </menu>\n"
"  <menu name=\"HelpMenu\" action=\"ActionHelpMenu\">\n"
"   <menuitem name=\"About\" action=\"ActionAbout\" />\n"
"  </menu>\n"
" </menubar>\n"
" <toolbar name=\"ToolBar\">\n"
"  <toolitem name=\"Run\" action=\"ActionRun\" />\n"
"  <toolitem name=\"Step\" action=\"ActionStep\" />\n"
"  <toolitem name=\"Break\" action=\"ActionBreak\" />\n"
" </toolbar>\n"
"</ui>\n";

GType
main_window_get_type ()
{
  static GType main_window_type = 0;

  if (!main_window_type)
  {
    static const GTypeInfo main_window_info =
      {
        sizeof (MainWindowClass),
        NULL, NULL,
        (GClassInitFunc) main_window_class_init,
        NULL, NULL,

        sizeof (MainWindow),
        0,
        (GInstanceInitFunc) main_window_init,
        NULL
      };

    main_window_type = g_type_register_static (GTK_TYPE_WINDOW,
                                               "MainWindow",
                                               &main_window_info, 0);
  }

  return main_window_type;
}

static void
main_window_class_init (MainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = main_window_dispose;
  object_class->finalize = main_window_finalize;

  widget_class->delete_event = main_window_delete_event;
}

static void
main_window_init (MainWindow *mainwin)
{
  GtkWidget *vbox, *menu_bar, *tool_bar, *hbox;
  GError *ui_error = NULL;
  int i;
  const MainWindowAction *a;

  mainwin->electron = NULL;

  /* We haven't created a disassembler dialog yet */
  mainwin->disdialog = NULL;

  /* Create a GtkVBox to hold the menu and the main display widget */
  vbox = gtk_vbox_new (FALSE, 0);

  /* Create the action group for the menu / toolbar actions */
  mainwin->action_group = gtk_action_group_new ("MenuActions");
  for (a = main_window_actions, i = 0;
       i < sizeof (main_window_actions) / sizeof (MainWindowAction);
       i++, a++)
  {
    GtkAction *action;

    if (main_window_actions[i].toggle)
    {
      action = GTK_ACTION (gtk_toggle_action_new
                           (a->name,
                            a->label
                            ? g_strip_context (a->label, gettext (a->label))
                            : NULL,
                            a->tooltip
                            ? g_strip_context (a->tooltip, gettext (a->tooltip))
                            : NULL,
                            main_window_actions[i].stock_id));
      if (main_window_actions[i].callback)
        g_signal_connect (G_OBJECT (action), "toggled",
                          main_window_actions[i].callback, mainwin);
    }
    else
    {
      action = gtk_action_new (a->name,
                               a->label
                               ? g_strip_context (a->label, gettext (a->label))
                               : NULL,
                               a->tooltip
                               ? g_strip_context (a->tooltip, gettext (a->tooltip))
                               : NULL,
                               main_window_actions[i].stock_id);
      if (main_window_actions[i].callback)
        g_signal_connect (G_OBJECT (action), "activate",
                          main_window_actions[i].callback, mainwin);
    }
    if (main_window_actions[i].short_label)
      g_object_set (G_OBJECT (action), "short-label",
                    g_strip_context (a->short_label, gettext (a->short_label)), NULL);
    gtk_action_group_add_action_with_accel (mainwin->action_group, action,
                                            main_window_actions[i].accelerator);
    g_object_unref (action);
  }

  /* Create the actual widgets */
  mainwin->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (mainwin->ui_manager, mainwin->action_group, 0);
  if (gtk_ui_manager_add_ui_from_string (mainwin->ui_manager, main_window_ui_definition,
                                         sizeof (main_window_ui_definition) - 1,
                                         &ui_error) == 0)
  {
    /* If the UI description failed to parse then it is a programming
       error so it should be reported with g_error */
    g_warning ("Error creating ui elements: %s", ui_error->message);
    g_error_free (ui_error);
  }

  /* Add the accelerators to this window */
  gtk_window_add_accel_group (GTK_WINDOW (mainwin),
                              gtk_ui_manager_get_accel_group (mainwin->ui_manager));

  /* Add the menu bar to the vbox */
  if ((menu_bar = gtk_ui_manager_get_widget (mainwin->ui_manager, "/MenuBar")))
    gtk_box_pack_start (GTK_BOX (vbox), menu_bar, FALSE, TRUE, 0);

  /* Add the tool bar to the vbox */
  if ((tool_bar = gtk_ui_manager_get_widget (mainwin->ui_manager, "/ToolBar")))
  {
    gtk_widget_hide (tool_bar);
    gtk_box_pack_start (GTK_BOX (vbox), tool_bar, FALSE, TRUE, 0);
  }

  /* Create an hbox for the main widgets */
  hbox = gtk_hbox_new (FALSE, 0);

  /* Create the main electron display */
  mainwin->ewidget = electron_widget_new ();
  g_object_weak_ref (G_OBJECT (mainwin->ewidget), main_window_electron_widget_notify, mainwin);
  gtk_widget_show (mainwin->ewidget);
  gtk_box_pack_start (GTK_BOX (hbox), mainwin->ewidget, TRUE, TRUE, 0);

  /* Create a debugger widget */
  mainwin->debugger = debugger_new ();
  g_object_weak_ref (G_OBJECT (mainwin->debugger), main_window_debugger_notify, mainwin);
  gtk_box_pack_start (GTK_BOX (hbox), mainwin->debugger, FALSE, TRUE, 0);

  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (mainwin), vbox);

  /* Give focus to the electron widget to start */
  gtk_widget_grab_focus (mainwin->ewidget);

  /* Update the sensitivity of the debug actions */
  main_window_update_debug_actions (mainwin);
}

GtkWidget *
main_window_new ()
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_MAIN_WINDOW, NULL);

  return ret;
}

GtkWidget *
main_window_new_with_electron (ElectronManager *electron)
{
  GtkWidget *ret;

  ret = g_object_new (TYPE_MAIN_WINDOW, NULL);

  main_window_set_electron (MAIN_WINDOW (ret), electron);

  return ret;
}

void
main_window_set_electron (MainWindow *mainwin, ElectronManager *electron)
{
  ElectronManager *oldelectron;

  g_return_if_fail (mainwin != NULL);
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if ((oldelectron = mainwin->electron))
  {
    g_signal_handler_disconnect (oldelectron, mainwin->started);
    g_signal_handler_disconnect (oldelectron, mainwin->stopped);
    g_signal_handler_disconnect (oldelectron, mainwin->rom_error);

    mainwin->electron = NULL;

    g_object_unref (oldelectron);
  }

  if (electron)
  {
    g_return_if_fail (IS_ELECTRON_MANAGER (electron));

    g_object_ref (electron);

    mainwin->electron = electron;

    mainwin->started
      = g_signal_connect_swapped (electron, "started",
                                  G_CALLBACK (main_window_update_debug_actions), mainwin);
    mainwin->stopped
      = g_signal_connect_swapped (electron, "stopped",
                                  G_CALLBACK (main_window_update_debug_actions), mainwin);
    mainwin->rom_error
      = g_signal_connect_swapped (electron, "rom-error",
                                  G_CALLBACK (main_window_on_rom_error), mainwin);
  }

  if (mainwin->ewidget)
    electron_widget_set_electron (ELECTRON_WIDGET (mainwin->ewidget), electron);
  if (mainwin->debugger)
    debugger_set_electron (DEBUGGER (mainwin->debugger), electron);

  main_window_update_debug_actions (mainwin);
}

static void
main_window_set_new_tape_buffer (MainWindow *mainwin)
{
  electron_set_tape_buffer (mainwin->electron->data, tape_buffer_new ());
  g_free (mainwin->tape_filename);
  mainwin->tape_filename = NULL;
}

static void
main_window_on_new_response (GtkDialog *dialog,
                             gint response_id,
                             MainWindow *mainwin)
{
  if (response_id == GTK_RESPONSE_YES)
    main_window_set_new_tape_buffer (mainwin);

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
main_window_on_new (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (tape_buffer_is_dirty (mainwin->electron->data->tape_buffer))
  {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("The tape buffer has unsaved changes "
                                       "that will be lost if you start a new "
                                       "tape. Are you sure you want a new "
                                       "tape?"));
    g_signal_connect (dialog, "response",
                      G_CALLBACK (main_window_on_new_response),
                      mainwin);
    gtk_widget_show (dialog);
  }
  else
    main_window_set_new_tape_buffer (mainwin);
}

void
main_window_open_tape (MainWindow *mainwin, const gchar *filename)
{
  GError *error = NULL;
  FILE *file;

  if ((file = fopen (filename, "rb")) == NULL)
    g_set_error (&error, G_FILE_ERROR, g_file_error_from_errno (errno),
                 "%s", strerror (errno));
  else
  {
    TapeBuffer *tbuf;

    if ((tbuf = tape_uef_load (file, &error)))
    {
      electron_set_tape_buffer (mainwin->electron->data, tbuf);
      g_free (mainwin->tape_filename);
      mainwin->tape_filename = g_strdup (filename);
    }

    fclose (file);
  }

  if (error)
  {
    gchar *display_name = g_filename_display_name (filename);
    GtkWidget *dialog
      = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR,
                                GTK_BUTTONS_CLOSE,
                                "Error opening \"%s\": %s",
                                display_name,
                                error->message);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);
    gtk_widget_show (dialog);
    g_free (display_name);
  }
}

static void
main_window_on_open_response (GtkDialog *dialog,
                             gint response_id,
                             MainWindow *mainwin)
{
  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
  {
    gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    if (filename)
    {
      main_window_open_tape (mainwin, filename);
      g_free (filename);
    }
  }
}

static void
main_window_show_open_dialog (MainWindow *mainwin)
{
  if (mainwin->open_dialog == NULL)
  {
    mainwin->open_dialog
      = gtk_file_chooser_dialog_new (_("Open tape"),
                                     GTK_WINDOW (mainwin),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                     NULL);
    g_object_ref_sink (mainwin->open_dialog);

    g_signal_connect (mainwin->open_dialog,
                      "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete),
                      NULL);

    mainwin->open_response_handler
      = g_signal_connect (mainwin->open_dialog,
                          "response",
                          G_CALLBACK (main_window_on_open_response),
                          mainwin);
  }

  gtk_window_present (GTK_WINDOW (mainwin->open_dialog));
}

static void
main_window_on_open_confirm_response (GtkDialog *dialog,
                                      gint response_id,
                                      MainWindow *mainwin)
{
  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_YES)
    main_window_show_open_dialog (mainwin);
}

static void
main_window_on_open (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (tape_buffer_is_dirty (mainwin->electron->data->tape_buffer))
  {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("The tape buffer has unsaved changes "
                                       "that will be lost if you open a new "
                                       "tape. Are you sure you want a open "
                                       "tape?"));
    g_signal_connect (dialog, "response",
                      G_CALLBACK (main_window_on_open_confirm_response),
                      mainwin);
    gtk_widget_show (dialog);
  }
  else
    main_window_show_open_dialog (mainwin);
}

static void
main_window_save_tape (MainWindow *mainwin, const gchar *filename)
{
  GError *error = NULL;
  FILE *file;

  if ((file = fopen (filename, "wb")) == NULL)
    g_set_error (&error, G_FILE_ERROR, g_file_error_from_errno (errno),
                 "%s", strerror (errno));
  else
  {
    if (tape_uef_save (mainwin->electron->data->tape_buffer,
#ifdef HAVE_ZLIB
                       TRUE,
#else
                       FALSE,
#endif
                       file, &error))
    {
      if (filename != mainwin->tape_filename)
      {
        g_free (mainwin->tape_filename);
        mainwin->tape_filename = g_strdup (filename);
      }

      tape_buffer_clear_dirty (mainwin->electron->data->tape_buffer);
    }

    fclose (file);
  }

  if (error)
  {
    gchar *display_name = g_filename_display_name (filename);
    GtkWidget *dialog
      = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR,
                                GTK_BUTTONS_CLOSE,
                                "Error opening \"%s\": %s",
                                display_name,
                                error->message);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);
    gtk_widget_show (dialog);
    g_free (display_name);
  }
}

static void
main_window_on_save (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->tape_filename)
    main_window_save_tape (mainwin, mainwin->tape_filename);
  else
    main_window_on_save_as (action, mainwin);
}

static void
main_window_on_save_response (GtkDialog *dialog,
                             gint response_id,
                             MainWindow *mainwin)
{
  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
  {
    gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    if (filename)
    {
      main_window_save_tape (mainwin, filename);
      g_free (filename);
    }
  }
}

static void
main_window_on_save_as (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->save_dialog == NULL)
  {
    mainwin->save_dialog
      = gtk_file_chooser_dialog_new (_("Save tape"),
                                     GTK_WINDOW (mainwin),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                     NULL);
    g_object_ref_sink (mainwin->save_dialog);

    g_signal_connect (mainwin->save_dialog,
                      "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete),
                      NULL);

    mainwin->open_response_handler
      = g_signal_connect (mainwin->save_dialog,
                          "response",
                          G_CALLBACK (main_window_on_save_response),
                          mainwin);
  }

  gtk_window_present (GTK_WINDOW (mainwin->save_dialog));
}

static void
main_window_on_rewind (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  electron_rewind_cassette (mainwin->electron->data);
}

static void
main_window_on_quit_response (GtkDialog *dialog,
                             gint response_id,
                             MainWindow *mainwin)
{
  if (response_id == GTK_RESPONSE_YES)
    gtk_widget_destroy (GTK_WIDGET (mainwin));

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
main_window_try_quit (MainWindow *mainwin)
{
  if (tape_buffer_is_dirty (mainwin->electron->data->tape_buffer))
  {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("The tape buffer has unsaved changes "
                                       "that will be lost if you quit now. "
                                       "Are you sure you want to quit?"));
    g_signal_connect (dialog, "response",
                      G_CALLBACK (main_window_on_quit_response),
                      mainwin);
    gtk_widget_show (dialog);
  }
  else
    gtk_widget_destroy (GTK_WIDGET (mainwin));
}

static void
main_window_on_quit (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  gtk_main_quit ();
}

static void
main_window_on_toggle_full_speed (GtkAction *action,
                                  MainWindow *mainwin)
{
  gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
  electron_manager_set_full_speed (mainwin->electron, active);
}

static void
main_window_on_preferences (GtkAction *action, MainWindow *mainwin)
{
  preferences_dialog_show (GTK_WINDOW (mainwin));
}

static void
main_window_on_toggle_toolbar (GtkAction *action, MainWindow *mainwin)
{
  GtkWidget *toolbar;

  if (mainwin->ui_manager && (toolbar = gtk_ui_manager_get_widget (mainwin->ui_manager,
                                                                   "/ToolBar")))
  {
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
      gtk_widget_show (toolbar);
    else
      gtk_widget_hide (toolbar);
  }
}

static void
main_window_on_toggle_debugger (GtkAction *action, MainWindow *mainwin)
{
  if (mainwin->debugger)
  {
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
      gtk_widget_show (mainwin->debugger);
    else
      gtk_widget_hide (mainwin->debugger);
  }
}

static void
main_window_on_about (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  about_dialog_show ();
}

static void
main_window_on_run (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_start (mainwin->electron);
}

static void
main_window_on_step (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_step (mainwin->electron);
}

static void
main_window_on_break (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_stop (mainwin->electron);
}

static void
main_window_on_reset (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    cpu_restart (&mainwin->electron->data->cpu);
}

static void
main_window_on_edit_breakpoint (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    breakpoint_edit_dialog_run (GTK_WINDOW (mainwin), mainwin->electron);
}

static void
main_window_on_disassembler (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->disdialog == NULL)
  {
    mainwin->disdialog = dis_dialog_new_with_electron (mainwin->electron);
    g_object_ref_sink (mainwin->disdialog);
    g_signal_connect (G_OBJECT (mainwin->disdialog), "response",
                      G_CALLBACK (gtk_widget_destroy), mainwin);
    mainwin->disdialog_destroy
      = g_signal_connect_swapped (G_OBJECT (mainwin->disdialog), "destroy",
                                  G_CALLBACK (main_window_forget_dis_dialog), mainwin);
    gtk_window_set_transient_for (GTK_WINDOW (mainwin->disdialog), GTK_WINDOW (mainwin));
    gtk_window_set_position (GTK_WINDOW (mainwin->disdialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size (GTK_WINDOW (mainwin->disdialog), 1, 500);
  }

  gtk_window_present (GTK_WINDOW (mainwin->disdialog));
}

static void
main_window_forget_dis_dialog (MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->disdialog)
  {
    g_signal_handler_disconnect (G_OBJECT (mainwin->disdialog), mainwin->disdialog_destroy);
    g_object_unref (mainwin->disdialog);
    mainwin->disdialog = NULL;
  }
}

static void
main_window_update_debug_actions (MainWindow *mainwin)
{
  GtkAction *action;

  if (mainwin->action_group)
  {
    gboolean is_running = FALSE;

    if (mainwin->electron)
      is_running = electron_manager_is_running (mainwin->electron);

    if ((action = gtk_action_group_get_action (mainwin->action_group, "ActionRun")))
      gtk_action_set_sensitive (action, mainwin->electron ? !is_running : FALSE);
    if ((action = gtk_action_group_get_action (mainwin->action_group, "ActionBreak")))
      gtk_action_set_sensitive (action, mainwin->electron ? is_running : FALSE);
    if ((action = gtk_action_group_get_action (mainwin->action_group, "ActionStep")))
      gtk_action_set_sensitive (action, mainwin->electron ? !is_running : FALSE);
  }
}

static void
main_window_forget_open_dialog (MainWindow *mainwin)
{
  if (mainwin->open_dialog)
  {
    g_signal_handler_disconnect (mainwin->open_dialog,
                                 mainwin->open_response_handler);
    gtk_widget_destroy (mainwin->open_dialog);
    g_object_unref (mainwin->open_dialog);
    mainwin->open_dialog = NULL;
  }
}

static void
main_window_forget_save_dialog (MainWindow *mainwin)
{
  if (mainwin->save_dialog)
  {
    g_signal_handler_disconnect (mainwin->save_dialog,
                                 mainwin->save_response_handler);
    gtk_widget_destroy (mainwin->save_dialog);
    g_object_unref (mainwin->save_dialog);
    mainwin->save_dialog = NULL;
  }
}

static void
main_window_finalize (GObject *obj)
{
  MainWindow *mainwin = MAIN_WINDOW (obj);

  g_free (mainwin->tape_filename);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
main_window_dispose (GObject *obj)
{
  MainWindow *mainwin;

  g_return_if_fail (IS_MAIN_WINDOW (obj));

  mainwin = MAIN_WINDOW (obj);

  if (mainwin->ewidget)
  {
    g_object_weak_unref (G_OBJECT (mainwin->ewidget),
                         main_window_electron_widget_notify, mainwin);
    mainwin->ewidget = NULL;
  }

  if (mainwin->debugger)
  {
    g_object_weak_unref (G_OBJECT (mainwin->debugger),
                         main_window_debugger_notify, mainwin);
    mainwin->debugger = NULL;
  }

  if (mainwin->ui_manager)
  {
    g_object_unref (G_OBJECT (mainwin->ui_manager));
    mainwin->ui_manager = NULL;
  }

  if (mainwin->action_group)
  {
    g_object_unref (G_OBJECT (mainwin->action_group));
    mainwin->action_group = NULL;
  }

  main_window_forget_open_dialog (mainwin);
  main_window_forget_save_dialog (mainwin);

  main_window_forget_dis_dialog (mainwin);

  main_window_set_electron (mainwin, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
main_window_electron_widget_notify (gpointer data, GObject *obj)
{
  MainWindow *mainwin;

  g_return_if_fail (IS_MAIN_WINDOW (data));

  mainwin = MAIN_WINDOW (data);

  g_return_if_fail (obj == G_OBJECT (mainwin->ewidget));

  mainwin->ewidget = NULL;
}

static void
main_window_debugger_notify (gpointer data, GObject *obj)
{
  MainWindow *mainwin;

  g_return_if_fail (IS_MAIN_WINDOW (data));

  mainwin = MAIN_WINDOW (data);

  g_return_if_fail (obj == G_OBJECT (mainwin->debugger));

  mainwin->debugger = NULL;
}

static void
main_window_on_rom_error (MainWindow *mainwin, GList *errors,
                          ElectronManager *eman)
{
  int error_count;
  gchar *note;
  GString *message;
  GtkWidget *dialog;

  g_return_if_fail (IS_MAIN_WINDOW (mainwin));
  g_return_if_fail (IS_ELECTRON_MANAGER (eman));
  g_return_if_fail (eman == mainwin->electron);

  error_count = g_list_length (errors);
  note = g_strdup_printf (ngettext ("An error occurred while loading a ROM",
                                    "Some errors occurred while loading the ROMs",
                                    error_count), error_count);

  message = g_string_new ("");
  for (; errors; errors = errors->next)
  {
    if (errors->prev)
      g_string_append_c (message, '\n');
    g_string_append (message, ((GError *) errors->data)->message);
  }

  dialog = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", note);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s", message->str);
  g_signal_connect_swapped (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy),
                            dialog);
  gtk_widget_show (dialog);

  g_string_free (message, TRUE);
  g_free (note);
}

static gboolean
main_window_delete_event (GtkWidget *widget, GdkEventAny *event)
{
  main_window_try_quit (MAIN_WINDOW (widget));

  /* Stop any further handlers so that the window won't automatically
     be destroyed */
  return TRUE;
}
