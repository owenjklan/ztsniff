#ifndef _PROJECT_INCLUDES_H_
#define _PROJECT_INCLUDES_H_

/* This file is simply a convenient bundling-place for
all the frequently used, common header files for this
project. This just cuts down clutter in the individual
source files. 

Written by Owen Klan  -  27th August, 2015 

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <pcap/pcap.h>

#include "pcapwrap.h"   /* Custom wrapper library. */

#ifndef _ZT_INTERNAL                   /* We want the full definition of */
#define _ZT_INTERNAL        	       /* the zt_plugin structure */
#endif
#include "zt-plugin.h"  /* Include file for plugins */

#endif  /* _PROJECT_INCLUDES_H_ */
