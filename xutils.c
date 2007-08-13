#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include "xutils.h"

Atom atoms[N_ATOMS];

static char*
text_property_to_utf8 (const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;
  
  list = NULL;

  count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    retval = NULL;
  else
    {
      retval = list[0];
      list[0] = g_strdup (""); /* something to free */
    }

  g_strfreev (list);

  return retval;
}

static char*
_wnck_get_utf8_property (Window  xwindow,
                         Atom    atom)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;
  int err, result;
  char *retval;
  Atom utf8_string;

  utf8_string = atoms[UTF8_STRING];
  
  gdk_error_trap_push ();
  type = None;
  val = NULL;
  result = XGetWindowProperty (gdk_display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **)&val);  
  err = gdk_error_trap_pop ();

  if (err != Success ||
      result != Success)
    return NULL;
  
  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      g_warning ("Property contained invalid UTF-8");
      XFree (val);
      return NULL;
    }
  
  retval = g_strndup (val, nitems);
  
  XFree (val);
  
  return retval;
}

static char*
_wnck_get_text_property (Window  xwindow,
                         Atom    atom)
{
  XTextProperty text;
  char *retval;
  
  gdk_error_trap_push ();

  text.nitems = 0;
  if (XGetTextProperty (gdk_display,
                        xwindow,
                        &text,
                        atom))
    {
      retval = text_property_to_utf8 (&text);

      if (text.value)
        XFree (text.value);
    }
  else
    {
      retval = NULL;
    }
  
  gdk_error_trap_pop ();

  return retval;
}

char*
get_window_name (Window xwindow)
{
  char *name;
  
  name = _wnck_get_utf8_property (xwindow, atoms[_NET_WM_NAME]);
  
  if (name == NULL)
    name = _wnck_get_text_property (xwindow, XA_WM_NAME);

  return name;
}

gboolean
get_window_atom (Window  xwindow,
                  Atom    atom,
                  Window *val)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  *val = 0;
  
  gdk_error_trap_push ();
  type = None;
  result = XGetWindowProperty (GDK_DISPLAY(),
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&w);  
  err = gdk_error_trap_pop ();
  if (err != Success ||
      result != Success)
    return FALSE;
  
  if (type != XA_WINDOW)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;
  
  XFree (w);

  return TRUE;
}

gboolean
get_window_list_atom (Window   xwindow,
                       Atom     atom,
                       Window **windows,
                       int     *len)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *data;
  int err, result;

  *windows = NULL;
  *len = 0;
  
  gdk_error_trap_push ();
  type = None;
  result = XGetWindowProperty (gdk_display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&data);  
  err = gdk_error_trap_pop ();
  if (err != Success ||
      result != Success)
    return FALSE;
  
  if (type != XA_WINDOW)
    {
      XFree (data);
      return FALSE;
    }

  *windows = g_new (Window, nitems);
  memcpy (*windows, data, sizeof (Window) * nitems);
  *len = nitems;
  
  XFree (data);

  return TRUE;  
}

#if 0
Window
get_group_leader (Window window)
{
  XWMHints *hints;
  Window leader = None;

  g_return_val_if_fail (window, None);

  gdk_error_trap_push();

  hints = XGetWMHints(GDK_DISPLAY(), window);
  if (!hints)
    return None;

  if (gdk_error_trap_pop ()) {
    g_warning ("X error");
    return None;
  }

  if (hints->flags & WindowGroupHint)
    leader = hints->window_group;
  
  XFree (hints);

  return leader;
}
#endif

char *
get_class (Window window)
{
  XClassHint hints;
  char *class;
  
  g_return_val_if_fail (window, NULL);

  gdk_error_trap_push ();
  if (!XGetClassHint (GDK_DISPLAY (), window, &hints)) {
    return NULL;
  }

  if (gdk_error_trap_pop ())
    return NULL;

  class = g_strdup (hints.res_class);
  
  XFree (hints.res_name);
  XFree (hints.res_class);

  return class;
}

void
init_atoms (void)
{
  atoms[_MB_APP_WINDOW_LIST_STACKING] =
    gdk_x11_get_xatom_by_name ("_MB_APP_WINDOW_LIST_STACKING");
  atoms[_MB_CURRENT_APP_WINDOW] =
    gdk_x11_get_xatom_by_name ("_MB_CURRENT_APP_WINDOW");
  atoms[UTF8_STRING] =
    gdk_x11_get_xatom_by_name ("UTF8_STRING");
  atoms[_NET_WM_NAME] =
    gdk_x11_get_xatom_by_name ("_NET_WM_NAME");
  atoms[_NET_ACTIVE_WINDOW] =
    gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW");  
}

gboolean
compare (const char *s1, const char *s2)
{
  if (s1 == NULL && s2 == NULL)
    return TRUE;

  if (s1 == NULL || s2 == NULL)
    return FALSE;

  return strcmp (s1, s2) == 0;
}

