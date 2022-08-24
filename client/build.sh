#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -Llib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -Ofast -s -fwhole-program -flto -fomit-frame-pointer -fno-signed-zeros \
  -march=native -fopenmp -fno-math-errno -funsafe-math-optimizations -ffinite-math-only -fno-trapping-math -fsingle-precision-constant -o client
