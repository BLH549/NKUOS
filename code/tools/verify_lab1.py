#!/usr/bin/env python3
"""Record real Lab1 build, QEMU and GDB output; not a course grader."""

import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time


CODE = Path(__file__).resolve().parents[1]
LOGS = CODE.parent / "report" / "logs"
CHECKS = ("reset_pc", "firmware_entry", "kernel_entry", "stack_pointer", "tail_jump")


def run(command, name, timeout=30, required=True):
    result = subprocess.run(command, cwd=CODE, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout)
    LOGS.joinpath(name).write_text("$ " + " ".join(command) + "\n" + result.stdout
                                 + f"\nExit status: {result.returncode}\n", encoding="utf-8")
    if required and result.returncode:
        raise RuntimeError(f"{name}: command failed ({result.returncode})")
    return result


def stop(process):
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
    try:
        return process.communicate(timeout=5)[0]
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        return process.communicate()[0]


def boot_output():
    process = subprocess.Popen(["make", "qemu"], cwd=CODE, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        output, _ = process.communicate(timeout=4)
    except subprocess.TimeoutExpired:
        output = stop(process)
    finally:
        if process.poll() is None:
            stop(process)
    LOGS.joinpath("qemu.log").write_text("$ make qemu\n" + output
        + "\nThe verifier stops QEMU after observing the non-returning kernel.\n",
        encoding="utf-8")
    if "OpenSBI" not in output or "(THU.CST) os is loading ..." not in output:
        raise RuntimeError("QEMU did not print the firmware and kernel messages")


def boot_debug():
    with socket.socket() as probe:
        if probe.connect_ex(("127.0.0.1", 1234)) == 0:
            raise RuntimeError("Port 1234 is occupied; stop the existing debug session first")
    process = subprocess.Popen(["make", "debug"], cwd=CODE, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        deadline = time.monotonic() + 10
        while True:
            if process.poll() is not None:
                raise RuntimeError("QEMU debug process exited before GDB connected")
            with socket.socket() as probe:
                if probe.connect_ex(("127.0.0.1", 1234)) == 0:
                    break
            if time.monotonic() >= deadline:
                raise RuntimeError("QEMU did not open the GDB port")
            time.sleep(0.1)
        result = run(["riscv64-unknown-elf-gdb", "--batch", "-x", "tools/boot.gdb"],
                     "gdb.log", timeout=40)
        for check in CHECKS:
            if f"CHECK {check}: PASS" not in result.stdout:
                raise RuntimeError(f"GDB check missing: {check}")
    finally:
        LOGS.joinpath("debug-qemu.log").write_text("$ make debug\n" + stop(process),
                                                 encoding="utf-8")


def main():
    LOGS.mkdir(parents=True, exist_ok=True)
    LOGS.joinpath("verification.json").unlink(missing_ok=True)
    versions = []
    for command in (["uname", "-srmo"], ["riscv64-unknown-elf-gcc", "--version"],
                    ["riscv64-unknown-elf-gdb", "--version"],
                    ["qemu-system-riscv64", "--version"], ["make", "--version"]):
        result = subprocess.run(command, text=True, capture_output=True, check=True)
        versions.append("$ " + " ".join(command) + "\n" + result.stdout.splitlines()[0])
    LOGS.joinpath("environment.log").write_text("\n\n".join(versions) + "\n", encoding="utf-8")
    run(["make", "clean"], "clean.log")
    run(["make"], "build.log")
    run(["riscv64-unknown-elf-readelf", "-h", "-S", "bin/kernel"], "elf.log")
    run(["riscv64-unknown-elf-nm", "-n", "bin/kernel"], "symbols.log")
    boot_output()
    boot_debug()
    result = {
        "timestamp": datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(),
        "build": "passed", "qemu": "passed", "gdb_checks": list(CHECKS),
        "course_grade": "not_run: Lab1 verification covers startup and GDB exercises",
        "kernel_sha256": hashlib.sha256(CODE.joinpath("bin/kernel").read_bytes()).hexdigest(),
        "image_sha256": hashlib.sha256(CODE.joinpath("bin/ucore.img").read_bytes()).hexdigest(),
    }
    LOGS.joinpath("verification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
