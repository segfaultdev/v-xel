#!/usr/bin/sh

gcc -Llib -Iinclude "v!xel.c" -O3 -o "v!xel" -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -g
