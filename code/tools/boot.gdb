set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1234

echo \n=== RESET ROM: first instructions ===\n
info registers pc sp
x/6i $pc
x/2gx 0x1018
if $pc != 0x1000
  echo CHECK reset_pc: FAIL\n
  quit 1
end
echo CHECK reset_pc: PASS\n

echo \n=== KERNEL IMAGE: present before CPU execution ===\n
x/4i 0x80200000

hbreak *0x80000000
continue
echo \n=== OPENSBI ENTRY ===\n
info registers pc a0 a1
x/8i $pc
if $pc != 0x80000000
  echo CHECK firmware_entry: FAIL\n
  quit 1
end
echo CHECK firmware_entry: PASS\n
delete breakpoints

hbreak *0x80200000
continue
echo \n=== KERNEL ENTRY: before stack initialization ===\n
info registers pc sp ra a0 a1
x/6i $pc
p/x &bootstack
p/x &bootstacktop
if $pc != (unsigned long)&kern_entry
  echo CHECK kernel_entry: FAIL\n
  quit 1
end
echo CHECK kernel_entry: PASS\n
set $entry_ra = $ra
delete breakpoints

si
si
echo \n=== AFTER la sp, bootstacktop ===\n
info registers pc sp ra
p/x &bootstacktop
if $sp != (unsigned long)&bootstacktop
  echo CHECK stack_pointer: FAIL\n
  quit 1
end
echo CHECK stack_pointer: PASS\n

tbreak *kern_init
continue
echo \n=== AFTER tail kern_init ===\n
info registers pc sp ra
disassemble kern_init
if $pc != (unsigned long)&kern_init || $ra != $entry_ra
  echo CHECK tail_jump: FAIL\n
  quit 1
end
echo CHECK tail_jump: PASS\n
printf "BSS range: edata=0x%lx end=0x%lx bytes=%lu\n", (unsigned long)&edata, (unsigned long)&end, (unsigned long)&end - (unsigned long)&edata
detach
echo \nGDB boot verification completed.\n
