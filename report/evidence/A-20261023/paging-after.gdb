set pagination off
set confirm off
target remote localhost:1234
break kern_init
continue
info registers pc satp
x/gx (char *)&boot_page_table_sv39 + 16
x/gx (char *)&boot_page_table_sv39 + 4088
detach
quit
