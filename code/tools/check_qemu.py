#!/usr/bin/env python3
"""Rebuild both managers and capture actual QEMU boots in report/evidence."""
import argparse
from datetime import datetime, timezone, timedelta
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=ROOT.parent / "report/evidence/A-20261023")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    make = os.environ.get("MAKE", "make")
    prefix = os.environ.get("GCCPREFIX", "riscv64-unknown-elf-")
    qemu = os.environ.get("QEMU", "qemu-system-riscv64")
    for tool in (make, prefix + "gcc", prefix + "ld", qemu):
        if shutil.which(tool) is None:
            raise SystemExit("Missing executable: " + tool)

    versions = {}
    for tool in (make, prefix + "gcc", prefix + "ld", qemu):
        result = subprocess.run([tool, "--version"], capture_output=True,
                                text=True, check=True)
        versions[tool] = result.stdout.splitlines()[0]
    runs = []
    for manager in ("first_fit", "best_fit"):
        command = [make, "-j2", "PMM=" + manager, "GCCPREFIX=" + prefix]
        with (output / (manager + "-build.log")).open("w") as log:
            log.write("$ " + shlex.join(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        elf_hash = sha256(ROOT / "bin/kernel")
        image_hash = sha256(ROOT / "bin/ucore.img")
        sizes = (128,) if manager == "first_fit" else (16, 64, 128, 256)
        for size in sizes:
            command = [qemu, "-machine", "virt", "-m", str(size),
                       "-nographic", "-bios", "default", "-kernel",
                       "bin/ucore.img"]
            path = output / (manager + "-" + str(size) + "M-boot.log")
            succeeded = False
            with path.open("w") as log:
                proc = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                        stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        text = path.read_text(errors="replace")
                        if "kernel panic" in text or proc.poll() is not None:
                            break
                        if "satp physical address:" in text:
                            succeeded = True
                            break
                        time.sleep(0.05)
                finally:
                    if proc.poll() is None:
                        proc.terminate()
                    proc.wait(timeout=5)
            text = path.read_text(errors="replace")
            assert succeeded and "check_alloc_page() succeeded!" in text, text
            name = "default_pmm_manager" if manager == "first_fit" else "best_fit_pmm_manager"
            assert "memory management: " + name in text, text
            match = re.search(r"free memory:\s+\[0x([0-9a-f]+), 0x([0-9a-f]+)\), (\d+) pages", text)
            assert match is not None, text
            begin, end, count = (int(match[1], 16), int(match[2], 16), int(match[3]))
            assert end == min(0x80000000 + size * 1024 * 1024, 0x88000000)
            assert begin % 4096 == 0 and count == (end - begin) // 4096
            runs.append({"manager": manager, "ram_mib": size,
                         "command": command, "log": path.name,
                         "image_sha256": image_hash, "elf_sha256": elf_hash,
                         "free_pages": count, "managed_end": hex(end), "passed": True})
            print(manager + " " + str(size) + " MiB: PASS (" + str(count) + " free pages)")

    originals = subprocess.check_output(["git", "show", "HEAD:code/kern/mm/best_fit_pmm.c"], cwd=ROOT).decode()
    current = (ROOT / "kern/mm/best_fit_pmm.c").read_text()
    marker = "// Framework tests: students must not modify basic_check or best_fit_check."
    assert originals[originals.index(marker):] == current[current.index(marker):]
    framework_hash = hashlib.sha256(current[current.index(marker):].encode()).hexdigest()
    manifest = {str(p.relative_to(ROOT.parent)): sha256(p)
                for p in sorted(ROOT.rglob("*")) if p.is_file()
                and p.suffix in (".c", ".h", ".S", ".ld", ".mk", ".py", ".sh")
                and "obj" not in p.relative_to(ROOT).parts and "bin" not in p.relative_to(ROOT).parts}
    manifest["code/Makefile"] = sha256(ROOT / "Makefile")
    summary = {"date": datetime.now(timezone(timedelta(hours=8))).isoformat(),
               "git_base": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
               "tools": versions, "runs": runs,
               "best_fit_framework_tests_unchanged": True,
               "best_fit_framework_sha256": framework_hash,
               "source_sha256": manifest,
               "termination": "QEMU terminated by harness after success marker; kernel normally loops forever."}
    (output / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n")
    print("QEMU checks: PASS; default build restored to Best-Fit")


if __name__ == "__main__":
    main()
