#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

cp -R "$here/../../../code" "$work/code"
cd "$work/code"
make --quiet clean
cp "$here/fixture.c" kern/schedule/c_fixture.c
cp "$here/fixture.S" kern/schedule/c_probe.S
sed -i '/^int kern_init(void) __attribute__/i void test_c_schedule(void);' kern/init/init.c
sed -i '/^    cpu_idle();/i\    test_c_schedule();' kern/init/init.c

# Exercise the tick path even when the old Lab3 grading macro is defined.
make --quiet DEFS=-DDEBUG_GRADE > "$work/build.log" 2>&1 || {
    cat "$work/build.log" >&2
    exit 1
}

"${GCCPREFIX:-riscv64-unknown-elf-}nm" -n bin/kernel | \
    awk '$3 ~ /^(kern_entry|forkret|forkrets|__trapret|kernel_thread_entry|proc_run|switch_to)$/ {print "[C symbol]", $0}'

timeout 15s "${QEMU_REAL:-qemu-system-riscv64}" -machine virt -smp 1 -nographic \
    -monitor none -bios default -kernel bin/ucore.img </dev/null 2>&1 | tr -d '\r' | tee "$work/run.log"
grep -Fq '[C result] PASS' "$work/run.log"
