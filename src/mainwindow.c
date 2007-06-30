#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>

#include "mainwindow.h"
#include "electronmanager.h"
#include "electronwidget.h"
#include "debugger.h"
#include "aboutdialog.h"
#include "intl.h"

typedef struct _MainWindowAction MainWindowAction;

/* Replacement for GtkActionEntry because there doesn't seem to be a
   nice way to set the short label in it */
struct _MainWindowAction
{
  const gchar *name, *stock_id, *label, *short_label, *accelerator, *tooltip;
  GCallback callback;
};

static void main_window_class_init (MainWindowClass *klass);
static void main_window_init (MainWindow *mainwin);
static void main_window_dispose (GObject *obj);
static void main_window_electron_widget_notify (gpointer data, GObject *obj);
static void main_window_debugger_notify (gpointer data, GObject *obj);

static void main_window_on_action_quit (GtkAction *action, MainWindow *mainwin);
static void main_window_on_action_about (GtkAction *action, MainWindow *mainwin);
static void main_window_on_action_run (GtkAction *action, MainWindow *mainwin);
static void main_window_on_action_step (GtkAction *action, MainWindow *mainwin);
static void main_window_on_action_break (GtkAction *action, MainWindow *mainwin);

static void main_window_on_stopped (ElectronManager *electron, MainWindow *mainwin);
static void main_window_on_started (ElectronManager *electron, MainWindow *mainwin);

static gpointer parent_class;

static const MainWindowAction main_window_actions[] =
  {
    { "ActionFileMenu", NULL, N_("Menu|_File"), NULL, NULL,
      NULL, NULL },
    { "ActionDebugMenu", NULL, N_("Menu|_Debug"), NULL, NULL,
      NULL, NULL },
    { "ActionHelpMenu", NULL, N_("Menu|_Help"), NULL, NULL,
      NULL, NULL },
    { "ActionQuit", GTK_STOCK_QUIT, N_("MenuFile|_Quit"), NULL,
      "<control>Q", N_("Quit the program"), G_CALLBACK (main_window_on_action_quit) },
    { "ActionRun", NULL, N_("MenuDebug|_Run"), NULL,
      "F5", N_("Start the emulator"), G_CALLBACK (main_window_on_action_run) },
    { "ActionStep", NULL, N_("MenuDebug|_Step"), NULL,
      "F11", N_("Single-step one instruction"), G_CALLBACK (main_window_on_action_step) },
    { "ActionBreak", NULL, N_("MenuDebug|_Break"), NULL,
      "F9", N_("Stop the emulator"), G_CALLBACK (main_window_on_action_break) },
    { "ActionAbout", GTK_STOCK_ABOUT, N_("MenuHelp|_About"), NULL,
      NULL, N_("Display the about box"), G_CALLBACK (main_window_on_action_about) }
  };

static const char main_window_ui_definition[] =
"<ui>\n"
" <menubar name=\"MenuBar\">\n"
"  <menu name=\"FileMenu\" action=\"ActionFileMenu\">\n"
"   <menuitem name=\"Quit\" action=\"ActionQuit\" />\n"
"  </menu>\n"
"  <menu name=\"DebugMenu\" action=\"ActionDebugMenu\">\n"
"   <menuitem name=\"Run\" action=\"ActionRun\" />\n"
"   <menuitem name=\"Step\" action=\"ActionStep\" />\n"
"   <menuitem name=\"Break\" action=\"ActionBreak\" />\n"
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

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = main_window_dispose;
}

static void
main_window_init (MainWindow *mainwin)
{
  GtkWidget *vbox, *menu_bar, *tool_bar, *hbox;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;
  GError *ui_error = NULL;
  int i;
  const MainWindowAction *a;

  mainwin->electron = NULL;
  
  /* Create a GtkVBox to hold the menu and the main display widget */
  vbox = gtk_vbox_new (FALSE, 0);

  /* Create the action group for the menu / toolbar actions */
  action_group = gtk_action_group_new ("MenuActions");
  for (a = main_window_actions, i = 0;
       i < sizeof (main_window_actions) / sizeof (MainWindowAction);
       i++, a++)
  {
    GtkAction *action = gtk_action_new (a->name,
					a->label
					? g_strip_context (a->label, gettext (a->label))
					: NULL,
					a->tooltip
					? g_strip_context (a->tooltip, gettext (a->tooltip))
					: NULL,
					main_window_actions[i].stock_id);
    if (main_window_actions[i].short_label)
      g_object_set (G_OBJECT (action), "short-label",
		    g_strip_context (a->short_label, gettext (a->short_label)), NULL);
    if (main_window_actions[i].callback)
      g_signal_connect (G_OBJECT (action), "activate",
			main_window_actions[i].callback, mainwin);
    gtk_action_group_add_action_with_accel (action_group, action,
					    main_window_actions[i].accelerator);
    g_object_unref (action);
  }

  /* Create the actual widgets */
  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
  if (gtk_ui_manager_add_ui_from_string (ui_manager, main_window_ui_definition,
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
			      gtk_ui_manager_get_accel_group (ui_manager));

  /* Add the menu bar to the vbox */
  if ((menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MenuBar")))
    gtk_box_pack_start (GTK_BOX (vbox), menu_bar, FALSE, TRUE, 0);
  
  /* Add the tool bar to the vbox */
  if ((tool_bar = gtk_ui_manager_get_widget (ui_manager, "/ToolBar")))
    gtk_box_pack_start (GTK_BOX (vbox), tool_bar, FALSE, TRUE, 0);

  /* We no longer need the UI manager or action group */
  g_object_unref (ui_manager);
  g_object_unref (action_group);

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
  gtk_widget_show (mainwin->debugger);
  gtk_box_pack_start (GTK_BOX (hbox), mainwin->debugger, FALSE, TRUE, 0);

  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (mainwin), vbox);

  /* Give focus to the electron widget to start */
  gtk_widget_grab_focus (mainwin->ewidget);
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
				  G_CALLBACK (main_window_on_started), mainwin);
    mainwin->stopped
      = g_signal_connect_swapped (electron, "stopped",
				  G_CALLBACK (main_window_on_stopped), mainwin);
  }

  if (mainwin->ewidget)
    electron_widget_set_electron (ELECTRON_WIDGET (mainwin->ewidget), electron);
  if (mainwin->debugger)
    debugger_set_electron (DEBUGGER (mainwin->debugger), electron);
}

static void
main_window_on_action_quit (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));
  
  gtk_main_quit ();
}

static void
main_window_on_action_about (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  about_dialog_show ();
}

static void
main_window_on_action_run (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_start (mainwin->electron);
}

static void
main_window_on_action_step (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_step (mainwin->electron);
}

static void
main_window_on_action_break (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));

  if (mainwin->electron)
    electron_manager_stop (mainwin->electron);
}

static void
main_window_on_started (ElectronManager *electron, MainWindow *mainwin)
{
}

static void
main_window_on_stopped (ElectronManager *electron, MainWindow *mainwin)
{
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
