#!/usr/bin/env python3
"""Verify the real B handler, timer boundaries, QEMU shutdown and integration."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time


GDB_SCRIPT = r'''
import gdb, json
MASK = (1 << 64) - 1
def value(expr):
    return int(gdb.parse_and_eval(expr)) & MASK
def check(condition, message):
    if not condition:
        raise AssertionError(message)
def advance():
    gdb.execute('continue', to_string=True)
try:
    entry = gdb.Breakpoint('*interrupt_handler', type=gdb.BP_HARDWARE_BREAKPOINT)
    entry.condition = 'ticks == 0 || ticks == 98 || ticks == 99 || ticks == 100 || ticks == 998 || ticks == 999'
    records = []
    for before in (0, 98, 99, 100, 998, 999):
        advance()
        check(value('$pc') == value('&interrupt_handler'), 'handler entry')
        check(value('ticks') == before, 'ticks at entry')
        address = value('$a0')
        raw = bytes(gdb.selected_inferior().read_memory(address, 288))
        words = [int.from_bytes(raw[i:i+8], 'little') for i in range(0, 288, 8)]
        check(words[35] == (1 << 63) | 5, 'S timer scause')
        check(words[32] & 2 == 0, 'SIE cleared inside handler')
        destination = 'sbi_shutdown' if before == 999 else '__trapret'
        landing = gdb.Breakpoint('*' + destination, type=gdb.BP_HARDWARE_BREAKPOINT,
                                 temporary=True)
        entry.enabled = False
        advance()
        check(value('$pc') == value('&' + destination), 'return/shutdown reached')
        check(value('ticks') == before + 1, 'one increment')
        after = bytes(gdb.selected_inferior().read_memory(address, 288))
        check(after == raw, 'all trapframe bytes unchanged, including epc')
        records.append(dict(before=before, after=before+1, epc=hex(words[33]),
                            cause=hex(words[35]), frame=hex(address),
                            destination=destination, passed=True))
        print('PASS: ticks %d -> %d, cause=%#x, epc=%#x unchanged, reached %s' %
              (before, before+1, words[35], words[33], destination))
        entry.enabled = True
    entry.delete()
    with open(RESULT_PATH, 'w') as output:
        json.dump(dict(passed=True, boundaries=records), output, indent=2)
    gdb.execute('detach', to_string=True)
except Exception:
    import traceback
    traceback.print_exc()
    gdb.execute('quit 1')
'''


def run(command, cwd, logfile, timeout=90):
    with logfile.open('w') as output:
        output.write('$ ' + ' '.join(command) + '\n')
        output.flush()
        result = subprocess.run(command, cwd=cwd, stdout=output,
                                stderr=subprocess.STDOUT, timeout=timeout)
        output.write(f'\nEXIT_CODE={result.returncode}\n')
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=Path('../report/evidence/B-20261104'))
    parser.add_argument('--integration', action='store_true',
                        help='also run original teacher grade and A regression')
    parser.add_argument('--a-qemu', default=os.environ.get('A_QEMU'),
                        help='optional alternate QEMU for A CSR-return regression')
    args = parser.parse_args()
    code = Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for filename in ('summary.json', 'timer-summary.json'):
        (out / filename).unlink(missing_ok=True)
    qemu = os.environ.get('QEMU', 'qemu-system-riscv64')
    debugger = os.environ.get('GDB', 'gdb-multiarch')
    versions = {}
    for command in ('gcc', 'make', 'riscv64-unknown-elf-gcc', qemu, debugger):
        result = subprocess.run([command, '--version'], text=True, capture_output=True)
        versions[command] = dict(path=shutil.which(command),
                                 version=result.stdout.splitlines()[0])
    environment = dict(os_release=Path('/etc/os-release').read_text(), tools=versions,
                       makeflags=os.environ.get('MAKEFLAGS', ''),
                       qemu=qemu, debugger=debugger)
    (out / 'environment.json').write_text(json.dumps(environment, indent=2) + '\n')
    files = ('kern/trap/trap.c', 'kern/trap/trapentry.S', 'kern/init/init.c',
             'kern/driver/clock.c', 'tools/grade.sh', 'tools/test_lab3.c')
    hashes = {name: hashlib.sha256((code / name).read_bytes()).hexdigest() for name in files}
    (out / 'tested-source.json').write_text(json.dumps(hashes, indent=2) + '\n')

    with tempfile.TemporaryDirectory(prefix='nkuos-lab3-b-') as directory:
        temp = Path(directory)
        source = (code / 'kern/trap/trap.c').read_text()
        start = source.index('void interrupt_handler(')
        end = source.index('/* C: exception handling. */', start)
        handler = source[start:end].strip()
        assert handler.endswith('}') and handler.count('{') == handler.count('}')
        definitions = '\n'.join(re.findall(
            r'^#define (?:TICK_NUM|SHUTDOWN_AFTER)\s+\d+\s*$', source, re.M))
        (temp / 'b_handler.inc').write_text(definitions + '\n' + handler + '\n')
        irqs = '\n'.join(re.findall(r'^#define IRQ_.*$',
                                    (code / 'libs/riscv.h').read_text(), re.M))
        (temp / 'b_irq.h').write_text(irqs + '\n')
        (temp / 'defs.h').write_text('#include <stdbool.h>\n')
        assert run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(temp), '-I' + str(code / 'kern/trap'),
                    str(code / 'tools/check_b_handler.c'), '-o', str(temp / 'host')],
                   code, out / 'host-build.log') == 0, 'host build failed'
        assert run([str(temp / 'host')], code, out / 'host.log') == 0, 'host check failed'
    print('PASS: host IRQ branches, boundaries, call order and frame invariance', flush=True)

    assert run(['make', 'clean'], code, out / 'clean.log') == 0
    assert run(['make', '-j4'], code, out / 'build.log') == 0, 'kernel build failed'
    assert run(['make', 'qemu'], code, out / 'qemu.log', timeout=25) == 0, 'QEMU failed'
    serial = (out / 'qemu.log').read_text()
    assert serial.splitlines().count('100 ticks') == 10, 'expected exactly ten timer prints'
    assert 'check_alloc_page() succeeded!' in serial
    assert 'panic' not in serial.lower()
    print('PASS: production QEMU, ten timer prints and natural shutdown', flush=True)

    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    script = ('set pagination off\nset confirm off\nset architecture riscv:rv64\n'
              'set remotetimeout 10\n' + f'file {code}/bin/kernel\n'
              f'target remote 127.0.0.1:{port}\npython\n'
              f'RESULT_PATH = {str(out / "timer-summary.json")!r}\n' +
              GDB_SCRIPT + '\nend\nquit\n')
    (out / 'timer.gdb').write_text(script)
    with (out / 'timer-qemu.log').open('w') as serial_output:
        process = subprocess.Popen([qemu, '-machine', 'virt', '-nographic',
                                    '-bios', 'default', '-kernel', str(code / 'bin/ucore.img'),
                                    '-monitor', 'none', '-S', '-gdb', f'tcp:127.0.0.1:{port}'],
                                   stdin=subprocess.DEVNULL, stdout=serial_output,
                                   stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=1):
                        break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise TimeoutError('QEMU GDB port')
                    time.sleep(0.1)
            assert run([debugger, '-q', '-nx', '-batch', '-x', str(out / 'timer.gdb')],
                       code, out / 'timer-gdb.log', timeout=40) == 0, 'GDB failed'
            assert json.loads((out / 'timer-summary.json').read_text())['passed']
            process.wait(timeout=10)
            assert process.returncode == 0, 'debugged QEMU exit'
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    assert (out / 'timer-qemu.log').read_text().splitlines().count('100 ticks') == 10
    print('PASS: real GDB timer boundaries and unchanged EPC/frame', flush=True)

    summary = dict(b_passed=True, host=True, production_qemu=True,
                   timer_gdb=True, timer_prints=10, shutdown_ticks=1000)
    if args.integration:
        grade_exit = run(['make', 'grade'], code, out / 'grade.log', timeout=100)
        grade = (out / 'grade.log').read_text()
        score = re.search(r'Total Score: (\d+)/100', grade)
        summary['teacher_grade'] = dict(exit_code=grade_exit,
                                       score=int(score[1]) if score else None,
                                       c_handler_pending='exception_handler YOUR CODE' in source)
        a_qemu = args.a_qemu or qemu
        summary['a_regression_qemu'] = a_qemu
        summary['a_regression_qemu_version'] = subprocess.check_output(
            [a_qemu, '--version'], text=True).splitlines()[0]
        a_exit = run(['env', 'QEMU=' + a_qemu, 'python3', 'tools/check_a.py',
                      '--output', str(out / 'a-regression')],
                     code, out / 'a-regression.log')
        summary['a_regression_passed'] = a_exit == 0
        assert a_exit == 0, 'A regression failed'
        assert run(['make', '-j4'], code, out / 'final-build.log') == 0
        print('Teacher grade: ' + (score[0] if score else 'no score; inspect grade.log'), flush=True)
    current = {name: hashlib.sha256((code / name).read_bytes()).hexdigest() for name in files}
    assert current == hashes, 'verification changed production sources'
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('B verification: PASS; evidence in ' + str(out))


if __name__ == '__main__':
    main()
