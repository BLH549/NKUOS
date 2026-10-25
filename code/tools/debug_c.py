#!/usr/bin/env python3
"""Drive two real GDB sessions through QEMU 4.1.1's MMU (C, 2411238).

Uses only Python's standard library. No guest code or page tables are patched.
The cold-load experiment executes the existing sfence.vma, then repeats an
existing ld instruction. See the accompanying report for the controlled steps.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


class Session:
    def __init__(self, binary, log, name):
        self.log = log.open("w")
        self.name = name
        self.proc = subprocess.Popen(
            [binary, "-q", "-nx", "-nh"], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=ROOT,
            bufsize=0)
        self.pending = b""
        self.prompt = b"(gdb) "
        self.wait()
        self.cmd("set pagination off")
        self.cmd("set confirm off")
        self.cmd("set print thread-events off")

    def send(self, command):
        self.log.write("\n(%s) %s\n" % (self.name, command))
        self.log.flush()
        self.proc.stdin.write((command + "\n").encode())
        self.proc.stdin.flush()

    def wait(self, timeout=35):
        deadline = time.monotonic() + timeout
        while self.prompt not in self.pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(self.name + ": " + self.pending.decode(errors="replace"))
            ready, _, _ = select.select([self.proc.stdout], [], [], remaining)
            if ready:
                data = os.read(self.proc.stdout.fileno(), 65536)
                if not data:
                    raise RuntimeError(self.name + " exited: " + self.pending.decode(errors="replace"))
                self.pending += data
        data, self.pending = self.pending.split(self.prompt, 1)
        result = data.decode(errors="replace")
        self.log.write(result)
        self.log.flush()
        if any(s in result for s in ("Error in sourced", "No symbol ", "Cannot access memory",
                                     "Connection timed out", "No registers.",
                                     "The program is not being run")):
            raise RuntimeError(result)
        return result

    def cmd(self, command, timeout=35):
        self.send(command)
        return self.wait(timeout)

    def close(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.log.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--qemu", required=True, type=Path)
    ap.add_argument("--source", required=True, type=Path)
    ap.add_argument("--gdb", default="riscv64-unknown-elf-gdb")
    ap.add_argument("--port", default=1236, type=int)
    ap.add_argument("--output", type=Path, default=ROOT.parent / "report/evidence/C-20261024")
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    qemu = args.qemu.resolve()
    src = args.source.resolve()
    assert (src / "VERSION").read_text().strip() == "4.1.1"
    assert "version 4.1.1" in subprocess.check_output([str(qemu), "--version"], text=True)
    # These addresses are verified against this lab's baseline ELF below.
    disasm = subprocess.check_output(
        ["riscv64-unknown-elf-objdump", "-d", "bin/kernel"], cwd=ROOT, text=True)
    for address, insn in (("ffffffffc0200102", "sd\tra,8(sp)"),
                          ("ffffffffc0200274", "ld\ta1,0(s0)"),
                          ("ffffffffc020004e", "sfence.vma")):
        assert any(address + ":" in line and insn in line for line in disasm.splitlines()), (
            "Kernel layout changed; select addresses from the new disassembly", address)
    (out / "kernel-disassembly.log").write_text(disasm)
    host = guest = None
    try:
        host = Session("/usr/bin/gdb", out / "host-mmu-gdb.log", "host")
        host.cmd("file " + str(qemu))
        host.cmd("handle SIGPIPE nostop noprint pass")
        host.cmd("set args -machine virt -m 128 -nographic -monitor none "
                 "-serial file:" + str(out / "dual-qemu-serial.log") +
                 " -L " + str(src / "pc-bios") +
                 " -bios default -kernel bin/ucore.img -S -gdb tcp:127.0.0.1:" + str(args.port))
        host.cmd("break riscv_cpu_tlb_fill if address == 0x80206000 && access_type == 1")
        host.cmd("break riscv_cpu_tlb_fill if address == 0xffffffffc0204ff8 && access_type == 1")
        host.cmd("break riscv_cpu_tlb_fill if address == 0xffffffffc0206008 && access_type == 0")
        host.cmd("disable 2 3")
        host.send("run")
        guest = Session(args.gdb, out / "guest-mmu-gdb.log", "guest")
        guest.cmd("file bin/kernel")
        guest.cmd("set architecture riscv:rv64")
        # The host must be running so the remote stub can answer.
        for _ in range(40):
            import socket
            sock = socket.socket()
            try:
                sock.bind(("127.0.0.1", args.port))
            except OSError:
                break
            finally:
                sock.close()
            time.sleep(0.05)
        guest.cmd("target remote :" + str(args.port))
        guest.cmd("hbreak *0x80200008")
        guest.cmd("continue")
        guest.cmd("info registers pc satp t0 a0")
        guest.cmd("x/i $pc")
        guest.send("si")
        host.wait()
        print("Bare store: host stopped in real TLB miss", flush=True)
        host.cmd("bt 8")
        host.cmd("set $cenv = (CPURISCVState *)cs->env_ptr")
        host.cmd("p/x $cenv->satp")
        host.cmd("p $cenv->priv")
        host.cmd("tbreak get_physical_address")
        host.cmd("continue")
        host.cmd("info args")
        host.cmd("until cpu_helper.c:196")
        host.cmd("p vm")
        host.cmd("next")
        host.cmd("p/x *physical")
        host.cmd("finish")
        host.cmd("p/x pa")
        host.cmd("p prot")

        def record_fill():
            host.cmd("tbreak tlb_set_page_with_attrs")
            host.cmd("continue")
            host.cmd("info args")
            host.cmd("until cputlb.c:837")
            host.cmd("p index")
            host.cmd("p/x *te")
            host.cmd("p/x te->addend + vaddr")
            host.cmd("finish")
            host.cmd("finish")
            host.cmd("finish")

        record_fill()
        host.cmd("disable 1")
        host.cmd("enable 2")
        host.send("continue")
        guest.wait()
        guest.cmd("delete breakpoints")
        guest.cmd("hbreak *0xffffffffc0200102")
        guest.cmd("continue")
        guest.cmd("info registers pc satp sp ra")
        guest.cmd("x/gx (char *)&boot_page_table_sv39+16")
        guest.cmd("x/gx (char *)&boot_page_table_sv39+4088")
        guest.cmd("x/i $pc")
        guest.send("si")
        host.wait()
        print("Sv39 stack store: stepping the root leaf", flush=True)
        host.cmd("bt 8")
        host.cmd("p/x $cenv->satp")
        host.cmd("p $cenv->priv")
        host.cmd("tbreak get_physical_address")
        host.cmd("continue")
        host.cmd("info args")
        host.cmd("until cpu_helper.c:223")
        host.cmd("p vm")
        host.cmd("p levels")
        host.cmd("p/x base")
        host.cmd("until cpu_helper.c:244")
        for expr in ("i", "ptshift", "idx", "pte_addr"):
            host.cmd("p/x " + expr)
        host.cmd("until cpu_helper.c:254")
        host.cmd("p/x pte")
        host.cmd("next")
        host.cmd("p/x ppn")
        host.cmd("until cpu_helper.c:333")
        host.cmd("p/x pte & 0xff")
        host.cmd("next")
        host.cmd("next")
        host.cmd("p/x *physical")
        host.cmd("p/x *physical | (addr & 0xfff)")
        host.cmd("finish")
        host.cmd("p/x pa")
        host.cmd("p prot")
        record_fill()
        host.cmd("disable 2")
        host.send("continue")
        guest.wait()
        guest.cmd("delete breakpoints")
        guest.cmd("hbreak *0xffffffffc0200274")
        guest.cmd("continue")
        guest.cmd("info registers pc satp s0")
        guest.cmd("x/i $pc")
        # Reuse the existing sfence.vma without changing registers or memory.
        guest.cmd("set $pc = 0xffffffffc020004e")
        guest.cmd("si")
        guest.cmd("set $pc = 0xffffffffc0200274")
        # Stop the host while the guest is stopped to enable the load condition.
        host.proc.send_signal(signal.SIGINT)
        host.wait()
        host.cmd("enable 3")
        host.send("continue")
        guest.send("si")
        host.wait()
        host.cmd("bt 8")
        host.cmd("info args")
        host.cmd("frame 2")
        host.cmd("p/x tlb_addr")
        host.cmd("p/x entry->addr_read")
        host.cmd("p index")
        host.cmd("list 1268,1280")
        host.cmd("frame 5")
        host.cmd("p/x itb->tc.ptr")
        host.cmd("x/28i itb->tc.ptr")
        host.cmd("frame 0")
        host.cmd("set $loads = 1")
        # Logging breakpoints count any additional fill of the same address.
        host.cmd("commands 3\nsilent\nset $loads = $loads + 1\n"
                 "printf \"EXTRA LOAD FILL count=%d\\n\", $loads\ncontinue\nend")
        host.cmd("tbreak tlb_set_page_with_attrs")
        host.cmd("continue")
        host.cmd("info args")
        host.cmd("until cputlb.c:837")
        host.cmd("p/x *te")
        host.cmd("set $cte = te")
        host.cmd("set $host_object = te->addend + 0xffffffffc0206008")
        host.cmd("p/x $host_object")
        host.cmd("x/gx $host_object")
        host.cmd("finish")
        host.cmd("finish")
        host.cmd("finish")
        host.send("continue")
        guest.wait()
        guest.cmd("p/x $a1")
        guest.cmd("set $pc = 0xffffffffc0200274")
        guest.cmd("si")
        guest.cmd("p/x $a1")
        host.proc.send_signal(signal.SIGINT)
        host.wait()
        print("Repeated load executed; inspecting fill count and cached tag", flush=True)
        count_result = host.cmd("p $loads")
        assert count_result.strip().endswith("= 1"), count_result
        host.cmd("p/x *$cte")
        host.cmd("p/x $cte->addr_read")
        host.cmd("p/x $cte->addend")
        host.cmd("p/x $cenv->gpr[11]")
        host.cmd("list tcg_out_tlb_load")
        host.send("continue")
        guest.cmd("delete breakpoints")
        guest.cmd("hbreak *0xffffffffc020012c")
        guest.cmd("continue")
        guest.cmd("info registers pc satp")
        assert "[SLUB] all checks passed" in (out / "dual-qemu-serial.log").read_text()
        guest.cmd("detach")
        host.proc.send_signal(signal.SIGINT)
        host.wait()
        host.cmd("kill")
        version = subprocess.check_output([str(qemu), "--version"], text=True).splitlines()[0]
        summary = {
            "author": "Liu Huabin (2411238)", "qemu_version": version,
            "qemu": str(qemu), "source": str(src),
            "kernel_base": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "kernel_sha256": hashlib.sha256((ROOT / "bin/kernel").read_bytes()).hexdigest(),
            "image_sha256": hashlib.sha256((ROOT / "bin/ucore.img").read_bytes()).hexdigest(),
            "bare_store": {"pc": "0x80200008", "address": "0x80206000", "satp": "0"},
            "sv39_store": {"pc": "0xffffffffc0200102", "address": "0xffffffffc0204ff8"},
            "repeat_load": {"pc": "0xffffffffc0200274", "address": "0xffffffffc0206008",
                            "method": "Execute existing sfence.vma, execute ld twice by restoring PC"},
            "logs": ["host-mmu-gdb.log", "guest-mmu-gdb.log", "dual-qemu-serial.log"]}
        (out / "debug-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print("Dual GDB experiment completed", flush=True)
    finally:
        if guest:
            guest.close()
        if host:
            # Killing GDB with an attached inferior could leave the child running.
            children = subprocess.run(["pgrep", "-P", str(host.proc.pid)],
                                      capture_output=True, text=True).stdout.split() if host.proc.poll() is None else []
            for pid in children:
                try:
                    os.kill(int(pid), signal.SIGTERM)
                except ProcessLookupError:
                    pass
            host.close()


if __name__ == "__main__":
    main()
