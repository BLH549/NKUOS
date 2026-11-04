# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab3：中断、异常与 Trap 上下文恢复 |
| 小组成员 | 匡航逸 2412235、柯云超 2413575、刘华彬 2411238 |
| A 部分完成日期 | 2026-11-04 |
| 代码位置 | `../code/`，`lab3` 分支 |
| 运行与验证入口 | `make`、`make qemu`、`make check-a` |

实验分工见 [Lab3-任务分工.md](资料/Lab3-任务分工.md)。本次完成模块 A：导入 Lab3 框架，迁移物理内存初始化和 Best-Fit，实现 `RESTORE_ALL`、`__alltraps`，并验证 Trap 进入、C 函数调用和返回过程。

## 一、实验目的与整体逻辑

Lab2 已经建立高地址内核映射，并完成物理页的分配与回收。Lab3 在此基础上处理时钟中断和指令异常：内核暂时离开当前执行位置，完成对应处理，再继续执行原来的代码。这个过程需要保存寄存器、记录返回地址，并在处理结束后恢复。

一次 Trap 包含硬件和软件两部分工作。硬件记录 `sepc`、`scause`、`stval`，调整 `sstatus`，并跳转到 `stvec` 指定的入口；汇编代码随后将通用寄存器和这些 CSR 保存到当前内核栈，形成 Trap frame。C 函数通过 `struct trapframe *` 读取和修改这份现场，返回后由汇编恢复寄存器并执行 `sret`。因此，保存布局、C 结构体和恢复代码必须逐项对应。

```text
OpenSBI → kern_entry：建立 Sv39 映射、设置内核栈
  → kern_init：清零 BSS、解析 DTB、初始化控制台和物理内存
  → idt_init：清零 sscratch，将 stvec 指向 __alltraps
  → clock_init / intr_enable：设置时钟事件并开启 S 模式中断

Trap → __alltraps → SAVE_ALL
  → a0 = sp → trap(tf) → trap_dispatch(tf)
  → interrupt_handler(tf) / exception_handler(tf)
  → __trapret → RESTORE_ALL → sret → sepc 指定的位置
```

## 二、实验环境与 Lab2 衔接

| 项目 | 本次环境 |
| --- | --- |
| 宿主机 | Windows，WSL2 Ubuntu 24.04.4 LTS，x86-64 |
| AI 工具 | Codex，GPT-6 |
| 交叉 GCC | `riscv64-unknown-elf-gcc` 13.2.0 |
| ISA / ABI | `rv64imac_zicsr_zifencei` / `lp64` |
| QEMU / OpenSBI | 8.2.2 / 1.3 |
| GDB / Make | `gdb-multiarch` 15.1 / GNU Make 4.3 |
| 内存与地址 | 128 MiB RAM；内核 PA `0x80200000`，VA `0xffffffffc0200000` |

起始代码来自[课程代码目录](http://oslab.mobisys.top/lab2026/)中 `lab3.zip` 的 `lab3_new/`。压缩包 SHA-256 和 Lab2 迁移来源见 [framework.json](evidence/A-20261104/framework.json)，工具版本见 [environment.json](evidence/A-20261104/environment.json)。

Lab3 框架保留了两处 Lab2 填空：`pmm.c` 中的 `page_init()`，以及 `best_fit_pmm.c` 中的分配器实现。迁移内容包括 DTB 内存边界检查、Page 数组初始化，以及 Best-Fit 的查找、拆分和相邻块合并。默认页管理器为 Best-Fit，`basic_check()` 和 `best_fit_check()` 沿用教师框架的原文。

与 Lab2 相比，Lab3 在 `alloc_pages()`、`free_pages()` 和 `nr_free_pages()` 外层增加了中断保护。迁移分配器时保留这层接口：进入操作前保存并关闭中断，结束后恢复原来的中断状态。这样，页分配器仍使用原来的数据结构，同时适应开启时钟中断后的执行环境。

启动部分沿用 Lab2 的临时恒等映射，保证写入 `satp` 后仍能从低地址取指；跳到高地址后删除临时项并刷新 TLB。`kern_entry` 放入链接脚本优先收集的 `.text.kern_entry`，使内核入口位于裸镜像开头。QEMU 启动与评分命令使用 `-kernel`，将镜像及入口交给 OpenSBI。

## 三、Trap 入口与上下文恢复

### 3.1 保存布局决定恢复顺序

`SAVE_ALL` 按寄存器编号保存 x0～x31，再保存四个 CSR；`trap.h` 中的结构体使用相同顺序。RV64 下 `uintptr_t` 占 8 字节，整份 Trap frame 的大小为 `36 × 8 = 288` 字节。288 是 16 的整数倍，因此在原栈指针已经对齐的条件下，建立帧后的栈仍符合 C 调用约定。

| 字段 | 槽号 | 字节偏移 | 恢复用途 |
| --- | --- | --- | --- |
| x0 | 0 | 0 | 保存零值，x0 始终为零 |
| x1 / ra | 1 | 8 | 恢复被打断时的返回地址 |
| x2 / sp | 2 | 16 | 最后恢复原栈指针 |
| x3～x31 | 3～31 | 24～248 | 恢复其余通用寄存器 |
| status / sstatus | 32 | 256 | 恢复 Trap 返回状态 |
| epc / sepc | 33 | 264 | 恢复返回地址 |
| badvaddr / stval | 34 | 272 | 记录异常相关地址或信息 |
| cause / scause | 35 | 280 | 记录 Trap 原因 |

sp 的保存需要单独处理。`SAVE_ALL` 先执行 `csrw sscratch, sp`，再为 Trap frame 分配空间，最后将 `sscratch` 中的旧栈指针写入第 2 槽。其余加载都以当前 sp 为帧基址，因此恢复 x2 必须放在最后；如果提前恢复，后续偏移就会落到原栈指针附近，读到错误的数据。

### 3.2 RESTORE_ALL

`RESTORE_ALL` 先用 t0、t1 读取帧内的 `status` 和 `epc`，分别写入 `sstatus` 和 `sepc`。接着从对应槽位恢复 x1、x3～x31，最后执行 `LOAD x2, 2*REGBYTES(sp)`。读取 CSR 保存值时用到的 t0、t1，也在通用寄存器恢复阶段取回原值。

`badvaddr` 和 `cause` 用于分类与诊断，返回过程使用通用寄存器、`sstatus` 和 `sepc`。x0 始终为零，无需加载；`sscratch` 在 `SAVE_ALL` 中已被清零，返回时保持这一状态。

Trap 的寄存器保护范围比普通函数调用更大。中断可以在任意指令之间发生，被打断的代码没有机会提前保存 caller-saved 寄存器。因此，a、t、s、ra、gp、tp 和 sp 都需要恢复，才能使原代码继续使用中断前的计算结果。

### 3.3 __alltraps 与 C 调用

`SAVE_ALL` 执行后，sp 指向 Trap frame 的首地址。按照 RISC-V 调用约定，`mv a0, sp` 将这个地址作为第一个参数，`jal trap` 调用 C 处理函数。原 a0 和 ra 已经保存在帧内，因此这次调用对两个寄存器的改写会在返回阶段恢复。

C 函数返回时撤销自己的栈空间，sp 重新指向 Trap frame，控制流顺序进入 `__trapret`。`RESTORE_ALL` 从帧内读取处理后的状态，因此 `tf->epc` 的修改会直接决定写入 `sepc` 的值，进而决定实际返回位置。

### 3.4 sret 与实际返回模式

本次内核和验证样例都运行在 S 模式。Trap 发生后，保存的 `sstatus.SPP=1`；执行 `sret` 时，处理器据此返回 S 模式，同时令 `SIE=SPIE`、`SPIE=1`、`SPP=0`，并把 `sepc` 送入 PC。

四次异常和三次时钟中断返回后的 `$priv` 均为 1，对应 S 模式。四次异常在开启全局中断前触发，返回后 SIE 为 0；时钟中断在开启全局中断后触发，返回后 SIE 为 1。实测结果与保存帧中的 SPP、SPIE 对应。

## 四、spec、提示词与实现过程

spec 首先确定输入状态：进入恢复宏时，sp 指向一份由 `SAVE_ALL` 建立的 Trap frame。接着规定输出状态：通用寄存器恢复为帧内值，`status`、`epc` 分别写入 `sstatus`、`sepc`，原 sp 最后恢复。入口部分则规定通过 a0 传递帧地址，并保持调用时的栈对齐。完整约定见 [A-spec.md](资料/A-spec.md)。

提示词沿用 Lab2 的组织方式，将已有代码、实现约定和验证方法分开描述。`SAVE_ALL` 和 `struct trapframe` 提供布局依据，sp 的特殊处理决定恢复顺序，`trap()` 的签名决定参数传递方式。这样，代码生成可以根据现有接口推导实现。输入与结构化提示词见 [prompt.md](prompt.md)。

初次迁移后，编译器报告 `PHYSICAL_MEMORY_END` 和 `KERNEL_BEGIN_PADDR` 未定义。两个常量由 Lab2 的 `page_init()` 使用，而 Lab3 的头文件只保留了虚拟地址边界和映射偏移。分别用 `KERNTOP - PHYSICAL_MEMORY_OFFSET` 和 `KERNBASE - PHYSICAL_MEMORY_OFFSET` 定义对应物理地址后，编译和物理页自检通过。

新增汇编测试文件后，QEMU 打印 OpenSBI 信息，但始终没有命中 `kern_init`。反汇编显示，测试函数排在了 `0xffffffffc0200000`，内核入口被放到后面。裸镜像加载到 `0x80200000` 后直接从开头执行，因此首先进入了测试函数。将 `kern_entry` 放入链接脚本优先收集的 `.text.kern_entry` 后，入口位置固定下来，启动断点正常命中。

GDB 脚本最初在复位阶段读取高地址代码，此时页表尚未启用，读取失败。调整为先运行到 `kern_init`，再反汇编返回路径。另一次调试中，`stepi` 执行 `sret` 后停在下一次 Trap 入口；为直接观察返回位置，改在帧内 `epc` 对应地址设置临时硬件断点，到达目标后检查 PC、全部整数寄存器和特权级。

## 五、验证方法与结果

### 5.1 基线启动

正式源码编译后，通过 QEMU 启动。内核解析到 `[0x80000000, 0x88000000)` 的 RAM，选用 `best_fit_pmm_manager`，输出 `check_alloc_page() succeeded!`，随后完成时钟初始化。记录见 [构建日志](evidence/A-20261104/build.log)和[启动日志](evidence/A-20261104/baseline-boot.log)。

### 5.2 实际 Trap 与完整恢复

[check_a.py](../code/tools/check_a.py)在临时副本中加入汇编触发样例和最小处理桩。样例先设置通用寄存器，再触发四种指令异常；异常桩根据指令编码更新 `epc`。随后开启时钟中断，由时钟桩重设下一事件并递增 `ticks`，检查连续三次中断返回。正式源码中的 B/C 处理函数继续保留待填位置。

GDB 依次停在 `__alltraps`、`trap` 第一条指令、`__trapret`、`sret` 和实际返回目标。每次验证都比较入口快照与帧内的 32 个寄存器槽，检查 `a0=sp=旧sp-288`、16 字节栈对齐、四个 CSR 保存值，并在返回目标再次比较全部整数寄存器。

| 触发样例 | scause | 原 epc | 返回地址 | 结果 |
| --- | --- | --- | --- | --- |
| 16 位断点 `0x9002` | `0x3` | `0xffffffffc02000f0` | `0xffffffffc02000f2` | 通过 |
| 32 位断点 `0x00100073` | `0x3` | `0xffffffffc02000f4` | `0xffffffffc02000f8` | 通过 |
| 16 位非法指令 `0x0000` | `0x2` | `0xffffffffc02000fa` | `0xffffffffc02000fc` | 通过 |
| 32 位非法指令 `0x0000007b` | `0x2` | `0xffffffffc02000fe` | `0xffffffffc0200102` | 通过 |
| 第一次时钟中断 | `0x8000000000000005` | `0xffffffffc02008f8` | 原 epc | 通过 |
| 第二、三次时钟中断 | `0x8000000000000005` | `0xffffffffc0200184` | 原 epc | 通过 |

第一份异常帧地址为 `0xffffffffc0205e50`，原栈指针为 `0xffffffffc0205f70`，差值为 `0x120=288`。进入 C 函数时，a0 和 sp 都等于帧地址；在 `__trapret` 处，sp 再次等于该地址。七次返回的全部整数寄存器比较均通过，最终执行到触发样例之后的标记，`ticks=3`，QEMU 正常关机。

![GDB 布局与首次返回日志节选](images/A-01-frame-and-return.png)

![七次 Trap 的逐项检查结果](images/A-02-trap-results.png)

完整记录见 [GDB 日志](evidence/A-20261104/trap-gdb.log)、[逐次结果](evidence/A-20261104/trap-summary.json)、[测试镜像反汇编](evidence/A-20261104/fixture-disassembly.log)和[串口日志](evidence/A-20261104/fixture-qemu.log)。两张图展示对应日志的节选。

### 5.3 复现

在仓库的 WSL 终端执行：

```bash
cd code
make
make check-a
```

脚本重新构建临时副本、启动 QEMU、连接 GDB，并将结果写入 `report/evidence/A-20261104/`。手动断点步骤与处理函数接入位置见[运行说明](资料/运行说明.md)。教师 `make grade` 的整体验证在 B/C 处理函数完成后进行，检查周期输出、关机阈值和异常输出。

## 六、与 OS 原理的联系

| 知识点 | 本次实现与原理的联系 |
| --- | --- |
| 现场保存与恢复 | 寄存器和返回状态保存在内核栈上的 Trap frame 中，处理代码可以使用寄存器，结束后再取回原值。 |
| 中断与异常 | 两者共用入口与恢复路径，`scause` 决定分发方向。中断返回保持原 epc，测试异常通过修改帧内 epc 跳过触发指令。 |
| 特权级与返回指令 | SPP 决定返回特权级，SPIE 保存进入 Trap 前的中断使能状态；实测路径由 S 模式返回 S 模式。 |
| 调用约定 | a0 传递帧地址，sp 保持对齐。调用时覆盖的参数寄存器和 ra 在恢复阶段取回原值。 |
| 临界区 | Lab3 页分配接口保存并关闭中断，在操作完成后恢复原状态，避免单核上处理过程打断空闲链表操作。 |

Trap 现场恢复为进程切换提供了基础。本次七次测试都返回同一个内核执行流；进程调度还需要选择下一个进程，切换它的内核栈和地址空间。用户态 Trap 则涉及用户栈与内核栈切换、用户地址检查和系统调用接口。这些内容将在后续实验中展开。

本次验证将保存、调用和恢复放在同一次 Trap 中检查：入口快照用于核对保存帧，C 入口用于核对参数和栈，返回目标用于核对恢复结果。这样既能定位槽位或寄存器错误，也能检查修改后的 `epc` 是否真正成为执行地址。
