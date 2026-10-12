# Lab1 构建入口。源文件编译为 obj/ 下的目标文件，链接成 bin/kernel，
# 再由 objcopy 生成 bin/ucore.img；运行与调试都加载同一裸镜像。
# 本文件的注释解释变量、辅助宏和目标，不改变原有构建参数。
PROJ	:= lab1
EMPTY	:=
SPACE	:= $(EMPTY) $(EMPTY)
SLASH	:= /

V       := @

#ifndef GCCPREFIX
# 交叉工具链前缀：使用 RISC-V 裸机 GCC/Binutils，不能用宿主 x86 编译器替代。
GCCPREFIX := riscv64-unknown-elf-
#endif

ifndef QEMU
QEMU := qemu-system-riscv64
endif


# eliminate default suffix rules
.SUFFIXES: .c .S .h

# delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# define compiler and flags
HOSTCC		:= gcc
HOSTCFLAGS	:= -Wall -O2

GDB		:= $(GCCPREFIX)gdb

CC		:= $(GCCPREFIX)gcc
# medany 支持本实验的地址布局；gnu99 启用 GNU C 语法；
# fno-builtin/nostdinc 使用内核自带库与头文件；-g 保留 GDB 所需调试信息。
# 按函数/数据分段后，链接器可删除未使用的段。
CFLAGS  := -mcmodel=medany -std=gnu99 -Wno-unused -Werror
CFLAGS	+= -fno-builtin -Wall -O2 -nostdinc $(DEFS)
CFLAGS	+= -fno-stack-protector -ffunction-sections -fdata-sections
CFLAGS	+= -g
CTYPE	:= c S

LD      := $(GCCPREFIX)ld
# 生成 ELF64 RISC-V；不链接宿主标准库；--gc-sections 删除不可达的输入段。
# 因此源码中的未使用输入接口即使引用了缺失实现，也可能不进入最终镜像。
LDFLAGS	:= -m elf64lriscv
LDFLAGS	+= -nostdlib --gc-sections

OBJCOPY := $(GCCPREFIX)objcopy
OBJDUMP := $(GCCPREFIX)objdump

COPY	:= cp
MKDIR   := mkdir -p
MV		:= mv
RM		:= rm -f
AWK		:= awk
SED		:= sed
SH		:= sh
TR		:= tr
TOUCH	:= touch -c

OBJDIR	:= obj
BINDIR	:= bin

ALLOBJS	:=
ALLDEPS	:=
TARGETS	:=

# 加载通用构建宏，随后用它们收集源码、生成依赖和创建目标规则。
include tools/function.mk

# listf_cc(dir)：使用 listf 选取目录中的 .c 和 .S 源文件，结果为路径列表。
listf_cc = $(call listf,$(1),$(CTYPE))

# for cc
# add_files_cc(files,packet,extra_flags,dir)：使用交叉 GCC 建立编译规则，
# 并把对象登记到指定分组；额外参数用于添加内核包含目录。
add_files_cc = $(call add_files,$(1),$(CC),$(CFLAGS) $(3),$(2),$(4))
# create_target_cc(target,packets,objects)：用交叉 GCC 为分组与对象创建链接目标。
# 这是模板辅助接口，当前内核最终链接实际使用下面的 LD 规则。
create_target_cc = $(call create_target,$(1),$(2),$(3),$(CC),$(CFLAGS))

# for hostcc
# add_files_host(files,packet,dir)：用宿主 GCC 编译构建期工具，登记其对象。
# Lab1 当前没有通过此接口构建额外的宿主工具。
add_files_host = $(call add_files,$(1),$(HOSTCC),$(HOSTCFLAGS),$(2),$(3))
# create_target_host(target,packets,objects)：使用宿主 GCC 创建工具程序目标。
create_target_host = $(call create_target,$(1),$(2),$(3),$(HOSTCC),$(HOSTCFLAGS))

# cgtype(files,old,new)：替换文件后缀；objfile 转为对象路径，
# asmfile/outfile/symfile 据此派生反汇编、输出与符号文件名。
cgtype = $(patsubst %.$(2),%.$(3),$(1))
# objfile(files)：调用 toobj，返回源码对应的对象路径。
objfile = $(call toobj,$(1))
# asmfile(files)：把对象路径的 .o 改为 .asm，供保存反汇编使用。
asmfile = $(call cgtype,$(call toobj,$(1)),o,asm)
# outfile(files)：把对象路径的 .o 改为 .out，供保存辅助输出使用。
outfile = $(call cgtype,$(call toobj,$(1)),o,out)
# symfile(files)：把对象路径的 .o 改为 .sym，供保存符号清单使用。
symfile = $(call cgtype,$(call toobj,$(1)),o,sym)

# for match pattern
# match(patterns,text)：通过 AWK 检查目标名是否匹配模式，匹配时返回 1，
# 未匹配返回 0；下面用于决定清理等目标是否应加载自动依赖文件。
match = $(shell echo $(2) | $(AWK) '{for(i=1;i<=NF;i++){if(match("$(1)","^"$$(i)"$$")){exit 1;}}}'; echo $$?)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
# include kernel/user

# libs 提供标准头文件的内核替代实现；LIBDIR 中的 .c/.S 全部登记进 libs 分组。
INCLUDE	+= libs 

CFLAGS	+= $(addprefix -I,$(INCLUDE))

LIBDIR	+= libs

$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)

# -------------------------------------------------------------------
# kernel

# 内核头文件搜索目录及源码目录，部分目录是模板为后续实验预留的，
# 目录在变量中出现并不代表当前 Lab1 已经实现对应模块。
KINCLUDE	+= kern/debug/ \
			   kern/driver/ \
			   kern/trap/ \
			   kern/libs/\
			   kern/mm/ \
			   kern/arch/

KSRCDIR		+= kern/init \
			   kern/debug \
			   kern/libs \
			   kern/driver \
			   kern/trap \
			   kern/mm

KCFLAGS		+= $(addprefix -I,$(KINCLUDE))

$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))

# 合并 kernel 和 libs 两个分组的对象，作为内核链接输入。
KOBJS	= $(call read_packet,kernel libs)

# create kernel target
kernel = $(call totarget,kernel)

# 链接脚本也是构建依赖：改动布局或注释会使链接目标重新生成。
$(kernel): tools/kernel.ld

# 内核链接：按 kernel.ld 安排各段，输出 ELF，并生成反汇编和符号清单。
# ELF 保留符号及调试信息，供 GDB 分析；它不是下面运行用的裸镜像。
$(kernel): $(KOBJS)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

$(call create_target,kernel)

# -------------------------------------------------------------------
# create ucore.img
UCOREIMG	:= $(call totarget,ucore.img)


# objcopy 去掉元数据并转换为 binary，只保留需要加载的镜像内容。
$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@

$(call create_target,ucore.img)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

# 在分组与目标登记完成后汇总依赖，并为输出目录建立创建规则。
$(call finish_all)

IGNORE_ALLDEPS	= clean \
				  dist-clean \
				  grade \
				  touch \
				  print-.+ \
				  handin

ifeq ($(call match,$(MAKECMDGOALS),$(IGNORE_ALLDEPS)),0)
-include $(ALLDEPS)
endif

# files for grade script

TARGETS: $(TARGETS)

.DEFAULT_GOAL := TARGETS

# qemu：普通启动目标。使用 virt 机器、终端串口和默认 OpenSBI。
# -kernel 同时让 QEMU 加载镜像并准备固件下一阶段入口信息。
.PHONY: qemu 
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
#	$(V)$(QEMU) -kernel $(UCOREIMG) -nographic
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-kernel $(UCOREIMG)

# debug：与普通运行加载相同镜像；-s 启用默认端口 1234 的 GDB 服务，
# -S 让 CPU 在执行复位指令前暂停，供练习 2 观察初始 PC 和启动交接。
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-kernel $(UCOREIMG)\
		-s -S

# gdb：读取 bin/kernel 的符号，选择 RV64 架构，连接本地 QEMU。
# 调试 ELF 与 QEMU 加载的 ucore.img 必须来自同一轮构建。
gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'

	
.PHONY: grade touch

GRADE_GDB_IN	:= .gdb.in
GRADE_QEMU_OUT	:= .qemu.out
HANDIN			:= proj$(PROJ)-handin.tar.gz

TOUCH_FILES		:= kern/trap/trap.c

MAKEOPTS		:= --quiet --no-print-directory

# grade：模板评分入口，先清理再执行 tools/grade.sh。
# 当前仓库没有该脚本，所以此目标不能作为本实验已通过评分的依据。
grade:
	$(V)$(MAKE) $(MAKEOPTS) clean
	$(V)$(SH) tools/grade.sh

# touch：更新模板指定文件的时间戳以触发重编译。
# 当前 TOUCH_FILES 指向后续实验的 trap.c，Lab1 不依赖这个目标。
touch:
	$(V)$(foreach f,$(TOUCH_FILES),$(TOUCH) $(f))

# print-NAME：诊断目标，将 NAME 转为大写并显示同名 Make 变量的值。
print-%:
	@echo $($(shell echo $(patsubst print-%,%,$@) | $(TR) [a-z] [A-Z]))

.PHONY: clean dist-clean handin packall tags
# clean：删除生成目录、调试临时文件和索引，保留源码与报告。
clean:
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
	-$(RM) -r $(OBJDIR) $(BINDIR)

# dist-clean：在 clean 基础上删除模板打包生成的提交归档。
dist-clean: clean
	-$(RM) $(HANDIN)

# handin：依赖 packall 生成归档，并输出模板的提交提示；不会自动上传。
handin: packall
	@echo Please visit http://learn.tsinghua.edu.cn and upload $(HANDIN). Thanks!

# packall：清理构建结果后，把当前代码目录打包为实验提交归档。
packall: clean
	@$(RM) -f $(HANDIN)
	@tar -czf $(HANDIN) `find . -type f -o -type d | grep -v '^\.*$$' | grep -vF '$(HANDIN)'`

# tags：收集 C/头文件/汇编文件，生成 cscope 与 ctags 导航索引。
tags:
	@echo TAGS ALL
	$(V)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
	$(V)find . -type f -name "*.[chS]" >cscope.files
	$(V)cscope -bq 
	$(V)ctags -L cscope.files
