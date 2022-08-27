#!/usr/bin/sh

gcc $(find . -name "*.c") ../vx_packet.c -Iinclude -lm -Og -g -fsanitize=address -I../include -o server
mkdir -p chunks
