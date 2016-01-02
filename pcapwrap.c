/* A collection of useful wrappers around common Libpcap functionality.
 * Some of the routines provided are:
 * 
 *   open_pcap_session(char *src, int use_file, char *errbuf);
 *    - Open a libpcap session either as live from an interface or
 *      from a capture file. 'use_file' is non-zero if the source string
 *      in 'src' is the path of a file to open "offline". If zero, then
 *      'src' specifies the name of a network interface.
 * 
 *   set_session_filter(pcap_t *s, char *filter);
 *    - Compile and apply BPF filter to Pcap session using all-one's
 *      netmask (255.255.255.255).
 * 
 * EDIT:  27th August, 2015
 * In the event of error:
 *   These routines can be used in the same way as you would normally with
 *   pcap. If your handle routine returns null, use pcap's error functions to
 *   get more info. If the functions that compile/apply a BPF filter return less
 *   than zero, use pcap's functions to get more error info.
 *
 * Written by Owen Klan  -  23rd September, 2003
 */

/* HISTORY:
   27th August, 2015 - Edited commenting
                     - brought in calc_ip_offset_for_link() from Jigsaw
                       project code. Figured it has a suitable home here.
 */

#include "pcapwrap.h"

/* Calculate offset to add to bypass link-layer headers, so
 * that we can find the IP Packet Header's data structure in
 * memory. This offset is intended to be applied to the 'bytes'
 * pointer that is passed as an argument to a pcap callback routine.*/
static int calc_ip_offset_for_link(pcap_t *pc)
{
        int if_type = pcap_datalink(pc);
    
        switch (if_type) {
	  case DLT_EN10MB:                 /* Ethernets */
	            return 14;
	  case DLT_RAW:                    /* Raw IP capture */
	            return 0;
	  case DLT_LINUX_SLL:              /* Linux 'cooked' capture */
	            return 16;
	}
        return 0;                          /* RAW by default */
}

/* Open a pcap session from an interface or a file, depending on
 * the value of 'use_file'. Returns NULL on failure. */
pcap_t *open_pcap_session(char *src, int use_file, char *errbuf)
{
    pcap_t *sess = NULL;
#ifndef USE_PROMISC
# define USE_PROMISC  1
#endif
    
    if (use_file) {		       /* Use file as source */
	sess = pcap_open_offline(src, errbuf);
    } else {			       /* Use live capture as source */
	sess = pcap_open_live(src, 65535, USE_PROMISC, 3, errbuf);
    }
    
    return sess;
}

/* Function that attempts to compile and apply a BPF filter string
 * to a Libpcap context. Returns -1 on failure */
int set_session_filter(pcap_t *s, char *filter)
{
    int retval;
    struct bpf_program bpf_prog;
    bpf_u_int32 mask = 0xFFFFFFFF;
    
    if ((retval = pcap_compile(s, &bpf_prog, filter, 1, mask)) < 0)
      return retval;
    
    /* Attempt to apply the filter */
    if ((retval = pcap_setfilter(s, &bpf_prog)) < 0)
      return retval;
    
    pcap_freecode(&bpf_prog);
    
    return retval;
}
