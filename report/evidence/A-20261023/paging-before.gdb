set pagination off
set confirm off
target remote localhost:1234
break *0x80200034
continue
info registers pc satp stvec
x/3i $pc
stepi
info registers pc satp
stepi
info registers pc scause sepc stval
detach
quit
