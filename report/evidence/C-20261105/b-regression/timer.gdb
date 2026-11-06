set pagination off
set confirm off
set architecture riscv:rv64
set remotetimeout 10
file /home/liu/projects/nku-os/labs/code/bin/kernel
target remote 127.0.0.1:55965
python
RESULT_PATH = '/home/liu/projects/nku-os/labs/report/evidence/C-20261105/b-regression/timer-summary.json'

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

end
quit
