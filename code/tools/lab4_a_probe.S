    .section .text.a_probe,"ax",@progbits
    .option push
    .option norvc
    .globl a_store
a_store:
    sd a1, 0(a0)
    ret

    .globl a_load
a_load:
    ld a0, 0(a0)
    ret
    .balign 4
    .globl a_bootstrap_fault
a_bootstrap_fault:
    j a_bootstrap_fault
    .option pop
