#!/usr/bin/env bash
# Let OpenSBI obtain its next address from QEMU's kernel option.
set -euo pipefail

args=()
while (( $# )); do
    if [[ $1 == -device && ${2:-} == loader,file=bin/ucore.img,addr=0x80200000 ]]; then
        args+=(-kernel bin/ucore.img)
        shift 2
    else
        args+=("$1")
        shift
    fi
done

exec "${QEMU_REAL:-qemu-system-riscv64}" "${args[@]}"
