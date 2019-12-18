#include "ncurses-ztfw.h"

BorderedWindow *generate_bordered_panel(int w, int h, int x, int y,
					char *title, char *subtitle) {
    BorderedWindow *bw = (BorderedWindow *)malloc(sizeof(BorderedWindow));
    
    if (!bw) {
	zterror(fw_manifest.tag,
		"Failed allocating memory for new BorderedWindow!\n");
	return NULL;
    }

    LOCK_CURSES;

    WINDOW *_pi;
    WINDOW *menu_win;
    WINDOW *pi_win;
    PANEL  *pi_panel = NULL;

    _pi = subwin(stdscr, h, w, y, x);
    pi_win = derwin(_pi, h-2, w-2, 1, 1);
    pi_panel = new_panel(_pi/*pi_win*/);

    wattron(pi_win, A_BOLD|COLOR_PAIR(3));
    wbkgd(pi_win, COLOR_PAIR(6));
    werase(_pi);
    werase(pi_win);

    wattron(_pi, A_BOLD|A_UNDERLINE|COLOR_PAIR(3));
    box(_pi, ACS_VLINE, ACS_HLINE);
    if (title)
	mvwprintw(_pi, 0, 2, " %s ", title);

    if (subtitle) {
	wattron(_pi, A_BOLD);
	mvwprintw(_pi, h-1, 2, " %s ", subtitle);
	wattroff(_pi, A_BOLD);
    }

    wrefresh(_pi);
    wrefresh(pi_win);

    hide_panel(pi_panel);
    update_panels();

    doupdate();

    bw->parent = _pi;
    bw->content = pi_win;
    bw->content_panel = pi_panel;

    UNLOCK_CURSES;

    return (void *)bw;
}

zt_list_window *ztf_new_list_window(char *title, char *subtitle,
				    int width, int height,
				    int x, int y,
				    zt_list_callback select) {
    zt_list_window *ret_window;
    ret_window = (zt_list_window *)malloc(sizeof(zt_list_window));;

    if (ret_window == NULL) {
	/* Failed allocating memory for new window! */
	return NULL;
    }

    if (title) ret_window->title = strdup(title);
    else       ret_window->title = NULL;
    if (subtitle) ret_window->subtitle = strdup(subtitle);
    else          ret_window->subtitle = NULL;
    ret_window->owner = g_thread_self();
    ret_window->items = NULL;
    /* Generate bordered window, receive back as PANEL */
    BorderedWindow *bw =
	generate_bordered_panel(width, height, x, y,
				title, subtitle);
    ret_window->fw_ptr = bw;
    ret_window->select = (zt_list_callback)select;

    return ret_window;
}

/* Destroy resources associated with a list window */
void ztf_list_window_destroy(zt_list_window *lw) {
    
}

/* Add a new item to the list window. */
void ztf_list_window_add(zt_list_window *lw, char *text,
			 char *descr, void *userdata) {
    zt_list_window_item *item = NULL ;
    int  item_size = sizeof(zt_list_window_item);
    char *text_arg, *descr_arg;

    item = (zt_list_window_item *)malloc(item_size);

    if (item == NULL) {
	zterror(" CORE ", "!!! Couldn't allocate memory for item!\n");
	return;
    }

    if (text)
	text_arg = text;
    else
	text_arg = "<null>";

    if (descr)
	descr_arg = descr;
    else
	descr_arg = "<null>";

    item->text = strdup(text_arg);
    item->descr = strdup(descr_arg);
    item->userdata = userdata;

    /* Add new item to list window's 'item list' */
    lw->items = g_slist_prepend(lw->items, (gpointer)item);
}

/* Take a 'list window' that has had items added to it then await a
   selection and return the associated 'userdata' handle */
void *ztf_list_window_show(zt_list_window *lw) {
    if (!lw)  return NULL;

    LOCK_CURSES;
    
    /* Create an array for our ITEMs */
    int num_items = g_slist_length(lw->items);
    ITEM **list_items;

    /* Reverse list */
    lw->items = g_slist_reverse(lw->items);

    list_items = (ITEM **)malloc(sizeof(ITEM *) * num_items+1);

    /* Add items to list for ncurses 'menu' */
    int i = 0;
    for (i = 0; i < num_items; i++) {
	zt_list_window_item *li = 
	    (zt_list_window_item *)g_slist_nth_data(lw->items, i);
	char *text, *descr;

	text = li->text;
	descr = li->descr;
	zterror(fw_manifest.tag, "li->text:  '%s'  li->descr:  '%s'\n",
		text, descr);

	list_items[i] = (ITEM *)new_item(text, descr);
	if (list_items[i] == NULL) {
	    zterror(fw_manifest.tag, "NULL in menu! i = %d\n", i);
	    if (errno == E_BAD_ARGUMENT) {
		zterror("XXX", "BAD ARG\n");
	    }
	}
	set_item_userptr(list_items[i], li->userdata);
    }
    list_items[num_items] = (ITEM *)NULL;

    MENU *my_menu = new_menu((ITEM **)list_items);

    set_menu_fore(my_menu, COLOR_PAIR(7));
    set_menu_back(my_menu, COLOR_PAIR(4));
    set_menu_mark(my_menu, "-> ");

    set_menu_win(my_menu, ((BorderedWindow *)lw->fw_ptr)->menu_win);
    set_menu_sub(my_menu, ((BorderedWindow *)lw->fw_ptr)->content);

    post_menu(my_menu);

    show_panel(((BorderedWindow *)lw->fw_ptr)->content_panel);
//    wrefresh(((BorderedWindow *)lw->fw_ptr)->content);

    update_panels();
    doupdate();

    int menu_ret = nc_menu_driver(my_menu);
    void *ret_ptr = NULL;

    /* If we actually selected something, return the associated
       'user data', otherwise NULL to indicate 'cancel' */
    if (menu_ret == ITEM_SELECTED) {
	ret_ptr = item_userptr(current_item(my_menu));
    } else {
	ret_ptr = NULL;
    }

    /* hide panel, don't destroy menu items */
    hide_panel(((BorderedWindow *)lw->fw_ptr)->content_panel);
    show_panel(stdout_panel);
    show_panel(stderr_panel);
    update_panels();
    doupdate();

    UNLOCK_CURSES;
    return ret_ptr;
}
