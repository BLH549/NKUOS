# C：从一次访存观察 QEMU 的地址翻译与软件 TLB

负责人：刘华彬，2411238。实验日期：2026-10-24。工具：Codex（GPT-6）、宿主机 GDB、RISC-V GDB。代码基线为 `846907658d70cffb55ad6e3ad81605a0a5aa6bb1`。

## 1. 环境与调试版 QEMU

| 项目 | 本轮使用值 |
| --- | --- |
| 宿主机 | WSL2 Ubuntu 22.04，x86-64，Linux 6.6.87.2 |
| 内核交叉编译器 | RISC-V GCC 15.1.0 |
| 内核 ISA / ABI | `rv64imac_zicsr_zifencei` / `lp64` |
| guest 调试器 | `riscv64-unknown-elf-gdb` 16.3.90.20250610-git |
| host 调试器 | Ubuntu GDB 12.1 |
| 基线启动 | 已有 QEMU 7.0.0 / OpenSBI 1.0 |
| 正式源码调试与回归 | QEMU 4.1.1 / 自带 OpenSBI v0.4 |
| QEMU 源码 | `/home/liu/.local/src/qemu-4.1.1` |
| 调试二进制 | `build-debug/riscv64-softmmu/qemu-system-riscv64`，位于上述源码目录内 |

先用已有 7.0.0 启动，看到页分配与 SLUB 检查通过，再停在 `kern_init` 记录寄存器和页表。这一步确认本机构建的内核能运行。之后的源码单步、TLB 和 Bare 对比都使用 4.1.1。

源码包来自 [QEMU 官方下载目录](https://download.qemu.org/)，本次下载包 SHA-256 为：

```text
ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2
```

本机已有 glib 和 zlib 开发文件，但 pixman 只有运行库。第一次 configure 报 `pixman >= 0.21.8 not present`。我们用 `apt download libpixman-1-dev` 下载开发包，`dpkg-deb -x` 解包到仓库外，调整本地 `.pc` 文件的 prefix，让 configure 找到头文件和库。

第一版二进制链接成功，但运行时缺 `libtinfow.so.6`。检查发现当前 shell 的 Conda 环境影响了 pkg-config 的依赖选择。最终把搜索目录限定到系统和本地 pixman，并关闭本次不用的 curses、curl，重新构建：

```bash
export PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_PATH=/home/liu/.local/src/qemu-deps/pkgconfig
cd /home/liu/.local/src/qemu-4.1.1/build-debug
../configure --target-list=riscv64-softmmu --enable-debug \
  --disable-werror --disable-docs --disable-gtk --disable-sdl \
  --disable-vnc --disable-tools --disable-curses --disable-curl \
  --python=/usr/bin/python3
make -j12
```

配置中 `CFLAGS=-g`，没有开启优化，函数局部变量可以直接观察。我们直接使用构建目录中的可执行文件；已有的 QEMU 7.0.0 继续保留。准备步骤收录在 [prepare_qemu_c.sh](../../code/tools/prepare_qemu_c.sh)，初次失败、依赖下载、最终配置和构建分别保存在 [C 证据目录](../evidence/C-20261024/summary.json)。

## 2. 两个 GDB 怎样配合

宿主机 GDB 调试的是一个 x86-64 Linux 进程，也就是 QEMU；RISC-V GDB 通过 QEMU 的 remote stub 调试模拟出来的 CPU。两者暂停的层次不同：guest 已经暂停时，QEMU 还要继续处理调试协议；host 暂停后，guest GDB 的请求也会等待。

因此次序是：host GDB 启动 QEMU并 `run`，QEMU 用 `-S` 暂停 guest，另一个 GDB连接 stub；guest 停在目标指令后，提前给 host 设置条件断点，再在 guest 执行 `si`。命中 host 断点后完成源码单步，host `continue`，guest 的 `si` 才会返回。

本机 `ptrace_scope=1`，采用 GDB 直接启动子进程的方式。手工操作可用下面的终端布局；串口单独写入文件，在第三个终端查看。

```bash
# 终端 1：在 code/ 中启动 host GDB
gdb /home/liu/.local/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64
```

```gdb
# host GDB
handle SIGPIPE nostop noprint pass
set args -machine virt -m 128 -nographic -monitor none -serial file:/tmp/lab2-c-serial.log -L /home/liu/.local/src/qemu-4.1.1/pc-bios -bios default -kernel bin/ucore.img -S -gdb tcp:127.0.0.1:1236
break riscv_cpu_tlb_fill if address == 0xffffffffc0204ff8 && access_type == 1
run
```

```bash
# 终端 2：guest GDB
riscv64-unknown-elf-gdb bin/kernel
```

```gdb
set architecture riscv:rv64
target remote :1236
hbreak *0xffffffffc0200102
continue
info registers pc satp sp
x/i $pc
si
```

```bash
# 终端 3：串口
tail -f /tmp/lab2-c-serial.log
```

这轮把同样的操作次序写进 [debug_c.py](../../code/tools/debug_c.py)，脚本通过管道驱动两个真实 GDB，会分别保存 host、guest 与串口日志。它先核对版本和反汇编中的指令，内核布局发生变化时会停止，避免拿旧地址观察另一条指令。

截图使用 [capture_c.py](../../code/tools/capture_c.py) 将本轮日志按主题显示在 xterm 中，再截取窗口。完整连续输出保存在 [host 日志](../evidence/C-20261024/host-mmu-gdb.log) 和 [guest 日志](../evidence/C-20261024/guest-mmu-gdb.log)。

## 3. 从 guest 指令确定翻译输入

本轮内核根页表位于虚拟地址 `0xffffffffc0205000`。寄存器为：

```text
satp = 0x8000000000080205
MODE = 8，Sv39
ASID = 0
根页表物理地址 = 0x80205 << 12 = 0x80205000
```

进入高地址后，根表第 2 项为 0，第 511 项为 `0x200000cf`。第 2 项原本提供开启分页后的临时恒等映射；跳入高地址后，入口执行 `sd zero,16(t0)` 清除它，再执行 `sfence.vma`。

我们选择 `kern_init` 中的一条栈写入：

```text
PC = 0xffffffffc0200102
指令 = sd ra,8(sp)
sp = 0xffffffffc0204ff0
访存目标 VA = sp + 8 = 0xffffffffc0204ff8
```

指令地址和访存地址在这里不同。条件断点应按 `0xffffffffc0204ff8` 筛选，并指定 store，而不是按 PC 筛选。host 中断在 `riscv_cpu_tlb_fill`，`access_type=MMU_DATA_STORE`、`mmu_idx=1`、`probe=false`，调用栈还能回溯到 `store_helper`、`helper_le_stq_mmu` 和 `code_gen_buffer`。这些信息说明此次翻译来自正在执行的 guest 指令。

GDB 的 `x` 命令本身也会请求地址转换，它使用 `riscv_cpu_get_phys_page_debug()` 调用 `get_physical_address()`。若只给后者设置地址条件，就可能先观察到调试器自己的读取。我们以实际访存的 `riscv_cpu_tlb_fill()` 为入口，再在其内部临时断到翻译函数。

![C 的 guest 指令与 Bare/Sv39 状态](../images/C-01-guest-and-bare.png)

## 4. 单步观察 Sv39 页表翻译

相关实现是 QEMU 4.1.1 的 [target/riscv/cpu_helper.c](https://gitlab.com/qemu-project/qemu/-/blob/v4.1.1/target/riscv/cpu_helper.c)。在 `get_physical_address()` 中读出 `satp` 后，现场得到：

```text
vm       = 8
levels   = 3
ptidxbits= 9
ptesize  = 8
base     = 0x80205000
```

Sv39 的普通虚拟地址可以按 `VPN[2] | VPN[1] | VPN[0] | offset` 理解，分别为 9、9、9、12 位。QEMU 的循环从最高层开始；局部变量 `i=0` 表示第一次循环，即架构中最高的 level 2。此时 `ptshift=(levels-1)*9=18`。

进入循环前还有虚拟地址合法性检查：Sv39 要求位 63～39 都是位 38 的符号扩展。源码用 `va_bits=39` 提取包含符号位的高部，要求它全 0 或全 1。本次高地址满足这一关系；若高部不合法，会在读取任何 PTE 前返回翻译失败。

### 4.1 从虚拟地址找到 PTE

源码计算：

```c
idx = (addr >> (12 + ptshift)) & ((1 << 9) - 1);
pte_addr = base + idx * 8;
```

代入本次地址：

```text
idx = (0xffffffffc0204ff8 >> 30) & 0x1ff = 511
pte_addr = 0x80205000 + 511 * 8 = 0x80205ff8
```

读取前 QEMU 检查 PTE 地址的 PMP 权限，再通过 `ldq_phys()` 按物理地址读出 8 字节，现场值为：

```text
pte = 0x200000cf
ppn = pte >> 10 = 0x80000
```

页表保存的 PPN 指向 guest 物理内存；读取 PTE 本身不会再按当前 Sv39 页表递归翻译。

### 4.2 判断叶子、权限和对齐

`0xcf` 表示 V/R/W/X/A/D 都为 1，U/G 为 0。有效位为 1，R/W/X 不全为 0，因此它已经是叶子项；本次在根层结束遍历。若 R/W/X 全为 0，才将 `base=ppn<<12`，取下一组 VPN 继续。Sv39 支持三级页表，不等于每次访问都走满三级。

接下来源码检查叶子标志组合、U/S 权限、超级页对齐及本次访问所需的 W 权限。根层叶子要求低 18 位 PPN 为零；`0x80000` 满足这个对齐关系，对应 1 GiB 映射。当前为 S 模式写入 supervisor 页，W 为 1，权限检查通过。A/D 已经置位，所以本次不用写回 PTE。

### 4.3 算出物理地址

大页映射保留虚拟地址的低 30 位作为页内偏移：

```text
叶子物理基址 = 0x80000 << 12 = 0x80000000
1 GiB 内偏移 = VA & 0x3fffffff = 0x204ff8
最终 PA       = 0x80000000 + 0x204ff8 = 0x80204ff8
```

QEMU 在函数中先形成 4 KiB 对齐的物理页地址：

```c
vpn = addr >> 12;
*physical = (ppn | (vpn & ((1L << ptshift) - 1))) << 12;
```

单步后看到 `*physical=0x80204000`；加上本次页内偏移 `addr&0xfff=0xff8`，才是 `0x80204ff8`。这个中间值让我意识到，调试中看到的 `pa` 不一定已经包含完整访存偏移，要沿着后面的缓存和访问代码继续看。

返回时 `ret=0`、`prot=7`，随后 `tlb_set_page()` 以 `vaddr=0xffffffffc0204000`、`paddr=0x80204000`、`size=4096` 填充软件 TLB。本次架构页表使用 1 GiB 叶子，而这版 RISC-V 后端按 4 KiB 粒度填软件缓存。

![根层叶子与地址计算](../images/C-02-sv39-walk.png)

## 5. 软件 TLB 的查找、填充和命中

### 5.1 冷访问经过哪里

另选 `dtb_init()` 中的读取：

```text
PC = 0xffffffffc0200274
指令 = ld a1,0(s0)
s0 = 0xffffffffc0206008，即 boot_dtb 的存储地址
```

这里加载到的内容是 DTB 物理地址 `0x82200000`。要区分“变量 boot_dtb 所在地址”和“变量里保存的 DTB 地址”。

为了控制冷访问，先让 PC 临时指向现有 `0xffffffffc020004e` 的 `sfence.vma`，执行一条后立刻恢复到 load 的 PC。没有执行后面的设置栈指令。随后执行该 load，host 中断在真实的读访问填充入口。

在 [accel/tcg/cputlb.c](https://gitlab.com/qemu-project/qemu/-/blob/v4.1.1/accel/tcg/cputlb.c) 的 `load_helper()` 栈帧里看到：

```text
index = 6
tlb_addr = 0xffffffffffffffff
entry->addr_read = 0xffffffffffffffff
```

这是失效标签。C 代码在 `!tlb_hit(tlb_addr,addr)` 时检查 victim TLB；两处均未命中，调用 `tlb_fill()`，由 CPU 类的方法进入 `riscv_cpu_tlb_fill()`，再调用前面的页表翻译函数。实际调用链为：

```text
TCG 生成的 guest load 对应代码
  → helper_le_ldq_mmu
  → load_helper：主 TLB / victim TLB 检查
  → tlb_fill
  → riscv_cpu_tlb_fill
  → get_physical_address
  → tlb_set_page
  → tlb_set_page_with_attrs
```

填充函数不只保存 guest PA，还要把 guest 物理页放进 QEMU 的 `MemoryRegion` 地址空间，区分 RAM、ROM 和 MMIO。本次是 RAM，最终缓存为：

```text
addr_read  = 0xffffffffc0206000
addr_write = 0xffffffffc0206000
addr_code  = 0xffffffffc0206000
addend     = 0x80001fe00000
```

由本次进程的缓存偏移得到 host 地址：

```text
(uintptr_t)0xffffffffc0206008 + 0x80001fe00000
  = 0x7fffe0006008             （按 64 位无符号运算）
该 host 地址中的 8 字节内容 = 0x82200000
```

guest 的 `a1` 同样变为 `0x82200000`。这把 guest VA、guest PA、host 指针和实际加载值串在了一起。host 指针受进程内存布局影响，每次运行可以不同。

### 5.2 重复执行为什么绕过遍历

第一遍 load 返回后，把 PC 恢复为 `0xffffffffc0200274`，不刷新，第二次执行同一条 `ld`。两次 `a1` 都是 `0x82200000`。host 给目标地址的 fill 断点加了计数，第一次记为 1，若再次进入便递增；第二次 load 返回后，计数仍为 1，标签仍是 `0xffffffffc0206000`。

还需要解释一个现象：第二遍连 `load_helper()` 的慢路径都可以绕过。x86-64 的 TCG 后端在 [tcg/i386/tcg-target.inc.c](https://gitlab.com/qemu-project/qemu/-/blob/v4.1.1/tcg/i386/tcg-target.inc.c) 的 `tcg_out_tlb_load()` 中生成内联检查。我们在第一遍的调用栈中找到当前 TranslationBlock，反汇编它的 `tc.ptr`，看到：

```asm
shr $0x7,%rdi                 # 与 TLB 表项大小结合，形成索引
and -0x40(%rbp),%rdi         # 当前 MMU 模式的表掩码
add -0x38(%rbp),%rdi         # TLB 表基址
lea 0x7(%rbx),%rsi
and $0xfffffffffffff000,%rsi  # 含访问长度/跨页条件的标签
cmp (%rdi),%rsi              # 比较 addr_read
jne slow_path
add 0x18(%rdi),%rsi          # 加上 CPUTLBEntry.addend
mov (%rsi),%rbx              # 直接读取 host RAM
```

`0x18` 是这个构建中 `addend` 的偏移，32 字节表项使索引计算中的移位为 `12-5=7`。命中就加偏移并读 RAM；未命中才跳到调用 helper 的代码。日志中，`code_gen_buffer+258` 是 `jne`，`+264` 是加 addend，`+268` 是实际 host load，慢路径在 `+312`。这个分支结构与第二遍没有新增 fill 的现象一致。

反汇编只分析该 TB 的指令区。后面紧邻的常量和其他缓冲区内容不能继续当成这条 load 的指令解释。

![冷访问标签与重复访问](../images/C-03-tlb-cold-and-hot.png)

![实际生成的 TLB 快路径](../images/C-04-tcg-fast-path.png)

## 6. Bare 模式为什么也有软件 TLB

开启分页前，在 `PC=0x80200008` 执行 `sd a0,0(t0)`，现场 `t0=0x80206000`、`satp=0`。host 中的 `env->priv=1`，确认为 S 模式。

它与 Sv39 的 store 一样，从生成代码经 `store_helper` 和 `tlb_fill` 进入 RISC-V 后端。但 `get_physical_address()` 读出 `vm=0` 后直接执行：

```c
*physical = addr;
*prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
return TRANSLATE_SUCCESS;
```

所以本次 PA 为 `0x80206000`，没有读取页表。返回之后仍执行 PMP 检查和 `tlb_set_page_with_attrs()`；现场 `index=6`，三个标签均为 `0x80206000`，`addend=0x7fff5fe00000`，相加后指向 host 的 `0x7fffe0006000`。

| 比较项 | S 模式 Bare 写入 | S 模式 Sv39 写入 |
| --- | --- | --- |
| PC | `0x80200008` | `0xffffffffc0200102` |
| 输入地址 | `0x80206000` | `0xffffffffc0204ff8` |
| `satp.MODE` | 0 | 8 |
| 后端行为 | 直接令 PA=地址 | 从根表读取叶子项 |
| 最终物理地址 | `0x80206000` | `0x80204ff8` |
| 软件 TLB | 填充直接访问缓存 | 填充翻译后的访问缓存 |

真实 CPU 的 TLB 逻辑上缓存虚拟到物理的翻译与权限。QEMU 的软件 TLB 还承担“怎样访问宿主机缓冲区或设备”的工作，所以 Bare 模式下也有缓存价值。它有读/写/取指标签、addend、MMIO 和脏页等标志，服务于模拟器执行；它的冲突、容量、victim TLB 和 4 KiB 填充粒度不应当拿来测量某款真实 RISC-V CPU 的微架构性能。

## 7. 调试中实际遇到的问题

**缺源码与依赖。** 已有 7.0.0 能解析符号，却找不到原构建目录中的源码。正式调试改为下载并构建 4.1.1，源码、二进制和 GDB 行号来自同一目录。pixman 开发包和 Conda 的 curses 依赖问题按前述方式解决。

**汇编符号没有完整类型。** 初次 guest GDB 命令写成 `&boot_page_table_sv39+16`，报 `Cannot perform pointer math on incomplete type`。它是汇编导出的符号，GDB 没有数组类型；改成 `(char *)&boot_page_table_sv39+16` 后，按字节定位第 2 项。第 511 项同样加 `4088`。

**第一轮驱动启动到了尚未重新链接的二进制。** 修复依赖后，构建仍在后台进行，调试脚本提前启动了旧二进制，guest 连接超时。早期脚本还继续执行，产生了 `No registers`，让错误位置变得不清楚。等待构建退出后重启，并让驱动在连接超时、没有寄存器等错误出现时立即停止，避免把后续空操作误判成已经命中 MMU。

**中断后停在宿主机的 ppoll。** guest 暂停后，host 常在事件循环等请求。向 host GDB 发送 SIGINT，停在 glibc 的 `ppoll.c`，提示本机没有该源文件。早期脚本把所有 `No such file or directory` 都当成失败，于是中止了已经成功完成的页表单步。后来将连接和寄存器错误单独识别，允许这种宿主机库源码提示。切回 vCPU 的调用栈或使用保存的 `CPURISCVState *`，仍然能检查模拟 CPU 状态。

**看到 physical 少了偏移。** 栈写入的翻译函数返回 `0x80204000`，而手算是 `0x80204ff8`。沿源码看到它先返回 4 KiB 对齐物理页、由后续访问补回偏移，就能解释两者关系。报告同时记录中间值与最终值。

这轮设置了 `handle SIGPIPE nostop noprint pass`；真正用于切换观察层次的中断是脚本发出的 SIGINT。早期输出保存在 `attempt-01/`、`attempt-02/`，后两份 `attempt-03/`、`attempt-04/` 是逐步补充观察内容时的成功运行记录。

## 8. 回归与复现

完整双层调试最后继续到 `kern_init` 的等待循环，串口中 Best-Fit、页分配检查与 SLUB 均通过。随后使用同一个 4.1.1 二进制重新运行：

- First-Fit + SLUB，128 MiB；Best-Fit + SLUB，128 MiB。
- Buddy + SLUB，64、128、256 MiB；256 MiB 中管理上界仍截到 `0x88000000`。
- A 的主机分配模型与初始化边界检查、B 的 Buddy/SLUB 独立模型及 ASan/UBSan。
- 教师评分 `20/20`，之后重建默认 Best-Fit。

![本轮启动、评分及回归结果](../images/C-05-boot-and-grade.png)

复现入口：

```bash
source /home/liu/projects/nku-os/scripts/env.sh
cd /home/liu/projects/nku-os/labs/code
bash tools/prepare_qemu_c.sh
make PMM=best_fit
python3 tools/debug_c.py \
  --qemu /home/liu/.local/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64 \
  --source /home/liu/.local/src/qemu-4.1.1 \
  --output ../report/evidence/C-recheck
```

每个调试断点和单步命令都在两份 GDB 日志里。内核布局变化时，先用 `objdump -d bin/kernel` 更新脚本中三个 PC、访存地址和刷新指令位置。本轮调试镜像 SHA-256 为 `59694a8f82155635c5048b391dabdee5119c56a61d35daaaf14b856a959886bf`。

## 9. 这次观察带来的理解

真正走一遍后，地址翻译不再只是“虚拟页号换成物理页号”。它包含模式选择、地址合法性、PTE 读取、权限和大页对齐检查，再接上模拟器的物理地址空间与访问缓存。各层的地址含义必须分开，连函数中间返回值也要看清楚粒度。

另一个收获是，模拟器里负责“像硬件一样做事”的代码，也要考虑宿主机上的执行效率。软件 TLB 的快路径已经被生成成 x86-64 机器码，命中后不会重新逐行执行 C 翻译函数。把 GDB 调用栈、缓存标签和生成代码放在一起看，才看清楚这一层分工。
