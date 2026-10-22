#!/bin/sh
# Builds the copy in ~/mangekyo that scripts/linux-suite.ps1 sends, Release and
# without the install and packaging rules, then runs the result-file test.
set -eu
cd "$(dirname "$0")/../.."
# A newer CMake from pip (the distribution's is older than 3.22) sits here.
[ -d "$HOME/.local/bin" ] && PATH="$HOME/.local/bin:$PATH"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGPU_BENCH_ENABLE_PACKAGING=OFF
cmake --build build -j"$(nproc)"
# The stand-alone OpenGL ES 2.0 GPU Burn, for GPUs whose driver offers only
# OpenGL ES 2.0 (Zhaoxin C-960); needs the EGL and GLES headers.
if [ -e /usr/include/EGL/egl.h ] && [ -e /usr/include/GLES2/gl2.h ]; then
    g++ -std=c++17 -O2 -Wall -Wextra src/es2burn_cli.cpp -lEGL -lGLESv2 -lm -o build/mangekyo_es2burn
fi
ctest --test-dir build --output-on-failure
