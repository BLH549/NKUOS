"""Run from WSL: python3 report/evidence/gdb-boot-20261011/capture.py."""

import hashlib
import json
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time


EVIDENCE = Path(__file__).resolve().parent
ROOT = EVIDENCE.parents[2]
CODE = ROOT / "code"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, log, timeout=60):
    result = subprocess.run(
        args, cwd=CODE, stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=timeout,
    )
    (EVIDENCE / log).write_text(
        "$ " + " ".join(args) + "\n" + result.stdout
        + f"\nExit code: {result.returncode}\n", encoding="utf-8",
    )
    if result.returncode:
        raise RuntimeError(f"{log}: exit {result.returncode}")
    return result.stdout


def main():
    versions = {}
    for tool in ("make", "riscv64-unknown-elf-gcc", "riscv64-unknown-elf-ld",
                 "riscv64-unknown-elf-objcopy", "gdb-multiarch", "qemu-system-riscv64"):
        versions[tool] = {
            "path": shutil.which(tool),
            "version": subprocess.check_output([tool, "--version"], text=True).splitlines()[0],
        }
    sources = {str(p.relative_to(CODE)): sha256(p)
               for p in CODE.rglob("*")
               if p.is_file() and (p.suffix in (".c", ".h", ".S", ".ld", ".mk")
                                   or p.name == "Makefile")}
    run(["make", "-B"], "build.log")
    run(["riscv64-unknown-elf-readelf", "-h", "-S", "bin/kernel"], "elf.log")
    run(["riscv64-unknown-elf-nm", "-n", "bin/kernel"], "symbols.log")
    run(["riscv64-unknown-elf-objdump", "-d", "--disassemble=kern_entry", "bin/kernel"],
        "entry-disassembly.log")
    image = CODE / "bin/ucore.img"
    # Bind first so an existing debug session is never replaced.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 1234))
    qemu_args = ["qemu-system-riscv64", "-machine", "virt", "-nographic",
                 "-bios", "default", "-kernel", "bin/ucore.img", "-s", "-S"]
    with tempfile.TemporaryDirectory(prefix="nku-lab1-gdb-") as temporary:
        loaded = Path(temporary) / "loaded.img"
        with (EVIDENCE / "qemu.log").open("w", encoding="utf-8") as qemu_log:
            qemu_log.write("$ " + " ".join(qemu_args) + "\n")
            qemu_log.flush()
            qemu = subprocess.Popen(qemu_args, cwd=CODE, stdin=subprocess.DEVNULL,
                                    stdout=qemu_log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while True:
                    if qemu.poll() is not None:
                        raise RuntimeError("QEMU exited before GDB connected")
                    try:
                        with socket.create_connection(("127.0.0.1", 1234), timeout=0.2):
                            break
                    except OSError:
                        if time.monotonic() > deadline:
                            raise RuntimeError("QEMU GDB port did not open")
                        time.sleep(0.05)
                gdb_args = ["gdb-multiarch", "-q", "-nx", "-batch", "bin/kernel",
                            "-ex", "set architecture riscv:rv64",
                            "-ex", "target remote localhost:1234",
                            "-ex", f"dump binary memory {loaded} 0x80200000 "
                                   f"{0x80200000 + image.stat().st_size:#x}",
                            "-x", str(EVIDENCE / "trace.gdb")]
                run(gdb_args, "gdb.log")
                deadline = time.monotonic() + 5
                while "(THU.CST) os is loading ..." not in (EVIDENCE / "qemu.log").read_text():
                    if time.monotonic() > deadline:
                        raise RuntimeError("Kernel output was not observed")
                    time.sleep(0.05)
                summary = {
                    "date": "2026-10-11", "environment": "Ubuntu 24.04.4 LTS (WSL)",
                    "tools": versions, "qemu_command": qemu_args,
                    "image_size": image.stat().st_size, "image_sha256": sha256(image),
                    "elf_sha256": sha256(CODE / "bin/kernel"),
                    "reset_memory_sha256": sha256(loaded),
                    "reset_memory_matches_image": loaded.read_bytes() == image.read_bytes(),
                    "sources_unchanged": all(sha256(CODE / name) == digest
                                             for name, digest in sources.items()),
                    "kernel_output_observed": True,
                    "trace": "gdb.log",
                }
                if not summary["reset_memory_matches_image"]:
                    raise RuntimeError("Reset memory differs from the kernel image")
                (EVIDENCE / "summary.json").write_text(
                    json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8",
                )
            finally:
                qemu.terminate()
                try:
                    qemu.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    qemu.kill()
                    qemu.wait()
    print("Captured reset instructions, OpenSBI, kernel entry and kernel output.")


if __name__ == "__main__":
    main()
