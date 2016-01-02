/* Basic ZTSniff I/O Framework that handles the
   Standard C/Unix I/O Streams:  stdin(not yet), stdout & stdarg

   ALL FRAMEWORKS MUST SUPPLY:
   - init_framework() - routine to initialise framework. If nothing
   is to be done, then a stub function must be supplied.
   - error_handler    - routine to handle output to the "Error stream"
   - output_handler   - routine to handle output to the "Output Stream" */
#include "zt-io.h"
#include "zt-framework.h"
#include "ncurses-ztfw.h"

#include <errno.h>

GThreadFunc Stderr_Handler (gpointer data);
GThreadFunc Stdout_Handler (gpointer data);
GThreadFunc Input_Handler(gpointer data);

WINDOW *std_win = NULL;
WINDOW *_stdout = NULL, *_stderr = NULL;
WINDOW *stdout_win = NULL, *stderr_win = NULL;
WINDOW *focus_win;
PANEL *stdout_panel = NULL, *stderr_panel = NULL;

extern zt_plugin *_g_plugin_list;

extern GTree *_g_key_bindings;

zt_framework fw_manifest = {
    .name = "NCurses Basic I/O Framework",
    .tag  = "NCurse\0",
    ._f_stdout = (GThreadFunc)Stdout_Handler,
    ._f_stderr = (GThreadFunc)Stderr_Handler,
    ._f_input  = (GThreadFunc)Input_Handler
};

/* Ncurses output mutex */
GMutex curses_mutex;

int _nc_height = 0, _nc_width = 0;

int init_framework() {
    g_mutex_lock(&curses_mutex);

    std_win = initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    start_color();

    /* Remove visibility of cursor */
    curs_set(0);

    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLUE);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_BLACK, COLOR_WHITE);
    init_pair(7, COLOR_BLACK, COLOR_YELLOW);

    /* Determine window height */
    getmaxyx(std_win, _nc_height, _nc_width);
#define MAX_H  (_nc_height)
#define WIN_H  (MAX_H/2)
#define WIN_W  (_nc_width)
#define PAD_H  (WIN_H - 2)
#define PAD_W  (WIN_W - 2)
    /* Create window with border */
    _stdout =  newwin(WIN_H, WIN_W, 0, 0);
    stdout_panel = new_panel(_stdout);
    wattron(_stdout, COLOR_PAIR(3)|A_BOLD);
    box(_stdout, ACS_VLINE, ACS_HLINE);
    wrefresh(_stdout);
    wattroff(_stdout, COLOR_PAIR(3));
    stdout_win = derwin(_stdout, PAD_H, PAD_W, 1, 1);
    wbkgd(stdout_win, COLOR_PAIR(2)|A_BOLD);
    werase(stdout_win);

    wattron(_stdout, A_UNDERLINE);
    mvwprintw(_stdout, 0, 2, "  Standard Output ");
    wattroff(_stdout, A_UNDERLINE);
    
    /* Set background for 'stdout' panel */
    wattron(stdout_win, COLOR_PAIR(2));
    mvwprintw(stdout_win, 0, 0, "Program Started\n");

    wsetscrreg(stdout_win, 0, PAD_H);
    scrollok(stdout_win, TRUE);
    wrefresh(_stdout);
    wrefresh(stdout_win);

    _stderr = newwin(WIN_H, WIN_W, WIN_H, 0);
    stderr_panel = new_panel(_stderr);

    wattron(_stderr, COLOR_PAIR(3)|A_BOLD);
    box(_stderr, ACS_VLINE, ACS_HLINE);
    wattroff(_stderr, COLOR_PAIR(3));
    stderr_win = derwin(_stderr, PAD_H, _nc_width-2, 1, 1);

    wattron(_stderr, A_UNDERLINE);
    mvwprintw(_stderr, 0, 2, "  Standard Error ");
    wattroff(_stderr, A_UNDERLINE);

    /* Set background for 'stderr' stream panel */
    wbkgd(stderr_win, COLOR_PAIR(1)|A_BOLD);
    werase(stderr_win);
    wattron(stderr_win, COLOR_PAIR(1));
    mvwprintw(stderr_win, 0, 0, "Error Console Ready\n");

    wsetscrreg(stderr_win, 0, PAD_H);
    scrollok(stderr_win, TRUE);
    wrefresh(_stderr);
    wrefresh(stderr_win);

    focus_win = stdout_win;
    mvwprintw(_stdout, 0, 2, "#");

    update_panels();
    refresh();
    doupdate();

    g_mutex_unlock(&curses_mutex);

    return 0;
}

GThreadFunc Stderr_Handler (gpointer data) {
    zt_outmsg *out_msg;
    
    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stderr);

	g_mutex_lock(&curses_mutex);

	if (strcmp(out_msg->pi_tag, " CORE ") == 0) {
	    wattron(stderr_win, COLOR_PAIR(5)|A_BOLD);
	    wprintw(stderr_win, "[%-8s] %s", out_msg->pi_tag, out_msg->content);
	    wrefresh(stderr_win);
	    wattron(stderr_win, COLOR_PAIR(1)|A_BOLD);
	} else {
	    wprintw(stderr_win, "[%-8s] %s", out_msg->pi_tag, out_msg->content);
	}
	wrefresh(stderr_win);
	doupdate();

	g_mutex_unlock(&curses_mutex);

	free(out_msg->content);
	free(out_msg);
    }
}

GThreadFunc Stdout_Handler (gpointer data) {
    zt_outmsg *out_msg; 
   
    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stdout);

	g_mutex_lock(&curses_mutex);

	if (strncmp(out_msg->pi_tag, " CORE ", 6) == 0) {
	    wattron(stdout_win, COLOR_PAIR(4));
	    wprintw(stdout_win, "[%-8s] %s", out_msg->pi_tag, out_msg->content);
	    wrefresh(stdout_win);
	    wattroff(stdout_win, COLOR_PAIR(4));
	    wattron(stdout_win, COLOR_PAIR(2)|A_BOLD);
	} else {
	    wprintw(stdout_win, "[%-8s] %s", out_msg->pi_tag, out_msg->content);
	    wrefresh(stdout_win);
	}

	g_mutex_unlock(&curses_mutex);

	free(out_msg->content);
	free(out_msg);
    }
}

void change_stdio_focus() {    
    if (focus_win == stderr_win) {
	mvwprintw(_stderr, 0, 2, " ");
	mvwprintw(_stdout, 0, 2, "#");
	focus_win = stdout_win;
    } else if (focus_win == stdout_win) {
	mvwprintw(_stdout, 0, 2, " ");
	mvwprintw(_stderr, 0, 2, "#");
	focus_win = stderr_win;
    }
    wrefresh(_stderr);
    wrefresh(_stdout);
}

void redraw_background() {
    g_mutex_lock(&curses_mutex);

    wattron(_stderr, COLOR_PAIR(3)|A_BOLD);
    box(_stderr, ACS_VLINE, ACS_HLINE);
    wattroff(_stderr, COLOR_PAIR(3));
    mvwprintw(_stderr, 0, 2, "  Standard Error ");               

    wattron(_stdout, COLOR_PAIR(3)|A_BOLD);
    box(_stdout, ACS_VLINE, ACS_HLINE);
    wattroff(_stdout, COLOR_PAIR(3));
    mvwprintw(_stdout, 0, 2, "  Standard Output ");
    wattron(stdout_win, COLOR_PAIR(2));
    wrefresh(_stdout);
    wrefresh(_stderr);
    g_mutex_unlock(&curses_mutex);
}


int nc_dialog_driver(zt_dialog *dw) {
    int c;

    while((c = getch()) != 'q'/*27*/) {
	switch(c) {
	case 10:
	    return ITEM_SELECTED;
	case 'q':
	case 'Q':
	    return 0;
	}
    }
}


int nc_menu_driver(MENU *m) {
    int c;
    
    while((c = getch()) != 'q'/*27*/) {
	switch(c) {
	case KEY_DOWN:                                                  
	    menu_driver(m, REQ_SCR_DLINE);
	    menu_driver(m, REQ_DOWN_ITEM);                    
	    break;                                          
	case KEY_UP:                                            
	    menu_driver(m, REQ_SCR_ULINE);
	    menu_driver(m, REQ_UP_ITEM);
	    break;                                          
	case 10:
	    return ITEM_SELECTED;
	}
    }
}

/* Uses getch() to read a key. Binds to commands. */
GThreadFunc Input_Handler(gpointer data) { 
    int read_ch = 0;
    zt_key_command zt;

    while (1) {
	read_ch = getch();

	switch (read_ch) {
	case 'Q':
	case 'q':
	    ztl_call_master(FWTAG, g_thread_self(), MC_EXIT, NULL);
	    break;
	case '\t':
	    g_mutex_lock(&curses_mutex);
	    change_stdio_focus();
	    g_mutex_unlock(&curses_mutex);
	    break;
	default:
	    /* Look for function in our key bindings, given this key */
	    zt = (zt_key_command)g_tree_lookup(_g_key_bindings,
					       (gpointer)read_ch);

	    /* if zt() is valid, call it */
	    if (zt) {
		zt();
	    } else {
		zterror(FWTAG, "Unbound key:  '%c'\n", read_ch);
	    }
	}; /*  switch (read_ch) {  */
    }
}

/* Function to map a standard key symbol, in ZT's eyes (eg. ZT_ESC for Escape)
   to the UI module's value. */
int ztf_map_ui_key(zt_key_sym ztkey) {
    switch (ztkey) {
    case ZT_KEY_F9:           return KEY_F(9);
    case ZT_KEY_ENTER:        return KEY_ENTER;
    case ZT_KEY_TAB:          return '\t';
    case ZT_KEY_F10:          return KEY_F(10);
    }

    /* Unmapped key! */
    return 0;
}
