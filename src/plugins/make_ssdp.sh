#!/bin/bash

gcc ssdp.c ssdp-util.c ssdp-xml.c -fPIC -I.. \
    `pkg-config --cflags glib-2.0` \
    `pcap-config --cflags` \
    `curl-config --cflags --libs` \
    `pkg-config --cflags --libs libxml-2.0` \
    -ggdb -O2 -rdynamic -shared -o ssdp.so