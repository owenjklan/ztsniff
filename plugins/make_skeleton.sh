#!/bin/bash

gcc skeleton.c -fPIC -I.. `pkg-config --cflags glib-2.0` \
		`pcap-config --cflags` \
		-g -O2 -rdynamic -shared -o skeleton.so