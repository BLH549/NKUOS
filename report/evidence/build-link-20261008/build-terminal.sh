#!/usr/bin/env bash
set -u
set -o pipefail
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
TASK_EVIDENCE=/home/liu/projects/nku-os/labs/report/evidence/build-link-20261008
printf 'Lab 1 A | Liu Huabin 2411238 | 2026-10-08 Asia/Shanghai\n'
printf '$ make clean\n'
make clean 2>&1 | tee "$TASK_EVIDENCE/clean.log"
TASK_CLEAN_RC=${PIPESTATUS[0]}
printf 'clean exit: %s\n' "$TASK_CLEAN_RC"
printf '%s\n' "$TASK_CLEAN_RC" > "$TASK_EVIDENCE/clean.exit"
if (( TASK_CLEAN_RC != 0 )); then exit "$TASK_CLEAN_RC"; fi
printf '$ make -j2 V=\n'
make -j2 V= 2>&1 | tee "$TASK_EVIDENCE/build.log"
TASK_BUILD_RC=${PIPESTATUS[0]}
printf 'build exit: %s\n' "$TASK_BUILD_RC"
printf '%s\n' "$TASK_BUILD_RC" > "$TASK_EVIDENCE/build.exit"
if (( TASK_BUILD_RC != 0 )); then exit "$TASK_BUILD_RC"; fi
printf '$ riscv64-unknown-elf-readelf -h bin/kernel\n'
riscv64-unknown-elf-readelf -h bin/kernel | sed -n '1,20p'
printf '$ sha256sum bin/kernel bin/ucore.img\n'
sha256sum bin/kernel bin/ucore.img
printf 'Build evidence complete; window held for an actual screenshot.\n'
