#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "xutils.h"

static GtkWidget *panel, *box;
static Window current_toplevel = None;
static char *current_class = NULL;
static int current_count = 0;
/* Hash of XIDs to GtkButtons */
static GHashTable *window_hash;

static void update_panel (void);

/*
 * Callback from clicking on a button on the panel, to change the active
 * application.
 */
static void
on_button_clicked (GtkWidget *button, gpointer user_data)
{
  XEvent xev;
  Window window, toplevel;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;
  
  window = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "window-id"));
  toplevel = GDK_WINDOW_XWINDOW (gtk_widget_get_toplevel (button)->window);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY ();
  xev.xclient.window = window;
  xev.xclient.message_type = atoms[_NET_ACTIVE_WINDOW]; 
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 2;
  xev.xclient.data.l[1] = gtk_get_current_event_time ();
  xev.xclient.data.l[2] = toplevel;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  
  XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
              False, SubstructureRedirectMask, &xev);
}

/*
 * Called when an event happens on a window we are monitoring, so that we can
 * update the title on the panel.
 */
static GdkFilterReturn
window_filter_func (GdkXEvent *gdkxevent, GdkEvent *event, gpointer data)
{
  XEvent *xevent = gdkxevent;
  Window window;
  GtkButton *button;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.state == PropertyNewValue &&
      (xevent->xproperty.atom == XA_WM_NAME ||
       xevent->xproperty.atom == atoms[_NET_WM_NAME])) {

    window = xevent->xproperty.window;
    button = g_hash_table_lookup (window_hash, GINT_TO_POINTER (window));
    if (button)
      gtk_button_set_label (button, get_window_name (window));
  }

  return GDK_FILTER_CONTINUE;
}

static void
remove_filter (gpointer data)
{
  GdkWindow *window;
  window = gdk_window_foreign_new (GPOINTER_TO_INT (data));
  if (window) {
    gdk_window_remove_filter (window, window_filter_func, NULL);
    g_object_unref (window);
  }
}

static GSList *
create_button (GSList *group, Window window, GtkContainer *box)
{
  GdkWindow *gdk_win;
  GtkWidget *button;
  GdkEventMask events;
  
  button = gtk_radio_button_new_with_label (group, get_window_name (window));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), window == current_toplevel);
  g_signal_connect (button, "toggled", G_CALLBACK (on_button_clicked), NULL);
  gtk_widget_show (button);
  gtk_container_add (box, button);

  gdk_win = gdk_window_foreign_new (window);
  events = gdk_window_get_events (gdk_win);
  if ((events & GDK_PROPERTY_CHANGE_MASK) == 0) {
    gdk_window_set_events (gdk_win, events & GDK_PROPERTY_CHANGE_MASK);
  }
  gdk_window_add_filter (gdk_win, window_filter_func, NULL);
  g_object_set_data_full (G_OBJECT (button), "window-id", GINT_TO_POINTER (window), remove_filter);
  g_object_unref (gdk_win);

  g_hash_table_insert (window_hash, GINT_TO_POINTER (window), button);
  return gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
}

static void
populate_panel (Window *windows, int win_count)
{
  GList *group = NULL, *l;
  GSList *radio_group = NULL;
  int i;

  /* Start by clearing the panel */
  gtk_container_foreach (GTK_CONTAINER (box), (GtkCallback)gtk_widget_destroy, NULL);
  g_hash_table_remove_all (window_hash);
  
  for (i = 0; i < win_count; i++) {
    char *this_class;
    this_class = get_class (windows[i]);
    if (compare (this_class, current_class)) {
      group = g_list_prepend (group, GINT_TO_POINTER (windows[i]));
    }
    g_free (this_class);
  }
  
  if (g_list_length (group) == 1) {
    /* The group contains one window, which is the window we started with, so
       hide the panel */
    gtk_widget_hide (panel);
    g_list_free (group);
    return;
  }

  for (l = group; l ; l = l->next) {
    Window w = GPOINTER_TO_INT (l->data);
    radio_group = create_button (radio_group, w, GTK_CONTAINER (box));
  }

  g_list_free (group);

  gtk_widget_show_all (panel);
}

static void
update_panel ()
{
  Window *windows = NULL;
  Window old_toplevel = current_toplevel;
  char *old_class = current_class;
  int old_count = current_count;

  if (!get_window_atom (GDK_ROOT_WINDOW(),
                        atoms[_MB_CURRENT_APP_WINDOW],
                        &current_toplevel)) {
    g_warning ("Could not get top level window ID");
    gtk_widget_hide (panel);
    return;
  }
  
  if (current_toplevel == None) {
    gtk_widget_hide (panel);
    return;
  }
  
  /* This can fail if the window has just been destroyed */
  current_class = get_class (current_toplevel);

  if (!current_class) {
    gtk_widget_hide (panel);
    return;
  }

  get_window_list_atom (GDK_ROOT_WINDOW (),
                        atoms[_MB_APP_WINDOW_LIST_STACKING],
                        &windows, &current_count);
  
  if (current_count != old_count || !compare (current_class, old_class)) {
    /* Rebuild the panel is new windows have appeared, or we have switched
       class */
    populate_panel (windows, current_count);
  } else {
    /* Otherwise re-select the active application */
    GtkToggleButton *button;
    button = g_hash_table_lookup (window_hash, GINT_TO_POINTER (current_toplevel));
    gtk_toggle_button_set_active (button, TRUE);
  }

  g_free (windows);
  g_free (old_class);
}

/*
 * Called when an event occurs on the root window, used to keep track of the
 * currently focused window.
 */
static GdkFilterReturn
root_filter_func (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  XEvent *xev = (XEvent *) xevent;
  
  if (xev->type == PropertyNotify) {
    if (xev->xproperty.atom == atoms[_MB_CURRENT_APP_WINDOW]) {
      /* If the current application window changed, update the panel */
      update_panel ();
    }
  }
  
  return GDK_FILTER_CONTINUE;
}

int
main (int argc, char **argv)
{
  GdkEventMask events;
  GdkWindow *root;

  gtk_init (&argc, &argv);
  window_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  init_atoms ();

  /* Listen to property changes on the root window, so we know about the window
     list */
  root = gdk_get_default_root_window ();
  events = gdk_window_get_events (root);
  if ((events & GDK_PROPERTY_CHANGE_MASK) == 0) {
    gdk_window_set_events (root, events | GDK_PROPERTY_CHANGE_MASK);
  }
  gdk_window_add_filter (root, root_filter_func, NULL);

  /* Create the panel */
  panel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (panel), "Window Tabs");
  /* This tells matchbox to position it horizontally */
  gtk_window_set_default_size (GTK_WINDOW (panel), 10, 5);
  gtk_widget_realize (panel);
  gdk_window_set_type_hint (panel->window, GDK_WINDOW_TYPE_HINT_DOCK);
  box = gtk_hbox_new (TRUE, 4);
  gtk_container_add (GTK_CONTAINER (panel), box);
  
  update_panel ();
  
  gtk_main ();

  return 0;
}
