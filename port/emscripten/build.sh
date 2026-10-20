#!/bin/bash
# Builds the WebAssembly target into <build dir>/bin: Game.js, Game.wasm and ui/.
#
#   port/emscripten/build.sh [build dir] [configure]
#
# The build dir defaults to build-wasm in the source tree. "configure" reruns CMake;
# a build dir without build.ninja is configured on its own. Needs Emscripten's
# environment (source emsdk_env.sh first; built with emsdk 6.0.12), CMake and Ninja.
#
# The edition, platform and copyright options are the ones the Windows 99 build uses;
# pass your own through OPENTS_CMAKE_ARGS to replace them.
set -e
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$SRC/build-wasm}"
if [ -z "$EMSDK" ]; then
  echo "Emscripten's environment is not set: source emsdk_env.sh first." >&2
  exit 1
fi
if [ "$2" = "configure" ] || [ ! -f "$OUT/build.ninja" ]; then
  if [ -z "${OPENTS_CMAKE_ARGS+set}" ]; then
    OPENTS_CMAKE_ARGS=(-DOPENTS_EDITION="Windows 99 Build" -DOPENTS_ARCH_NAME=NT9X
      -DOPENTS_COPYRIGHT="1999 Electronic Arts, All Rights Reserved")
  else
    read -r -a OPENTS_CMAKE_ARGS <<< "$OPENTS_CMAKE_ARGS"
  fi
  cmake -S "$SRC" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Release "${OPENTS_CMAKE_ARGS[@]}" \
    -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
fi
cmake --build "$OUT"
