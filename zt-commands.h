#ifndef _ZT_COMMANDS_H_
#define _ZT_COMMANDS_H_

#include "zt-plugin.h"

/* Enum for 'command' field used when calling the Master
   thread */
enum _master_cmd {
    MC_KILL_SELF,
    MC_SESSION_STATS,
    MC_KILL_ALL,
    MC_EXIT
};
typedef enum _master_cmd master_cmd;


extern void _zt_call_master(zt_plugin *pi, GThread *thid,
			    master_cmd command, gpointer args);

#endif  /*  _ZT_COMMANDS_H_  */
