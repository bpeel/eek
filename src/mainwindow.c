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

#include "mainwindow.h"
#include "electronmanager.h"
#include "electronwidget.h"
#include "debugger.h"
#include "aboutdialog.h"
#include "intl.h"
#include "breakpointeditdialog.h"
#include "disdialog.h"

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
static void main_window_electron_widget_notify (gpointer data, GObject *obj);
static void main_window_debugger_notify (gpointer data, GObject *obj);

static void main_window_on_quit (GtkAction *action, MainWindow *mainwin);
static void main_window_on_about (GtkAction *action, MainWindow *mainwin);
static void main_window_on_run (GtkAction *action, MainWindow *mainwin);
static void main_window_on_step (GtkAction *action, MainWindow *mainwin);
static void main_window_on_break (GtkAction *action, MainWindow *mainwin);
static void main_window_on_edit_breakpoint (GtkAction *action, MainWindow *mainwin);
static void main_window_on_disassembler (GtkAction *action, MainWindow *mainwin);

static void main_window_update_debug_actions (MainWindow *mainwin);

static void main_window_on_toggle_toolbar (GtkAction *action, MainWindow *mainwin);
static void main_window_on_toggle_debugger (GtkAction *action, MainWindow *mainwin);

static void main_window_forget_dis_dialog (MainWindow *mainwin);

static gpointer parent_class;

static const MainWindowAction main_window_actions[] =
  {
    { "ActionFileMenu", NULL, N_("Menu|_File"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionViewMenu", NULL, N_("Menu|_View"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionDebugMenu", NULL, N_("Menu|_Debug"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionHelpMenu", NULL, N_("Menu|_Help"), NULL, NULL,
      NULL, FALSE, NULL },
    { "ActionQuit", GTK_STOCK_QUIT, N_("MenuFile|_Quit"), NULL,
      NULL, N_("Quit the program"), FALSE, G_CALLBACK (main_window_on_quit) },
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
"  <menu name=\"FileMenu\" action=\"ActionFileMenu\">\n"
"   <menuitem name=\"Quit\" action=\"ActionQuit\" />\n"
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

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = main_window_dispose;
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
  }

  if (mainwin->ewidget)
    electron_widget_set_electron (ELECTRON_WIDGET (mainwin->ewidget), electron);
  if (mainwin->debugger)
    debugger_set_electron (DEBUGGER (mainwin->debugger), electron);

  main_window_update_debug_actions (mainwin);
}

static void
main_window_on_quit (GtkAction *action, MainWindow *mainwin)
{
  g_return_if_fail (IS_MAIN_WINDOW (mainwin));
  
  gtk_main_quit ();
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
