#!/usr/bin/sh

echo "WARNING: change PYTHON_PATH inside build.sh before building!"

export PYTHON_PATH=/usr/bin
export PATH="$PYTHON_PATH:$PATH"

# gcc -Llib -Iinclude "v!xel.c" -Ofast -o "v!xel" -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -s

emcc -Os "v!xel_web.c" -Lweb -Iinclude -lraylib --memory-init-file 0 -s ASYNCIFY -s USE_GLFW=3 -s TOTAL_MEMORY=268435456 -o "v!xel.html" \
  --shell-file web/shell.html -DPLATFORM_WEB -Wall -Wno-unused-variable -Wno-unused-but-set-variable \
