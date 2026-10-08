#!/usr/bin/env bash
set -u
set -o pipefail
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
TASK_EVIDENCE=/home/liu/projects/nku-os/labs/report/evidence/build-link-20261008
printf 'Lab 1 A | Liu Huabin 2411238 | 2026-10-08 Asia/Shanghai\n'
printf '$ timeout -k 2s 5s make qemu < /dev/null\n'
timeout -k 2s 5s make qemu < /dev/null 2>&1 | tee "$TASK_EVIDENCE/boot.log"
TASK_BOOT_RC=${PIPESTATUS[0]}
printf 'boot command exit: %s (124 = timeout after observation)\n' "$TASK_BOOT_RC"
printf '%s\n' "$TASK_BOOT_RC" > "$TASK_EVIDENCE/boot.exit"
if (( TASK_BOOT_RC != 124 )); then exit 1; fi
if ! rg -q '\(THU.CST\) os is loading' "$TASK_EVIDENCE/boot.log"; then exit 1; fi
printf 'PASS: kernel startup text observed; infinite loop ends only by timeout.\n'
printf 'Boot evidence complete; window held for an actual screenshot.\n'
