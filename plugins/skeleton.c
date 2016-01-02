/* Must include the following for plugin API */
#include "../zt-plugin.h"

/* Put any additional include files that your code will need here */

/* Thread function that will read from the queue. */
packet_handler process(struct proc_thread_args *args) {
    /* This is our only arg, a handle for the queue that
       we use to read packets, marked as interesting by
       our capture function */
    GAsyncQueue *packet_q = zt_proc_get_queue(args);
};

/* Plugin-specific capture routine. This is used identically to 
any function that would be passed in as the callback parameter
for pcap_loop(), because that's where this is ultimately called from. */
pcap_handler capture(u_char *args, const struct pcap_pkthdr *h,
				 u_char *bytes) {
    GAsyncQueue *packet_q = zt_cap_get_queue(args);
}

/* Init routine, if we want it. Will be run during plugin load.
 * If there is a problem, return -1 and the plugin loader will
 * abort loading this plugin. */
int init_plugin() {
    printf("Hello world, from your friendly local plugin!\n");
}

/* Define our plugin's manifest. This structure must be present
   and named as presented here. Otherwise, our plugin loader can't
   find what it needs. */
zt_plugin manifest = {
	.name   = "skeleton plugin name",  /* Your plugin's name here, max 64 chars */
	.filter = "udp",             /* Your plugin's specific BPF code */
    ._f_capture = (pcap_handler)&capture,
    ._f_process = (packet_handler)&process
};
