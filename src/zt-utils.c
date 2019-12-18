/* Various utility functions made available to plugins
   through the ZTSniff framework.

   Written by Owen Klan  -  31st August, 2015 */

/* HISTORY:
   31st August, 2015 - created file, started out moving string
                       table code from SSDP plugin to utils. This
		       is inspired also by the fact that currently,
		       developing this function is tricky, and doing
		       it from within a dynamically-loaded shared object,
		       while multi-threaded... mmmm... fun.
   2nd Sept., 2015   - Fixed creation/destruction logic so we don't have
                       heap-corruptions. The code was the usual mess
		       associated with triple indirection. For now at
		       least, to help keep track of what level of ind.
		       relates to what, bag/line/word, we have convenient
		       typedefs. The logic in the 'guts' is still working
		       with a raw chunck of memory alone. To make this
		       thread safe we'll need to abstract further into a
		       data structure.
		     - Added convenient 'ztl_string_bag_dump()' function.
 */

#include "project-includes.h"

#include "zt-io.h"
#include "zt-utils.h"

/* Return a LineTokens for each line. Start is where processing
   for the line will begin. 'next' is an out parameter that 
   will receive the pointer to the beginning of the next line.
   If we've processed the last line, 'next' will be set to NULL */
LineTokens zt_create_line_tokens(char *start, char **next) {
    if (!start || !next) {
	return NULL;   /* Not even going to touch that "data" */
    }

    LineTokens ret_val = NULL;
    char *i = start, *a = start;
    int  more_words = 1;  /* Processing loop flag */

    /* Make new GSList for this line's words */
    GSList *word_list = NULL;
    while (more_words) {
	if (*i == ' ' && !(*(i+1) == '\x0d' && *(i+2) == '\x0a')) {
	    *i = '\0';  /* Nul-terminate then setup for next word */
	    word_list = g_slist_prepend(word_list, (gpointer)(a));
	    
	    i++;
	    a = i;     /* Reset start ptr (a) to next word */
	} else if (*i == '\x0d' && *(i+1) == '\x0a') { /* End of Line */
	    *i = '\0';
	    more_words = 0;
	    /* if 'a' hasn't moved, then we have no more lines, empty
	     * we'll set our out-parameter, 'next', to NULL so higher
	     * functions know that we're done. */
	    if (a == i) {  
		word_list = g_slist_prepend(word_list, (gpointer)NULL);
		*next = NULL;
	    } else {
		word_list = g_slist_prepend(word_list, (gpointer)a);

		i += 2; /* Point past the '\r\n' newline sequence, set our
			   out-parameter*/
		*next = i;		
	    }
	} else {
	    i++;
	}
    } /* end   while (more_words) {   */
    /* Now, given our word list, reverse it, then build an
       array to hold the WordToken's for this line */
    word_list = g_slist_reverse(word_list);

    int word_count = g_slist_length(word_list);
    int alloc_count = word_count + 1; // Add one for our NULL

    ret_val = (LineTokens)malloc(alloc_count * sizeof(WordToken));

    int j = 0;
    for (j = 0; j < alloc_count; j++) {
	WordToken w = (WordToken)g_slist_nth_data(word_list, j);
	ret_val[j] = w;
	
	if (w == NULL)  break;   // hit our terminator
    }

    return ret_val;
}

/* Given a string bag, iterate to the next line.
   Thread safe in regards to internal data management. */
LineTokens ztl_line_tokens_next(StringBag bag) {
    return NULL;
}

StringBag ztl_string_bag_create(char *data, int len) {
    /* Make a duplicate of the entire buffer */
    char *dup = malloc(len);
    memcpy(dup, data, len);
    char *i = dup;
    
    /* Turn our char * into an array of LineTokens */
    GSList *lines_list = NULL;
    while (i != NULL) {
	/* We'll reverse the list once it's built, making our
	   insert operation O(1) */
	LineTokens test_line = zt_create_line_tokens(i, &i);
	lines_list = g_slist_prepend(lines_list,
				     (gpointer)test_line);
    }
    lines_list = g_slist_reverse(lines_list);

    int line_count = g_slist_length(lines_list);
    int alloc_count = line_count + 1;

    StringBag bag = (StringBag)malloc(alloc_count * sizeof(LineTokens));
    int k = 0;
    for (k = 0; k < line_count; k++) {
	bag[k] = g_slist_nth_data(lines_list, k);
    }
    bag[line_count] = NULL;

    return bag;
}

/* Destroy string table built by ztl_string_table_create() */
void ztl_string_bag_destroy(StringBag bag) {
    int i = 0;

    if (!bag) return;

    while (1) {
	if (*(bag + i) != NULL) {
	    free(*(bag + i));
	    i++;
	} else {
	    break;
	}
    }

//    if (bag[0])
//	free(*(bag[0]));

    free(bag);
}

/* Utility function to display text dump of entire
   string bag */
void ztl_string_bag_dump(StringBag bag) {
    int num_lines = 0;
    int i;

    if (!bag) {
	ztprint("DEBUG ", "Where's the bag?\n");
	return;
    }

    /* Determine number of lines in this bag */
    while (*(bag+num_lines))   { num_lines++; }

    ztprint("DEBUG ", "Bag has %d lines\n", num_lines);

    for (i = 0; i <  num_lines; i++) {
	char line_buffer[512];
	int j = 0;

	memset(line_buffer, 0x00, 512);

	while (*((bag+i)[j])) {
	    strcat(line_buffer, *(bag+i)[j]);
	    strcat(line_buffer, ", ");
	}
	*(strrchr(line_buffer, ',')) = '\0';

	ztprint("DEBUG", "Line %d, Word[0,1]:  '%s','%s'\n", i, *(bag+i)[0],
		(*(bag+i))[1]);
    }
}
