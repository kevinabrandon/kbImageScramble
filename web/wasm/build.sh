#!/usr/bin/env bash
# Build the Image Scramble engine to WebAssembly.
# Requires emsdk (source ~/.emsdk/emsdk_env.sh first, or have emcc on PATH).
set -euo pipefail
cd "$(dirname "$0")"

emcc -O2 \
  -include msvc_compat.h \
  kbImageMap.cpp kbEngine.cpp api.cpp \
  -o ../public/engine.js \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sEXPORT_NAME=createEngine \
  -sASYNCIFY \
  -sALLOW_MEMORY_GROWTH=1 \
  -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,HEAP32,FS,ccall,UTF8ToString,stringToNewUTF8 \
  -sEXPORTED_FUNCTIONS=_malloc,_free

echo "Built ../public/engine.js + engine.wasm"
