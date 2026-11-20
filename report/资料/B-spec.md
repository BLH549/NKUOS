# Lab4 分工 B 规格

负责人为柯云超，学号 2413575。实现位置为 `code/kern/process/proc.c` 的 `//LAB4 function2 2413575` 区域。规格依据为 Lab4 指导书的功能模块二、仓库任务分工及当前框架的 PCB、Trap frame 和汇编定义。

## 依赖与边界

- 物理内存与堆分配复用 `kmalloc`、`kfree`、`setup_kstack`、`put_kstack`，内核线程沿用 `boot_pgdir_pa`。
- 进程登记复用 `proc_list`、`hash_proc`、`get_pid`、`find_proc`、`wakeup_proc`，使用框架的中断保存与恢复接口。
- 首次运行沿用 `struct context`、`struct trapframe`、`forkret`、`forkrets`、`__trapret` 和 `kernel_thread_entry` 的布局与调用约定。
- 创建范围为单 hart 的 Lab4 内核线程。用户地址空间复制、普通退出后的回收以及上下文切换由后续实验或对应模块负责。

## 函数契约

| 函数 | 契约 |
| --- | --- |
| `alloc_proc()` | 分配 PCB。成功时为 `PROC_UNINIT`、`pid = -1`、页目录为 `boot_pgdir_pa`，两个链表节点初始化为自环，其余字段为零。失败返回 `NULL`。 |
| `proc_init()` | 一次性初始化链表与 PID 哈希桶。创建 PID 0 的 `idleproc`，复用 `bootstack`，设为 runnable 并置位 `need_resched`，计入进程总数且令 `current` 指向它。以 idle 为父线程创建 PID 1 的 `initproc`，入口为 `init_main`，参数为 `Hello world!!`。启动阶段分配失败触发 panic。 |
| `kernel_thread(fn, arg, flags)` | 构造 S 态初始 Trap frame。在 `s0/s1` 中传入函数和参数，`epc` 指向 `kernel_thread_entry`，设置 `SPP/SPIE` 并清除 `SIE`。以 `flags \| CLONE_VM` 和空初始栈参数调用 `do_fork()`。 |
| `do_fork(flags, stack, tf)` | 输入当前内核线程、有效 Trap frame 指针和可选的新线程栈指针。成功返回正 PID。进程数达到 `MAX_PROCESS` 返回 `-E_NO_FREE_PROC`，PCB 或栈分配失败返回 `-E_NO_MEM`，地址空间初始化失败向上传递错误。准备完所有资源后分配 PID、挂哈希与全局链表、增加计数并唤醒子线程。 |
| `copy_thread(proc, esp, tf)` | 将传入的现场按值复制到子线程内核栈顶部，子线程 `a0 = 0`。`esp` 非零时用它作为恢复后的 `sp`，否则用子线程内核栈顶。`context.sp` 指向保存的 Trap frame，`context.ra` 指向 `forkret`。 |
| `copy_mm(flags, proc)` | 父线程 `mm == NULL` 时，子线程继续使用内核页目录并保持 `mm == NULL`。遇到非空父 `mm` 时返回 `-E_INVAL`，由 `do_fork()` 释放已分配的内核栈和 PCB。 |

## 验收

1. 教师的 Process Create 检查通过，额外核对 PCB 缺省值、idle/init 的关系、PID 哈希和链表。
2. 检查 `tf` 的位置与 16 字节对齐、`a0/sp`、`s0/s1`、`epc`、`sstatus` 和 `context.ra/sp`，并验证显式传入栈指针的语义。
3. 注入 PCB 和内核栈分配失败。用非空父 `mm` 和进程数上限检查另外两条失败路径。核对分配与释放调用、链表、哈希、计数和空闲页。
4. 在 QEMU/GDB 中检查首次运行的真实返回链，让两个可按 PID 查找的线程分别收到各自参数。调度器联调及全量评分在 A/C 实现合入后进行。
