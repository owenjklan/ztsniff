/* Main source file for ZT Sniff.

   Written by Owen Klan  -  27th August, 2015 */

/* HISTORY:
   9th Oct. 2015  - Now that 'List Windows' work in the Ncurses UI module,
                    began implementing interface to allow runtime loading
		    and unloading of plugins. Interface code first, then
		    unloading code. probably need to rework loading code.
		    Plugin options to be handled by 'F9' key.

   1st Sept. 2015 - Finally commented start of file, lot's of things
                    changed between 27th and today that I won't bother
		    mentioning :P
		  - Moved "string table" creation/destruction code out of
		    SSDP plugin, into newly created 'zt-utils.c' As of this
		    writing, there is still a memory error to be hunted
		    down and squashed (I "love" triple indirection)
		  - Finally removed the horrible busy-waiting at the end
		    of main(). Now, we have a GAsyncQueue, 'comm_q', that
		    will serve as a control channel between plugins and the
		    master thread (eg. for things like 'plugin_unload_self()',
		    'exit_zsniff()', or 'plugin_create_new_thread_pair()'.

		    NOTE: Previously mentioned function names are
		    hypothetical at this point and when implemented, will have
		    more meaningful/cohesive names. Also, 'comm_q' will be
		    global in definition but accessible ONLY through the Core
		    API, as it should be. (Note-to-self: Re-examine whether
		    comm_q NEEDS to be global, module-local with globally
		    defined API functions sounds more like it)
 */

/* TODO:
   - Abstract 'create thread pairs'. In its current (1st Sept.)
     form, it makes an assumption about data to be handed to the
     capture thread (In this case, that it will want a pcap_t *) */

#include "project-includes.h"
#include "zt-core.h"
#include "zt-io.h"
#include "zt-framework.h"
#include "zt-utils.h"

#include <sys/socket.h>
#include <arpa/inet.h>

/* From  plugin-core.c  */
extern void load_plugins(char *dir);
extern void unload_plugins();
extern pcap_t *zt_setup_pcap (char *device, char *filter);

extern zt_plugin *_g_plugin_list;
extern GMutex pi_list_mutex;

/* From zt-io.c */
extern void _zt_init_io_streams();
extern void _zt_load_framework(char *path);

/* From above, but temporary dummy framework */
extern GThreadFunc zt_error_thread(gpointer data);
extern GThreadFunc zt_stdout_thread(gpointer data);

static void zt_main_loop();

/* Binary tree mapping key chars to command functions */
GTree *_g_key_bindings = NULL;

/*
 * Built-in commands prototypes */
/* For interfaces */
zt_key_command ztc_manage_interfaces();

/* For plugin management */
zt_key_command ztc_manage_plugins();
zt_key_command ztc_show_plugins();


/* List of our thread_pairs. ( zt_thread_pair )*/
GSList *_g_thread_pairs = NULL;
GMutex  thread_pairs_mutex;

/* We will use this to pass commands back to the master thread.
   This also gives us a means to put the master thread to
   sleep (No more busy wait!) */
GAsyncQueue  *master_q = NULL;

/*
1234567890123478901234567890123456789012345678901234789012345678901234567890
*/
/* When launching a capture/process pair, this is the function we 
   use for the capture. One of it's
   arguments is a function handle suitable for use
   with pcap_loop() */
GThreadFunc zt_capture_stub(gpointer args) {
    struct cap_thread_args *a = (struct cap_thread_args *)args;
    pcap_t *pcap_h = a->pcap_h;
    
    pcap_loop(pcap_h, -1, a->capture, (u_char *)a);

    return 0;  /* This will be the end of our thread */
}

/* Thread handles for error and output 'stream handler'
   threads, as provided by the I/O Framework */
GThread *err_thread;
GThread *out_thread;

/* Basic key binding test function */
zt_key_command test_func() {
    char *test_str = "test thing\r\nanother line for us\r\n\r\n";
    StringBag bag = ztl_string_bag_create(test_str, strlen(test_str));
    
    ztl_string_bag_dump(bag);

    ztl_string_bag_destroy(bag);

    ztprint(" **TEST** ", "Test function activated\n");

    return NULL;
}

/* Used for inserting single character 'keys' into
   the key-bindings command tree. */
int key_sort(gpointer a, gpointer b) {

    if (a < b) return -1;
    if (a == b) return 0;
    if (a > b) return 1;

    return 0;
}

int main(int argc, char *argv[]) {
    char   *plugin_dir = argv[1];

    master_q = g_async_queue_new(); /* Communication from plugin-to-core */

    /* Initialise I/O Framework streams to emulatie stdio */
    /* By doing this now, calls to zterror and ztprint will
       be buffered to be read and displayed once the I/O framework
       is in place. */
    _zt_init_io_streams();

    printf(" -[  ZTSniff  Ver. %d.%02d-%s ]- \n",
	   ZT_VER_MAJOR, ZT_VER_MINOR, ZT_VER_CODENAME);

    /* Load plugins, this may exit on critical failure, or
       if we end up with zero plugins successfully loaded. */
    load_plugins(plugin_dir);    

    /* initialise tree for key bindings */
    _g_key_bindings = g_tree_new((GCompareFunc)key_sort);

    /* For each loaded plugin, create a Glib ASyncQueue, then
       launch a capture/process thread pair */
    zt_plugin *ztp = _g_plugin_list;

    printf("\nCreating thread pairs\n");

    while (ztp) {
	GAsyncQueue *comms_q = NULL;
	GThread *cap_h = NULL, *proc_h = NULL;
	struct cap_thread_args  *cap_args;
	struct proc_thread_args *proc_args;
	zt_thread_pair          *zt_thp;

	/* Create GAsyncQueue for Inter-Thread communication */
	comms_q = g_async_queue_new();

	/* Allocate new argument structures for thread pair */
	cap_args = (struct cap_thread_args *)  \
	    malloc(sizeof(struct cap_thread_args));
	proc_args = (struct proc_thread_args *)  \
	    malloc(sizeof(struct proc_thread_args));
	zt_thp = (zt_thread_pair *)  \
	    malloc(sizeof(zt_thread_pair));

	if (cap_args == NULL || proc_args == NULL || zt_thp == NULL) {
	    fprintf(stderr, "Failed allocating memory for thread "
		    "arguments!\n%s\n\nAborting...\n", strerror(errno));

	    if (cap_args)  free(cap_args);
	    if (proc_args) free(proc_args);
	    if (zt_thp)    free(zt_thp);

	    /* TODO: More clean-up code here?  */
	    exit(1);
	}

	/* Each thread pair's capture thread will require a
	   pcap_t context, let's make one */
	pcap_t *pcap_h;
	if ((pcap_h = zt_setup_pcap("eth0", ztp->filter)) == NULL) {
	    fprintf(stderr, "Failed creating LibPCap contexts! Aborting.\n");

	    /* TODO: Better cleanup? */
	    exit(1);
	}

	/* Build argument structures for threads, build
	   thread-pair structure, then add to list and fire
	   up threads. */
	cap_args->pcap_h = pcap_h;
	cap_args->packet_q = comms_q;
	cap_args->capture  = ztp->_f_capture;

	printf("===  Launching threads for  '%s'...\n", ztp->name);

	/* Capture thread. Our supplied thread function is
	   zt_capture_stub(), which calls plugin-supplied
	   capture() to feed into pcap_loop(). */
	cap_h = g_thread_new("CapThread", (GThreadFunc)(zt_capture_stub),
			     cap_args);

	proc_args->packet_q = comms_q;

	/* Processing thread. */
	proc_h = g_thread_new("ProcThread", (GThreadFunc)ztp->_f_process,
			      (gpointer)proc_args);

	/* Thread-pair structure for internal purposes */
        zt_thp->parent_pi   = ztp;        /* Set parent plugin */
	zt_thp->producer    = cap_h;      /* Capture thread handle */
	zt_thp->consumer    = proc_h;     /* Process thread handle */
	zt_thp->comm_queue  = comms_q;    /* ASync Queue for cap -> proc */

	/* Add to list */
	_g_thread_pairs = g_slist_append(_g_thread_pairs, zt_thp);

	ztp = ztp->next;
    }
    printf("\n");

    printf("UI to be loaded from: '%s'\n", argv[2]);
    printf("Press 'Enter' to load UI and continue...\n");
    getc(stdin);

    /* Load up the framework */
    zt_load_framework(argv[2]);

    /* Bind keys for our commands */
    g_tree_insert(_g_key_bindings, (gpointer)'t', (gpointer)test_func);
    g_tree_insert(_g_key_bindings, (gpointer)ztf_map_ui_key(ZT_KEY_F9),
		  (gpointer)ztc_manage_plugins);
    g_tree_insert(_g_key_bindings, (gpointer)ztf_map_ui_key(ZT_KEY_F10),
		  (gpointer)ztc_manage_interfaces);
    
    /* Enter command loop, sleeping until plugin threads pass
       us a command message */
    zt_main_loop();

    ztprint(" CORE ", "Main loop exited...\n");

//    unload_thread_pairs();
    unload_plugins();
    
    return 0;
}


/*
1234567890123478901234567890123456789012345678901234789012345678901234567890
*/
static void zt_main_loop() {
    int loop_continue = 1;
    while (loop_continue) {
	int command = 0;
	char call_tag[ZT_PLUGIN_TAG_MAX];
	
	zt_master_call_args *args =				\
	    (zt_master_call_args *)g_async_queue_pop(master_q);
	if (!args) {
	    /* TODO:  Handle a NULL better */
	    zterror(" CORE ", "Null arguments passed with "
		    "command arguments\n");
	    loop_continue = 0;
	}
	
	memcpy(call_tag, args->tag, ZT_PLUGIN_TAG_MAX);
	command = args->command;
	
	switch (command) {
	case MC_KILL_ALL:
	    loop_continue = 0;
	    break;
	case MC_KILL_SELF:
	    ztprint(" CORE ", "Thread '%p' wants out\n",args->thread_id);
	    break;
	case MC_EXIT:
	    ztprint(" CORE ", "Exit request received from '%s'\n", call_tag);
	    loop_continue = 0;
	    break;
	default:
	    zterror(" CORE ", "Unknown command specifier byte received:   %d\n",
		   args->command);
	}
    }
}

/* This routine packages up a command request then pushes
   it onto the master Queue to wake up the master thread */
void ztl_call_master(char *tag, GThread *thid,
		     master_cmd command, void * args) {
    zt_master_call_args *mc_args =					\
	(zt_master_call_args *)malloc(sizeof(zt_master_call_args));
    
    mc_args->tag       = tag;
    mc_args->thread_id = thid;
    mc_args->command   = command;
    mc_args->cmd_args  = args;

    g_async_queue_push(master_q, (gpointer)mc_args);
}

/* Built-in commands */
zt_key_command ztc_show_plugins() {
    char *tag_arg = NULL;   /* We use this to avoid null pointers for UI */
    char *name_arg = NULL;

    /* For each plugin, run through and show ti's tag and name */
    zt_plugin *ztp = _g_plugin_list;
    zt_list_window *zlw = ztf_new_list_window("Loaded Plugins",
					      "'q' to cancel",
					      62, 10, 15, 6,  NULL);
    if (!zlw) {
	zterror(" CORE ", "Failed creating window!\n");
	return NULL;
    }

    /* For each plugin loaded, create a selectable menu item that returns
       a pointer to the plugin handle */
    while (ztp) {
	if (ztp->tag) {
	    if (strlen(ztp->tag) < 1) {
		tag_arg = "<null>";
	    } else {
		tag_arg = ztp->tag;
	    }
	} else {
	    tag_arg = "<null>";
	}

	/* Check name field */
	if (ztp->name) {
	    if (strlen(ztp->tag) < 1) {
		name_arg = "<null>";
	    } else {
		name_arg =  ztp->name;
	    }
	} else {
	    name_arg = "<null>";
	}

	ztf_list_window_add(zlw, tag_arg, name_arg, ztp);
	ztp = ztp->next;
    
    }
    /* Get the data handle for the selected plugin */
    ztp = (zt_plugin *)ztf_list_window_show(zlw);
    if (ztp) {
	char title_buffer[256];
	memset(title_buffer, 0x00, 256);
	sprintf(title_buffer, "%s Info", ztp->tag);

	zt_dialog *ztd = ztf_new_dialog(title_buffer, "", 40, 5,
					20, 10, NULL, NULL);
	ztf_dialog_vcat(ztd, "Name:    '%s'\n", ztp->name);
	ztf_dialog_vcat(ztd, "Filter:  '%s'\n", ztp->filter);
	if (ztd) {
	    ztf_dialog_show(ztd);
	}
    }

    /* Todo:  Destroy list_window */
    // ztf_list_window_destroy(zlw);

    return NULL;
}

/* Generic routine that returns string representation of an address */
char *ztc_sockaddr_toa (struct sockaddr *sa) {
    if (!sa)
	return "<null>";

    if (sa->sa_family == AF_INET) {  /* IPv4 */
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;
	return inet_ntoa(sin->sin_addr);
    } else if (sa->sa_family == AF_INET6) {  /* IPv6 */
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
	return inet_ntop(AF_INET6, &sin6->sin6_addr, NULL, sizeof(sin6->sin6_addr));
    }

    return "<WHOOPS>";
}

zt_key_command ztc_show_interfaces() {
    /* Use pcap functions to scan all available interfaces */
    char errbuff[PCAP_ERRBUF_SIZE];
    char *ifname = NULL;
    pcap_if_t *if_list;
    int if_cnt = 0;

    zt_list_window *if_lw = 
	ztf_new_list_window("Available Interfaces",
			    "Press 'q' to abort",
			    70, 15, 5, 3, NULL);

    if (pcap_findalldevs(&if_list, errbuff) < 0) {
	zterror(" CORE ", "Failed querying interfaces! %s\n",
		errbuff);
	return NULL;
    }

    /* Scan over list, may be NULL to begin with */
    while (if_list && if_cnt < 32) {
	if (if_list->name) {
	    char *if_address = ztc_sockaddr_toa(if_list->addresses->addr);

	    if (if_address == NULL) if_address = "<null>";
   
	    zterror(" CORE ", "Interface Address:  '%s'\n", if_address);

	    ztf_list_window_add(if_lw, if_list->name,
				if_address,
				if_list->name);
	}
	if_list = if_list->next;
	if_cnt++;
    }

    ifname = ztf_list_window_show(if_lw);

    if (ifname) 
	ztprint(" CORE ", "Selected iface: \"%s\"\n", ifname);
    else 
	zterror(" CORE ", "No command function attached to menu");

    return 0;
}

/* Main routine for handling interfaces during runtime */
zt_key_command ztc_manage_interfaces() {
    zt_list_window *if_lw;
    zt_key_command zt = NULL;

    if_lw = ztf_new_list_window("Interface manager",
				"Press 'q' to abort",
				40, 10, 15, 4, NULL);
    
    ztf_list_window_add(if_lw, "Info", "Interface Info", ztc_show_interfaces);
    ztf_list_window_add(if_lw, "Capture", "Attach to plugin", ztc_show_plugins);
    
    zt = (zt_key_command)ztf_list_window_show(if_lw);

    if (zt)
	zt();
    else
	zterror(" CORE ", "No command function attached to menu");

    return 1;
}

/* Main routine for handling the 'plugin management' during runtime */
zt_key_command ztc_manage_plugins() {
    zt_list_window *main_lw;
    zt_key_command zt = NULL;

    main_lw = ztf_new_list_window("Manage Plugins", NULL,
				  46, 7, 10, 6, NULL);
    ztf_list_window_add(main_lw, "Info", "Plugin Info", ztc_show_plugins);
    ztf_list_window_add(main_lw, "Filters", "Edit Filters", ztc_show_plugins);
    ztf_list_window_add(main_lw, "CapLoad", "Load capture file into plugin", NULL);
    ztf_list_window_add(main_lw, "Load", "Load Plugin", NULL);
    ztf_list_window_add(main_lw, "Unload", "Unload Plugin", ztc_show_plugins);

    zt = ztf_list_window_show(main_lw);

    if (zt)
	zt();

    return 0;
}
