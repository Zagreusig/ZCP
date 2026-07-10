#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-./build}"
DEPFILES=$(find "$BUILD_DIR" -name '*.cpp.o.d' 2>/dev/null || true)

if [ -z "$DEPFILES" ]; then
   exit 0
fi

for header in "$@"; do
   grep -l -F "$header" $DEPFILES 2>/dev/null || true
done | sort -u | sed -e "s#^${BUILD_DIR}/##" -e 's#\.o\.d$##'