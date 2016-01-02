/* Basic ZTSniff I/O Framework that handles the
   Standard C/Unix I/O Streams:  stdin(not yet), stdout & stdarg

   ALL FRAMEWORKS MUST SUPPLY:
   - init_framework() - routine to initialise framework. If nothing
   is to be done, then a stub function must be supplied.
   - error_handler    - routine to handle output to the "Error stream"
   - output_handler   - routine to handle output to the "Output Stream" */
#include "zt-framework.h"

GThreadFunc Stderr_Handler (gpointer data);
GThreadFunc Stdout_Handler (gpointer data);

zt_framework fw_manifest = {
    .name = "Basic STDIO FrameworkZZZ",
    .tag  = "BASIC-STDIO",
    ._f_stdout = (GThreadFunc)Stdout_Handler,
    ._f_stderr = (GThreadFunc)Stderr_Handler
};

int init_framework() {
    fprintf(stderr, "HELLOOOOOO\n");
    
    return 0;
}

GThreadFunc Stderr_Handler (gpointer data) {
    zt_outmsg *out_msg;
    
    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stderr);
	fprintf(stderr, "\x1B[31m[!!> %s] %s\x1B[0m", out_msg->pi_tag, out_msg->content);
	fflush(stderr);
	free(out_msg->content);
	free(out_msg);
    }
}

GThreadFunc Stdout_Handler (gpointer data) {
    zt_outmsg *out_msg; 
   
    while (1) {
	out_msg = (zt_outmsg  *)g_async_queue_pop(Stdout);

	if (strcmp(out_msg->pi_tag, " CORE ") == 0) {
	    fprintf(stdout, "\x1B[33m[__> %s] %s\x1B[0m", out_msg->pi_tag, out_msg->content);
	} else {
	    fprintf(stdout, "[__> %s] %s", out_msg->pi_tag, out_msg->content);
	}
	fflush(stdout);
	free(out_msg->content);
	free(out_msg);
    }
}
