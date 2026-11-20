#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
mode=$1
work=$(cd -- "$2" && pwd)

case "$mode" in
    prepare)
        mkdir "$work/code"
        tar -C "$root/code" --exclude=./bin --exclude=./obj -cf - . |
            tar -C "$work/code" -xf -
        cd "$work/code"
        python3 tools/inject.py "$PWD"
        # Leave the teacher's Process Create check active in this copy.
        sed -i '/^    check_pgdir();$/d; /^    check_boot_pgdir();$/d; /^    test_lab4_pmm_basic();$/d' kern/mm/pmm.c
        sed -i '/^    test_lab4_schedule();$/d' kern/process/proc.c
        sed -i '/^#include <proc.h>$/a void test_lab4_b_run(void) __attribute__((noreturn));' kern/init/init.c
        sed -i '/^    clock_init();/d; /^    intr_enable();/d; s/^    cpu_idle();.*/    test_lab4_b_run();/' kern/init/init.c
        sed -i '/^LDFLAGS.*--gc-sections/a LDFLAGS += --wrap=kmalloc --wrap=kfree --wrap=alloc_pages --wrap=free_pages' Makefile
        sed -i '/^#include <proc.h>$/a #include <sbi.h>' kern/process/proc.c
        # The fixture is included in proc.c to inspect its private PID table.
        printf '\n#include "lab4_b_fixture.c.inc"\n' >> kern/process/proc.c
        cp "$here/fixture.c" kern/process/lab4_b_fixture.c.inc
        make -j2 > "$work/build.log" 2>&1 || {
            tail -60 "$work/build.log"
            exit 1
        }
        printf '[B fixture] built %s/code/bin/kernel\n' "$work"
        ;;
    run|debug)
        cd "$work/code"
        qemu=("${QEMU:-qemu-system-riscv64}" -machine virt -display none -monitor none
              -serial stdio -bios default -kernel bin/ucore.img)
        if [[ $mode == debug ]]; then
            exec "${qemu[@]}" -gdb tcp:127.0.0.1:12340 -S </dev/null
        fi
        timeout 25s "${qemu[@]}" </dev/null > "$work/qemu.log" 2>&1 || {
            cat "$work/qemu.log"
            exit 1
        }
        cat "$work/qemu.log"
        grep -Fq '[Test Process Create] PASS' "$work/qemu.log"
        grep -Fq '[B result] PASS' "$work/qemu.log"
        ;;
    *)
        printf 'Usage: bash %s {prepare|run|debug} EXISTING_WORK_DIR\n' "$0" >&2
        exit 2
        ;;
esac
