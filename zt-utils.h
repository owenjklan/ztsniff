#ifndef _ZT_UTILS_H_
#define _ZT_UTILS_H_

#include "zt-commands.h"

typedef char *** StringBag;
typedef char **  LineTokens;
typedef char *   WordToken;

/* From zt-utils.c */
extern StringBag ztl_string_bag_create(char *data, int len);
extern void ztl_string_bag_destroy(StringBag bag);
extern void ztl_string_bag_dump(StringBag bag);

/* From ztsniff.c main source */
extern void ztl_call_master(char *tag, GThread *thid,
			    master_cmd command, void *args);

#endif  /*  _ZT_UTILS_H_  */
