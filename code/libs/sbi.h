/*
 * SBI 服务接口的旧模板声明。部分接口没有对应定义，且当前 Lab1 不使用它们。
 * 本头文件不是现代 SBI 扩展接口的完整实现，实际可调用函数以 sbi.c 为准。
 */
#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

/*
 * memory_block_info：旧接口的内存块描述。base 为起始地址，size 为字节大小，
 * node_id 为节点标识；当前 Lab1 没有查询或使用这个结构。
 */
typedef struct {
  unsigned long base;
  unsigned long size;
  unsigned long node_id;
} memory_block_info;

/*
 * sbi_query_memory：旧模板的内存块查询接口声明。
 * 参数 id：查询编号；p：用于接收 base、size、node_id 的结构地址。
 * 返回类型为 unsigned long，但本仓库没有实现，无法在这里约定有效返回值。
 * Lab1 未调用它；不能仅凭此声明认为内核已经探测物理内存。
 */
unsigned long sbi_query_memory(unsigned long id, memory_block_info *p);

/*
 * sbi_set_timer：调用编号为 0 的旧式 SBI 定时器服务。
 * 参数 stime_value：下一次定时器事件的绝对计时值，使用平台计时单位。
 * RV64 下通过 a0 传递 64 位值；其余参数为零。返回值：无。
 * 本函数只请求设置定时器，Lab1 没有调用它，也没有建立定时中断处理流程。
 */
void sbi_set_timer(unsigned long long stime_value);
/*
 * sbi_send_ipi：旧模板的处理器间中断发送接口声明。
 * 参数 hart_id：模板命名的目标 hart 标识；返回值：无。
 * 本仓库没有实现，不应由声明推断底层 SBI 参数编码；Lab1 不使用多核通知。
 */
void sbi_send_ipi(unsigned long hart_id);
/*
 * sbi_clear_ipi：旧模板的处理器间中断清除接口声明。
 * 参数：无；返回类型：unsigned long。当前无实现，具体返回语义未定义。
 * Lab1 不调用本接口，也没有相应中断处理流程。
 */
unsigned long sbi_clear_ipi(void);
/*
 * sbi_shutdown：请求固件关闭机器的接口声明。
 * 参数：无；返回类型：void。当前源码未实现此函数，启动末尾使用无限循环，
 * 没有通过这个声明执行关机。
 */
void sbi_shutdown(void);

/*
 * sbi_console_putchar：调用编号为 1 的旧式 SBI 控制台输出服务。
 * 参数 ch：要输出的字节；返回值：无，忽略 sbi_call 的返回值。
 * 字符放在第一个参数中，另外两个参数置零。真正的设备输出由固件完成。
 */
void sbi_console_putchar(unsigned char ch);
int sbi_console_getchar(void);

#endif
