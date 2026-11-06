#!/usr/bin/env python3
"""Verify the production exception handler on host and actual QEMU/GDB traps."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from check_a import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=Path('../report/evidence/C-20261105'))
    args = parser.parse_args()
    code = Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'summary.json').unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='nkuos-lab3-c-') as directory:
        temp = Path(directory)
        source = (code / 'kern/trap/trap.c').read_text()
        start = source.index('void exception_handler(')
        end = source.index('static inline void trap_dispatch', start)
        (temp / 'c_handler.inc').write_text(source[start:end])
        causes = '\n'.join(re.findall(r'^#define CAUSE_.*$',
                                      (code / 'libs/riscv.h').read_text(), re.M))
        (temp / 'c_causes.h').write_text(causes + '\n')
        (temp / 'defs.h').write_text('#include <stdbool.h>\n')
        run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
             '-I' + str(temp), '-I' + str(code / 'kern/trap'),
             str(code / 'tools/check_c_handler.c'), '-o', str(temp / 'host')],
            code, out / 'host-build.log')
        run([str(temp / 'host')], code, out / 'host.log')
    print('PASS: exception output, 16/32-bit EPC, full frame invariance, 13 fallbacks', flush=True)
    run(['python3', 'tools/check_a.py', '--production-handlers',
         '--output', str(out / 'context')], code, out / 'context.log', timeout=90)
    serial = (out / 'context/fixture-qemu.log').read_text()
    summary = json.loads((out / 'context/trap-summary.json').read_text())
    for record in summary['traps'][:4]:
        label = 'Breakpoint' if record['cause'] == '0x3' else 'Illegal instruction'
        expected = f"{label} at 0x{int(record['epc_before'], 16):016x}"
        assert serial.count(expected) == 1, expected
    assert 'four exception returns and three timer returns completed' in serial
    versions = {}
    for command in ('riscv64-unknown-elf-gcc', os.environ.get('QEMU', 'qemu-system-riscv64'),
                    os.environ.get('GDB', 'gdb-multiarch'), 'make', 'gcc'):
        versions[command] = subprocess.check_output([command, '--version'], text=True).splitlines()[0]
    (out / 'environment.json').write_text(json.dumps(versions, indent=2) + '\n')
    files = ('kern/trap/trap.c', 'kern/trap/trapentry.S', 'Makefile',
             'tools/check_a.py', 'tools/check_c.py', 'tools/check_c_handler.c')
    (out / 'tested-source.json').write_text(json.dumps({
        file: hashlib.sha256((code / file).read_bytes()).hexdigest()
        for file in files}, indent=2) + '\n')
    (out / 'summary.json').write_text(json.dumps(dict(
        passed=True, production_handlers=True, host_cases=19,
        real_exceptions=4, timer_returns=3, registers_per_return=32,
        output_addresses_checked=True), indent=2) + '\n')
    print('PASS: production A/B/C, 4 real exceptions, 3 timer returns, exact output addresses')


if __name__ == '__main__':
    main()
