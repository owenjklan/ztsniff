
#include <stdarg.h>

#include "../zt-io.h"
#include "../zt-framework.h"
#include "ncurses-ztfw.h"

extern BorderedWindow *generate_bordered_panel(int w, int h, int x, int y, 
					       char *title, char *subtitle);


void ztf_dialog_vcat(zt_dialog *dw, char *format, ...) {
    WINDOW *dwin = (WINDOW *)((BorderedWindow *)dw->fw_ptr)->content;
    
    va_list arg_ptr;
    
    va_start(arg_ptr, format);
        
    vwprintw(dwin, format, arg_ptr);
}

int ztf_dialog_show(zt_dialog *dw) {
    if (!dw)  return -1;

    LOCK_CURSES;

    WINDOW *dwin = (WINDOW *)((BorderedWindow *)dw->fw_ptr)->content;
    
    wprintw(dwin, "%s\n", dw->text);

    show_panel(((BorderedWindow *)dw->fw_ptr)->content_panel);
//    wrefresh(((BorderedWindow *)lw->fw_ptr)->content);

    update_panels();
    doupdate();

    int dialog_ret = nc_dialog_driver(dw);

    int ret_val = 1;

    /* If we actually selected something, return the associated
       'user data', otherwise NULL to indicate 'cancel' */
    if (dialog_ret == 1) {
	ret_val = 1;
    } else {
	ret_val = 0;
    }

    /* hide panel, don't destroy menu items */
    hide_panel(((BorderedWindow *)dw->fw_ptr)->content_panel);
    show_panel(stdout_panel);
    show_panel(stderr_panel);
    update_panels();
    doupdate();

    UNLOCK_CURSES;
    return ret_val;
}


zt_dialog *ztf_new_dialog(char *title, char *text,
			  int width, int height, 
			  int x, int y, int *exit_val,
			  zt_list_callback select) {
    BorderedWindow *bw = NULL;
    zt_dialog *ret_dialog;
    ret_dialog = (zt_dialog *)malloc(sizeof(zt_dialog));;

    if (ret_dialog == NULL) {
	/* Failed allocating memory for new window! */
	return NULL;
    }

    if (title) ret_dialog->title = strdup(title);
    else       ret_dialog->title = strdup("<NO TITLE GIVEN>");
    if (text) ret_dialog->text = strdup(text);
    else          ret_dialog->text = strdup("<NO TEXT PROVIDED>\n");

    ret_dialog->owner = g_thread_self();

    /* Generate bordered window, receive back as PANEL */
    bw = (BorderedWindow *)generate_bordered_panel(width, height, x, y, 
						   title, NULL);
    ret_dialog->fw_ptr = (void *)bw;
    ret_dialog->select = (zt_list_callback)select;

    return ret_dialog;
}
