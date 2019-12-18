#ifndef _NCURSES_ZTFW_H_
#define _NCURSES_ZTFW_H_

#include <glib.h>

#include <curses.h>
#include <menu.h>
#include <panel.h>

#include "../zt-framework.h"
#include "../zt-io.h"


#define MENU_CANCEL 0
#define ITEM_SELECTED 1


/* ANYTHING that uses a refresh(), doupdate(), update_panels() etc.
   MUST lock this mutex!. */
extern GMutex curses_mutex;
#define LOCK_CURSES   g_mutex_lock(&curses_mutex);
#define UNLOCK_CURSES g_mutex_unlock(&curses_mutex);

/* Defined in ncurses.c MANDATORY */
extern zt_framework fw_manifest;

extern int _nc_height, _nc_width;

struct _ncfw_bordered_window {
    WINDOW *parent;         /* The one we use for our border */
    WINDOW *menu_win;       /* For a 'menu', if used */
    WINDOW *content;        /* The one that's panelised and receives content */
    PANEL *content_panel;   /* PANEL for above */
};
typedef struct _ncfw_bordered_window BorderedWindow;

extern PANEL *stdout_panel;
extern PANEL *stderr_panel;

#endif  /*  _NCURSES_ZTFW_H_  */
