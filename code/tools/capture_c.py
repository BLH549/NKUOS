#!/usr/bin/env python3
"""Capture actual C transcript excerpts in X11 terminals; no image synthesis."""
import argparse
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--evidence", type=Path, default=ROOT.parent / "report/evidence/C-20261024")
    ap.add_argument("--images", type=Path, default=ROOT.parent / "report/images")
    args = ap.parse_args()
    ev, images = args.evidence.resolve(), args.images.resolve()
    images.mkdir(parents=True, exist_ok=True)
    host = (ev / "host-mmu-gdb.log").read_text()
    guest = (ev / "guest-mmu-gdb.log").read_text()

    def block(text, command, index=0):
        # Preserve the actual commands and values, just select their blocks.
        prefix = "(host) " if text is host else "(guest) "
        parts = re.split(r"\n(?=\((?:host|guest)\) )", text)
        return [p for p in parts if p.startswith(prefix + command + "\n")][index].strip() + "\n"

    screens = {}
    screens["C-01-guest-and-bare"] = (
        "Sources: guest-mmu-gdb.log / host-mmu-gdb.log\n\n" +
        block(guest, "info registers pc satp t0 a0") +
        block(guest, "x/i $pc", 0) +
        block(host, "p/x $cenv->satp", 0) + block(host, "p $cenv->priv", 0) +
        block(host, "p vm", 0) + block(host, "p/x *physical", 0) +
        block(host, "p/x *te", 0) +
        "\n--- Sv39 guest store ---\n" +
        block(guest, "info registers pc satp sp ra") +
        block(guest, "x/gx (char *)&boot_page_table_sv39+16") +
        block(guest, "x/gx (char *)&boot_page_table_sv39+4088") +
        block(guest, "x/i $pc", 1))
    screens["C-02-sv39-walk"] = (
        "Source: host-mmu-gdb.log / real stack store VA=0xffffffffc0204ff8\n\n" +
        "\n".join(block(host, command, index) for command, index in [
            ("p vm", 1), ("p levels", 0), ("p/x base", 0), ("p/x i", 0),
            ("p/x ptshift", 0), ("p/x idx", 0), ("p/x pte_addr", 0),
            ("p/x pte", 0), ("p/x ppn", 0), ("p/x *physical", 1),
            ("p/x *physical | (addr & 0xfff)", 0), ("p prot", 1)]))
    screens["C-03-tlb-cold-and-hot"] = (
        "Sources: host-mmu-gdb.log / guest-mmu-gdb.log\n\n" +
        block(guest, "info registers pc satp s0") +
        block(guest, "x/i $pc", 2) +
        block(host, "p/x tlb_addr") + block(host, "p/x entry->addr_read") +
        block(host, "p index", 2) +
        block(host, "x/gx $host_object") +
        block(guest, "p/x $a1", 0) + block(guest, "p/x $a1", 1) +
        block(host, "p $loads") + block(host, "p/x $cte->addr_read") +
        block(host, "p/x $cte->addend"))
    asm = block(host, "x/28i itb->tc.ptr")
    screens["C-04-tcg-fast-path"] = (
        "Source: host-mmu-gdb.log / generated x86-64 code for guest ld\n\n" +
        asm + "\n" + block(host, "list 1268,1280"))
    serial = (ev / "regression/best_fit-128M-boot.log").read_text()
    screens["C-05-boot-and-grade"] = (
        "Sources: regression/best_fit-128M-boot.log / regression/grade.log\n\n" +
        serial[serial.index("DTB Init"):].strip() + "\n\n" +
        "\n".join((ev / "regression/grade.log").read_text().splitlines()[-3:]) + "\n\n" +
        (ev / "regression-driver.log").read_text())
    for name, content in screens.items():
        path = ev / (name + ".txt")
        path.write_text("C / Liu Huabin 2411238 / 2026-10-24\nRecorded session excerpt\n" + content)
        title = "Lab2 " + name
        proc = subprocess.Popen([
            "xterm", "-title", title, "-geometry", "164x53+0+0",
            "-fa", "DejaVu Sans Mono", "-fs", "10", "-bg", "#101820", "-fg", "#e8edf2",
            "-e", "bash", "-c", 'cat "$1"; read -r _', "capture", str(path)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            window = None
            for _ in range(30):
                tree = subprocess.check_output(["xwininfo", "-root", "-tree"], text=True)
                match = re.search(r'(0x[0-9a-f]+) "' + re.escape(title) + r'"', tree)
                if match:
                    window = match.group(1)
                    break
                time.sleep(0.1)
            if not window:
                raise RuntimeError("Cannot find xterm window " + title)
            time.sleep(0.3)
            subprocess.run(["import", "-window", window, str(images / (name + ".png"))], check=True)
            print(images / (name + ".png"))
        finally:
            proc.terminate()
            proc.wait(timeout=3)


if __name__ == "__main__":
    main()
