#ifndef _ZT_CORE_H_
#define _ZT_CORE_H_

/* HISTORY:
 *  9th Oct. 2015  -  Added version symbols
 */

#include "zt-commands.h"

#include <glib.h>

/* Version information symbols */
#define ZT_VER_MAJOR    0
#define ZT_VER_MINOR    10
#define ZT_VER_CODENAME "spaceman"


/* Utility functions that should only be used by core code. */
extern zt_plugin *_zt_map_plugin_from_tag(char *tag);
extern zt_plugin *_zt_map_plugin_from_tid(GThread *tid);

/* Definition of Core Configuration data structure and support
   mutex. Structure populated in  zt-config.c  */
struct _zt_config {
    char *plugin_base;              /* Base directory for plugins */
    char *profile_file;             /* Path to profile file */
    int  thread_cap;                /* maximum threads to launch */
};
typedef struct _zt_config zt_config;

extern zt_config  _g_config_data;
extern GMutex     zt_config_mutex;

/* Definition of data structure to hold details of
   a generic, minimalist Producer/Consumer thread
   relationship. */
struct _zt_thread_pair {
    GThread      *producer;
    GThread      *consumer;
    GAsyncQueue  *comm_queue;

    zt_plugin    *parent_pi;   /* Handle for ZT Plug-in
				  that is the parent of this
				  pair */
};
typedef struct _zt_thread_pair zt_thread_pair;

/* Parameters structre that is used to send commands
   back to the master thread. Much of this is populated
   by helper macros from zt-plugin.h */
struct _zt_master_call_args {
    char    *tag;          /* Either plugin or framework tag */
    GThread *thread_id;
    master_cmd command;
    gpointer cmd_args;
};
typedef struct _zt_master_call_args zt_master_call_args;

#endif
