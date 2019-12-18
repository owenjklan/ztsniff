/* Input/Output functions for ZTSniff. This includes
   generic library options (like ztprint() and zterror())
   as well as the code to handle the I/O Framework
   loading and interfacing.

   Written by Owen Klan  -  6th September, 2015 */

/** HISTORY
 - 5th October, 2015     - Added 'zt_fix_weak_syms' to resolve
                           'weakly-linked' symbols in the Core
			   code. An example of this is functions
			   that are to be provided by the UI plugin,
			   but that the ZTSniff Core code uses itself.
			   The particular culprit that comes to mind
			   (meaning, forced this path) is 'ztf_new_list_window'.
 */

#include <dlfcn.h>

#include "project-includes.h"

#include "zt-io.h"
#include "zt-framework.h"

GAsyncQueue *Stdin;
GAsyncQueue *Stdout;
GAsyncQueue *Stderr;

/* Our sole Framework structure */
zt_framework *fwork;

static void _zt_queue_output(GAsyncQueue *out_q,
			     zt_outmsg *message) {

}

/* String-handles for weak symbols. These are used by _zt_fix_weak_syms() */
static char *weak_symbol_names[] = {
    "ztf_new_list_window",
    "ztf_list_window_show",
    "ztf_list_window_add",
    NULL
};

/* This routine is responsible for ensuring all 'weak' symbols
   in the ZTSniff Core are properly resolved after UI is loaded. */
static void _zt_fix_weak_syms(void *dl_handle) {
    int i = 0;
    char *sname = NULL;

    ztprint(" CORE ", "Resolving weak symbols that should be found in UI...\n");

    /* Run through array of symbol names to resolve now.
       NULL sentinel value marks the end of this array. */
    while ((sname  = weak_symbol_names[i++])) {
	void *temp_sym = NULL;

	if ((temp_sym = dlsym(dl_handle, sname)) == NULL) {
	    ztprint(" CORE ", "%-40s FAIL\n", sname);
	} else {
	    ztprint(" CORE ", "%-40s OKAY\n", sname);
	}
    }
}


/* This routine will load the I/O framework to use.
   TODO:  Allow fallback to stdio built-in's if I/O
   framework loading fails... <<-- one day */
/* Currently exit(1)'s on error :P */
void zt_load_framework(char *path) {
    int (*init_func)();		       /* Initialisation function pointer */
    void (*shutdown_func)();           /* Shutdown function pointer */
    void *dl_handle = NULL;
    
    /* Clear framework data structure */
    memset(&fwork, 0x00, sizeof(zt_framework));
    printf("Loading I/O Framework '%s'...\n", path);
    
    /* Call to Dlsym. */
    if ((dl_handle = dlopen(path, RTLD_NOW|RTLD_GLOBAL)) == NULL) {
	fprintf(stderr, "Failed loading framework! %s\n",
		dlerror());
	exit(1);
    }

    /* Look up shutdown_framework(). */
    if ((shutdown_func = dlsym(dl_handle, "shutdown_framework")) == NULL) {
      fprintf(stderr, "---  No shutdown routine found in %s\n", path);
      exit(1);
    }
    
    /* look up init_framework(). Function */
    if ((init_func = dlsym(dl_handle, "init_framework")) == NULL) {
	fprintf(stderr, "---  No initialisation routine found in %s\n",
		path); 
    } else {
	/* Call the init. routine. If it returns non-zero then we report
	 * failure and abort loading it */
	if (init_func() < 0) {
	    fprintf(stderr,
		    "!!!  %s not loaded due to initialisation failure!\n",
		    path);
	    dlclose(dl_handle);
	    exit(1);
	}

	ztprint(" ZT-IO ", "Framework initialisation finished successfully\n");
    }

    /* resolve the manifest structure. This must be present so if
       we can't find it, we'll abort loading the UI . */
    if ((fwork = (zt_framework *)dlsym(dl_handle, "fw_manifest")) == NULL) {
	fprintf(stderr, "!!!  No manifest found in %s! Aborting load of plugin.\n",
		path);
	dlclose(dl_handle);
	exit(1);
    }

    /* Resolve symbols for IO Framework in advance */
    _zt_fix_weak_syms(dl_handle);

    /* Start our input/output handlers from loaded framework code */
    g_thread_new("StderrHandler", fwork->_f_stderr, (gpointer)NULL);
    g_thread_new("StdoutHandler", fwork->_f_stdout, (gpointer)NULL);
    g_thread_new("InputHandler", fwork->_f_input, (gpointer)NULL);
}

void _zt_init_io_streams() {
    /* Create new GAsyncQueues for our standard I/O
       "streams" */
    Stdin = g_async_queue_new();
    Stdout = g_async_queue_new();
    Stderr = g_async_queue_new();
}

/* generic output function that "prints" messages to
 * the "Stdout" queue for use by the I/O framework. */
void ztprint(char *tag, char *f, ...)
{
    zt_outmsg *out_msg = malloc(sizeof(zt_outmsg));
    char *out_buffer = malloc(ZT_STDOUT_MAX_SIZE);
    memset(out_buffer, 0x00, ZT_STDOUT_MAX_SIZE);
    
    va_list arg_ptr;
    va_start(arg_ptr, f);
    vsnprintf(out_buffer, ZT_STDOUT_MAX_SIZE, f, arg_ptr);

    memcpy(out_msg->pi_tag, tag, ZT_PLUGIN_TAG_MAX);
    out_msg->content = out_buffer;

    g_async_queue_push(Stdout, (gpointer)out_msg);
}

/* generic output function that "prints" messages to
 * the "Stderr" queue for use by the I/O framework. */
void zterror(char *tag, char *f, ...)
{
    zt_outmsg *out_msg = malloc(sizeof(zt_outmsg));
#define ZT_STDOUT_MAX_SIZE  256
    char *out_buffer = malloc(ZT_STDOUT_MAX_SIZE);
    memset(out_buffer, 0x00, ZT_STDOUT_MAX_SIZE);
    
    va_list arg_ptr;
    va_start(arg_ptr, f);
    vsnprintf(out_buffer, ZT_STDOUT_MAX_SIZE, f, arg_ptr);

    memcpy(out_msg->pi_tag, tag, ZT_PLUGIN_TAG_MAX);
    out_msg->content = out_buffer;

    g_async_queue_push(Stderr, (gpointer)out_msg);
}


/***
   THE FOLLOWING WILL GO INTO A SEPERATE FRAMEWORK MODULE
 ***/
/* Temp. function to read Stderr queue, and fprintf() messages
   straight out to C's stderr stream. */
GThreadFunc zt_error_thread(gpointer data) {
    zt_outmsg *out_msg;

    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stderr);
	fprintf(stderr, "[%s] %s", out_msg->pi_tag, out_msg->content);
	fflush(stderr);
	free(out_msg->content);
	free(out_msg);
    }
}

/* Temp. function to read Stdout queue, and fprintf() messages
   straight out to C's stdout stream. */
GThreadFunc zt_stdout_thread(gpointer data) {
    zt_outmsg *out_msg; 
   
    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stdout);
	fprintf(stdout, "[%s] %s", out_msg->pi_tag, out_msg->content);
	fflush(stdout);
	free(out_msg->content);
	free(out_msg);
    }
}
