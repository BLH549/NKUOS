#!/usr/bin/env python3
"""Build an isolated Lab3 A fixture and inspect actual traps with guest GDB."""

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


HANDLERS = r"""
/* A verification fixture: installed only in the temporary copy. */
void interrupt_handler(struct trapframe *tf) {
    assert(tf->cause == (((uintptr_t)1 << 63) | IRQ_S_TIMER));
    clock_set_next_event();
    ++ticks;
}

void exception_handler(struct trapframe *tf) {
    assert(tf->cause == CAUSE_BREAKPOINT ||
           tf->cause == CAUSE_ILLEGAL_INSTRUCTION);
    unsigned short insn = *(const unsigned short *)tf->epc;
    tf->epc += (insn & 3) == 3 ? 4 : 2;
}
"""


def context_assembly():
    # Preserve the caller's ABI state while giving each tested register a value.
    saved = [1, 3, 4, 8, 9] + list(range(18, 28))
    lines = ['.section .text.a_context,"ax",@progbits', '.align 2',
             '.globl a_context_probe', 'a_context_probe:',
             'addi sp, sp, -128']
    lines += [f'sd x{reg}, {slot * 8}(sp)' for slot, reg in enumerate(saved)]
    lines += [f'li x{reg}, {0x100 + reg}' for reg in range(1, 32)
              if reg not in (2, 3, 4)]
    for name, insn in [('break16', '.2byte 0x9002'),
                       ('break32', '.4byte 0x00100073'),
                       ('illegal16', '.2byte 0x0000'),
                       ('illegal32', '.4byte 0x0000007b')]:
        lines += [f'.globl a_{name}', f'a_{name}:', insn, 'nop']
    lines += [f'ld x{reg}, {slot * 8}(sp)' for slot, reg in enumerate(saved)]
    lines += ['addi sp, sp, 128', 'ret']
    return '\n'.join(lines) + '\n'


GDB_SCRIPT = r'''
import gdb, json

MASK = (1 << 64) - 1
def val(expr):
    return int(gdb.parse_and_eval(expr)) & MASK
def regs():
    return [val('$x%d' % i) for i in range(32)]
def csr():
    return {name: val('$' + name) for name in
            ('sstatus', 'sepc', 'stval', 'scause')}
def frame(addr):
    raw = bytes(gdb.selected_inferior().read_memory(addr, 288))
    return [int.from_bytes(raw[i:i+8], 'little') for i in range(0, 288, 8)]
def advance():
    gdb.execute('continue', to_string=True)
def check(condition, message):
    if not condition:
        raise AssertionError(message)

try:
    gdb.Breakpoint('*kern_init', type=gdb.BP_HARDWARE_BREAKPOINT, temporary=True)
    advance()
    # Break at the first instruction, before compiler prologues change sp/a0.
    b_entry = gdb.Breakpoint('*__alltraps', type=gdb.BP_HARDWARE_BREAKPOINT)
    b_c = gdb.Breakpoint('*trap', type=gdb.BP_HARDWARE_BREAKPOINT)
    b_ret = gdb.Breakpoint('*__trapret', type=gdb.BP_HARDWARE_BREAKPOINT)
    instructions = gdb.selected_frame().architecture().disassemble(
        val('&__trapret'), count=40)
    sret = next(i['addr'] for i in instructions if i['asm'].strip() == 'sret')
    b_sret = gdb.Breakpoint('*%#x' % sret, type=gdb.BP_HARDWARE_BREAKPOINT)
    b_done = gdb.Breakpoint('*a_probe_done', type=gdb.BP_HARDWARE_BREAKPOINT)
    tf_type = gdb.lookup_type('struct trapframe')
    layout = {field.name: field.bitpos // 8 for field in tf_type.fields()}
    check(tf_type.sizeof == 288, 'sizeof trapframe')
    check(layout == dict(gpr=0, status=256, epc=264, badvaddr=272, cause=280),
          'trapframe offsets')
    print('LAYOUT: size=288, status=256, epc=264, badvaddr=272, cause=280')
    results = []
    for index, (cause, length) in enumerate([(3,2), (3,4), (2,2), (2,4)] +
                                          [((1 << 63) | 5, 0)] * 3):
        advance()
        check(val('$pc') == val('&__alltraps'), 'entry breakpoint')
        before, hardware = regs(), csr()
        check(hardware['scause'] == cause, 'trap cause')
        if PRODUCTION_HANDLERS and index < 4:
            symbol, encoding = [('a_break16', 0x9002),
                                ('a_break32', 0x00100073),
                                ('a_illegal16', 0x0000),
                                ('a_illegal32', 0x0000007b)][index]
            check(hardware['sepc'] == val('&' + symbol), 'trigger address')
            raw = bytes(gdb.selected_inferior().read_memory(hardware['sepc'], length))
            check(int.from_bytes(raw, 'little') == encoding, 'trigger encoding')
        check(hardware['sstatus'] & 0x100 != 0, 'SPP=S')
        check(hardware['sstatus'] & 2 == 0, 'SIE cleared on trap')
        advance()
        check(val('$pc') == val('&trap'), 'C entry breakpoint')
        addr = val('$a0')
        check(addr == val('$sp') == before[2] - 288, 'frame pointer and sp')
        check(addr % 16 == 0, 'stack alignment')
        words = frame(addr)
        check(words[:32] == before, 'all 32 saved register slots')
        check(words[32:] == [hardware[k] for k in
                            ('sstatus', 'sepc', 'stval', 'scause')], 'saved CSRs')
        check(val('$sscratch') == 0, 'sscratch cleared by SAVE_ALL')
        advance()
        check(val('$pc') == val('&__trapret'), 'return breakpoint')
        check(val('$sp') == addr, 'C restored frame sp')
        final = frame(addr)
        check(final[33] == hardware['sepc'] + length, 'handler EPC update')
        advance()
        check(val('$pc') == sret, 'sret breakpoint')
        check(regs() == final[:32], 'all registers restored before sret')
        check(val('$sstatus') == final[32], 'sstatus restored')
        check(val('$sepc') == final[33], 'sepc restored')
        landing = gdb.Breakpoint('*%#x' % final[33],
                                 type=gdb.BP_HARDWARE_BREAKPOINT, temporary=True)
        advance()
        check(val('$pc') == final[33], 'PC after sret: actual=%#x expected=%#x '
              'cause=%#x sepc=%#x' % (val('$pc'), final[33], val('$scause'), val('$sepc')))
        check(regs() == before, 'all registers unchanged after sret')
        status = val('$sstatus')
        check(status & 0x100 == 0 and status & 0x20 != 0, 'SPP/SPIE after sret')
        check((status & 2) == ((final[32] >> 4) & 2), 'SIE restored from SPIE')
        check(val('$priv') == 1, 'returned to S privilege')
        result = dict(index=index+1, cause=hex(cause), instruction_bytes=length,
                      frame=hex(addr), old_sp=hex(before[2]),
                      epc_before=hex(hardware['sepc']), epc_after=hex(final[33]),
                      status_trap=hex(hardware['sstatus']), status_after=hex(status),
                      registers_checked=32, privilege='S', passed=True)
        results.append(result)
        print('TRAP %d PASS: cause=%#x frame=%#x old_sp=%#x epc=%#x -> %#x; '
              '32 registers restored, priv=S' %
              (index+1, cause, addr, before[2], hardware['sepc'], final[33]))
    for bp in (b_entry, b_c, b_ret, b_sret):
        bp.delete()
    advance()
    check(val('$pc') == val('&a_probe_done'), 'post-trap continuation')
    check(val('ticks') == 3, 'three timer returns completed')
    with open(RESULT_PATH, 'w') as f:
        json.dump(dict(layout=layout, frame_bytes=288, traps=results,
                       timer_ticks=3, passed=True), f, indent=2)
    print('A verification: PASS (4 exception returns, 3 timer returns)')
    b_done.delete()
    gdb.execute('detach', to_string=True)
except Exception:
    import traceback
    traceback.print_exc()
    gdb.execute('quit 1')
'''


def run(command, cwd, logfile, timeout=60):
    with logfile.open('w') as output:
        subprocess.run(command, cwd=cwd, stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=Path('../report/evidence/A-20261104'))
    parser.add_argument('--production-handlers', action='store_true',
                        help='verify A/B/C together without replacing real handlers')
    args = parser.parse_args()
    code = Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'trap-summary.json').unlink(missing_ok=True)
    qemu = os.environ.get('QEMU', 'qemu-system-riscv64')
    debugger = os.environ.get('GDB', 'gdb-multiarch')
    with tempfile.TemporaryDirectory(prefix='nkuos-lab3-a-') as directory:
        fixture = Path(directory) / 'code'
        shutil.copytree(code, fixture,
                        ignore=shutil.ignore_patterns('bin', 'obj', '__pycache__'))
        trap = fixture / 'kern/trap/trap.c'
        source = trap.read_text()
        start = source.index('//lab3 function2')
        end = source.index('static inline void trap_dispatch', start)
        if not args.production_handlers:
            trap.write_text(source[:start] + HANDLERS + '\n' + source[end:])
        init = fixture / 'kern/init/init.c'
        source = init.read_text().replace('#include <dtb.h>',
                                         '#include <dtb.h>\n#include <sbi.h>')
        position = source.rindex('    idt_init();')
        line_end = source.index('\n', position)
        source = source[:line_end] + ('\n    extern void a_context_probe(void);'
                                     '\n    a_context_probe();') + source[line_end:]
        source = source.replace('    /* do nothing */', '''
    while (ticks < 3) { asm volatile("" ::: "memory"); }
    asm volatile(".globl a_probe_done\\na_probe_done:");
    cprintf("A fixture: four exception returns and three timer returns completed\\n");
    sbi_shutdown();
    /* do nothing */''')
        init.write_text(source)
        (fixture / 'kern/init/a_context.S').write_text(context_assembly())
        run(['make', '-j4'], fixture, out / 'fixture-build.log')
        run(['riscv64-unknown-elf-objdump', '-d', 'bin/kernel'], fixture,
            out / 'fixture-disassembly.log')
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        script = ('set pagination off\nset confirm off\n'
                  'set architecture riscv:rv64\nset remotetimeout 10\n'
                  f'file {fixture}/bin/kernel\n'
                  f'target remote 127.0.0.1:{port}\npython\n'
                  f'RESULT_PATH = {str(out / "trap-summary.json")!r}\n' +
                  f'PRODUCTION_HANDLERS = {args.production_handlers!r}\n' +
                  GDB_SCRIPT + '\nend\nquit\n')
        script_path = Path(directory) / 'verify.gdb'
        script_path.write_text(script)
        (out / 'verify.gdb').write_text(script)
        with (out / 'fixture-qemu.log').open('w') as serial:
            process = subprocess.Popen(
                [qemu, '-machine', 'virt', '-m', '128M', '-nographic',
                 '-bios', 'default', '-kernel', str(fixture / 'bin/ucore.img'),
                 '-monitor', 'none', '-S', '-gdb', f'tcp:127.0.0.1:{port}'],
                stdin=subprocess.DEVNULL, stdout=serial, stderr=subprocess.STDOUT)
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
                run([debugger, '-q', '-nx', '-batch', '-x', str(script_path)],
                    fixture, out / 'trap-gdb.log', timeout=60)
                if not json.loads((out / 'trap-summary.json').read_text())['passed']:
                    raise AssertionError('GDB verification failed')
                process.wait(timeout=10)
                if process.returncode != 0:
                    raise AssertionError('QEMU exit status')
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
        digest = hashlib.sha256((code / 'kern/trap/trapentry.S').read_bytes()).hexdigest()
        (out / 'tested-source.json').write_text(json.dumps(
            {'trapentry_sha256': digest,
             'trap_c_sha256': hashlib.sha256((code / 'kern/trap/trap.c').read_bytes()).hexdigest(),
             'handlers': 'production A/B/C' if args.production_handlers else 'temporary A fixtures'}, indent=2)+'\n')
    print('PASS: frame layout, 32 registers, 4 exceptions, 3 timer returns, S privilege')


if __name__ == '__main__':
    main()
