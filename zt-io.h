#ifndef _ZT_IO_H_
#define _ZT_IO_H_

#include "project-includes.h"

#include "zt-io.h"

#define ZT_STDOUT_MAX_SIZE  256

/** @brief Define handles used for I/O Framework's
   emulation of standard streams. These will
   be buffered Async Queues of messages. */
extern GAsyncQueue *Stdin;
extern GAsyncQueue *Stdout;
extern GAsyncQueue *Stderr;

/** @brief Data structure defining data associated with a menu item as
   used in a ZT "List Window". Holds display text plus */
struct _zt_list_window_item {
    char *text;      /** list display text */
    char *descr;     /** Optional description */
    void *userdata;  /** Data attached to this item */
};
typedef struct _zt_list_window_item zt_list_window_item;

typedef void (*zt_list_callback)(void *userdata);

typedef void (*zt_key_command)();

/** @brief A "List Window" is a widget that provides a window with
   a column menu of possible selections. */
struct _zt_list_window {
    char *title;       /** Title text */
    char *subtitle;     /** Optional subtitle */
    void *fw_ptr;       /** Framework-specific window pointer */
    GThread *owner;     /** ID of thread that called this */
    GSList *items;      /** linked list of 'zt_list_window_items' */

    zt_list_callback  select;
};
typedef struct _zt_list_window zt_list_window;

/* A "List Window" is a widget that provides a window with
   a column menu of possible selections. */
struct _zt_dialog {
    char *title;       // Title text
    char *text;    // Optional subtitle
    void *fw_ptr ;      // Framework-specific window pointer
    GThread *owner;    // ID of thread that called this

    zt_list_callback  select;
};
typedef struct _zt_dialog zt_dialog;


/* A "General Window" is a widget that provides a window with
   a scroll-buffer of */
struct _zt_window {
    char *title;       // Title text
    char *subtitle;    // Optional subtitle
    void *fw_ptr;      // Framework-specific window pointer
    GThread *owner;    // ID of thread that called this
    GList *items;     // double-linked list of 'zt_list_window_items'

    zt_list_callback  delete;  /* Callback for 'delete' event */
    zt_list_callback  select;  /* Callback for 'select' ie. enter */
    
};
typedef struct _zt_list_window zt_list_window;

extern void zt_load_framework(char *path);

/* Generic replacements for 'printf' */
#define MYTAG  (manifest.tag)  /* Useable by plugins */
#define FWTAG  (fw_manifest.tag) /* Useable by the I/O Framework */
extern void ztprint(char *tag, char *f, ...);
extern void zterror(char *tag, char *f, ...);

/* Definition of an out-going I/O Framework
   message. */
struct _zt_outmsg {
    char pi_tag[ZT_PLUGIN_TAG_MAX];
    char *content;
};
typedef struct _zt_outmsg zt_outmsg;


#endif  /* _ZT_IO_H_ */
