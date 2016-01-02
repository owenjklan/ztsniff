#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <netinet/ip.h>
#include <netinet/udp.h>

#include "../zt-plugin.h"
#include "../zt-io.h"

#include "ssdp.h"
#include "ssdp_data/discovery-packets.h"

unsigned char *ssdp_build_udp_from_buffer(unsigned char *data, int data_len,
			       int *packet_len_ptr) {
    struct iphdr *iph;
    struct udphdr *udph;
    unsigned char *packet_data;
    int packet_max, packet_size = 0;

    /* Generate our Multicast IPv4 packet */
    packet_max = sizeof(struct iphdr) + sizeof(struct udphdr) + data_len;
    if ((packet_data = (unsigned char *)malloc(packet_max)) == NULL) {
	zterror(MYTAG, "SSDP Failed allocating discovery packet!\n");
	return NULL;
    }
    memset(packet_data, 0x00, data_len);

    iph = (struct iphdr *)packet_data;
    iph->ihl = (sizeof(struct iphdr) >> 2);
    udph = (struct udphdr *)((char *)packet_data + 20);

    /* Fill in IP header */
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = packet_max;
    iph->id = 0x5319;  // <--- purely arbitrary
    iph->ttl = 16;     // <---    "     "   " 
    iph->frag_off = 0;
    iph->protocol = IPPROTO_UDP;
    iph->check = 0x0000;
    iph->saddr = 0x0000; // <--- let the kernel fill it
    iph->daddr = (u_int32_t)inet_addr("239.255.255.250");

    /* Fill in UDP header */
    udph->source = htons(1900);
    udph->dest   = htons(1900);
    udph->len    = htons(data_len);
    udph->check  = 0x0000;

    struct sockaddr_in mcast_dest;
    mcast_dest.sin_family = AF_INET;
    mcast_dest.sin_addr.s_addr = inet_addr("239.255.255.250");
    mcast_dest.sin_port  =  htons(1900);

    /* Copy payload data to end of headers */
    memcpy((packet_data + sizeof(struct iphdr) + sizeof(struct udphdr)),
	   data, data_len);

    if (packet_len_ptr)
	*(packet_len_ptr) = packet_max;  /* Copy in packet size */
    return packet_data;
}

/* This is our discovery-probe function. If we haven't pulled anything off
   the packet capture queue for a while, we'll see if we can stir-up some
   noise by sending out discovery packets. */
void ssdp_send_discover(int type) {
    unsigned char *ssdp_data;
    int ssdp_data_len;
    int packet_max = 0;
    unsigned char *packet_data;

    switch (type) {
    case SSDP_MEDIASERVER_DISC:
	ssdp_data = _ssdp_MediaServer_discover;
	ssdp_data_len = _ssdp_MediaServer_discover_length;
	break;
    case SSDP_MEDIARENDERER_DISC:
	ssdp_data = _ssdp_MediaRenderer_discover;
	ssdp_data_len = _ssdp_MediaRenderer_discover_length;
	break;
    default:
	/* No valid packet data available! Abort quietly */
	return;
    }

    if ((packet_data = ssdp_build_udp_from_buffer(ssdp_data, ssdp_data_len,
						  &packet_max)) == NULL) {
	zterror(MYTAG, "We had a failure building a UDP packet!\n");
	return;
    }

    /* Send the datagram */
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
	zterror(MYTAG, "Failed opening socket to send discovery!\n%s\n",
		 strerror(errno));
	free(packet_data);
	return;
    }

    struct sockaddr_in mcast_dest;
    mcast_dest.sin_family = AF_INET;
    mcast_dest.sin_addr.s_addr = inet_addr("239.255.255.250");
    mcast_dest.sin_port  =  htons(1900);
    
    /* Send packet */
    if (sendto(sock, packet_data, packet_max, 0x00,
	       (struct sockaddr *)&mcast_dest,
	       sizeof(struct sockaddr_in)) != packet_max){
	zterror(MYTAG, "Failed sending SSDP discovery! %s\n",
		strerror(errno));
    } else {
	zterror(MYTAG, "SSDP Discovery sent\n");
    }
    free(packet_data);
    close(sock);
}

char *ssdp_shorten_urn_type(char *urn) {
    char *upnp_type;

    if (strcmp(urn, "upnp:rootdevice") == 0)
	return "UPnp-RootDevice";
    
    if (strstr(urn, "urn:schemas-upnp-org:device:") != NULL) {
	upnp_type = "U-PnP Device";
    } else if (strstr(urn, "urn:schemas-upnp-org:service:") != NULL) {
	upnp_type = "U-PnP Service";
    } else {
	upnp_type = urn;
    }

    return upnp_type;
}

char *ssdp_shorten_urn_class(char *urn) {
    char *upnp_class;
    
    upnp_class = strrchr(urn, ':');
    if (!upnp_class)
	return NULL;   /* Not found. Sure this is a URN?? */
    /* Else */
    *upnp_class = '$';
    upnp_class = strrchr(urn, ':');
    if (!upnp_class)
	return NULL;
    upnp_class++; /* Point past final ':' */

    /* replace our '$' from earlier */
    char *drep = strrchr(upnp_class, '$');
    *drep = ':';

    return upnp_class;
}
