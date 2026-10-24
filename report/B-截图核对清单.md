# Lab2 B 截图与交付核对清单

负责人：柯云超，2413575。核对日期：2026-10-24（北京时间）。

## 1. 截图核对结果

**B 部分必需截图内容已齐全。七项内容由六张原图覆盖，B-04 与 B-05 合并，不需要重复补图。** 已逐张查看图片，并与本人保存的五份运行日志核对；图片内容未做裁剪、重绘或改写，仅整理路径和合并图文件名。

| 原计划编号 | 归档图片 | 已核实的内容 | 关联记录 |
| --- | --- | --- | --- |
| B-01 | [环境与身份](images/B-01-environment.png) | 柯云超 / 2413575、工作目录、lab2、7e9a242、各工具版本、ARCH | 图中完整环境输出 |
| B-02 | [Buddy 全量构建](images/B-02-buddy-build.png) | Buddy/SLUB 编译、链接、objcopy、RISC-V ELF 与镜像文件 | [构建日志](evidence/B-manual-buddy-build.log) |
| B-03 | [Buddy + SLUB 启动](images/B-03-buddy-slub-boot.png) | Buddy 管理器、页分配检查、SLUB 检查、31929 页恢复 | [Buddy 启动日志](evidence/B-manual-buddy-boot.log) |
| B-04 / B-05 | [完整主机测试合并图](images/B-04-05-host-checks.png) | Buddy 20000 次模型操作、三种 PMM 各 5000 次对象操作、误释放、ASan/UBSan | [主机测试日志](evidence/B-manual-host.log) |
| B-06 | [Best-Fit + SLUB 启动](images/B-06-best-fit-boot.png) | Best-Fit 管理器、原始页检查、SLUB 检查、31929 页恢复 | [Best-Fit 启动日志](evidence/B-manual-best-fit-boot.log) |
| B-07 | [教师评分](images/B-07-grade.png) | Page Init OK、Best-Fit OK、Total Score: 20/20 | [评分日志](evidence/B-manual-grade.log) |

图片已从仓库根目录移至 `report/images/`。原 `B-04-buddy-host.png` 归档为 `B-04-05-host-checks.png`，表明同时覆盖两项。主报告只展示一次，避免重复插图。

## 2. B 交付内容

- [x] Buddy 与简化 SLUB 源码、内核自检和独立测试。
- [x] [实现 spec](B-spec.md) 与 [四份实现/测试提示词](B-prompts.md)。
- [x] [设计、边界、迭代问题和测试覆盖说明](B-Buddy与SLUB.md)。
- [x] [知识点与 OS 原理对应总结](B-知识点总结.md)。
- [x] [自动回归证据与源码哈希](evidence/B-20261023/summary.json)。
- [x] 本人运行日志、六张截图、报告图片引用。
- [ ] 本人完整流程演示和答辩准备。

指南中的 B-08 回归矩阵图为可选补充，不属于缺图。64/128/256 MiB Buddy 及三种 PMM 的自动回归已有原始日志；本次没有将其宣称为本人重新执行的回归矩阵。

## 3. 小组最终提交仍需处理

1. C 提供 QEMU 源码、页表翻译、TLB 和分页前后比较的调试报告与真实证据。
2. A/C 按各自职责完成整合复核和本人验收，三人完成演示及答辩准备。
3. C 按教师模板整合全组报告，核对 A/C 的截图、AI 工具信息和其余验收项。
4. B 的代码、报告、六张截图和日志纳入本次 lab2 提交；全组最终 Git 交付与课程提交仍须在 A/C 补齐并完成验收后处理。

B 报告的截图占位已全部替换。以上待办是小组验收事项，不影响本次 B 的截图完整性结论。
