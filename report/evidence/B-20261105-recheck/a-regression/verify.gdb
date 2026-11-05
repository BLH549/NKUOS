set pagination off
set confirm off
set architecture riscv:rv64
set remotetimeout 10
file /tmp/nkuos-lab3-a-py7xqle3/code/bin/kernel
target remote 127.0.0.1:45783
python
RESULT_PATH = '/home/kyc/project/os/NKUOS-lab3/report/evidence/B-20261105-recheck/a-regression/trap-summary.json'

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

end
quit
