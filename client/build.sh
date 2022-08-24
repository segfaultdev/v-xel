#!/usr/bin/sh

OPT_FLAGS="-g -fwhole-program -flto -Ofast -fomit-frame-pointer -fno-signed-zeros -march=native -fopenmp -fno-math-errno 
-funsafe-math-optimizations -ffinite-math-only -fno-trapping-math -fsingle-precision-constant"

gcc $(find . -name "*.c") -Iinclude -Llib \
    -lGL -lc -lm -lrt -lX11 -ldl -pthread \
    lib/libraylib.a \
    $OPT_FLAGS -o client