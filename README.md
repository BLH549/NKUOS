# Lab4 工作区

当前分支：`lab4`。代码起点为 2026-11-18 下载的课程 Lab4 框架，见 [来源记录](UPSTREAM.md)。

- [ABC 任务划分](report/资料/Lab4-任务分工.md)
- [报告入口](report/report.md)

本仓库为提交给助教查看的 GitHub 仓库 `BLH549/NKUOS`，当前使用 `lab4` 分支。`code/` 与网站 Lab4 ZIP 的 `lab4_new/` 一致，报告位于 `report/`。Lab3 成果保存在 `lab3` 分支。

```bash
cd /home/liu/projects/nku-os/labs
source ../scripts/env.sh
cd code
# 补齐前置依赖及本次练习后：
make
make qemu
make grade
```

初始提交保留全部教学空缺，因此当前不具备完整构建/运行条件。分工中的复现与验收项目均待实际完成后勾选。分支仅在本地创建。
