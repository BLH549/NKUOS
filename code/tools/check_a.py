#!/usr/bin/env python3
"""Verify Lab4 A in a temporary kernel while C scheduling is unfinished."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time


def run(command, cwd, log, timeout=90):
    with log.open('w') as output:
        subprocess.run(command, cwd=cwd, stdin=subprocess.DEVNULL,
                       stdout=output, stderr=subprocess.STDOUT,
                       timeout=timeout, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('../report/evidence/A-lab4'))
    parser.add_argument('--memory', nargs='+', default=['128M'])
    parser.add_argument('--trace-bootstrap', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not args.trace_bootstrap:
        (output / 'summary.json').unlink(missing_ok=True)
    qemu = os.environ.get('QEMU', 'qemu-system-riscv64')
    results = []
    with tempfile.TemporaryDirectory(prefix='nkuos-lab4-a-') as temporary:
        kernel = Path(temporary) / 'code'
        shutil.copytree(root, kernel, ignore=shutil.ignore_patterns('bin', 'obj', '__pycache__'))
        run(['python3', 'tools/inject.py', str(kernel)], kernel, output / 'inject.log')
        # The teacher's PMM and Process Create checks remain active.
        proc = kernel / 'kern/process/proc.c'
        text = proc.read_text()
        assert text.count('    test_lab4_schedule();') == 1
        proc.write_text(text.replace('    test_lab4_schedule();', ''))
        for source in ('lab4_a_fixture.c', 'lab4_a_probe.S'):
            shutil.copyfile(root / 'tools' / source, kernel / 'kern/test' / source)
        makefile = kernel / 'Makefile'
        text = makefile.read_text() + '\nLDFLAGS += --wrap=trap --wrap=cpu_idle\n'
        makefile.write_text(text)
        run(['make', '-j4'], kernel, output / 'fixture-build.log')
        run(['riscv64-unknown-elf-objdump', '-d', 'bin/kernel'], kernel,
            output / 'fixture-disassembly.log')
        if args.trace_bootstrap:
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                port = listener.getsockname()[1]
            source = (kernel / 'kern/mm/pmm.c').read_text().splitlines()
            line = next(i + 1 for i, text in enumerate(source) if 'strcpy((void *)0x100' in text)
            command = Path(temporary) / 'bootstrap.gdb'
            command.write_text('set pagination off\nset confirm off\n'
                               f'file {kernel}/bin/kernel\n'
                               f'target remote 127.0.0.1:{port}\n'
                               f'hbreak kern/mm/pmm.c:{line}\ncontinue\n'
                               'info registers pc a0 a1 satp stvec scause sepc stval\n'
                               'p/x boot_pgdir_va[0]\n'
                               'set $stvec = &a_bootstrap_fault\n'
                               'hbreak *a_bootstrap_fault\n'
                               f'hbreak kern/mm/pmm.c:{line + 1}\ncontinue\n'
                               'info registers pc scause sepc stval stvec\n'
                               'detach\nquit\n')
            with (output / 'bootstrap-qemu.log').open('w') as serial:
                process = subprocess.Popen([qemu, '-machine', 'virt', '-nographic',
                    '-bios', 'default', '-kernel', 'bin/ucore.img', '-monitor', 'none',
                    '-S', '-gdb', f'tcp:127.0.0.1:{port}'], cwd=kernel,
                    stdin=subprocess.DEVNULL, stdout=serial, stderr=subprocess.STDOUT)
                try:
                    time.sleep(0.3)
                    run([os.environ.get('GDB', 'gdb-multiarch'), '-q', '-nx', '-batch',
                         '-x', str(command)], kernel, output / 'bootstrap-gdb.log', timeout=30)
                finally:
                    process.terminate()
                    process.wait(timeout=5)
            return
        for memory in args.memory:
            logfile = output / ('qemu-' + memory + '.log')
            run([qemu, '-machine', 'virt', '-m', memory, '-display', 'none',
                 '-monitor', 'none', '-serial', 'stdio', '-bios', 'default',
                 '-kernel', 'bin/ucore.img'], kernel, logfile, timeout=15)
            serial = logfile.read_text()
            expected = ['check_alloc_page() succeeded!', 'check_pgdir() succeeded!',
                        'check_boot_pgdir() succeeded!', 'check_vmm() succeeded.',
                        '[Test PMM] PASS', '[Test Process Create] PASS',
                        '[A mapping]', '[A failure]', '[A hardware]', '[A model]',
                        '[A result] PASS']
            assert all(marker in serial for marker in expected), logfile
            assert 'kernel panic' not in serial, logfile
            results.append({'memory': memory, 'passed': True, 'log': logfile.name})
            print(memory + ': PASS (teacher PMM/Process Create, mapping, faults, TLB, model)')
        hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                  for name in ('kern/mm/pmm.c', 'kern/mm/default_pmm.c', 'kern/mm/pmm.h',
                               'kern/process/proc.c', 'tools/test_lab4.c', 'tools/inject.py',
                               'tools/lab4_a_fixture.c', 'tools/lab4_a_probe.S',
                               'tools/check_a.py', 'Makefile')}
        summary = {'cases': results, 'random_operations_per_case': 4096,
                   'model_seed': '0x2412235', 'sources_sha256': hashes,
                   'fixture_kernel_sha256': hashlib.sha256((kernel / 'bin/kernel').read_bytes()).hexdigest(),
                   'C_schedule': 'not invoked; cpu_idle wrapped by temporary verification entry',
                   'teacher_tests': 'PMM and Process Create active; Schedule not invoked'}
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')


if __name__ == '__main__':
    main()
