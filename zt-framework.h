#ifndef _ZT_FRAMEWORK_H_
#define _ZT_FRAMEWORK_H_

#include <glib.h>

#include "zt-io.h"

#define ZT_FWORK_NAME_MAX       64
#define ZT_FWORK_TAG_MAX        12

struct _zt_framework {
    char name[ZT_FWORK_NAME_MAX];
    char tag[ZT_FWORK_TAG_MAX];
    GThreadFunc _f_stdout;
    GThreadFunc _f_stderr;
    GThreadFunc _f_input;
};
typedef struct _zt_framework zt_framework;

extern GAsyncQueue *Stdin;
extern GAsyncQueue *Stdout;
extern GAsyncQueue *Stderr;

/* Enum for ZT-generic Key name symbols */
enum _zt_key_sym {ZT_KEY_F1, ZT_KEY_F2, ZT_KEY_F3, ZT_KEY_F4, ZT_KEY_F5,
		  ZT_KEY_F6, ZT_KEY_F7, ZT_KEY_F8, ZT_KEY_F9, ZT_KEY_F10,
		  ZT_KEY_ENTER, ZT_KEY_TAB, ZT_KEY_ESC};
typedef enum _zt_key_sym zt_key_sym;

/* Generic UI widgets and functions */
/* These are to be supplied by a framework plugin */
/* Functions for 'ZT List Window' */
extern zt_list_window *ztf_new_list_window(char *title, char *subtitle,
					   int width, int height,
					   int x, int y,
					   zt_list_callback select)
    __attribute__((weak));;

extern void ztf_list_window_add(zt_list_window *lw, char *text,
				char *descr, void *userdata)
    __attribute__((weak));;

extern void *ztf_list_window_show(zt_list_window *lw)
    __attribute__((weak));;

extern void ztf_list_window_destroy(zt_list_window *lw)
    __attribute__((weak));;

extern int  ztf_map_ui_key(zt_key_sym key)
    __attribute__((weak));;

extern zt_dialog *ztf_new_dialog(char *title, char *text,
				 int width, int height,
				 int x, int y, int *exit_val,
				 zt_list_callback select)
    __attribute__((weak));;

extern int ztf_dialog_show(zt_dialog *dw)
    __attribute__((weak));;

extern void ztf_dialog_vcat(zt_dialog *dw, char *format, ...)
    __attribute__((weak));;

#endif  /*  _ZT_FRAMEWORK_H_  */
