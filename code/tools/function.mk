# Make 构建辅助函数集合，使用 GNU Make 的 call/eval/二次展开生成规则。
# 普通赋值宏返回文本；define 中的宏通常生成规则并更新分组或目标变量。
OBJPREFIX	:= __objs_

# 启用依赖项二次展开：先保留含美元符号的目录表达式，
# 在具体规则处理时再按目标路径求出目录，配合顺序依赖创建输出目录。
.SECONDEXPANSION:
# -------------------- function begin --------------------

# list all files in some directories: (#directories, #types)
# listf(directories,types)：列出各目录的直接子文件，用扩展名过滤。
# types 为空时保留全部匹配项；wildcard 不递归进入子目录。
listf = $(filter $(if $(2),$(addprefix %.,$(2)),%),\
		  $(wildcard $(addsuffix $(SLASH)*,$(1))))

# get .o obj files: (#files[, packet])
# toobj(files,packet)：去掉源码后缀，追加 .o，再加 obj/ 和可选分组路径。
# 保留源码相对目录结构，以避免不同目录中的同名文件发生冲突。
toobj = $(addprefix $(OBJDIR)$(SLASH)$(if $(2),$(2)$(SLASH)),\
		$(addsuffix .o,$(basename $(1))))

# get .d dependency files: (#files[, packet])
# todep(files,packet)：先计算对象路径，再把 .o 换成 .d，得到依赖文件路径。
todep = $(patsubst %.o,%.d,$(call toobj,$(1),$(2)))

# totarget(names)：给目标名添加 bin/ 前缀，返回最终输出路径列表。
totarget = $(addprefix $(BINDIR)$(SLASH),$(1))

# change $(name) to $(OBJPREFIX)$(name): (#names)
# packetname(names)：给分组名添加 __objs_ 前缀，生成存放对象列表的变量名。
packetname = $(if $(1),$(addprefix $(OBJPREFIX),$(1)),$(OBJPREFIX))

# cc compile template, generate rule for dep, obj: (file, cc[, flags, dir])
# cc_template(file,cc,flags,dir)：为一个源文件生成依赖规则和对象编译规则。
# -MM 只生成非系统头文件依赖，-MT 同时指定对象和依赖文件两个目标。
# 目录作为顺序依赖，保证存在但目录时间戳变化不会单独触发重编译。
# 还把对象路径加入 ALLOBJS；$$ 用于把变量引用留给后续 Make 展开。
define cc_template
$$(call todep,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	@$(2) -I$$(dir $(1)) $(3) -MM $$< -MT "$$(patsubst %.d,%.o,$$@) $$@"> $$@
$$(call toobj,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	@echo + cc $$<
	$(V)$(2) -I$$(dir $(1)) $(3) -c $$< -o $$@
ALLOBJS += $$(call toobj,$(1),$(4))
endef

# compile file: (#files, cc[, flags, dir])
# do_cc_compile(files,cc,flags,dir)：遍历源文件列表，逐个 eval cc_template。
# 效果是创建实际规则并登记对象，不是立即执行编译命令。
define do_cc_compile
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(4))))
endef

# add files to packet: (#files, cc[, flags, packet, dir])
# do_add_files_to_packet(files,cc,flags,packet,dir)：初始化指定分组变量，
# 为源文件创建编译规则，并把相应对象列表追加到该分组。
define do_add_files_to_packet
__temp_packet__ := $(call packetname,$(4))
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
__temp_objs__ := $(call toobj,$(1),$(5))
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(5))))
$$(__temp_packet__) += $$(__temp_objs__)
endef

# add objs to packet: (#objs, packet)
# do_add_objs_to_packet(objects,packet)：把已有对象加入分组，不创建编译规则。
# 用于让不同来源的对象统一参与后续目标链接。
define do_add_objs_to_packet
__temp_packet__ := $(call packetname,$(2))
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
$$(__temp_packet__) += $(1)
endef

# add packets and objs to target (target, #packes, #objs[, cc, flags])
# do_create_target(target,packets,objects,cc,flags)：合并分组对象和显式对象，
# 将 bin/target 登记进 TARGETS；有 cc 时生成链接命令，无 cc 时只登记依赖，
# 由调用方定义真正的链接规则，例如当前内核的 LD 命令。
define do_create_target
__temp_target__ = $(call totarget,$(1))
__temp_objs__ = $$(foreach p,$(call packetname,$(2)),$$($$(p))) $(3)
TARGETS += $$(__temp_target__)
ifneq ($(4),)
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
	$(V)$(4) $(5) $$^ -o $$@
else
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
endif
endef

# finish all
# do_finish_all()：把 ALLOBJS 的 .o 路径转换成 ALLDEPS 的 .d 路径，
# 并为所有对象目录及 bin/、obj/ 创建 mkdir -p 规则。应在登记完成后调用。
define do_finish_all
ALLDEPS = $$(ALLOBJS:.o=.d)
$$(sort $$(dir $$(ALLOBJS)) $(BINDIR)$(SLASH) $(OBJDIR)$(SLASH)):
	@$(MKDIR) $$@
endef

# --------------------  function end  --------------------
# compile file: (#files, cc[, flags, dir])
# cc_compile(files,cc,flags,dir)：用 eval 展开 do_cc_compile，把生成文本作为 Make 规则。
cc_compile = $(eval $(call do_cc_compile,$(1),$(2),$(3),$(4)))

# add files to packet: (#files, cc[, flags, packet, dir])
# add_files(files,cc,flags,packet,dir)：公共包装宏，展开规则并把源文件加入分组。
add_files = $(eval $(call do_add_files_to_packet,$(1),$(2),$(3),$(4),$(5)))

# add objs to packet: (#objs, packet)
# add_objs(objects,packet)：公共包装宏，仅将现有对象登记进分组。
add_objs = $(eval $(call do_add_objs_to_packet,$(1),$(2)))

# add packets and objs to target (target, #packes, #objs, cc, [, flags])
# create_target(target,packets,objects,cc,flags)：公共包装宏，创建并登记目标。
create_target = $(eval $(call do_create_target,$(1),$(2),$(3),$(4),$(5)))

# read_packet(packets)：依次读取每个分组变量的对象列表并合并，返回对象路径文本。
read_packet = $(foreach p,$(call packetname,$(1)),$($(p)))

# add_dependency(target,dependencies)：通过 eval 为目标追加依赖规则。
add_dependency = $(eval $(1): $(2))

# finish_all()：公共收尾包装宏，调用 do_finish_all 生成目录和依赖汇总。
finish_all = $(eval $(call do_finish_all))

