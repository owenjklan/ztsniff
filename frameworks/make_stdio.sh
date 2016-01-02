#!/bin/bash

gcc stdio.c -fPIC -I.. `pkg-config --cflags glib-2.0` \
		`pkg-config --cflags --libs ncurses`\
		-g -O2 -rdynamic -shared -o stdio.so