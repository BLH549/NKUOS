# Lab2 实验报告

本次已完成 A、B 的独立代码、分析报告与自动验证。A：匡航逸，2412235；B：柯云超，2413575；C：刘华彬，2411238。实现日期：2026-10-23；B 本人复现、截图与整理日期：2026-10-24（北京时间）。代码位于 `../code/`，默认页分配器为 Best-Fit，`make PMM=buddy` 可切换 Buddy，三种页分配器启动后均运行 SLUB 自检。B 的截图已齐全，C 的调试报告及全组最终验收仍待补齐。

- [A：物理内存初始化、First-Fit、Best-Fit 与内存范围 Challenge](A-初始化与基础分配.md)
- [A：实现前 spec](A-spec.md)
- [B：Buddy System 与 SLUB Challenge 报告](B-Buddy与SLUB.md)
- [B：实现 spec](B-spec.md)
- [B：四份实现与测试提示词](B-prompts.md)
- [B：逐步截图指南（七项内容，现由六张图覆盖）](B-截图操作指南.md)
- [B：截图核对结果与剩余事项](B-截图核对清单.md)
- [B：实验知识点与 OS 原理总结](B-知识点总结.md)
- [B：真实测试、回归与源码校验记录](evidence/B-20261023/summary.json)
- [实际提示词与迭代过程](prompt.md#记录-5完成-a-部分)
- [实验分工与协作验收清单](Lab2-任务分工.md)
- [运行环境与复现命令](../code/README.md)
- [真实运行与源码校验记录](evidence/A-20261023/summary.json)

A 已通过 First-Fit 基线、Best-Fit 原始检查、教师评分 **20/20**、两组各 10,000 次随机分配对照检查和初始化边界检查。B 已通过 Buddy 的 20,000 次独立位图模型操作、SLUB 在三种 PMM 上各 5,000 次随机对象操作及 ASan/UBSan；QEMU 中 First-Fit、Best-Fit、Buddy 与 SLUB 的集成运行均通过，Buddy 另测 64/128/256 MiB，教师回归 **20/20**。B 的六张本人运行截图和五份日志已整理入报告，其中 B-04/B-05 合并，必需截图无遗漏。C 的 QEMU 源码与 TLB 调试仍由其任务继续。
