# B 的输入与结构化实现约束

负责人：柯云超，2413575；日期：2026-11-04。

## 本轮实际输入

用户提供 `https://github.com/BLH549/NKUOS/tree/lab2` 和本地 `NKU-OS-book`，要求拉取最新更新、完成 Lab3 分工 B；姓名柯云超、学号 2413575；截图先用占位符，并提供详细逐步操作指南。

代理读取远端 `lab3` 的分工文档、A 实现、教师框架、评分脚本和本地指导书后，按下面的结构化约束实现。本轮是同一 Codex 会话完成，下面是实现和验收约束的整理，不冒充另一次独立模型调用或用户逐字输入。

## 实现约束（四段式）

```text
[ROLE]
在现有 ucore RV64 Lab3 框架内完成 B 的中断处理。

[RELY]
读取 trap.c、trap.h、riscv.h、clock.c/h、intr.c、sync.h、sbi.h/c、
trapentry.S、Makefile、grade.sh。A 已完成入口和恢复；C 负责异常。
TICK_NUM=100，print_ticks() 已实现；SBI 提供 sbi_shutdown()。
原框架没有 SHUTDOWN_AFTER；在 B 区域补充 10，并在报告声明。

[SPECIFICATION]
保留签名，只填写 B 区域。去掉 cause 最高中断位后分类。
覆盖头文件列出的原因；非时钟中断输出提示，未知原因输出编号和完整帧。
S_TIMER：重设下一次事件、ticks 恰好加一、整百调用 print_ticks()。
第十次周期输出完成后 sbi_shutdown()。不修改 epc 或其他帧字段。
保留 lab3 标记，B 填学号 2413575，不改 C 的待填实现。

[VERIFY]
构建正式内核；运行到十行 100 ticks 后自然退出；保存日志。
验证所有分支和计数边界，原样运行教师评分并报告 C 未完成的失败项。
检查 A 回归；提供截图占位符和可直接复现的操作指南。
```

## 测试约束（四段式）

```text
[ROLE]
为 B 编写可重复的验证入口，不以修改教师测试来获得分数。

[RELY]
直接提取正式源码中的 interrupt_handler，使用实际 trapframe/IRQ 定义。
宿主桩仅替换 clock_set_next_event、print_ticks、sbi_shutdown、打印与帧输出。
内核端使用正式汇编、正式 B 函数、正式时钟及 SBI。

[SPECIFICATION]
逐个检查已知非时钟原因和未知原因，不增加 ticks、不重设时钟、不关机。
0→1、98→99、99→100、100→101、998→999、999→1000、连续 1000 次。
断言 rearm→计数/输出→shutdown 的顺序、每次仅一次加一和帧逐字节不变。
QEMU 必须自然退出且有十行周期输出，无 panic，不把超时杀进程判定为通过。
用 GDB 检查真实路径的关键计数边界和 epc 保持。
日志写入独立 B 目录，不覆盖 A 原始证据。

[VERIFY]
记录工具链、构建参数、源码哈希、命令退出码和 JSON 结果。
教师评分保留原文；区分 B 验证、A 的桩验证、全组尚缺的 C 验证。
```

实际遇到的问题与解决方法见 [B 报告](B-中断与时钟.md)，不补写未发生的错误经历。
