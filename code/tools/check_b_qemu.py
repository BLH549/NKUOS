#!/usr/bin/env python3
"""Capture real B checks, manager regressions and teacher grade with hashes."""
import argparse
from datetime import datetime, timezone, timedelta
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", default=os.environ.get("ARCH", "rv64imac"))
    parser.add_argument("--output", type=Path,
                        default=ROOT.parent / "report/evidence/B-20261023")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    qemu = os.environ.get("QEMU", "qemu-system-riscv64")
    prefix = os.environ.get("GCCPREFIX", "riscv64-unknown-elf-")
    make_args = ["ARCH=" + args.arch, "GCCPREFIX=" + prefix, "QEMU=" + qemu]
    versions = {}
    for tool in ("make", prefix + "gcc", qemu, "cc", "python3"):
        versions[tool] = subprocess.check_output(
            [tool, "--version"], text=True).splitlines()[0]

    def capture(command, filename, timeout=90):
        path = output / filename
        with path.open("w") as log:
            log.write("$ " + shlex.join(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=timeout)
        return path.read_text()

    capture(["make", "check-b"], "host.log")
    runs = []
    # Last build restores the required Best-Fit default.
    for manager, sizes in (("first_fit", (128,)),
                           ("buddy", (64, 128, 256)),
                           ("best_fit", (128,))):
        capture(["make", "-j2", "PMM=" + manager, *make_args],
                manager + "-build.log")
        elf_hash, image_hash = digest(ROOT / "bin/kernel"), digest(ROOT / "bin/ucore.img")
        for size in sizes:
            command = [qemu, "-machine", "virt", "-m", str(size), "-nographic",
                       "-bios", "default", "-kernel", "bin/ucore.img"]
            path = output / (manager + "-" + str(size) + "M-boot.log")
            with path.open("w") as log:
                log.write("$ " + shlex.join(command) + "\n")
                log.flush()
                proc = subprocess.Popen(command, cwd=ROOT, stdout=log,
                                        stderr=subprocess.STDOUT)
                succeeded = False
                try:
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        text = path.read_text(errors="replace")
                        if "kernel panic" in text or proc.poll() is not None:
                            break
                        if "[SLUB] all checks passed; free pages restored:" in text:
                            succeeded = True
                            break
                        time.sleep(0.05)
                finally:
                    if proc.poll() is None:
                        proc.terminate()
                    proc.wait(timeout=5)
            text = path.read_text(errors="replace")
            expected = "default_pmm_manager" if manager == "first_fit" else manager + "_pmm_manager"
            assert succeeded and "memory management: " + expected in text, text
            assert "check_alloc_page() succeeded!" in text, text
            if manager == "buddy":
                assert "[Buddy] rounding/alignment/split/merge/count: PASS" in text, text
            runs.append({"manager": manager, "ram_mib": size, "command": command,
                         "log": path.name, "elf_sha256": elf_hash,
                         "image_sha256": image_hash, "passed": True})
            print(manager + " + SLUB, " + str(size) + " MiB: PASS")

    grade = capture(["make", "grade", "PMM=best_fit", *make_args], "grade.log")
    assert "Total Score: 20/20" in grade, grade
    # grade cleans the local build before testing a temporary copy.
    capture(["make", "-j2", "PMM=best_fit", *make_args], "final-best_fit-build.log")
    # Check A's immutable framework tests against the fetched base revision.
    baseline = subprocess.check_output(
        ["git", "show", "7e9a242:code/kern/mm/best_fit_pmm.c"], cwd=ROOT).decode()
    current = (ROOT / "kern/mm/best_fit_pmm.c").read_text()
    assert current == baseline
    manifest = {str(p.relative_to(ROOT.parent)): digest(p)
                for p in sorted(ROOT.rglob("*")) if p.is_file()
                and ("kern" in p.relative_to(ROOT).parts or
                     p.parent == ROOT / "tools" or p.name == "Makefile")
                and p.suffix in (".c", ".h", ".S", ".py", ".sh", ".ld", ".mk", "")}
    summary = {"date": datetime.now(timezone(timedelta(hours=8))).isoformat(),
               "author": "Ke Yunchao (2413575)", "git_base": "7e9a242",
               "arch": args.arch, "tools": versions, "runs": runs,
               "host_checks": "host.log", "teacher_grade": "20/20",
               "best_fit_source_unchanged": True, "source_sha256": manifest,
               "termination": "Harness stops QEMU after final SLUB success marker; kernel otherwise loops."}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("Teacher grade: 20/20; B QEMU checks: PASS; default restored to Best-Fit")


if __name__ == "__main__":
    main()
