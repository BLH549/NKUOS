#!/usr/bin/env python3
"""B's actual allocator sources under ASan/UBSan, with an independent oracle."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from check_host import ROOT, RUNTIME


def main():
    cc = os.environ.get("HOSTCC", "cc")
    with tempfile.TemporaryDirectory(prefix="lab2-b-") as directory:
        tmp = Path(directory)
        runtime = tmp / "runtime.c"
        runtime.write_text(RUNTIME)
        flags = ["-g", "-O1", "-fno-builtin",
                 "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                 "-fno-omit-frame-pointer", "-std=gnu99", "-Wall", "-Werror",
                 "-Wno-unused"]
        if sys.platform.startswith("linux"):
            flags += ["-fno-pie", "-no-pie"]
        subprocess.run([cc, *flags, "-c", str(runtime), "-o",
                        str(tmp / "runtime.o")], check=True)
        includes = ["-I" + str(ROOT / p)
                    for p in ("libs", "kern/mm", "kern/debug", "kern/driver")]
        sources = ["tools/check_b_allocators.c", "kern/mm/buddy_pmm.c",
                   "kern/mm/slub.c", "kern/mm/best_fit_pmm.c",
                   "kern/mm/default_pmm.c"]
        binary = tmp / "check_b"
        subprocess.run([cc, *flags, *includes, *(str(ROOT / p) for p in sources),
                        str(tmp / "runtime.o"), "-o", str(binary)], check=True)
        for manager in ("buddy", "best_fit", "first_fit"):
            subprocess.run([str(binary), manager], check=True, timeout=30)
        for case in ("size", "tail", "double"):
            result = subprocess.run([str(binary), "buddy", case],
                                    capture_output=True, text=True, timeout=5)
            assert result.returncode == 100 and "panic at" in result.stderr, result
            print("Buddy invalid free " + case + ": PASS (expected panic)")
    print("B host checks: PASS (ASan + UBSan)")


if __name__ == "__main__":
    main()
