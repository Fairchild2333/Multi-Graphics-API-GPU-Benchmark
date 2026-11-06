#!/usr/bin/env bash
# Build ghostlock + make_profile with the NDK (mirrors src/Makefile flags;
# this machine has no GNU make). Run from anywhere; script cds itself.
set -euo pipefail

NDK="${NDK_ROOT:-/c/Users/lzz28/AppData/Local/Android/Sdk/ndk/28.2.13676358}"
PREBUILT="$NDK/toolchains/llvm/prebuilt/windows-x86_64"
CXX="$PREBUILT/bin/clang++.exe"
TARGET=aarch64-linux-android35

COMMON_FLAGS=(-O2 -flto -Wall -Wextra -Wconversion -Wsign-conversion
  -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function
  -Icore '-DTARGET_CONFIG_H="kernel/target.h"')
CXXFLAGS=("${COMMON_FLAGS[@]}" -std=c++23 -fno-rtti)
LDFLAGS=(-fPIE -pie -pthread -flto -static-libstdc++)

CXX_SRCS=(
core/main.cpp
core/attack/ops.cpp
core/profile/entry.cpp
core/memory/address_space.cpp
core/memory/heap_context.cpp
core/race/pi_race.cpp
core/route/route_controller.cpp
core/route/route_middleware.cpp
core/race/threads.cpp
core/route/tcp_zerocopy_route.cpp
core/route/select_stack_route.cpp
core/route/multicast_waiter_route.cpp
core/route/sigreturn_stamp.cpp
core/memory/payload_builder.cpp
core/session/runtime_config.cpp
core/profile/binary.cpp
core/support/util.cpp
core/session/exploit_session.cpp
core/session/root_child_frontend.cpp
core/session/backend/cve_2026_43499_backend.cpp
core/session/handoff_probe.cpp
core/session/victim_process.cpp
core/session/victim_context.cpp
core/support/native_resource.cpp
core/support/run_state.cpp
)

cd "$(dirname "$0")/src"
OUT=../build/native
mkdir -p "$OUT"

OBJS=()
for s in "${CXX_SRCS[@]}"; do
  o="$OUT/${s%.cpp}.o"
  mkdir -p "$(dirname "$o")"
  echo "CXX $s"
  "$CXX" --target=$TARGET "${CXXFLAGS[@]}" -c "$s" -o "$o"
  OBJS+=("$o")
done

echo "LINK ghostlock"
"$CXX" --target=$TARGET "${OBJS[@]}" "${LDFLAGS[@]}" -o "$OUT/ghostlock"

echo "CXX make_profile"
"$CXX" --target=$TARGET "${CXXFLAGS[@]}" ../tools/make_profile.cpp core/profile/binary.cpp "${LDFLAGS[@]}" -o "$OUT/make_profile"

echo "OK: $OUT/ghostlock $OUT/make_profile"
