#!/usr/bin/env python3
"""Compile actual PMM sources with ASan/UBSan; no RISC-V tools required."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = r'''
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
int cprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vprintf(fmt, ap);
    va_end(ap);
    return result;
}
__attribute__((noreturn))
void __panic(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "panic at %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(100);
}
'''


def main():
    cc = os.environ.get("HOSTCC", "cc")
    with tempfile.TemporaryDirectory(prefix="lab2-host-") as directory:
        tmp = Path(directory)
        runtime = tmp / "runtime.c"
        runtime.write_text(RUNTIME)
        flags = ["-g", "-O1", "-fno-builtin", "-fsanitize=address,undefined",
                 "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
                 "-std=gnu99", "-Wno-unused"]
        subprocess.run([cc, *flags, "-c", str(runtime), "-o",
                        str(tmp / "runtime.o")], check=True)
        includes = ["-I" + str(ROOT / p)
                    for p in ("libs", "kern/mm", "kern/debug", "kern/driver")]
        for name, sources in (
            ("allocators", ["tools/check_allocators.c", "kern/mm/best_fit_pmm.c",
                            "kern/mm/default_pmm.c"]),
            ("page_init", ["tools/check_page_init.c"]),
        ):
            subprocess.run([cc, *flags, *includes,
                            *(str(ROOT / p) for p in sources),
                            str(tmp / "runtime.o"), "-o", str(tmp / name)],
                           check=True)
        for mode in ("best_fit", "first_fit"):
            subprocess.run([str(tmp / "allocators"), mode], check=True, timeout=30)
        cases = {"valid": None, "small_unaligned": None, "cap": None,
                 "empty": None, "zero": "invalid DTB memory range",
                 "base": "invalid DTB memory range",
                 "overflow": "invalid DTB memory range",
                 "kernel": "RAM does not contain the kernel",
                 "descriptors": "not enough RAM for page descriptors"}
        for case, panic in cases.items():
            result = subprocess.run([str(tmp / "page_init"), case],
                                    capture_output=True, text=True, timeout=30)
            expected_code = 100 if panic else 0
            if result.returncode != expected_code or (
                    panic and panic not in result.stderr):
                raise RuntimeError(case + ": " + result.stdout + result.stderr)
            print("page_init {}: PASS{}".format(case, " (expected panic)" if panic else ""))
    print("Host checks: PASS (ASan + UBSan)")


if __name__ == "__main__":
    main()
