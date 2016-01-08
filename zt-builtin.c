/* zt-builtin.c - This file houses commands that are provided
 * by the core framework. These commands are mostly bound to keys
 * at runtime.
 * Most of this code was pulled out from the main ztsniff.c file
 * on 08th Jan, 2016
 * 
 * - Owen Klan  08th January 2016 */
#include "project-includes.h"

#include "zt-plugin.h"
#include "zt-io.h"

extern zt_plugin *_g_plugin_list;
extern GMutex pi_list_mutex;

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