#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -lm -Og -g -fsanitize=address -o server
mkdir -p chunks
