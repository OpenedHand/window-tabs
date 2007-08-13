enum {
  _MB_APP_WINDOW_LIST_STACKING,
  _MB_CURRENT_APP_WINDOW,
  UTF8_STRING,
  _NET_WM_NAME,
  _NET_ACTIVE_WINDOW,
  N_ATOMS,
};

extern Atom atoms[N_ATOMS];

void init_atoms (void);

char * get_window_name (Window xwindow);

gboolean
get_window_list_atom (Window   xwindow,
                       Atom     atom,
                       Window **windows,
                       int     *len);

gboolean
get_window_atom (Window  xwindow,
                  Atom    atom,
                  Window *val);

Window get_group_leader (Window window);

char * get_class (Window window);

gboolean compare (const char *s1, const char *s2);
