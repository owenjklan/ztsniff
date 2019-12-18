#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <curl/curl.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "../zt-plugin.h"
#include "../zt-utils.h"
#include "../zt-io.h"

#include "ssdp.h"

extern zt_plugin manifest;
extern GAsyncQueue *xml_q;
extern GThread *xml_thread;

int execute_xpath_expression(const char* filename,
			     const xmlChar* xpathExpr,
			     const xmlChar* nsList);
int register_namespaces(xmlXPathContextPtr xpathCtx,
			const xmlChar* nsList);

GThreadFunc _dl_xml(gpointer curl) {
}

/* Utilise Libcurl's Easy interface to retrieve
   a device description file, as XML */
GThreadFunc ssdp_retrieve_xml() {
    char *url;
    GThread *dl_thread;
    char *template = "ssdp_xml-XXXXXX";
    char *ua = "McDonald's Free WiFi Connector";
    char *outname = "ssdp_xml";
    FILE *outfile = NULL;

    /* We'll wait for URL's to be delivered. We then
       download the file and setup an XPath context on
       it. The resultant data structure is kept in
       'ssdp_xml_dls', keyed by the URL */
    while (1) {
	url = (char *)g_async_queue_pop(xml_q);
	
	outfile = fopen(outname, "w");    
	if (!outfile) {
	    zterror(MYTAG, "Failed saving XML to disk! %s\n", strerror(errno));
	    continue;
	}
	
	/* Create curl context and download */
	CURL *curl_h = curl_easy_init();
	curl_easy_setopt(curl_h, CURLOPT_URL, url);
	curl_easy_setopt(curl_h, CURLOPT_WRITEDATA, outfile);
	curl_easy_setopt(curl_h, CURLOPT_USERAGENT, ua);
	
	int retval = curl_easy_perform((CURL *)curl_h);
	
	if (retval != CURLE_OK)
	    zterror(MYTAG, "Failed downloading XML!\n");
	
	curl_easy_cleanup(curl_h);
	if (outfile)  fclose(outfile);
	ztprint(MYTAG, "Downloaded XML\n");

	execute_xpath_expression(outname, "//controlURL", "xmlns=urn:schemas-upnp-org:device-1-0");
    }
}

/* The following code was ripped directly from the XMLSoft.org example
   code page.
   all credit goes to the authors of that code. */
/**
 * register_namespaces:
 * @xpathCtx:		the pointer to an XPath context.
 * @nsList:		the list of known namespaces in 
 *			"<prefix1>=<href1> <prefix2>=href2> ..." format.
 *
 * Registers namespaces from @nsList in @xpathCtx.
 *
 * Returns 0 on success and a negative value otherwise.
 */
int 
register_namespaces(xmlXPathContextPtr xpathCtx, const xmlChar* nsList) {
    xmlChar* nsListDup;
    xmlChar* prefix;
    xmlChar* href;
    xmlChar* next;
    
    nsListDup = xmlStrdup(nsList);
    if(nsListDup == NULL) {
	fprintf(stderr, "Error: unable to strdup namespaces list\n");
	return(-1);	
    }
    
    next = nsListDup; 
    while(next != NULL) {
	/* skip spaces */
	while((*next) == ' ') next++;
	if((*next) == '\0') break;

	/* find prefix */
	prefix = next;
	next = (xmlChar*)xmlStrchr(next, '=');
	if(next == NULL) {
	    fprintf(stderr,"Error: invalid namespaces list format\n");
	    xmlFree(nsListDup);
	    return(-1);	
	}
	*(next++) = '\0';	
	
	/* find href */
	href = next;
	next = (xmlChar*)xmlStrchr(next, ' ');
	if(next != NULL) {
	    *(next++) = '\0';	
	}

	/* do register namespace */
	if(xmlXPathRegisterNs(xpathCtx, prefix, href) != 0) {
	    fprintf(stderr,"Error: unable to register NS with prefix=\"%s\" and href=\"%s\"\n", prefix, href);
	    xmlFree(nsListDup);
	    return(-1);	
	}
    }
    
    xmlFree(nsListDup);
    return(0);
}

void
print_xpath_nodes(xmlNodeSetPtr nodes, FILE* output) {
    xmlNodePtr cur;
    int size;
    int i;
    
    size = (nodes) ? nodes->nodeNr : 0;
    
    ztprint(MYTAG, "Result (%d nodes):\n", size);
    for(i = 0; i < size; ++i) {
	
	if(nodes->nodeTab[i]->type == XML_NAMESPACE_DECL) {
	    xmlNsPtr ns;
	        
	    ns = (xmlNsPtr)nodes->nodeTab[i];
	    cur = (xmlNodePtr)ns->next;
	    if(cur->ns) { 
		ztprint(MYTAG, "= namespace \"%s\"=\"%s\" for node %s:%s\n", 
			ns->prefix, ns->href, cur->ns->href, cur->name);
	    } else {
		ztprint(MYTAG, "= namespace \"%s\"=\"%s\" for node %s\n", 
			ns->prefix, ns->href, cur->name);
	    }
	} else if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	    cur = nodes->nodeTab[i];       
	    if(cur->ns) { 
		ztprint(MYTAG, "= element node \"%s:%s\"\n", 
			cur->ns->href, cur->name);
	    } else {
		ztprint(MYTAG, "= element node \"%s\"\n", 
			cur->name);
	    }
	} else {
	    cur = nodes->nodeTab[i];    
	    ztprint(MYTAG, "= node \"%s\": type %d\n", cur->name, cur->type);
	}
    }
}

/**
 * execute_xpath_expression:
 * @filename:the input XML filename.
 * @xpathExpr:the xpath expression for evaluation.
 * @nsList:the optional list of known namespaces in 
 *"<prefix1>=<href1> <prefix2>=href2> ..." format.
 *
 * Parses input XML file, evaluates XPath expression and prints results.
 *
 * Returns 0 on success and a negative value otherwise.
 */
int 
execute_xpath_expression(const char* filename, const xmlChar* xpathExpr,
			 const xmlChar* nsList) {
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx; 
    xmlXPathObjectPtr xpathObj; 
    
    /* Load XML document */
    doc = xmlParseFile(filename);
    if (doc == NULL) {
	fprintf(stderr, "Error: unable to parse file \"%s\"\n", filename);
	return(-1);
    }

    /* Create xpath evaluation context */
    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        fprintf(stderr,"Error: unable to create new XPath context\n");
        xmlFreeDoc(doc); 
        return(-1);
    }
    
    /* Register namespaces from list (if any) */
    if((nsList != NULL) && (register_namespaces(xpathCtx, nsList) < 0)) {
        fprintf(stderr,"Error: failed to register namespaces list \"%s\"\n", nsList);
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return(-1);
    }

    /* Evaluate xpath expression */
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if(xpathObj == NULL) {
        fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return(-1);
    }

    /* Print results */
    print_xpath_nodes(xpathObj->nodesetval, stdout);

    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx); 
    xmlFreeDoc(doc); 
    
    return(0);
}
