#ifndef _ZTP_SSDP_H_
#define _ZTP_SSDP_H_

#include "../zt-plugin.h"
extern zt_plugin manifest;

/* Symbolic definitions for "boredom scaling" */
/* After this many timeouts, double the timeout period */
#define SSDP_BOREDOM_THRESH 5
#define SSDP_BOREDOM_MIN_TIMEOUT 4000000  /* 15000000 */
#define SSDP_BOREDOM_MAX_TIMEOUT 1800000000  /* 30 minutes */

/* Definition of our data structure to pass info
   we want between our capture and our process thread */
struct _queue_msg {
    struct ether_header *ethh;
    struct iphdr *iph;
    struct ip6_hdr *ip6h;
    struct udphdr *udph;
    u_char *packet;
    u_char *payload;
    u_int32_t packet_len;
    u_int32_t payload_len;
};
typedef struct _queue_msg queue_msg;

/* Prototypes from other source modules */
/* From ssdp-xml.c */
extern GThreadFunc ssdp_retrieve_xml();

/* From ssdp-util.c */
extern unsigned char *ssdp_build_udp_from_buffer(unsigned char *data,
						 int data_len,
						 int *packet_len_ptr);
extern void ssdp_send_discover(int type);
extern char *ssdp_shorten_urn_type(char *urn);
extern char *ssdp_shorten_urn_class(char *urn);

#define SSDP_MEDIASERVER_DISC  1
#define SSDP_MEDIARENDERER_DISC 2

#endif  /*  _ZTP_SSDP_H_  */
