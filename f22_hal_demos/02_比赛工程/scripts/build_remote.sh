#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Use installed SDCC, or the locally extracted compiler used for this delivery.
if command -v sdcc >/dev/null; then
    task_sdcc=$(command -v sdcc)
    task_flags=()
elif [[ -x /tmp/f22-sdcc/root/usr/bin/sdcc ]]; then
    task_sdcc=/tmp/f22-sdcc/root/usr/bin/sdcc
    task_flags=(-I/tmp/f22-sdcc/root/usr/share/sdcc/include -L/tmp/f22-sdcc/root/usr/share/sdcc/lib/small)
else
    echo '需要 SDCC mcs51 编译器；已有烧录文件 remote/tle100/tle100.hex。' >&2
    exit 1
fi
mkdir -p remote/tle100/build
"$task_sdcc" -mmcs51 --std-c11 --model-small --iram-size 256 --xram-size 0 --code-size 8192 \
    "${task_flags[@]}" remote/tle100/main.c -o remote/tle100/build/tle100.ihx
# SDCC .ihx is already Intel HEX; normalize with packihx when available.
task_packer="$(dirname "$task_sdcc")/packihx"
if [[ -x "$task_packer" ]]; then
    "$task_packer" remote/tle100/build/tle100.ihx > remote/tle100/tle100.hex
else
    cp remote/tle100/build/tle100.ihx remote/tle100/tle100.hex
fi
