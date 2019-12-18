#!/bin/bash

gcc mdns.c -fPIC -I.. `pkg-config --cflags glib-2.0` \
		`pcap-config --cflags` \
		-g -O2 -rdynamic -shared -o mdns.so