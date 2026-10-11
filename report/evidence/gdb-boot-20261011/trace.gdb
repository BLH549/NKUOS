set pagination off
set confirm off

echo \n=== Reset: MROM ===\n
info registers pc sp t0 a0 a1 a2
x/6i $pc
x/4gx 0x1018
x/6gx 0x1028
x/3i 0x80200000
watch *(unsigned int *)0x80200000

echo \n=== Step 1: auipc ===\n
si
info registers pc t0
echo \n=== Step 2: addi ===\n
si
info registers pc a2
echo \n=== Step 3: mhartid ===\n
si
info registers pc a0
echo \n=== Step 4: device tree ===\n
si
info registers pc a1
echo \n=== Step 5: firmware address ===\n
si
info registers pc t0
echo \n=== Step 6: jump to OpenSBI ===\n
si
info registers pc a0 a1 a2
x/8i $pc

echo \n=== OpenSBI to kernel ===\n
break *0x80200000
continue
info registers pc sp a0 a1 a2 mepc mstatus satp
info breakpoints
x/3i $pc
p/x &bootstack
p/x &bootstacktop
p/x &kern_init
p/x &edata
p/x &end
delete 1

echo \n=== Execute the first kernel instruction ===\n
si
info registers pc sp
si
info registers pc sp
si
info registers pc sp
x/6i $pc

echo \n=== C initialization and output ===\n
break cprintf
continue
info registers pc sp a0 a1
x/s $a0
x/s $a1
detach
quit
