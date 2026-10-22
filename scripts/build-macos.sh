#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
preset="${1:-macos-debug}"
case "$preset" in macos-debug|macos-release) ;; *) echo "Invalid macOS preset" >&2; exit 1 ;; esac
: "${DAYMARK_QT:?Set DAYMARK_QT to the Qt 6.10.2 macOS kit directory}"
cmake --preset "$preset"
cmake --build --preset "$preset" --parallel 4
ctest --preset "$preset"
