#!/bin/bash

gcc ncurses.c ncurses-list-window.c ncurses-dialog.c -fPIC -I.. `pkg-config --cflags glib-2.0 ncurses menu panel` \
		`pkg-config --cflags --libs ncurses menu panel` \
		-ggdb -O2 -rdynamic -shared -o ncurses.so