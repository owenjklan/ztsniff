/* Header file for my Libpcap common routine 'wrapper' 'library'.
 * 
 * Written by Owen Klan  -  23rd September, 2003
 */

#ifndef __PCAP_WRAP_H_
#define __PCAP_WRAP_H_

#include <pcap.h>

extern pcap_t *open_pcap_session(char *src, int use_file, char *errbuf);
extern int set_session_filter(pcap_t *s, char *filter);

#endif
