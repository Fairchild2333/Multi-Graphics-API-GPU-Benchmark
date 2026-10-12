#!/bin/bash
# Compiles the engine sources that need no GPU (the CPU tests, the machine
# details and the results file) for macOS with the clang and the
# macOS headers that ship with zig. Runs inside the python:3.12-slim container
# with the repository mounted read-only at /repo; start it with
# scripts/check-apple.ps1.
#
# Objects only: nothing is linked or run. zig has no iOS headers, so the
# iOS-only code (TARGET_OS_IPHONE) is not compiled here. The x64 target needs
# the evex512 feature on the command line for the AVX-512 kernels, because zig
# passes an explicit feature list to clang.
pip install --quiet --root-user-action=ignore --disable-pip-version-check ziglang 2>&1 | tail -2
echo "zig $(python -m ziglang version)"
cd /tmp
status=0
for spec in "aarch64-macos" "x86_64-macos -mcpu=x86_64+evex512"; do
    echo "== $spec =="
    for file in cpu_membw.cpp cpu_benchmark.cpp cpumem_cli.cpp system_info.cpp \
        benchmark_results.cpp path_service.cpp; do
        # shellcheck disable=SC2086
        python -m ziglang c++ -target $spec -std=c++17 -O2 -Wall -Wextra \
            -Wno-nullability-completeness -I/repo/src -c "/repo/src/$file" -o "$file.o" 2> err.txt
        rc=$?
        size=$(stat -c %s "$file.o" 2>/dev/null || echo 0)
        echo "$file: exit $rc, object $size bytes"
        grep -v "ziglang/lib" err.txt | head -"${MAX_LINES:-40}"
        [ $rc -ne 0 ] && status=1
        rm -f "$file.o"
    done
done
[ $status -eq 0 ] && echo "ALL OBJECTS COMPILED" || echo "SOME FILES FAILED"
exit $status
