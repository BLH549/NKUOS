# Lab1 构建入口。源文件编译为 obj/ 下的目标文件，链接成 bin/kernel，
# 再由 objcopy 生成 bin/ucore.img；运行与调试都加载同一裸镜像。
# 本文件的注释解释变量、辅助宏和目标，不改变原有构建参数。
# 逐行：设置实验名称为 lab1；:= 立即展开，用于后面的提交归档名。
PROJ	:= lab1
# 逐行：定义空字符串变量，下一行借它构造空格。
EMPTY	:=
# 逐行：两个空变量之间的字面空格形成空格变量，避免赋值开头空格被忽略。
SPACE	:= $(EMPTY) $(EMPTY)
# 逐行：定义路径分隔符 /，供 function.mk 拼接目录与文件名。
SLASH	:= /

# 逐行：把命令前缀设为 @，使使用它的命令不回显命令文本。
V       := @

# 逐行：下面的 #ifndef/#endif 是普通注释，未构成实际 Make 条件。
#ifndef GCCPREFIX
# 交叉工具链前缀：使用 RISC-V 裸机 GCC/Binutils，不能用宿主 x86 编译器替代。
# 逐行：设置交叉工具名前缀；这里的 ifndef 是注释，不是有效条件。
GCCPREFIX := riscv64-unknown-elf-
#endif

# 逐行：仅当 QEMU 未定义或为空时采用后面的默认值，解析阶段决定。
ifndef QEMU
# 逐行：在 QEMU 未定义时提供默认模拟器名称，可用命令行变量指定其他路径。
QEMU := qemu-system-riscv64
# 逐行：结束最近的 Make 条件块，不是运行时的 shell 分支。
endif


# eliminate default suffix rules
# 逐行：设置后缀规则候选表；对象编译由 function.mk 的显式规则完成。
.SUFFIXES: .c .S .h

# delete target files if there is an error (or make is interrupted)
# 逐行：命令出错时删除该规则已改写的目标，避免保留未完成产物。
.DELETE_ON_ERROR:

# define compiler and flags
# 逐行：选择宿主 GCC，供开发机上的构建工具使用，不用于编译 RISC-V 内核。
HOSTCC		:= gcc
# 逐行：宿主工具开启常见警告及 O2 优化，当前未构建额外宿主工具。
HOSTCFLAGS	:= -Wall -O2

# 逐行：拼出交叉 GDB 名称；后面的 gdb recipe 实际直接写了程序名称。
GDB		:= $(GCCPREFIX)gdb

# 逐行：拼出 RISC-V 裸机 GCC 名称，用于生成内核目标文件。
CC		:= $(GCCPREFIX)gcc
# medany 支持本实验的地址布局；gnu99 启用 GNU C 语法；
# fno-builtin/nostdinc 使用内核自带库与头文件；-g 保留 GDB 所需调试信息。
# 按函数/数据分段后，链接器可删除未使用的段。
# 逐行：使用 medany 地址模型和 GNU99 语法，关闭 unused 类警告，其余启用的警告视为错误。
CFLAGS  := -mcmodel=medany -std=gnu99 -Wno-unused -Werror
# 逐行：禁用内建函数替换，开启警告及 O2，禁用系统头文件路径并追加 DEFS。
CFLAGS	+= -fno-builtin -Wall -O2 -nostdinc $(DEFS)
# 逐行：禁用需运行时支持的栈保护，并按函数和数据分段，供链接删除未使用内容。
CFLAGS	+= -fno-stack-protector -ffunction-sections -fdata-sections
# 逐行：保留 GDB 调试信息，使指令可对应源码行号和变量。
CFLAGS	+= -g
# 逐行：选择 c 和大写 S 扩展名；大写 S 在汇编前接受 C 预处理。
CTYPE	:= c S

# 逐行：拼出 RISC-V 链接器名称，后续按 kernel.ld 合并目标文件。
LD      := $(GCCPREFIX)ld
# 生成 ELF64 RISC-V；不链接宿主标准库；--gc-sections 删除不可达的输入段。
# 因此源码中的未使用输入接口即使引用了缺失实现，也可能不进入最终镜像。
# 逐行：选择 ELF64 小端 RISC-V 链接仿真格式。
LDFLAGS	:= -m elf64lriscv
# 逐行：只使用指定输入而不引入标准库，删除不可达的输入 section。
LDFLAGS	+= -nostdlib --gc-sections

# 逐行：拼出镜像转换工具名，用于从 ELF 提取裸二进制。
OBJCOPY := $(GCCPREFIX)objcopy
# 逐行：拼出反汇编工具名，用于导出指令和符号清单。
OBJDUMP := $(GCCPREFIX)objdump

# 逐行：把复制命令集中定义为 cp，供模板规则复用。
COPY	:= cp
# 逐行：定义 mkdir -p，递归创建父目录，目录已存在时不报错。
MKDIR   := mkdir -p
# 逐行：集中定义移动/重命名命令，当前主构建路径未使用它。
MV		:= mv
# 逐行：定义 rm -f，缺失文件不报错；清理目录时另加 -r。
RM		:= rm -f
# 逐行：定义 awk 文本工具，match 宏用它检测目标模式。
AWK		:= awk
# 逐行：定义 sed 文本工具，用于整理 objdump 的符号文本。
SED		:= sed
# 逐行：定义 sh 解释器，评分模板用它执行脚本。
SH		:= sh
# 逐行：定义字符转换工具，print-% 将变量名转为大写。
TR		:= tr
# 逐行：定义 touch -c，仅更新时间戳，不创建缺失文件。
TOUCH	:= touch -c

# 逐行：设置 obj 为对象和辅助清单目录，可以由 clean 删除。
OBJDIR	:= obj
# 逐行：设置 bin 为 ELF 和镜像输出目录，totarget 用它拼路径。
BINDIR	:= bin

# 逐行：初始化全部对象列表，后续编译模板逐个追加。
ALLOBJS	:=
# 逐行：初始化依赖文件列表，finish_all 根据对象名派生 .d 路径。
ALLDEPS	:=
# 逐行：初始化默认目标列表，create_target 后续登记 ELF 和镜像。
TARGETS	:=

# 加载通用构建宏，随后用它们收集源码、生成依赖和创建目标规则。
# 逐行：读取构建辅助宏，后面的 call 才能使用 listf/toobj/create_target。
include tools/function.mk

# listf_cc(dir)：使用 listf 选取目录中的 .c 和 .S 源文件，结果为路径列表。
# 逐行：call 把第 1 参数目录传给 listf，按 CTYPE 过滤源码；= 表示调用时展开。
listf_cc = $(call listf,$(1),$(CTYPE))

# for cc
# add_files_cc(files,packet,extra_flags,dir)：使用交叉 GCC 建立编译规则，
# 并把对象登记到指定分组；额外参数用于添加内核包含目录。
# 逐行：传入源码、交叉 CC、CFLAGS 加第 3 参数、第 2 参数分组及第 4 参数目录，生成编译规则。
add_files_cc = $(call add_files,$(1),$(CC),$(CFLAGS) $(3),$(2),$(4))
# create_target_cc(target,packets,objects)：用交叉 GCC 为分组与对象创建链接目标。
# 这是模板辅助接口，当前内核最终链接实际使用下面的 LD 规则。
# 逐行：传入目标、对象分组和显式对象，使用交叉 GCC/CFLAGS 生成目标规则。
create_target_cc = $(call create_target,$(1),$(2),$(3),$(CC),$(CFLAGS))

# for hostcc
# add_files_host(files,packet,dir)：用宿主 GCC 编译构建期工具，登记其对象。
# Lab1 当前没有通过此接口构建额外的宿主工具。
# 逐行：用 HOSTCC/HOSTCFLAGS 为第 1 参数源码创建宿主编译规则并分组。
add_files_host = $(call add_files,$(1),$(HOSTCC),$(HOSTCFLAGS),$(2),$(3))
# create_target_host(target,packets,objects)：使用宿主 GCC 创建工具程序目标。
# 逐行：用宿主 GCC 为目标和对象创建链接规则，预留给构建期工具。
create_target_host = $(call create_target,$(1),$(2),$(3),$(HOSTCC),$(HOSTCFLAGS))

# cgtype(files,old,new)：替换文件后缀；objfile 转为对象路径，
# asmfile/outfile/symfile 据此派生反汇编、输出与符号文件名。
# 逐行：patsubst 把第 1 参数路径的第 2 参数后缀换为第 3 参数后缀，% 匹配文件名前缀。
cgtype = $(patsubst %.$(2),%.$(3),$(1))
# objfile(files)：调用 toobj，返回源码对应的对象路径。
# 逐行：调用 toobj，返回第 1 参数源码的 .o 路径。
objfile = $(call toobj,$(1))
# asmfile(files)：把对象路径的 .o 改为 .asm，供保存反汇编使用。
# 逐行：先计算对象路径，再把 .o 改为 .asm，供保存源码混合反汇编。
asmfile = $(call cgtype,$(call toobj,$(1)),o,asm)
# outfile(files)：把对象路径的 .o 改为 .out，供保存辅助输出使用。
# 逐行：先计算对象路径，再把 .o 改为 .out，供保存辅助输出。
outfile = $(call cgtype,$(call toobj,$(1)),o,out)
# symfile(files)：把对象路径的 .o 改为 .sym，供保存符号清单使用。
# 逐行：先计算对象路径，再把 .o 改为 .sym，供保存符号清单。
symfile = $(call cgtype,$(call toobj,$(1)),o,sym)

# for match pattern
# match(text,patterns)：通过 AWK 检查目标名是否匹配模式，匹配时返回 1，
# 未匹配返回 0；下面用于决定清理等目标是否应加载自动依赖文件。
# 逐行：AWK 用 ^/$ 匹配完整目标名；第 1 参数是文本、第 2 参数是模式列表，匹配退出码 1，否则 0；$$ 留 $ 给 shell。
match = $(shell echo $(2) | $(AWK) '{for(i=1;i<=NF;i++){if(match("$(1)","^"$$(i)"$$")){exit 1;}}}'; echo $$?)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
# include kernel/user

# libs 提供标准头文件的内核替代实现；LIBDIR 中的 .c/.S 全部登记进 libs 分组。
# 逐行：将 libs 追加到公共头文件搜索目录，+= 不覆盖原目录。
INCLUDE	+= libs 

# 逐行：给公共头文件目录逐个加 -I，让 GCC 使用内核自带头文件。
CFLAGS	+= $(addprefix -I,$(INCLUDE))

# 逐行：登记 libs 为库源码目录，后面收集其中 c/S 文件。
LIBDIR	+= libs

# 逐行：列出库源码后登记进 libs 分组，第 3 参数为空，使用基础交叉编译选项。
$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)

# -------------------------------------------------------------------
# kernel

# 内核头文件搜索目录及源码目录，部分目录是模板为后续实验预留的，
# 目录在变量中出现并不代表当前 Lab1 已经实现对应模块。
# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：登记 kern/debug 调试头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第2行：登记 kern/driver 控制台等驱动头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第3行：登记 kern/trap 中断异常头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第4行：登记 kern/libs 内核库头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第5行：登记 kern/mm 内存定义头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第6行：登记 kern/arch 架构专用头文件搜索目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
KINCLUDE	+= kern/debug/ \
			   kern/driver/ \
			   kern/trap/ \
			   kern/libs/\
			   kern/mm/ \
			   kern/arch/

# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：登记 kern/init 内核入口与初始化源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第2行：登记 kern/debug 调试源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第3行：登记 kern/libs 内核库源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第4行：登记 kern/driver 控制台等驱动源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第5行：登记 kern/trap 中断异常源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
# 第6行：登记 kern/mm 内存定义源码目录；不存在的预留目录不会产生源码对象，末尾反斜杠用于继续列表。
KSRCDIR		+= kern/init \
			   kern/debug \
			   kern/libs \
			   kern/driver \
			   kern/trap \
			   kern/mm

# 逐行：用 addprefix 给每个 KINCLUDE 目录加 -I，得到内核头文件搜索参数。
KCFLAGS		+= $(addprefix -I,$(KINCLUDE))

# 逐行：收集内核源码、登记进 kernel 分组，附加内核头文件目录参数。
$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))

# 合并 kernel 和 libs 两个分组的对象，作为内核链接输入。
# 逐行：读取 kernel/libs 分组并合并对象，延迟展开以取得全部已登记内容。
KOBJS	= $(call read_packet,kernel libs)

# create kernel target
# 逐行：调用 totarget 加 bin/ 前缀，得到实际链接目标 bin/kernel。
kernel = $(call totarget,kernel)

# 链接脚本也是构建依赖：改动布局或注释会使链接目标重新生成。
# 逐行：为 ELF 添加链接脚本依赖，脚本更新后重新链接。
$(kernel): tools/kernel.ld

# 内核链接：按 kernel.ld 安排各段，输出 ELF，并生成反汇编和符号清单。
# ELF 保留符号及调试信息，供 GDB 分析；它不是下面运行用的裸镜像。
# 逐行：为同一 ELF 目标添加所有对象依赖，对象更新时执行下面 recipe。
$(kernel): $(KOBJS)
# 逐行：@ 隐藏 echo 命令，$@ 是当前目标名，输出简短链接提示。
	@echo + ld $@
# 逐行：按脚本链接所有对象，-o $@ 写出当前目标 ELF；V 控制命令回显。
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
# 逐行：-S 生成源码与指令混合反汇编，重定向写入 obj/kernel.asm。
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
# 逐行：导出符号表；sed 去表头、压缩字段、删空行；$$ 留给 sed 表示行尾，结果写入 obj/kernel.sym。
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

# 逐行：登记 ELF 为构建目标，不传编译器，链接命令由前面的规则提供。
$(call create_target,kernel)

# -------------------------------------------------------------------
# create ucore.img
# 逐行：立即保存裸镜像目标 bin/ucore.img。
UCOREIMG	:= $(call totarget,ucore.img)


# objcopy 去掉元数据并转换为 binary，只保留需要加载的镜像内容。
# 逐行：裸镜像依赖 ELF，更新 ELF 后重新转换，保证调试与加载内容一致。
$(UCOREIMG): $(kernel)
# 逐行：去除符号并按 binary 输出裸镜像，当前目标为 bin/ucore.img。
	$(OBJCOPY) $(kernel) --strip-all -O binary $@

# 逐行：把裸镜像登记到默认构建列表，实际转换命令在上面。
$(call create_target,ucore.img)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

# 在分组与目标登记完成后汇总依赖，并为输出目录建立创建规则。
# 逐行：汇总 .d 依赖并创建输出目录规则，应在源码登记完成后调用。
$(call finish_all)

# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：把 clean 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
# 第2行：把 dist-clean 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
# 第3行：把 grade 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
# 第4行：把 touch 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
# 第5行：把 print-.+ 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
# 第6行：把 handin 加入忽略依赖的模式列表，使对应维护目标不触发源码依赖重新生成。
IGNORE_ALLDEPS	= clean \
				  dist-clean \
				  grade \
				  touch \
				  print-.+ \
				  handin

# 逐行：以命令行目标作为文本、忽略列表作为模式，未匹配维护目标时才加载源码依赖。
ifeq ($(call match,$(MAKECMDGOALS),$(IGNORE_ALLDEPS)),0)
# 逐行：加载自动生成的依赖文件，前导 - 允许初次构建文件不存在。
-include $(ALLDEPS)
# 逐行：结束最近的 Make 条件块，不是运行时的 shell 分支。
endif

# files for grade script

# 逐行：建立汇总目标 TARGETS，依赖变量中登记的 ELF/镜像。
TARGETS: $(TARGETS)

# 逐行：把无参数 make 的默认目标设为 TARGETS，避免先出现的辅助规则抢占默认目标。
.DEFAULT_GOAL := TARGETS

# qemu：普通启动目标。使用 virt 机器、终端串口和默认 OpenSBI。
# -kernel 同时让 QEMU 加载镜像并准备固件下一阶段入口信息。
# 逐行：声明 qemu 为伪目标，即使同名文件存在也执行 recipe。
.PHONY: qemu 
# 逐行：启动前生成镜像；SWAPIMG/SFSIMG 当前未定义，展开为空，属于后续实验模板依赖。
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
#	$(V)$(QEMU) -kernel $(UCOREIMG) -nographic
# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：开始多行 QEMU 命令，末尾反斜杠把后续参数接到同一进程。
# 第2行：使用 RISC-V virt 平台，提供本实验采用的内存与设备布局。
# 第3行：关闭图形界面，把串口等输出接到终端，显示固件和启动消息。
# 第4行：加载 QEMU 自带默认固件，在当前环境中为 OpenSBI。
# 第5行：加载裸内核并准备固件下一阶段入口信息；有续行符时继续添加调试参数。
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-kernel $(UCOREIMG)

# debug：与普通运行加载相同镜像；-s 启用默认端口 1234 的 GDB 服务，
# -S 让 CPU 在执行复位指令前暂停，供练习 2 观察初始 PC 和启动交接。
# 逐行：调试前更新镜像，后续启动 GDB 服务并暂停 CPU；当前未将 debug 声明为伪目标。
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：开始多行 QEMU 命令，末尾反斜杠把后续参数接到同一进程。
# 第2行：使用 RISC-V virt 平台，提供本实验采用的内存与设备布局。
# 第3行：关闭图形界面，把串口等输出接到终端，显示固件和启动消息。
# 第4行：加载 QEMU 自带默认固件，在当前环境中为 OpenSBI。
# 第5行：加载裸内核并准备固件下一阶段入口信息；有续行符时继续添加调试参数。
# 第6行：-s 开放默认 GDB TCP 1234 端口，-S 在 CPU 开始执行前暂停。
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-kernel $(UCOREIMG)\
		-s -S

# gdb：读取 bin/kernel 的符号，选择 RV64 架构，连接本地 QEMU。
# 调试 ELF 与 QEMU 加载的 ucore.img 必须来自同一轮构建。
# 逐行：定义调试器连接目标，需先由 make debug 启动监听服务。
gdb:
# 以下续行块逐行解读（注释置于整段之前，保留反斜杠连续）：
# 第1行：启动交叉 GDB，每个 -ex 是一条启动时执行的调试命令。
# 第2行：读取 ELF 中的符号和调试信息，不通过此命令加载裸镜像。
# 第3行：选择 RV64 架构，按目标寄存器和指令格式解释调试内容。
# 第4行：连接本地 QEMU 的 GDB 远程调试端口。
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'

	
# 逐行：把评分和更新时间戳目标标为伪目标，显式请求时执行。
.PHONY: grade touch

# 逐行：记录评分模板的 GDB 临时输入文件名，clean 会清理它。
GRADE_GDB_IN	:= .gdb.in
# 逐行：记录评分模板的 QEMU 临时输出文件名，当前缺少评分脚本。
GRADE_QEMU_OUT	:= .qemu.out
# 逐行：将 PROJ 拼入归档名 projlab1-handin.tar.gz，供打包规则使用。
HANDIN			:= proj$(PROJ)-handin.tar.gz

# 逐行：指定模板中的 trap.c，当前 Lab1 不包含该后续实验源码。
TOUCH_FILES		:= kern/trap/trap.c

# 逐行：子 Make 使用安静模式并隐藏目录提示，只影响日志显示。
MAKEOPTS		:= --quiet --no-print-directory

# grade：模板评分入口，先清理再执行 tools/grade.sh。
# 当前仓库没有该脚本，所以此目标不能作为本实验已通过评分的依据。
# 逐行：声明评分模板目标；当前缺少 tools/grade.sh，不能据此声称评分通过。
grade:
# 逐行：递归调用 clean，使用 MAKE 变量保留递归构建约定，以 MAKEOPTS 减少输出。
	$(V)$(MAKE) $(MAKEOPTS) clean
# 逐行：用 sh 运行评分脚本，当前文件缺失，该目标无法正常完成。
	$(V)$(SH) tools/grade.sh

# touch：更新模板指定文件的时间戳以触发重编译。
# 当前 TOUCH_FILES 指向后续实验的 trap.c，Lab1 不依赖这个目标。
# 逐行：定义更新时间戳目标，不在普通构建时自动执行。
touch:
# 逐行：遍历模板路径并拼出 touch -c 命令，更新文件时间且不创建缺失文件。
	$(V)$(foreach f,$(TOUCH_FILES),$(TOUCH) $(f))

# print-NAME：诊断目标，将 NAME 转为大写并显示同名 Make 变量的值。
# 逐行：用模式规则接受 print-变量名，查询 Make 内部变量的展开值。
print-%:
# 逐行：取当前目标名去掉 print-，转大写，再间接引用同名 Make 变量并显示结果。
	@echo $($(shell echo $(patsubst print-%,%,$@) | $(TR) [a-z] [A-Z]))

# 逐行：将这组维护动作标记为伪目标，避免同名文件阻止执行。
.PHONY: clean dist-clean handin packall tags
# clean：删除生成目录、调试临时文件和索引，保留源码与报告。
# 逐行：定义生成文件清理目标，不删除源码和报告。
clean:
# 逐行：删除评分临时文件和源码索引，rm -f 允许它们不存在。
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
# 逐行：递归删除 obj/bin；首个 - 让 Make 忽略失败，rm -f 是工具自身参数。
	-$(RM) -r $(OBJDIR) $(BINDIR)

# dist-clean：在 clean 基础上删除模板打包生成的提交归档。
# 逐行：先 clean，再执行归档清理。
dist-clean: clean
# 逐行：删除提交归档，首个 - 忽略该命令失败，不修改源码。
	-$(RM) $(HANDIN)

# handin：依赖 packall 生成归档，并输出模板的提交提示；不会自动上传。
# 逐行：先生成归档再显示提交提示，本规则不会自动上传。
handin: packall
# 逐行：显示模板的历史提交地址及归档名称，不执行网络上传。
	@echo Please visit http://learn.tsinghua.edu.cn and upload $(HANDIN). Thanks!

# packall：清理构建结果后，把当前代码目录打包为实验提交归档。
# 逐行：打包前清理产物，避免将对象和镜像收入归档。
packall: clean
# 逐行：删除旧归档，避免它进入新包；额外 -f 与 RM 已有参数重复但保留原写法。
	@$(RM) -f $(HANDIN)
# 逐行：由命令替换收集路径，过滤仅由点组成的路径和归档名，再创建 gzip 压缩 tar 包；保留模板原有文件名处理方式。
	@tar -czf $(HANDIN) `find . -type f -o -type d | grep -v '^\.*$$' | grep -vF '$(HANDIN)'`

# tags：收集 C/头文件/汇编文件，生成 cscope 与 ctags 导航索引。
# 逐行：定义导航索引目标，需要安装 cscope/ctags。
tags:
# 逐行：显示索引生成提示，@ 隐藏 echo 命令自身。
	@echo TAGS ALL
# 逐行：删除旧索引，确保随后生成的结果不受旧文件影响。
	$(V)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
# 逐行：收集后缀为 c/h/S 的源文件路径，写入导航工具的输入清单。
	$(V)find . -type f -name "*.[chS]" >cscope.files
# 逐行：-b 只构建数据库，-q 创建快速符号查询的倒排索引。
	$(V)cscope -bq 
# 逐行：从清单读取源码路径，生成函数和符号跳转索引。
	$(V)ctags -L cscope.files
