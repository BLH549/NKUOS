set pagination off
set confirm off
set architecture riscv:rv64
target remote localhost:12345
echo === RESET ===\n
info registers pc
x/6i $pc
x/3i 0x80200000
x/3gx 0x1018
si
si
si
si
si
si
echo === OPENSBI ===\n
info registers pc a0 a1 a2
x/6gx $a2
hbreak *0x80200000
continue
echo === KERNEL ENTRY ===\n
info registers pc sp ra
p/x &bootstack
p/x &bootstacktop
p/x &edata
p/x &end
x/3i $pc
si
echo === AFTER AUIPC ===\n
info registers pc sp ra
si
echo === AFTER LA ===\n
info registers pc sp ra
si
echo === AFTER TAIL ===\n
info registers pc sp ra
x/4i $pc
hbreak cprintf
continue
echo === PRINT ===\n
info registers pc sp a0 a1
x/s $a0
x/s $a1
detach
quit
