#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deveco_dir="${DEVECO_HOME:-/Applications/DevEco-Studio.app/Contents}"
build_mode="${1:-debug}"
case "$build_mode" in debug|release) ;; *) echo 'Usage: bash scripts/build-ohos.sh [debug|release]' >&2; exit 2 ;; esac
for needed in sdk/default/openharmony/native tools/hvigor/bin/hvigorw tools/ohpm/bin/ohpm tools/node/bin/node jbr/Contents/Home/bin/java; do
  if [[ ! -e "$deveco_dir/$needed" ]]; then
    echo "DevEco component missing: $deveco_dir/$needed (set DEVECO_HOME)" >&2
    exit 2
  fi
done
export DEVECO_SDK_HOME="$deveco_dir/sdk"
export NODE_HOME="$deveco_dir/tools/node"
export JAVA_HOME="$deveco_dir/jbr/Contents/Home"
export PATH="$NODE_HOME/bin:$deveco_dir/tools/ohpm/bin:$deveco_dir/tools/hvigor/bin:$JAVA_HOME/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
cd "$repo_dir/ohos"
ohpm install --all
hvigorw assembleHap --mode module -p product=default -p "buildMode=$build_mode" --no-daemon
