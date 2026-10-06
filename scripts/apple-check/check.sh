#!/bin/bash
# Checks the Swift code of the iOS and macOS apps with Swift 6 on Linux.
# Runs inside the swift:6.0-jammy container with the repository mounted
# read-only at /repo; start it with scripts/check-apple.ps1.
#
# 1. Type-checks the engine wrapper, the models, the charts and the CPU, GPU
#    and Charts pages of both apps against stand-in modules (stubs/), the real
#    Foundation of Linux and the real C bridge headers.
# 2. Compiles the code that has no UI and runs tests.swift on it.
#
# This is not a build with the Apple SDK. See README.md for what it covers.
set -u
here=/repo/scripts/apple-check
mods=/tmp/mods
mkdir -p "$mods"
cd /tmp
status=0

echo "== stand-in modules =="
for name in SwiftUI AppKit UIKit QuartzCore Darwin; do
    if ! swiftc -swift-version 5 -parse-as-library -emit-module -module-name "$name" \
        -emit-module-path "$mods/$name.swiftmodule" "$here/stubs/$name.swift"; then
        echo "stand-in module $name failed to compile"
        status=1
    fi
done

check() {  # label, app directory, extra files
    echo "== type-check: $1 =="
    swiftc -swift-version 6 -typecheck -I "$mods" -module-name App \
        -import-objc-header "$2/Engine/GPUBenchmark-Bridging-Header.h" \
        "$2/Models/EngineOutput.swift" "$2/Models/ChartGeometry.swift" \
        "$2/Models/BenchResult.swift" "$2/Models/Localization.swift" \
        "$2/Components/BenchCharts.swift" "$2/Components/ResultCharts.swift" \
        "$2/Views/CpuView.swift" "$2/Views/ChartsView.swift" "$2/Views/RunView.swift" \
        "$2/Engine/BenchEngine.swift" "$here/stubs/AppStubs.swift" ${3:-} > /tmp/out.txt 2>&1
    rc=$?
    echo "exit $rc, $(grep -c 'error:' /tmp/out.txt) error(s), $(grep -c 'warning:' /tmp/out.txt) warning(s)"
    head -"${MAX_LINES:-80}" /tmp/out.txt
    [ $rc -ne 0 ] && status=1
}
check macOS /repo/macos-gui/GPUBenchmark
check iOS /repo/ios-gui/Mangekyo "$here/stubs/AppStubsIOS.swift"

echo "== files shared by both apps =="
for file in Models/EngineOutput.swift Models/ChartGeometry.swift Models/Localization.swift \
            Models/BenchResult.swift Components/BenchCharts.swift Components/ResultCharts.swift; do
    # Line endings depend on the checkout; compare the text only.
    if cmp -s <(tr -d '\r' < "/repo/macos-gui/GPUBenchmark/$file") <(tr -d '\r' < "/repo/ios-gui/Mangekyo/$file"); then
        echo "same  $file"
    else
        echo "DIFFERENT  $file"
        status=1
    fi
done

echo "== tests =="
# Top-level code needs a file called main.swift.
cp "$here/tests.swift" /tmp/main.swift
if swiftc -swift-version 6 -O /repo/ios-gui/Mangekyo/Models/EngineOutput.swift \
    /repo/ios-gui/Mangekyo/Models/ChartGeometry.swift /repo/ios-gui/Mangekyo/Models/Localization.swift \
    /tmp/main.swift -o /tmp/tests; then
    /tmp/tests "$here/fixtures" || status=1
else
    status=1
fi

[ $status -eq 0 ] && echo "ALL CHECKS PASSED" || echo "SOME CHECKS FAILED"
exit $status
