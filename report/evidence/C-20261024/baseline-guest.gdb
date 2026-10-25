set pagination off
set confirm off
file labs/code/bin/kernel
set arch riscv:rv64
target remote :1235
hbreak kern_init
continue
info registers pc satp sp
p/x &boot_page_table_sv39
x/gx (char *)&boot_page_table_sv39+16
x/gx (char *)&boot_page_table_sv39+4088
x/16i $pc
x/6i 0xffffffffc0200000
p/x &boot_dtb
p/x &boot_hartid
detach
quit
