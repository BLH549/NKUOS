# Lab 4 提示词与执行记录

## A · 虚拟内存页操作（匡航逸，2412235）

**最终提示词**

```text
[PROMPT]
完成 Lab 4 的页映射模块。实现 page_insert() 和 page_remove()，核对多级页表遍历、数据页引用计数和 TLB 失效之间的关系。完成 Sv32、Sv39、Sv48 的分页思考题，并说明 get_pte() 的查询与分配接口如何组织。

[RELY]
在 labs 仓库的 lab4 分支工作，以 c42cc35 的前置迁移和线程创建实现为基础。参考本地 Lab 4 指导书的页映射模块，以及 code/kern/mm/ 下的 pmm.c、pmm.h、mmu.h、memlayout.h 和 default_pmm.c。复用 Lab 2 的 Best-Fit 与物理页分配接口，以及 Lab 4 的 SLOB、VMA 和 4 KiB 页表接口。

[SPEC]
1. 在 pmm.c 的 LAB4 function1 区域标注学号 2412235。get_pte() 按 PDX1、PDX0、PTX 定位末级页表项。建立下级页表时先清零，非叶项由 ptd_create() 编码物理页号和 V 位，再执行全量 sfence.vma。叶项更新时按虚拟地址执行 sfence.vma，两种刷新都使用编译器内存屏障。
2. page_insert() 取得末级页表项后再增加新页引用。首次映射写入页号和权限。同一地址重映射到同一页时只更新权限，引用计数净变化为零。换页时撤销旧引用并在归零时释放旧页。不同虚拟地址引用同一页时分别计数。
3. 页表页分配失败时返回 -E_NO_MEM，目标映射与数据页引用保持原值。若第二次分配失败，已经挂接的中间页表由目录持有，后续重试可复用。
4. page_remove() 使用 get_pte(..., 0) 查询。目标无映射时直接返回。有映射时先清页表项并刷新对应 TLB，再减少旧页引用，归零后释放数据页。中间页表供下一次映射继续使用。
5. 分页思考题结合三种模式的页表层数、索引宽度和叶项位置，解释 get_pte() 两段目录遍历相似的原因，并比较共用遍历和显式拆分查找、创建入口时的失败语义。

[CHECK]
1. 运行 check_pgdir() 和 check_boot_pgdir()，分别核对页表结构、引用计数和实际访存。检查非叶项的 U/R/W/X/A/D 位为零，并运行教师 PMM 和 Process Create 检查。
2. 覆盖首次映射、同页权限升降、换页、别名、未对齐地址、重复删除和两级页表页分配失败。用独立模型对照 PTE、权限、引用和空闲页变化。
3. 用实际 load/store 检查换页后读取、只读写入异常、恢复写权限及删除后读取异常。分别通过 64、128 和 256 MiB 配置，并回归 B 的线程创建测试。记录命令、现象和定位依据。
```

**AI 回复摘要**

完成 `page_insert()` 和 `page_remove()`，用 `page_remove_pte()` 统一换页和删除时的旧映射清理，并整理三种分页模式与 `get_pte()` 接口的思考题。QEMU 首次执行 `check_boot_pgdir()` 时，对 `0x100` 的写入触发缺页。根据 GDB 读出的异常原因和根表项定位到非叶项的 U 位，两级目录项改用 `ptd_create()` 后重新验证。

**执行结果**

64、128、256 MiB 三组配置的页表检查、真实访存和教师 PMM、Process Create 检查均通过。独立模型累计对照 12,288 次操作，另用故障注入验证了两级页表页分配失败的处理。随后重新运行 B 的创建与首次执行测试，结果通过。

## B · 线程创建与初始化（柯云超，2413575）

**最终提示词**

```text
[PROMPT]
完成 Lab 4 的内核线程创建与初始化。实现 alloc_proc()、proc_init()、kernel_thread()、do_fork()、copy_thread() 和 copy_mm()，使内核能够登记启动线程 idle、创建首个工作线程 init，并为每个新线程准备独立的内核栈和首次运行现场。

[RELY]
在 labs 仓库的 lab4 分支工作，以 dad7267 的 Lab 2/3 前置迁移为基础。对照本地 Lab 4 指导书的 PCB 和内核线程创建部分，以及 code/kern/process/ 下的 proc.c、proc.h 和 entry.S。按 trap.h 与 trapentry.S 核对 Trap frame 布局，复用 kmalloc/kfree、setup_kstack/put_kstack、PID 哈希和 wakeup_proc()。

[SPEC]
1. 在 proc.c 的 LAB4 function2 区域标注学号 2413575。alloc_proc() 分配并清零 PCB，设为 PROC_UNINIT、pid=-1，页目录设为 boot_pgdir_pa，两个链表节点初始化为自环。分配失败时返回 NULL。
2. proc_init() 初始化进程链表和 PID 哈希桶。PID 0 的 idle 复用 bootstack，进入 PROC_RUNNABLE，设 need_resched=1，计入进程数并成为 current。以 idle 为父线程创建 PID 1 的 init，入口设为 init_main，传入 Hello world!!，再通过 PID 找回并命名。
3. kernel_thread() 构造初始 Trap frame，将函数与参数放入 s0 和 s1，将 epc 指向 kernel_thread_entry，置 SPP 和 SPIE、清 SIE。将调用方标志与 CLONE_VM 合并后调用 do_fork()。
4. do_fork() 检查容量、分配 PCB、记录父线程、分配两页内核栈，再依次调用 copy_mm() 和 copy_thread()。资源准备完毕后，关本 hart 中断，分配 PID、挂接哈希和全局链表、增加计数并唤醒线程。成功返回正 PID。容量超限返回 -E_NO_FREE_PROC，PCB 或内核栈分配失败返回 -E_NO_MEM。出错时按获取资源的逆序回收。
5. copy_mm() 让内核线程沿用启动页表并保持 mm=NULL。父线程带用户 mm 时返回 -E_INVAL，由 do_fork() 回收栈与 PCB。copy_thread() 将 Trap frame 复制到新线程栈顶，设置子线程 a0=0。帧中的 sp 取传入的非零栈指针或内核栈顶。context.sp 指向帧基址，context.ra 指向 forkret。

[CHECK]
1. 运行教师 Process Create 检查，并核对 PCB 初值、idle/init 的 PID 和父子关系、哈希查询、Trap frame 地址、16 字节对齐和 context.ra/sp。
2. 测试 PCB 分配失败、内核栈分配失败、非空父 mm 和进程数上限，核对资源清理及链表、哈希、进程计数和空闲页数。
3. 在 QEMU/GDB 中观察 forkret、forkrets、__trapret 和 kernel_thread_entry，确认子线程以正确的 SP 和中断状态进入入口，让两个线程分别收到各自参数。用显式栈指针额外验证 copy_thread() 的恢复规则。
```

**AI 回复摘要**

完成六个创建函数，将 PCB、内核栈和初始现场准备好后再登记 PID 并唤醒线程。独立验证脚本最初用 `"$qemu"` 展开命令数组，QEMU 缺少启动参数而超时。改用 `"${qemu[@]}"` 传入完整命令后，获得了串口和 GDB 记录。

**执行结果**

教师 Process Create 检查通过。四条失败路径均通过资源回收检查，进程计数和空闲页数保持原值。GDB 依次观察到 `forkret`、`forkrets`、`__trapret` 和 `kernel_thread_entry`。两个测试线程分别收到参数 `2413575` 和 `20261120`，另一个用例验证了显式传入的栈指针。

## C · 调度与上下文切换（刘华彬，2411238）

**最终提示词**

```text
[PROMPT]
完成 Lab 4 的内核线程调度和上下文切换。实现 cpu_idle()、schedule()、proc_run() 及 RV64 的 switch_to()，使 init 能从 idle 手中接管 CPU，已运行的内核线程能在重新被选中时恢复执行。说明嵌套开关中断如何恢复原有状态。

[RELY]
在 labs 仓库的 lab4 分支工作，以已合入页映射和线程创建的 690308b 为基础。参考本地 Lab 4 指导书的调度模块，并核对 code/kern/ 下的 process/proc.c、process/proc.h、process/switch.S、schedule/sched.c、trap/trap.c、trap/trapentry.S 和 sync/sync.h。按既有的 PCB、context、Trap frame 和教师测试接口实现。普通内核线程共用启动根页表。

[SPEC]
1. 在 LAB4 function3 对应区域标注学号 2411238。cpu_idle() 在启动栈上检查 current->need_resched，标记置位时调用 schedule()。
2. schedule() 保存并屏蔽本 hart 的中断，清除当前线程的重调度标记。idle 从进程链表头之后扫描，普通线程从自己的后继扫描。跨过哨兵、最多环绕一周，选择第一个 PROC_RUNNABLE 的线程。允许绕回时选中当前线程，无候选时回退到 idle。递增目标 runs，需要实际切换时调用 proc_run()，原线程恢复后还原自己的中断状态。
3. proc_run() 在目标等于 current 时直接返回。否则保存旧 PCB，在关中断期间更新 current，使用目标根页表的物理页号、Sv39 模式和 ASID 0 构造 SATP，写入后执行全量 sfence.vma，再调用 switch_to()。两套页表都应映射切换所需的内核指令、PCB 和栈。旧线程再次被选中后按当时保存的标记恢复中断。
4. switch_to() 用 a0、a1 接收旧、目标 context 指针，按 14 个 RV64 槽位依次保存和恢复 ra、sp、s0–s11。ret 首次进入 forkret，后续则回到该线程原来的调用续点。继续使用 forkrets、__trapret 和 sret 恢复首次运行现场。
5. 核对 Lab 3 迁入的 Trap 入口、恢复和处理函数，沿用 SUM、sstatus/sepc 恢复和时钟续订。移除 Lab 3 专用的定时终止，让 Lab 4 的时钟处理持续返回。中断 Challenge 要解释 local_intr_save() 和 local_intr_restore() 如何根据每层保存的 SIE 标记嵌套工作，已运行线程如何在恢复后用回原标记，新线程的 sret 如何从 SPIE 恢复 SIE，以及本 hart 屏蔽的作用范围。

[CHECK]
1. 构建并启动正式内核，确认从 idle 切换到 init，观察 PID 1 的输出和框架 do_exit() 入口。运行教师 PMM、Process Create 和 Schedule 三项评分。
2. 在临时副本中检查空链表、全睡眠、自选、直接调用 proc_run()、两个工作线程有序轮换并最终回到 idle。比较 current、SP、栈上局部变量、s0–s11、SATP 和 SIE。
3. 为两个测试线程构造保留公共内核映射的不同根页表，在同一虚拟地址读出各自物理页的值。验证嵌套中断屏蔽、16/32 位异常返回及超过 100 次定时中断后的执行。保存实际命令、运行现象和故障定位依据。
```

**AI 回复摘要**

接通 idle 调度、进程链表轮换、Sv39 SATP 更新和 RV64 上下文恢复，整理了嵌套中断状态的恢复过程。QEMU 7 启动时改用 `-kernel` 传递内核入口。独立探针最初放在 `kern/init/`，导致裸镜像入口移位，经 `nm` 核对后将探针移到临时测试树的 `kern/schedule/`。教师评分时用 `DEFS=-Wno-error=incompatible-pointer-types` 将 GCC 15 对测试回调签名的诊断降为警告。

**执行结果**

普通启动进入 PID 1 的 `init_main`，打印问候语后到达框架的 `do_exit()`。教师 PMM、Process Create、Schedule 三项均通过，得分 **30/30**。独立用例通过两个线程的 16 次有序让出、寄存器和栈恢复、不同根页表对同一虚拟地址的实际读取及 SIE 恢复检查。混合长度的指令异常和 101 次时钟中断均正常返回。
