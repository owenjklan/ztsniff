#ifndef _ZT_PLUGIN_H_
#define _ZT_PLUGIN_H_

/** @brief This file houses definitions of data structures and sync. mechanisms
   that plugins require access to. Code the comprises the core framework
   also uses these headers, but must define _ZT_INTERNAL before inclusion
   to allow extensions to some data structures that plugin code doesn't need 

   Written by Owen Klan  -  27th August, 2015
*/

#include <pcap/pcap.h>  /** Plugin developers will need libpcap dev too */
#include <glib.h>       /** We use glib for data primitives and threads */

typedef struct _zt_plugin zt_plugin;

#include "zt-commands.h"  // <- This is here because it will depend on the
                          //    above typedef


/** @brief Structure to define arguments that are passed in to the capture thread.
   In particular:
    - the handle for the async. queue that this thread will push packets
      of interest onto, for the processing thread to read.
    - the pcap_t handle structure that will ultimately be used in a call
      to pcap_loop, with our own filter applied.
    - a function pointer to the pcap_handler style function that will be
      passed to our call to pcap_loop. This will ultimately push packets
      onto the passed-in queue. */
struct cap_thread_args {
    zt_plugin   *self;         /** Passed in by core. DON'T touch!! */
    GAsyncQueue *packet_q; /** Glib Async. Queue to communicate \
			      with proc. thread. */
    pcap_t *pcap_h;        /** Pcap session for this interface/filter */
    pcap_handler capture;  /** Plugin-supplied pcap_loop() callback. */
};

struct proc_thread_args {
    zt_plugin   *self;         /*  Passed in by core. DON'T touch!!  */
    GAsyncQueue *packet_q; /* Glib Async. Queue, to read packets \
			      from cap thread. */
};

/* Utility function to access data structures opaquely */
#define zt_cap_get_queue(a) \
    (GAsyncQueue *)(((struct cap_thread_args *)(a))->packet_q)
#define zt_cap_get_pcap(a) \
    (pcap_t *)(((struct cap_thread_args *)(a))->pcap_h)
#define zt_proc_get_queue(a) \
    (GAsyncQueue *)(((struct proc_thread_args *)(a))->packet_q)

/** 
 @brief
 Definition for handler routine that acts as body of processing thread. */
 @param args Arguments to pass to thread 
typedef void (*packet_handler)(struct proc_thread_args *args);

/** struct  _zt_plugin... 
   @brief This data structure is used to by both the main framework and the plugin
   code. The plugin will define a zt_plugin by the name of 'plugin_manifest'.
   The framework program will look for, and duplicate the info in this
   data structure to keep track of plugin symbols */
  #define ZT_PLUGIN_NAME_MAX   64    /*  Maximum size total bytes (including any required null-terminator byte) for Name Strings */
  #define ZT_PLUGIN_FILTER_MAX  128  /* Maximum size total bytes (including any required null-terminator byte) for BPF Filter code string */

  #define ZT_PLUGIN_TAG_MAX     12   /** Maximum size total bytes (including any required null-terminator byte) for Tag Strings */

struct _zt_plugin {
    char name[ZT_PLUGIN_NAME_MAX]; /** Name of our plugin */
    char tag[ZT_PLUGIN_TAG_MAX]; /** "Tag" string for our plugin */
    char filter[ZT_PLUGIN_FILTER_MAX]; /** BPF Filter code string for our plugin */
    pcap_handler   _f_capture;  /** Address of our function to pass to pcap */
    packet_handler _f_process;  /** Address of our function to read queue that
				   is filled by pcap thread */
    attach_handler _f_attach;   /** Address to call to service the 'attach' request from the UI. */

  #ifdef _ZT_INTERNAL/** Code added only for non-plugin code, ie. The core */
    void *handle;               /** Handle used by the dynamic loader. */
    struct _zt_plugin *next;    /** List-maintenance pointer for loaded plugins */
  #endif /** _ZT_INTERNAL */
};   /**  struct  _zt_plugin  */

/** @brief Utility functions provided by the core code */
extern void ztl_call_master(char *tag, GThread *tid, master_cmd c, void *a);

#endif /*  _ZT_PLUGIN_H_  */
