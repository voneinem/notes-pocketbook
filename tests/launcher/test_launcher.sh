#!/usr/bin/env bash
# The reader's launcher (platform/pocketbook/pbnotes.app) on the host, with a fake pbnotes.bin:
#   - it is valid sh,
#   - a start that aborts with the GPU refusal (exit 134, "mali0") is retried exactly once and both results are logged,
#   - any other failure is not retried,
#   - the log of the launch before is kept as pbnotes.log.prev.
#
#   tests/launcher/test_launcher.sh [repo-root]
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
LAUNCHER="$ROOT/platform/pocketbook/pbnotes.app"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
fail=0
check() { if ! "$@"; then echo "FAIL: $*" >&2; fail=1; fi; }

sh -n "$LAUNCHER" || { echo "FAIL: launcher is not valid sh" >&2; exit 1; }

# run the launcher against a fake device layout; $1 = body of the fake binary
launch() {
    rm -rf "$tmp/app" "$tmp/data" "$tmp/root" "$tmp/count"; mkdir -p "$tmp/app" "$tmp/root"
    printf '#!/bin/sh\necho x >> "%s/count"\n%s\n' "$tmp" "$1" > "$tmp/app/pbnotes.bin"
    chmod +x "$tmp/app/pbnotes.bin"
    sed -e "s#/mnt/ext1/applications#$tmp/app#; s#/mnt/ext1/system/pbnotes#$tmp/data#; s#cd /mnt/ext1#cd $tmp/root#" \
        -e 's#^sleep 2$#true#; s#^    sleep 2$#    true#' "$LAUNCHER" > "$tmp/launch.sh"
    sh "$tmp/launch.sh"
}
runs() { wc -l < "$tmp/count" | tr -d ' '; }

# 1. GPU refusal on the first start, fine on the second
launch 'if [ -f "'"$tmp"'/ok" ]; then echo started; exit 0; fi; touch "'"$tmp"'/ok"; echo "failed to open device file /dev/mali0 with errno 13" >&2; kill -ABRT $$'
rm -f "$tmp/ok"
launch 'if [ -f "'"$tmp"'/ok" ]; then echo started; exit 0; fi; touch "'"$tmp"'/ok"; echo "failed to open device file /dev/mali0 with errno 13" >&2; kill -ABRT $$'
check [ "$(runs)" = 2 ]
check grep -q 'exit code: 134' "$tmp/data/pbnotes.log"
check grep -q 'retrying once' "$tmp/data/pbnotes.log"
check grep -q 'exit code (retry): 0' "$tmp/data/pbnotes.log"
check grep -q 'diagnostics: uid/groups' "$tmp/data/pbnotes.log"

# 2. GPU refusal twice: one retry only
launch 'echo "failed to open device file /dev/mali0 with errno 13" >&2; kill -ABRT $$'
check [ "$(runs)" = 2 ]

# 3. an abort that is not the GPU refusal is not retried
launch 'echo "something else" >&2; kill -ABRT $$'
check [ "$(runs)" = 1 ]

# 4. a normal exit is not retried, and the previous log is kept
launch 'echo first >&2; exit 0'
sed -e "s#/mnt/ext1/applications#$tmp/app#; s#/mnt/ext1/system/pbnotes#$tmp/data#; s#cd /mnt/ext1#cd $tmp/root#" "$LAUNCHER" > "$tmp/launch.sh"
printf '#!/bin/sh\necho second >&2; exit 0\n' > "$tmp/app/pbnotes.bin"
sh "$tmp/launch.sh"
check grep -q second "$tmp/data/pbnotes.log"
check grep -q first "$tmp/data/pbnotes.log.prev"

[ "$fail" = 0 ] && echo "launcher tests passed"
exit "$fail"
