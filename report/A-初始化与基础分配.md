# A：物理内存初始化与基础分配

负责人：匡航逸，2412235。独立模块完成日期：2026-10-23（北京时间）。使用工具：Codex，底层模型 GPT-6。实现前约定见 [A-spec.md](A-spec.md)，实际提示词和迭代见 [prompt.md](prompt.md#记录-5完成-a-部分)。

## 1. 完成范围与实验主线

完成 `page_init()`、物理内存范围 Challenge、First-Fit 原理分析、Best-Fit 实现与接入，以及两种分配器的运行验证。默认使用 Best-Fit，通过 `make PMM=first_fit` 切换 First-Fit。

此完成状态指 A 的独立实现、报告与自动验证。A 另有全组代码整合和最终回归责任，须待 B/C 模块就绪后继续；负责人亲自复现、完整流程演示及答辩准备尚未经本人确认，见 [分工清单第 3.6 节](Lab2-任务分工.md#36-a-的后续公共责任与本人验收)。

```text
OpenSBI 将 hartid/DTB 地址传给入口
  → entry.S 建立 Sv39 映射并进入高地址
  → kern_init 清零 BSS，dtb_init 解析 RAM
  → pmm_init 选择管理器，初始化其空闲链表
  → page_init 规划描述符及保留页，注册可用完整页
  → 当前管理器 check 验证分配、拆分、释放和合并
```

需要先建立可靠的页描述符和空闲页范围，后续 Buddy、SLUB 和地址翻译调试才有可运行基线。没有修改 `struct Page`、`pmm_manager` 接口或 Best-Fit 两个原始检查函数。

## 2. page_init 的地址与边界处理

实现位于 [pmm.c](../code/kern/mm/pmm.c)。设备树解析先于 `pmm_init()`，因此从 `get_memory_base()` 和 `get_memory_size()` 获取输入，而不是回退到固定的 128 MiB。

本框架固定 `nbase=DRAM_BASE/PGSIZE`，启动映射和内核链接位置也以 `0x80000000` 为物理基准。因此要求 DTB 起点等于 `DRAM_BASE`，大小非零，且起点与大小相加不溢出。无效时 panic。该校验针对已解析的内存区间；没有将起始 DTB 解析器扩展为任意不可信 DTB 的完整验证器。

设置 `va_pa_offset=0xffffffff40000000`。管理终点为 `min(DTB_BASE+DTB_SIZE, PHYSICAL_MEMORY_END)`，再向下对齐到 4 KiB。这样既不会超出实际 RAM，也不会超出框架的管理上界。映射覆盖的地址范围与实际存在、可分配的 RAM 是不同概念。

`npage` 表示绝对物理页号的上界，不能直接用作可用页数：

```text
nbase = 0x80000000 / 4096 = 524288
npage = 0x88000000 / 4096 = 557056       （默认 128 MiB）
描述符个数 = npage - nbase = 32768
pages[i] ↔ 物理页号 nbase+i ↔ 物理地址 (nbase+i)*4096
```

`pages` 从内核 `end` 向上按页对齐的虚拟地址开始。先检查整个数组能否放入可管理 RAM，再写入；用减法检查剩余容量，避免先计算超范围数组末地址。当前 RV64 `struct Page` 大小为 40 字节，默认数组占 `32768*40=0x140000` 字节。

所有描述符先清零，设置 `PG_reserved`，初始化链表指针。内核镜像包含启动栈和启动页表。RAM 开头的固件区域、内核、内核末尾的对齐间隙，以及描述符数组和末页对齐间隙均保留。只有数组末尾向上对齐后到管理终点之间的完整页面交给 `init_memmap()`。

没有剩余完整页面时，`page_init()` 不调用 `init_memmap()`。如果连描述符都放不下则终止初始化。框架后续分配自检需要足够空闲页；“初始化零空闲页时不错误注册”不代表小内存仍能通过该自检。

默认配置真实输出：

```text
memory management: best_fit_pmm_manager
physical memory: [0x0000000080000000, 0x0000000088000000)
managed memory:  [0x0000000080000000, 0x0000000088000000)
page descriptors: [0x0000000080207000, 0x0000000080347000), 32768 entries
free memory:     [0x0000000080347000, 0x0000000088000000), 31929 pages
check_alloc_page() succeeded!
```

日志使用左闭右开区间。描述符数组虚拟地址为 `0xffffffffc0207000`，物理地址为 `0x80207000`。31929 是空闲页数，32768 是描述符个数，557056 才是 `npage`。

## 3. First-Fit 数据结构与过程

[default_pmm.c](../code/kern/mm/default_pmm.c) 维护 `free_area`：双向循环链表 `free_list` 保存每个空闲块的首页，`nr_free` 保存总空闲页数。链表按地址递增，不是按块大小排列。

`Page.ref` 是页引用计数；`PG_reserved` 表示该页不能作为一般空闲页；`PG_property` 表示该描述符是空闲块首页，此时 `property` 才表示块大小。内部页面没有首页标志，不能通过可能残留的 `property` 数值判断页面是否空闲。

各函数的行为为：

1. `default_init()` 建立空循环链表，空闲页计数清零。
2. `default_init_memmap(base,n)` 要求输入是保留页，将其转换为空闲页，清零引用计数，设置块首页和长度，按地址插入并增加计数。原始实现不合并多次注册的相邻初始区间；本次 `page_init()` 只注册一段。
3. `default_alloc_pages(n)` 从链表开头查找第一个长度至少为 n 的块。找不到则返回 NULL；找到后取其前 n 页，剩余部分作为新空闲块放回原位置，减少计数并清除已分配首页的空闲标志。
4. `default_free_pages(base,n)` 清理页面状态，按地址插入回收块，先检查前一块是否紧邻，再检查后一块是否紧邻。合并后只保留最前面的首页标志。回收增加 n 页，合并本身不额外改变计数。

例如 `[100,108)` 空闲块申请 3 页，返回 `[100,103)`，链表留下 `[103,108)`。回收后，两块相邻，恢复为 8 页。地址有序使合并只需检查链表邻居。

本次修复原始代码的两个尾部插入分支：插入后缺少 `break`，下一轮会再次访问刚插入的节点并重复插入，破坏链表、导致卡死。只补退出条件，保留 `basic_check()` 和 `default_check()` 原文。修复后的 First-Fit 在 QEMU 128 MiB 下通过原始检查，空闲页数为 31929。

### 改进空间

First-Fit 最坏需扫描 F 个空闲块，复杂度 O(F)。可按大小分桶或增加索引跳过明显过小的块，或用 Next-Fit 游标减少反复扫描头部，但会改变选择策略和碎片分布。批量回收和合理合并可减少链表节点；并发场景还需要锁或每 CPU 缓存。应比较搜索成本、管理开销和外部碎片，而不是只比较算法名称。

## 4. Best-Fit 实现与接入

完整模块在 [best_fit_pmm.c](../code/kern/mm/best_fit_pmm.c)，通过已有头文件导出 `best_fit_pmm_manager`。注册结构实现初始化、注册内存、分配、释放、空闲计数和检查六个接口。

仍使用地址有序链表，不改变相邻合并规则。`best_fit_insert_block()` 将块插入合适位置，先合并前邻，再合并后邻，清除被吸收块的首页标志。初始化注册和释放复用此过程，支持乱序注册、相邻注册及乱序回收。

分配时记录当前最小的满足请求的块，继续扫描到链表末尾；精确匹配可以提前结束。相同大小保留先遇到的低地址块。需要拆分时，将余块插入原节点位置，再移除已分配块；返回恰好 n 页，计数减少 n。找不到连续块时返回 NULL，不改变链表和计数。

例如低地址有 8 页、高地址有 3 页，申请 2 页时 First-Fit 选择 8 页块，而 Best-Fit 选择 3 页块并留下 1 页。Best-Fit 可以减少本次选择留下的剩余块大小，但也可能制造较多小碎片，不保证长期外部碎片最少。

`pmm.c` 默认选择 Best-Fit，定义 `PMM_FIRST_FIT` 时选 First-Fit。Makefile 提供 `PMM=first_fit|best_fit` 并拒绝未知配置。配置标记同时依赖到 `pmm.o`、ELF 和裸镜像，配置变化时还给完整输出链添加 phony 强制依赖，确保切换会重编译并更新实际启动的镜像，处理 GNU Make 3.81 在同一秒内切换时的时间戳精度问题。

### 改进空间

当前最坏查找仍为 O(F)，释放除重置 n 个描述符外，插入查找为 O(F)，相邻合并为 O(1)。可同时维护按大小的平衡树和按地址的索引，将最小适配查询和邻接查询降到 O(log F)，代价是更复杂的一致性维护。大小分桶可近似 Best-Fit，减少搜索成本。为小剩余块设置拆分阈值可以控制碎片，但会改变“恰好 n 页”的占用语义，须与释放接口一起设计，不能直接套用当前接口。

原始 `basic_check()` 和 `best_fit_check()` 文本保持不变，脚本逐字比较原分支与当前文件的测试区并记录 SHA-256，见 [summary.json](evidence/A-20261023/summary.json)。

## 5. Challenge：不知道可用物理内存范围时如何获取

操作系统通常从固件或引导器传入的硬件描述获取区域及类型，再扣除保留和自身占用。常见机制包括：

| 机制 | 来源与内容 | OS 需要做的事 |
| --- | --- | --- |
| 设备树 | memory 节点的 `reg` 描述 RAM 地址和长度 | 依照 address/size cells 解析；扣除保留区域 |
| UEFI `GetMemoryMap()` | 固件返回带类型、地址和页数的内存描述符 | 引导器取得并传递内存图，区分可回收与运行时区域 |
| x86 BIOS E820 | BIOS 分次返回地址范围、长度与类型 | 引导阶段查询，保留类型信息供内核整理 |
| 板级引导器/平台内存控制器 | 已知板卡的 RAM 配置或启动参数 | 遵守平台约定，不能假设可移植到其他硬件 |

依据见 [Devicetree memory 节点规范](https://devicetree-specification.readthedocs.io/en/stable/devicenodes.html)、[UEFI 内存映射接口](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html)、[ACPI 对 E820 的定义](https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/15_System_Address_Map_Interfaces/int-15h-e820h---query-system-address-map.html)。RAM 节点描述物理内存布局；类型和保留信息决定哪些区域能用于分配。

本实验中，QEMU 根据虚拟机配置生成 DTB，经 OpenSBI 将 DTB 物理地址放入 a1、hartid 放入 a0，入口汇编将它们保存到 `.data` 的 `boot_dtb`、`boot_hartid`，避免被 BSS 清零。此寄存器传递约定也见 [RISC-V 内核启动要求](https://www.kernel.org/doc/html/next/riscv/boot.html)。

开启高地址映射后，`dtb_init()` 将物理地址加 `PHYSICAL_MEMORY_OFFSET`，校验 magic，解析大端 FDT 结构和字符串表，查找 memory 节点的 `reg`，保存起点和大小。`page_init()` 随后读取它们，计算管理边界与真正空闲区域。修改 QEMU 的 `-m` 后，不重编译同一 Best-Fit 镜像即可在日志中看到 DTB RAM 大小变化。

物理地址空间包括 RAM、MMIO、固件、设备窗口和没有实际存储的空洞。MMIO 访问可能触发设备行为，空洞可能产生异常；RAM 中也存在固件、内核、设备树及其他保留数据。不能遍历地址尝试写入来判断“可用内存”，更不能把最高地址之前的所有页都交给分配器。

沿用的简化解析器只读取第一个匹配 memory 节点的第一组 64 位地址/长度，未完整处理任意 `#address-cells`、多段 RAM、FDT reservation block 和 `/reserved-memory`。当前 QEMU 布局中的 OpenSBI 位于保留的前 2 MiB；DTB 在解析完成后不再读取，当前页分配实验允许其存储随后被复用。移植到其他平台或需要长期保留 DTB 时，应解析所有 RAM 和保留区、显式保留 DTB，并逐段注册剩余内存。此限制不影响指定 QEMU 布局的实验结论，但不能据此声称已实现通用硬件内存发现。

## 6. 运行验证与覆盖

实测环境：macOS arm64、GNU Make 3.81、xPack GCC 15.2.0、binutils 2.45、QEMU 9.2.4、默认 OpenSBI 1.5.1。工具下载自 xPack 官方 GitHub 发布，核对 SHA-256，解压在仓库外；工具配置见 [README](../code/README.md)。

### 真实 QEMU 与教师评分

| 配置 | 结果 | 初始空闲页 |
| --- | --- | ---: |
| First-Fit / 128 MiB | `default_check()` 通过 | 31929 |
| Best-Fit / 16 MiB | 原始两个检查通过 | 3537 |
| Best-Fit / 64 MiB | 原始两个检查通过 | 15705 |
| Best-Fit / 128 MiB | 原始两个检查通过 | 31929 |
| Best-Fit / 256 MiB | 通过，管理终点截断为 `0x88000000` | 31929 |
| `make grade` / Best-Fit 默认 128 MiB | Page Init、Best-Fit 均 OK | **20/20** |

证据：[启动检查总日志](evidence/A-20261023/qemu-checks.log)、[First-Fit 启动](evidence/A-20261023/first_fit-128M-boot.log)、[Best-Fit 默认启动](evidence/A-20261023/best_fit-128M-boot.log)、[评分日志](evidence/A-20261023/grade.log)、[评分串口输出](evidence/A-20261023/grade-serial.log)。其余内存配置、源码和镜像校验值见 [summary.json](evidence/A-20261023/summary.json)。

教师 Page Init 测试检查偏移、DTB 信息、npage、数组对齐和位置、空闲页、内核/描述符保留及地址转换；教师 Best-Fit 项检查当前注册的管理器。真正的选择、拆分和合并行为还由原始框架检查和独立测试覆盖。教师 npage 断言未考虑上界截断，因此评分在默认 128 MiB 下运行，256 MiB 截断用独立启动检查验证。

内核成功后无限循环是预期行为。脚本观察到检查成功及 satp 日志后终止 QEMU，日志末尾 SIGTERM 是测试程序退出模拟器所致。

### 主机侧边界与模型对照

运行 [check_host.py](../code/tools/check_host.py)，使用 ASan、UBSan 和遇错即终止选项编译真实源文件，不复制分配算法。主机测试不验证 RISC-V 汇编、固件或 MMU，这些由实际启动补足。

- First-Fit、Best-Fit 各执行原始检查及 10,000 次确定性随机操作。独立位图逐次计算应选的连续区间，核对返回地址、计数和保留空洞；最后回收所有分配并验证最大区间恢复。
- 验证乱序内存注册；Best-Fit 还验证相邻初始区间合并。碎片用“两段各 3 页、请求 4 页”验证总数足够时仍会失败且不改计数；随机操作覆盖最小选择、精确分配、拆分和双向回收合并。
- 初始化直接调用真实静态 `page_init()`，用模拟 DTB getters 和映射缓冲区覆盖默认、非整页 RAM、上界截断、没有完整空闲页，以及零大小、错误起点、加法溢出、内核放不下、描述符放不下 9 组输入。异常输入必须出现对应 panic。

实际输出见 [host-checks.log](evidence/A-20261023/host-checks.log)，全部通过。

## 7. 实际问题与修复

| 问题 | 证据与原因 | 修复 |
| --- | --- | --- |
| 本机没有交叉工具链和 QEMU | 命令查找为空，仅有 Apple 编译器 | 使用独立目录的已校验 xPack 包，明确 RV64/ABI，避免依赖默认架构 |
| 主机链接器不能读取默认 SDK | 报告 `arm64e.x1-macos` 未知架构 | 显式选择已安装的 macOS 26.5 SDK |
| First-Fit 检查卡住 | 主机过程停在 First-Fit；尾部插入后继续遍历自身 | 在 init_memmap/free_pages 的尾部插入分支补 `break` |
| UBSan 报空指针成员访问 | 原始 `offsetof` 使用 `&((type*)0)->member` | 改用 `__builtin_offsetof`，不改数据布局 |
| 原 QEMU loader 参数不进入内核 | [初次启动](evidence/A-20261023/initial-loader-boot.log) 的 Next Address 为 0 | 统一 qemu/debug/test/grade 使用 `-kernel bin/ucore.img` |
| 开启分页后取指缺页 | [修复前 GDB](evidence/A-20261023/paging-before-gdb.log)：`scause=12`，`sepc=stval=0x80200038` | 根表索引 2 临时恒等映射物理执行位置，跳到高地址后清零该项并刷新 TLB |
| 快速切换管理器启动旧镜像 | 同一秒内重编 pmm.o 后，旧 Make 未继续更新完整依赖链 | 配置变化时用 phony 依赖强制更新 pmm.o、ELF 和裸镜像；自动脚本验证实际启动名称 |

分页修复后，[GDB 证据](evidence/A-20261023/paging-after-gdb.log) 显示已进入 `pc=0xffffffffc02000ec`，`satp=0x8000000000080205`，根表第 2 项为 0，第 511 项为 `0x200000cf`。临时映射已经删除，最终保留原计划的高地址 1 GiB 叶子映射。该修复服务于 A 的可运行基线，不替代 C 的 QEMU 源码地址翻译/TLB 调试任务。

启动参数也符合 [QEMU virt 平台文档](https://www.qemu.org/docs/master/system/riscv/virt.html) 中使用默认 OpenSBI 和 `-kernel` 加载下一阶段的方式。评分仅调整启动选项复用、退出进程清理和串口日志保留，没有改评分条件。

## 8. 交接与复现

在 `code/` 内配置工具变量后执行：

```sh
make PMM=first_fit qemu
make PMM=best_fit qemu
make grade
make check-host
python3 tools/check_qemu.py --output ../report/evidence/A-recheck
```

本机主机检查需按 README 设置兼容 SDK。调试使用 `make debug` 与另一终端的 `make gdb`。`make grade` 会清理原构建产物，之后 `make` 即可重建默认 Best-Fit。

B 接入 Buddy 时可扩展 Makefile 的 `PMM` 选项与 `init_pmm_manager()`，保持页描述符和 `alloc_pages/free_pages/nr_free_pages` 接口约定。B/C 模块尚未在本次加入，全组代码整合、Buddy/SLUB 回归和最终提交清单继续保留未完成状态。
