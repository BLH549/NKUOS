#!/usr/bin/env python3
"""Recheck the current Lab 1 build without changing implementation files.

Run after: source scripts/env.sh; cd labs/code; make
This script writes evidence beside itself and owns/cleans up its debug process.
"""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import hashlib
import json
import os
import shlex
import signal
import socket
import struct
import subprocess
import tempfile
import time

OUT = Path(__file__).resolve().parent
LABS = OUT.parents[2]
CODE = LABS / "code"
ROOT = LABS.parent
SUMMARY = {}


def run(name, argv, expected=0, cwd=CODE):
    result = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    (OUT / name).write_text("$ " + shlex.join(argv) + "\n" + result.stdout
                           + "\nExit code: " + str(result.returncode) + "\n")
    if result.returncode != expected:
        raise RuntimeError(f"{name}: exit {result.returncode}, expected {expected}")
    return result.stdout


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


SUMMARY["date"] = datetime.now(ZoneInfo("Asia/Shanghai")).isoformat()
SUMMARY["code_commit"] = run("commit.log", ["git", "rev-parse", "HEAD"]).strip()
tools = ["make", "riscv64-unknown-elf-gcc", "riscv64-unknown-elf-ld",
         "riscv64-unknown-elf-objcopy", "riscv64-unknown-elf-gdb", "qemu-system-riscv64"]
versions = []
for tool in tools:
    resolved = run(tool + "-path.log", ["bash", "-c", 'command -v "$1"', "bash", tool]).strip()
    version = run(tool + "-version.log", [tool, "--version"]).splitlines()[0]
    versions.append({"tool": tool, "path": resolved, "version": version})
SUMMARY["tools"] = versions
run("elf.log", ["riscv64-unknown-elf-readelf", "-h", "-l", "-S", "-s", "-A", "bin/kernel"])
run("object.log", ["riscv64-unknown-elf-readelf", "-h", "-S", "-r", "obj/kern/init/entry.o"])
run("final-relocations.log", ["riscv64-unknown-elf-readelf", "-r", "bin/kernel"])
run("entry-disassembly.log", ["riscv64-unknown-elf-objdump", "-d", "--disassemble=kern_entry", "bin/kernel"])
run("console-relocations.log", ["riscv64-unknown-elf-objdump", "-r", "obj/kern/driver/console.o"])
assert not run("undefined.log", ["riscv64-unknown-elf-nm", "-u", "bin/kernel"]).strip()
nm = run("symbols.log", ["riscv64-unknown-elf-nm", "-n", "bin/kernel"])
symbols = {parts[2]: int(parts[0], 16) for line in nm.splitlines()
           if len(parts := line.split()) == 3}
run("incremental.log", ["make"])
run("header-dependency.log", ["make", "-n", "-W", "kern/mm/mmu.h"])
run("linker-dependency.log", ["make", "-n", "-W", "tools/kernel.ld"])
run("debug-command.log", ["make", "-n", "debug"])

# Examine the actual ELF instead of deriving addresses from textbook examples.
elf = (CODE / "bin/kernel").read_bytes()
image = (CODE / "bin/ucore.img").read_bytes()
header = struct.unpack_from("<16sHHIQQQIHHHHHH", elf)
ident, kind, machine, _, entry, phoff, shoff, flags, _, phsize, phnum, shsize, shnum, shstr = header
assert ident[:6] == b"\x7fELF\x02\x01" and kind == 2 and machine == 243
sections = [struct.unpack_from("<IIQQQQIIQQ", elf, shoff + i * shsize) for i in range(shnum)]
strings = elf[sections[shstr][4]:sections[shstr][4] + sections[shstr][5]]
loads = [struct.unpack_from("<IIQQQQQQ", elf, phoff + i * phsize) for i in range(phnum)]
loads = [p for p in loads if p[0] == 1]
assert all(p[3] == p[4] for p in loads), "Current VMA and LMA must match"
allocated = []
for sec in sections:
    noff, stype, sflags, addr, offset, size, _, _, align, _ = sec
    if sflags & 2 and stype == 1 and size:
        name = strings[noff:strings.index(b"\0", noff)].decode()
        allocated.append(dict(name=name, address=addr, size=size, offset=offset, align=align))
base = min(s["address"] for s in allocated)
end = max(s["address"] + s["size"] for s in allocated)
expected_image = bytearray(end - base)
for sec in allocated:
    pos = sec["address"] - base
    expected_image[pos:pos + sec["size"]] = elf[sec["offset"]:sec["offset"] + sec["size"]]
assert image == expected_image, "Image bytes differ from allocated ELF section contents"
assert entry == symbols["kern_entry"] == base == 0x80200000
assert symbols["bootstacktop"] - symbols["bootstack"] == 8192
assert symbols["bootstack"] % 4096 == 0
assert symbols["edata"] == symbols["end"]
SUMMARY.update(elf_size=len(elf), image_size=len(image), entry=hex(entry),
               sections=allocated, symbols={k: hex(symbols[k]) for k in
               ["kern_entry", "kern_init", "bootstack", "bootstacktop", "edata", "end"]},
               elf_sha256=sha(CODE / "bin/kernel"), image_sha256=sha(CODE / "bin/ucore.img"),
               image_section_bytes_match=True)

# A separate diagnostic link records section collection and ordering.
with tempfile.TemporaryDirectory(prefix="nku-lab1-A-") as tmp:
    tmp = Path(tmp)
    objects = ["obj/kern/init/entry.o", "obj/kern/init/init.o", "obj/kern/libs/stdio.o",
               "obj/kern/driver/console.o", "obj/libs/printfmt.o", "obj/libs/readline.o",
               "obj/libs/sbi.o", "obj/libs/string.o"]
    run("link-diagnostic.log", ["riscv64-unknown-elf-ld", "-m", "elf64lriscv", "-nostdlib",
        "--gc-sections", "-T", "tools/kernel.ld", "-Map=" + str(OUT / "kernel.map"),
        "--print-gc-sections", "--trace", "-o", str(tmp / "kernel"), *objects])
    assert (tmp / "kernel").read_bytes() == elf, "Diagnostic link changed ELF bytes"
    run("objcopy-repeat.log", ["riscv64-unknown-elf-objcopy", "bin/kernel", "--strip-all",
                                "-O", "binary", str(tmp / "ucore.img")])
    assert (tmp / "ucore.img").read_bytes() == image
    SUMMARY["diagnostic_link_and_objcopy_match"] = True

    # Confirm the freshly built image is loaded and its first instruction is reached.
    probe = socket.socket()
    try:
        probe.bind(("127.0.0.1", 1234))
    finally:
        probe.close()
    debug_log = open(OUT / "debug-qemu.log", "w")
    qemu = subprocess.Popen(["make", "debug"], cwd=CODE, stdin=subprocess.DEVNULL,
                            stdout=debug_log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        for _ in range(100):
            if qemu.poll() is not None:
                raise RuntimeError("make debug exited before GDB connected")
            try:
                with socket.create_connection(("127.0.0.1", 1234), timeout=.1):
                    break
            except OSError:
                time.sleep(.05)
        else:
            raise RuntimeError("Debug port did not become available")
        commands = ["set pagination off", "set architecture riscv:rv64",
                    "target remote localhost:1234", "info registers pc",
                    "x/3i 0x80200000",
                    f"dump binary memory {tmp / 'loaded.img'} {hex(base)} {hex(end)}",
                    "hbreak *0x80200000", "continue", "info registers pc",
                    "x/3i $pc", "detach"]
        argv = ["riscv64-unknown-elf-gdb", "-q", "-nx", "-batch", "bin/kernel"]
        for command in commands:
            argv += ["-ex", command]
        output = run("entry-gdb.log", argv)
        assert "0x1000" in output and "0x80200000" in output and "Breakpoint 1," in output
        assert (tmp / "loaded.img").read_bytes() == image
        SUMMARY["reset_memory_matches_image"] = True
        SUMMARY["entry_breakpoint_reached"] = True
        time.sleep(.3)
    finally:
        try:
            os.killpg(qemu.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            qemu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(qemu.pid, signal.SIGKILL)
            qemu.wait()
        debug_log.close()

before = json.loads((OUT / "source-before.json").read_text())
after = {name: sha(LABS / name) for name in before}
(OUT / "source-after.json").write_text(json.dumps(after, indent=2) + "\n")
assert before == after, "Implementation changed during verification"
SUMMARY["source_hashes_unchanged"] = True
assert sha(CODE / "bin/kernel") == SUMMARY["elf_sha256"]
assert sha(CODE / "bin/ucore.img") == SUMMARY["image_sha256"]
run("code-diff.log", ["git", "diff", "--exit-code", "--", "code"], cwd=LABS)
(OUT / "summary.json").write_text(json.dumps(SUMMARY, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(SUMMARY, ensure_ascii=False, indent=2))
