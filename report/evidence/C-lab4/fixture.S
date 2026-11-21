    .text

    # Each register carries a distinct value derived from a thread-specific seed.
    .macro set_saved reg, slot
    sd \reg, (8 * \slot)(sp)
    addi \reg, a0, \slot
    .endm

    .macro check_saved reg, slot
    addi t1, t0, \slot
    bne \reg, t1, .Lbad_register
    ld \reg, (8 * \slot)(sp)
    .endm

    .globl check_saved_registers
check_saved_registers:
    addi sp, sp, -112
    sd ra, 96(sp)
    sd a0, 104(sp)
    set_saved s0, 0
    set_saved s1, 1
    set_saved s2, 2
    set_saved s3, 3
    set_saved s4, 4
    set_saved s5, 5
    set_saved s6, 6
    set_saved s7, 7
    set_saved s8, 8
    set_saved s9, 9
    set_saved s10, 10
    set_saved s11, 11
    call schedule
    ld t0, 104(sp)
    check_saved s0, 0
    check_saved s1, 1
    check_saved s2, 2
    check_saved s3, 3
    check_saved s4, 4
    check_saved s5, 5
    check_saved s6, 6
    check_saved s7, 7
    check_saved s8, 8
    check_saved s9, 9
    check_saved s10, 10
    check_saved s11, 11
    ld ra, 96(sp)
    addi sp, sp, 112
    ret

    .globl check_exception_return
check_exception_return:
    li t0, 0x5678
    li a0, 0x1234
    .balign 4
    .2byte 0x9002       # c.ebreak
    .4byte 0x00100073   # ebreak at a two-byte boundary
    .2byte 0x0000       # illegal compressed instruction
    li t1, 0x5678
    bne t0, t1, .Lbad_register
    li t1, 0x1234
    bne a0, t1, .Lbad_register
    ret

.Lbad_register:
    tail fixture_bad_register
