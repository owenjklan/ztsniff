#include "../zt-plugin.h"
#include "../zt-utils.h"
#include "../zt-io.h"

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

packet_handler process(struct proc_thread_args *data);
pcap_handler capture(u_char *user, const struct pcap_pkthdr *h,
		     u_char *bytes);

/* Define our plugin's manifest */
zt_plugin manifest = {
    .name   = "Multicast DNS Sniffer",
    .tag    = " mDNS ",
    .filter = "udp port 5353",
    ._f_capture = (pcap_handler)&capture,
    ._f_process = (packet_handler)&process
};


/* Thread function that will read from the queue. */
packet_handler process(struct proc_thread_args *data) {
    struct proc_thread_args *args = (struct proc_thread_args *)data;
    GAsyncQueue *packet_q = zt_proc_get_queue(args);

    int boredom_level = 0;
    int timeout       = 2000000;

    while (1) {
	char *message = (char*)g_async_queue_timeout_pop(packet_q, timeout);

	if (message == NULL) {
	    ztprint(MYTAG, "Been %d seconds, MDNS is BOORRED!!\n", timeout / 1000000);
	    boredom_level++;
#define BOREDOM_SCALE_THRESH 3
	    if (boredom_level == BOREDOM_SCALE_THRESH) {
		ztprint(MYTAG, "  ... You know what, MDNS will come back later.\n");
		timeout <<= 1;
		boredom_level = 0;
	    }
	} else {
	    ztprint(MYTAG, "MDNS GOT A MESSAGE!!\n%s\n", message);
	    boredom_level = 0;
	    timeout = 2000000;
	}
    }
};

/* Init routine, if we want it */
void init_plugin() {
    printf("Hello world, from your friendly local plugin!\n");
}

/* Plugin-specific capture routine. This will currently just dump data. */
pcap_handler capture(u_char *user, const struct pcap_pkthdr *h,
				 u_char *bytes) {
    struct ether_header *ethh = (struct ether_header *)bytes;
    char *proto_type;
    unsigned short type_temp = ntohs(ethh->ether_type);
    GAsyncQueue *packet_q = zt_cap_get_queue(user);

    ztprint(MYTAG, "MDNS packet received\n");

    switch(type_temp) {
    case ETHERTYPE_IP:
	proto_type = "IPv4";
	break;
    case ETHERTYPE_IPV6:
	proto_type = "IPv6";
	break;
    default:
	proto_type = "???";
    }
    // printf("\t%s\n", proto_type);
    
    struct iphdr *iph = (struct iphdr *)(bytes + sizeof(struct ether_header));
    ztprint(MYTAG, "Vers: %d  HeadLen: %d  Protocol: %d\n",
	   iph->version, iph->ihl*4, iph->protocol);
 
    /* Quick and dirty, set our payload pointer to AFTER all those network
       headers, still currently assuming we have an IPv4 header... FIX THIS! */
    char *payload = (char *)(bytes + sizeof(struct ether_header) \
			     + (iph->ihl * 4) \
			     + 8 /* UDP Header Size */);
    ztprint(MYTAG, "Payload:  %s\n", payload);
    g_async_queue_push(packet_q, (gpointer)payload);

}
