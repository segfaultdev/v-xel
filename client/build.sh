#!/usr/bin/sh

gcc $(find . -name "*.c") ../vx_packet.c -Iinclude -Llib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -Ofast -s -I../include -o client
