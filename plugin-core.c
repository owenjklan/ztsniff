/* NOTE: A lot of this code was pulled from my Jigsaw project
         (from WAYYYY back in 2003) and modified accordingly.
 
   Written by Owen Klan  -  27th August, 2015 */

/* HISTORY:
 * 27th August, 2015  - Re-worked plugin loading code from
                       Jigsaw, into ZTsniff. Split
		       'load_plugins()' up so now each seperate
		       filename found, is passed into 'load_plugin_file()'
   28th August, 2015  - Fixed code so that we actually error
                        check the return value of our call to getcwd()
			down in 'load_plugins()'.
 */
#include "project-includes.h"

#include <dlfcn.h>		       /* For dlopen */
#include <libgen.h>		       /* For basename() */

#include "zt-plugin.h"

/* Function prototypes */
void load_plugins(char *dir);
void unload_plugins();

/* Globals !! */
GMutex pi_list_mutex;
zt_plugin *_g_plugin_list = NULL;

/* Close all opened plugin files */
void unload_plugins()
{
    g_mutex_lock(&pi_list_mutex);
    zt_plugin *current = _g_plugin_list, *next;
 
    printf("\n");
   
    do {
	printf("---  Unloading %s...\n", current->name);

	dlclose(current->handle);
	next = current->next;
	free(current);
	current = next;
    } while (current);
    g_mutex_unlock(&pi_list_mutex);
}

/* Helper utility to load_plugins(). load_plugins() is called with
   the name of a directory, then, for each file name of interest that
   it finds, it calls this function with just that file path. This function
   then does all the 'heavy-lifting' for dynamic loading of plugins.

   Returns zero on successful load, -1 in the event of plugin-load failure */
static int load_plugin_file(char *fname) {
    int (*init_func)();		       /* Initialisation function pointer */
    void *handle;                /* Our handle for use with the dynamic loader */

    g_mutex_lock(&pi_list_mutex);

    zt_plugin *pf, *current = _g_plugin_list;
    zt_plugin *manifest;

    fprintf(stdout, "\n===  Attempting to load '%s'...\n",
	    basename(fname));

    /* Open the file */
    handle = dlopen(fname, RTLD_LAZY);
    
    if (handle == NULL) {
	/* Failed, just continue */
	g_mutex_unlock(&pi_list_mutex);
	return -1;
    }
    
    /* Now try to resolve the initialisation routine and call it.
       if this results in NULL, then no init. routine was provided,
       this is  optional so we'll just move on. */
    if ((init_func = dlsym(handle, "init_plugin")) == NULL) {
	fprintf(stderr, "---  No initialisation routine found in %s\n",
		basename(fname)); 
    } else {
	/* Call the init. routine. If it returns non-zero then we report
	 * failure and abort loading it */
	if (init_func() < 0) {
	    fprintf(stderr,
		    "!!!  %s not loaded due to initialisation failure!\n",
		    fname);
	    dlclose(handle);
	    g_mutex_unlock(&pi_list_mutex);
	    return -1; /* Try next plugin */
	}

	fprintf(stdout, "+++  init_plugin() routine finished successfully\n");
    }
    
    /* Okay, we have the handle for this file so create a new
     * element in the plugin_handles list */
    if ((pf = malloc(sizeof(zt_plugin))) == NULL) {
	fprintf(stderr, "Memory allocation failure!\n");
	g_mutex_unlock(&pi_list_mutex);
	exit(1);
    }
    
    /* resolve the manifest structure. This must be present so if
       we can't find it, we'll abort loading this plugin and move
       to the next file in the directory. */
    if ((manifest = (zt_plugin *)dlsym(handle, "manifest")) == NULL) {
	fprintf(stderr, "!!!  No manifest found in %s! Aborting load of plugin.\n",
		basename(fname));
	dlclose(handle);
	free(pf);	

	g_mutex_lock(&pi_list_mutex);
	return -1; /* Try next plugin */
    }
    /* Proof we have our manifest */
//    printf("Loaded '%-24s' BPF: '%s'", manifest->name,
//	   manifest->filter);
    
    strncpy(pf->name,   manifest->name,   ZT_PLUGIN_NAME_MAX-1);
    strncpy(pf->tag,    manifest->tag,    ZT_PLUGIN_TAG_MAX-1);
    strncpy(pf->filter, manifest->filter, ZT_PLUGIN_FILTER_MAX-1);
    pf->_f_capture = manifest->_f_capture;
    pf->_f_process = manifest->_f_process;

    pf->handle = handle;
    pf->next = NULL;
    
    /* Add it to the list */
    if (!current) {
	_g_plugin_list = pf;
    } else {
	while (current->next)
	    current = current->next;
	
	current->next = pf;
    }
    /* This plugin file has been successfully loaded. */
    fprintf(stderr, "+++  [%-5s] '%s' successfully loaded...\n",
	    manifest->tag, manifest->name);

    g_mutex_unlock(&pi_list_mutex);

    return 0; /* Success */
}

/* Load all plugins from a given directory. On any critical
   failure, this function will exit() the program. */
void load_plugins(char *dir)
{    
    struct dirent **dir_files;
    int n, i;
    int success_loads = 0, failed_loads = 0;
    char base_dir[NAME_MAX * 2];
    char temp_fname[NAME_MAX * 2];
    
    fprintf(stdout, "Loading plugins from %s\n", dir);

    /* Clear our buffer, used for building absolute paths */
    memset(base_dir, 0x00, NAME_MAX * 2);

    /* Check the first character of our given path. If it's
       not a '/' character, then we've been given a relative path
       and we'll build an absolute path before we start loading plugins */
    if (dir && *dir != '/') {
	/* Actually check return of getcwd() now (Aug. 2015). If this does
	   for some reason fail, we'll abort program startup. */
	if (getcwd(base_dir, NAME_MAX) == NULL) {
	    fprintf(stderr, "Failed determining current working directory!\n");
	    fprintf(stderr, "%s\n,base_dir: '%s'\n", strerror(errno), base_dir);
	    
	    exit(1);
	};
	strcat(base_dir, "/");
	strcat(base_dir, dir);
    } else {
	strcat(base_dir, dir);
    }

    /* Ensure that our last character in our path is a trailing slash */
    int base_dir_len = strlen(base_dir);
    if (base_dir[base_dir_len - 1] != '/') {
	base_dir[base_dir_len++] = '/';
	base_dir[base_dir_len] = '\0';
    }
    
    /* This is a BSD routine... 4.3 */
    n = scandir(base_dir, &dir_files, NULL, alphasort);
    
    if (n < 0) {
	fprintf(stderr, "Failed loading plugins from %s! %s\n",
		dir, strerror(errno));
	fprintf(stderr, "base_dir:  '%s'\n", base_dir);
	exit(1);
    }
    
    /* Try each file */
    for (i = 0; i < n; i++) {
	register char *fname = dir_files[i]->d_name;

	/* Skip over the ever present self '.' and parent '..' refs. */
	if (strcmp(fname, ".") == 0
	    || strcmp(fname, "..") == 0) {
	    continue;
	}
	/* Ensure the name ends in '.so', otherwise, skip */
	if (strcmp((fname + strlen(fname) - 3), ".so") != 0)
	    continue;

	/* Create & clear a generous buffer, then build our full path for each
	   file, then hand that off to the dynamic loader. */
	memset(temp_fname, 0x00, NAME_MAX * 2);
	memcpy(temp_fname, base_dir, strlen(base_dir));
	strcat(temp_fname, dir_files[i]->d_name);

        if (load_plugin_file(temp_fname) < 0)
	    failed_loads++;
	else
	    success_loads++;
    }
    
    fprintf(stdout, "\n");
    fprintf(stdout, "Successfully loaded %d of %d plugins.\n",
	    success_loads, (success_loads + failed_loads));
    fprintf(stdout, "%d plugins failed to load due to errors.\n",
	    failed_loads);

    /* Just as a sanity check, if we're at this point, with zero
       successfully loaded plugins, then there's nothing for us
       to do! */
    if (success_loads == 0) {
	fprintf(stderr, "\nIt looks like no plugins loaded. With no plugins "
		"I have nothing to do!\n");
	exit(1);
    }

    chdir("..");
}

/* Setup plugin pcap session. One per filter / plugin.
 * Returns NULL on failure, otherwise, a fully configured pcap_t handle
 * ready to be passed as an argument to the pcap_loop  */
pcap_t *zt_setup_pcap (char *device, char *filter) {
    pcap_t *pcap_handle;
    char pcap_errbuff[PCAP_ERRBUF_SIZE];

    /* Open PCap session */
    if ((pcap_handle = open_pcap_session(device, 0, pcap_errbuff)) == NULL) {
	fprintf(stderr, "Failed creating packet capture session on '%s'!\n",
		device);
	fprintf(stderr, "%s\n", pcap_errbuff);
	return NULL;
    }

    /* Apply BPF Filter */
    if (set_session_filter(pcap_handle, filter) < 0) {
	fprintf(stderr, "Failed applying filter string, '%s', to session!\n",
		filter);
	fprintf(stderr, "%s\n", pcap_geterr(pcap_handle));
	pcap_close(pcap_handle);
	return NULL;
    }
  
    /* All is well, return our steamy fresh pointer */
    return pcap_handle;
}
