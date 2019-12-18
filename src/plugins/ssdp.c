#include "../zt-plugin.h"
#include "../zt-utils.h"
#include "../zt-io.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include "ssdp.h"

//extern ssdp_retrieve_xml(char *url);

/* Prototypes for manifest */
pcap_handler capture(u_char *args, const struct pcap_pkthdr *h,
		     u_char *bytes);
GThreadFunc process(gpointer *args);

/* Define our plugin's manifest */
#ifdef _ZT_INTERNAL
#undef _ZT_INTERNAL
#endif
zt_plugin manifest = {
    .name    = "SSDP Sniffer\0",
    .tag     = "xUPnPx\0",
    .filter  = "udp port 1900",
    ._f_capture = (pcap_handler)capture,
    ._f_process = (packet_handler)process
};

/* XML-processor's thread data */
GThread *xml_thread = NULL;
GAsyncQueue *xml_q = NULL;

void init_plugin() {
    xml_q = g_async_queue_new();
}

void ssdp_subproc_search(queue_msg *qm, StringBag bag) {
    int line_num = 0;
    LineTokens line;

    while ((line = *(bag + line_num)) != NULL) {
	if (line[0])
	    if (strcmp(line[0], "ST:") == 0) {
		ztprint(MYTAG,"M-SEARCH|%s|%s\n",
			ssdp_shorten_urn_type(line[1]),
			ssdp_shorten_urn_class(line[1]));
		break;
	    }
	line_num++;
    }
}

void ssdp_subproc_notify(queue_msg *qm, StringBag bag) {
    int line_num = 0;
    LineTokens line;

    char *nt = NULL, *nts = NULL;
    char *nt_class = NULL, *nt_type = NULL;
    char *state = NULL, *location = NULL;

    while ((line = *(bag + line_num)) != NULL) {
	if (!line[0]) break;
	
	if (strcmp(line[0], "NT:") == 0 && line[1]) {
	    nt_type = ssdp_shorten_urn_type(line[1]);
	    if (nt_type) { /* We will only extract a class if we */
		         /* Had a valid URN earlier for type-determination */
		nt_class = ssdp_shorten_urn_class(line[1]);
	    } else {
		nt_type = line[1];
	    }
	} else if (strcmp(line[0], "NTS:") == 0) {
	    nts = line[1];
	    if (strcmp(nts, "ssdp:alive") == 0) {
		state = "UP";
	    } else if (strcmp(nts, "ssdp:byebye") == 0) {
		state = "DOWN";
	    } else {
		state = "???";
	    }
	} else if (strcmp(line[0], "LOCATION:") == 0) {
	    location = line[1];
	}

	line_num++;
    }

    if (nt_type && nt_class && nts) {
	struct in_addr sip = {qm->iph->saddr};
	char *ip_str = inet_ntoa(sip);

	if (location)
	    ztprint(MYTAG,"<S:%s> NOTIFY<%s>|%s|%s|%s\n\t@%s\n", ip_str, state,
		    nt_type, nt_class, nts, location);
	else
	    ztprint(MYTAG,"<S:%s> NOTIFY<%s>|%s|%s|%s\n", ip_str, state,
		    nt_type, nt_class, nts);
    } else {
	zterror(MYTAG,"WHOA! Unexpected NULL pointers in ssdp_subproc_notify()!!\n");
	zterror(MYTAG, "nt_type:   %p    nt_class:   %p    nts:   %p\n",
		nt_type, nt_class, nts);
    }
}

void ssdp_subproc_response(queue_msg *qm, StringBag bag) {
    int line_num = 0;
    LineTokens line;

    struct in_addr sip = {qm->iph->saddr};
    char *ip_str = inet_ntoa(sip);

    char *retcode = NULL, *retstatus = NULL;
    char *location = NULL, *st  = NULL, *usn = NULL;
    char *st_type = NULL, *st_class = NULL;

    while ((line = *(bag + line_num)) != NULL) {
	if (!line[0]) break;

	if (strcmp(line[0], "HTTP/1.1") == 0) {
	    retcode = line[1];
	    retstatus = line[2];
	} else if (strcmp(line[0], "ST:") == 0) {
	    st = line[1];
	    st_type = ssdp_shorten_urn_type(line[1]);
	    st_class = ssdp_shorten_urn_class(line[1]);
	    ztprint(MYTAG, "<S:%s> FOUND  %s|%s\n",
		    ip_str, st_type, st_class);
	} else if (strcmp(line[0], "USN:") == 0) {
	    st_type = ssdp_shorten_urn_type(line[1]);
	    st_class = ssdp_shorten_urn_class(line[1]);
	} else if (strcmp(line[0], "LOCATION:") == 0) {
	    location = line[1];
	}
	line_num++;
    }
    /* If we get this far, and we have a Location URL,
       we can go ahead and attempt to retrieve / parse the
       XML. The XML processing code is responsible for adding
       new UPnP Devices/Serives upon successful parsing */
    if (location) {
	/* Check to see if the XML thread has been started. If not,
	   start it first */
	if (!xml_thread) {
	    xml_thread = g_thread_new("SSDP-XMLProc", (GThreadFunc)ssdp_retrieve_xml,
				      (gpointer)(NULL));
	}
	g_async_queue_push(xml_q, (gpointer)location); /* Add to queue */

//	ztprint(MYTAG, " ---> Downloading description XML:\n%s\n", location);
//	ssdp_retrieve_xml(location);
    }

    ztprint(MYTAG, "<S:%s> RESPONSE<%s,%s>|%s|%s@%s|%s\n",
	    ip_str, retcode, retstatus, st_type, st_class,
	    location, usn);
}

/* SSDP Disector function. */
void ssdp_process(queue_msg *qm, StringBag bag) {
    LineTokens line = *(bag+0);

    if (strcmp(line[0], "M-SEARCH") == 0) {
	ssdp_subproc_search(qm, bag);
    } else if (strcmp(line[0], "NOTIFY") == 0) {
	ssdp_subproc_notify(qm, bag);
    } else if (strcmp(line[0], "HTTP/1.1") == 0) {
	ssdp_subproc_response(qm, bag);
    }
}

/* Thread function that will read from the queue.
 * Passed-in pointer is cast to  struct proc_thread_args  **/
GThreadFunc process(gpointer *args) {
    struct proc_thread_args *in_args = \
	(struct proc_thread_args *)args;
    GAsyncQueue *packet_q = zt_proc_get_queue(args);

    /* "Boredom" probe scaling */
    int boredom_level = 0;
    int timeout = SSDP_BOREDOM_MIN_TIMEOUT;
    queue_msg *qm  = NULL;

    /* Processing loop */
    while (1) {
	qm = (queue_msg *)g_async_queue_timeout_pop(packet_q, timeout);
	if (qm == NULL) {
	    /* Left in for now, to prove that the plugin-to-master
	       communication is working */
	    ztl_call_master(MYTAG, g_thread_self(), MC_KILL_SELF, (gpointer)0x1);

	    /* Timed out waiting for packets. Send a 'Discover' message */
	    ssdp_send_discover(SSDP_MEDIASERVER_DISC);

	    boredom_level++;
	    if (boredom_level == SSDP_BOREDOM_THRESH) {
		zterror(MYTAG, "Doubling 'Boredom Timeout'\n");
		boredom_level = 0;
		timeout <<= 1;  /* Double timeout period */

		if (timeout >= SSDP_BOREDOM_MAX_TIMEOUT/4) {
		    ztl_call_master(MYTAG, g_thread_self(), 666, (gpointer)NULL);
		    timeout = SSDP_BOREDOM_MAX_TIMEOUT;
		}
	    }
	    continue;  /* Try again */
	}
	
	StringBag bag;
	bag = ztl_string_bag_create(qm->payload,
				    qm->payload_len);
	ssdp_process(qm, bag);

	ztl_string_bag_destroy(bag);

	/* Free message packet */
	if(qm) { 
	    
	    free(qm->packet);
	}
    } /* End of Main Processing Loop for this plugin */
};

/* Plugin-specific capture routine. This will currently just dump data. */
pcap_handler capture(u_char *args, const struct pcap_pkthdr *h,
				 u_char *bytes) {
    GAsyncQueue *packet_q = zt_cap_get_queue(args);
    pcap_t *pcap_h        = zt_cap_get_pcap(args);
    u_char *packet_dup    = malloc(h->caplen);
    queue_msg *qm     = (queue_msg *)malloc(sizeof(queue_msg));
    
    /* Copy packet data */
    memcpy(packet_dup, bytes, h->caplen);

    struct ether_header *ethh = (struct ether_header *)packet_dup;
    char *proto_type;
    unsigned short type_temp = ntohs(ethh->ether_type);
    char *payload = NULL;
    struct ip6_hdr *ip6h = NULL;
    struct iphdr *iph    = NULL;

    if (type_temp == ETHERTYPE_IP) { /* IPv4 */
	iph = (struct iphdr *)(packet_dup + sizeof(struct ether_header));
	payload = (char *)(packet_dup + sizeof(struct ether_header) \
				 + (iph->ihl * 4)			\
				 + 8 /* UDP Header Size */);
    } else if (type_temp == ETHERTYPE_IPV6) {  /* IPv6 */
	ip6h = (struct ip6_hdr *)(packet_dup + sizeof(struct ether_header));
	payload = (char *)(packet_dup + sizeof(struct ether_header) \
				 + sizeof(struct ip6_hdr));
    }
    qm->ethh = ethh;
    qm->iph  = iph;
    qm->ip6h = ip6h;
    qm->udph = NULL;
    qm->payload = payload;
    qm->packet  = packet_dup;
    qm->packet_len = h->caplen;
    qm->payload_len = h->caplen - (sizeof (struct ether_header) \
				   - (iph->ihl * 4) - 8);

    /* Push payload message to queue */
    g_async_queue_push(packet_q, (gpointer)qm);
}
