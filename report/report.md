# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab4：虚拟内存页操作与内核线程 |
| 本次整理 | 匡航逸，2412235，2026-11-21 |
| 代码位置 | `../code/`，`lab4` 分支 |
| 已有成果 | Lab2/3 前置迁移；B 的线程创建与初始化 |
| 本次完成 | A 的页映射、内存基线验证、分页思考题与报告汇总 |

实验分工沿用 [Lab4-任务分工.md](资料/Lab4-任务分工.md)。各部分材料：

- [A：页映射与内存基线](资料/A-页映射与内存.md)
- [B：线程创建与初始化](资料/B-线程创建.md)
- [Lab2/3 前置迁移](资料/Lab4-前置代码迁移.md)
- [spec 与提示词记录](prompt.md)

当前页映射和线程创建已分别完成验证，C 的调度与上下文切换仍待接入。

## 一、实验目的与整体逻辑

Lab4 在物理页分配和 Trap 恢复的基础上，进一步建立页映射与内核线程。页表操作负责地址到物理页的映射，线程创建负责分配 PCB、内核栈并组织初始现场，调度部分则负责选择线程和切换执行状态。

```text
kern_init
  → pmm_init：物理页、页表检查、SLOB
  → pic_init / idt_init：中断与异常入口
  → vmm_init：VMA 检查
  → proc_init：登记 idle，创建 init
  → clock_init / intr_enable
  → cpu_idle → schedule → proc_run → switch_to
```

前半段准备可分配的内存和可访问的地址，线程创建再使用这些接口构造可运行对象。新线程的 `context.ra` 指向 `forkret`，`context.sp` 指向栈顶 Trap frame；首次运行沿 `forkrets → __trapret → kernel_thread_entry` 进入线程函数。后续调度继续复用这份上下文布局。

## 二、实验环境

| 项目 | A 本轮验证环境 |
| --- | --- |
| 宿主机 | WSL2 Ubuntu 24.04.4 LTS，x86-64 |
| 交叉编译器 | `riscv64-unknown-elf-gcc` 13.2.0 |
| QEMU / OpenSBI | 8.2.2 / 1.3 |
| GDB / Make | gdb-multiarch 15.1 / GNU Make 4.3 |
| AI 工具 | Codex，GPT-6 |

源码采用已有 Lab4 框架和前置迁移，B 的实现来自提交 `c42cc35`。本轮版本记录见 [environment.json](evidence/A-lab4/environment.json)，B 原始环境和验证日期见 [B 报告](资料/B-线程创建.md)。

内核物理入口为 `0x80200000`，链接地址为 `0xffffffffc0200000`，普通页大小为 4 KiB。默认 RAM 为 128 MiB，页描述符大小为 64 字节。

## 三、页映射与内存基线

`page_insert()` 先定位末级页表项，再处理引用计数和旧映射。首次映射增加一个引用；同页重映射抵消临时增加的引用，更新权限；替换其他页时撤销旧引用，归零后释放。`page_remove()` 使用只读查询，缺少映射时直接返回，有效映射则清项、刷新 TLB、减少引用并按需释放。

独立测试同时检查 PTE、引用计数和空闲页数，覆盖首次映射、同页权限更新、换页、多地址引用、重复删除，以及两级页表申请失败。中间页表的保留与回收范围见 [A-spec.md](资料/A-spec.md)。

首轮运行通过了结构性的 `check_pgdir()`，但在 `check_boot_pgdir()` 实际写入 `0x100` 时发生存储缺页。GDB 显示 `scause=15`、`stval=0x100`，根表项低位包含 U。问题来自前置 `get_pte()`：两处非叶项使用了 `PTE_U | PTE_V`。

非叶项改为 `ptd_create()`，只保留物理页号和 V 位；权限继续由末级叶项决定。对应的内核检查改为要求非叶项 U/R/W/X/A/D 均为零。修复后字符串写入和全部页表检查通过。中间页表建立采用全量 TLB 刷新，叶项变化采用按地址刷新，两种刷新均加上编译器内存屏障。

Sv32、Sv39、Sv48 的普通页都是 4 KiB，对应两级、三级和四级页表。当前 `get_pte()` 的两段相似代码分别定位下一层页表，最后一层返回具体 PTE。查找和分配共用遍历可以减少重复，但 `create=1` 带来分配副作用，需要明确页表页的所有权与失败清理规则。完整实现过程、GDB 对照和分页思考题见 [A 报告](资料/A-页映射与内存.md)。

## 四、线程创建与初始化

B 已完成 `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()` 和 `copy_mm()`。

PCB 初始化后处于 `PROC_UNINIT`。创建过程先准备 PCB、内核栈和寄存器现场，再分配 PID、登记哈希与链表、增加进程数并唤醒。资源准备失败时按逆序回收，失败对象不会进入 PID 查询结构。当前内核线程共用启动页表，`copy_mm()` 对非空用户地址空间返回 `-E_INVAL`。

本轮沿用 B 的独立验证脚本重新检查：四种失败路径保持链表、进程数和空闲页数不变；多个 PID 可查询；新线程的 Trap frame 位于各自栈顶，首次执行收到正确参数。实现、原始提示词和首次运行 GDB 记录见 [B 报告](资料/B-线程创建.md)，本轮回归见 [B-regression-qemu.log](evidence/A-lab4/B-regression-qemu.log)。

## 五、验证与当前集成结果

| 项目 | 本轮结果 |
| --- | --- |
| A 独立内核 | 64、128、256 MiB 三组通过 |
| 内存与页表 | Best-Fit、`check_pgdir()`、`check_boot_pgdir()`、SLOB、VMA 检查通过 |
| 教师 PMM / Process Create | 三组均输出 PASS |
| 映射与失败路径 | 首次映射、权限更新、换页、别名、重复删除、页表分配失败通过 |
| 硬件访存 | 换页后读出新内容；只读写入和删除后读取触发预期缺页；恢复写权限后写入成功 |
| 独立模型 | 12,288 次操作，映射、权限和引用计数一致 |
| B 回归 | PCB、内核栈、`copy_mm`、容量上限回滚及两个线程首次运行通过 |
| 正式 `make` / `make qemu` | 链接缺少 C 的 `cpu_idle()` |
| 正式 `make grade` | 还缺少 `schedule()`；退出码 2，输出 `Total Score: 0/30` |

独立内核在临时副本中使用 A 的验证入口，保留教师 PMM 和 Process Create 检查。正式启动与调度评分在 C 接入后继续执行。当前结果和源码校验值见 [A 验证汇总](evidence/A-lab4/summary.json)，正式命令状态见 [integration-status.json](evidence/A-lab4/integration-status.json)。

复现命令：

```bash
cd code
make check-a
python3 ../report/evidence/A-lab4/regress_b.py
```

## 六、与 OS 原理的联系

物理页分配、虚拟地址映射和引用计数分别处理可用空间、访问位置和释放时机。一次换页操作需要同时维护这几层状态，TLB 则决定处理器何时观察到新的映射和权限。

内核线程通过 PCB 关联 PID、状态、地址空间和执行现场。独立内核栈保存各自的函数调用与 Trap frame，共用页表使这些线程能够访问相同的内核代码和数据。创建部分已经建立这些对象，调度部分还需要完成运行对象的选择、SATP 和寄存器上下文切换。

用户地址空间复制、写时复制、缺页分配、页面换出、多核 TLB 同步，以及进程退出后的完整资源回收，仍需后续机制支持。
