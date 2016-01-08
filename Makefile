CC=gcc
OBJS= ztsniff.o zt-builtin.o zt-utils.o zt-io.o plugin-core.o pcapwrap.o

GLIB2FLAGS=$(shell pkg-config --cflags glib-2.0)
GLIB2LIBS=$(shell pkg-config --libs libxml-2.0 glib-2.0)

XMLFLAGS=$(shell pkg-config --cflags libxml-2.0)
XMLLIBS=$(shell pkg-config --libs libxml-2.0)

PCAPFLAGS=$(shell pcap-config --cflags)
PCAPLIBS=$(shell pcap-config --libs)

CURLFLAGS=$(shell pkg-config --cflags libcurl)
CURLLIBS=$(shell pkg-config --libs libcurl)

CFLAGS=-Wall -fPIC -g -ggdb -rdynamic $(GLIB2FLAGS) $(XMLFLAGS) $(PCAPFLAGS) $(CURLFLAGS)
LIBS=$(XMLLIBS) $(PCAPLIBS) $(CURLLIBS) $(GLIB2LIBS) -ldl

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

all: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LIBS) -O2  -o ztsniff

debug: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LIBS)  -o ztsniff

static: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LIBS) -o ztsniff -static

 clean:
	rm -f *.o
	rm -f ztsniff
	rm -f *~

install:
	cp ztsniff /usr/local/bin
