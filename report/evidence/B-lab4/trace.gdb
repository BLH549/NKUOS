set pagination off
set confirm off
target remote 127.0.0.1:12340

thbreak *test_lab4_b_run
continue
printf "[B PCB] current==idle %d idle pid=%d state=%d stack=%#lx bootstack=%p\n", current == idleproc, idleproc->pid, idleproc->state, idleproc->kstack, &bootstack
printf "[B PCB] init pid=%d state=%d parent==idle %d mm=%p pgdir=%#lx\n", initproc->pid, initproc->state, initproc->parent == idleproc, initproc->mm, initproc->pgdir
printf "[B frame] size=%lu init tf=%p context.sp=%#lx context.ra=%#lx\n", sizeof(*initproc->tf), initproc->tf, initproc->context.sp, initproc->context.ra
printf "[B frame] tf alignment remainder=%lu stack_top=%#lx tf.sp=%#lx\n", (unsigned long)initproc->tf % 16, initproc->kstack + 8192, initproc->tf->gpr.sp

thbreak *forkret
continue
printf "[B trace] forkret pid=%d context.sp=%#lx sp=%#lx\n", current->pid, current->context.sp, $sp
if current->pid != 2 || $sp != current->context.sp
    quit 1
end

thbreak *forkrets
continue
printf "[B trace] forkrets a0=%#lx tf=%p\n", $a0, current->tf
if $a0 != (unsigned long)current->tf
    quit 1
end

thbreak *__trapret
continue
printf "[B trace] __trapret sp=%#lx tf=%p\n", $sp, current->tf
if $sp != (unsigned long)current->tf
    quit 1
end

thbreak *kernel_thread_entry
continue
printf "[B trace] kernel_thread_entry sp=%#lx tf.sp=%#lx a0=%#lx SIE=%lu\n", $sp, current->tf->gpr.sp, $a0, ($sstatus >> 1) & 1
printf "[B trace] s0=%#lx s1=%#lx s1_arg=%d\n", $s0, $s1, *(int *)$s1
if $sp != current->tf->gpr.sp || $a0 != 0 || ($sstatus & 2) == 0
    quit 1
end
if $s0 != (unsigned long)&b_worker || $s1 != current->tf->gpr.s1
    quit 1
end

thbreak *b_worker
continue
printf "[B trace] b_worker pid=%d a0=%#lx arg=%d\n", current->pid, $a0, *(int *)$a0
if $a0 != current->tf->gpr.s1 || *(int *)$a0 != 2413575
    quit 1
end
printf "[B trace] PASS\n"
detach
