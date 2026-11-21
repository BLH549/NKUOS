# Lab4

本仓库对应 GitHub `BLH549/NKUOS`，当前分支为 `lab4`。

`code/` 基于课程网站的 Lab4 框架，已完成 Lab2/3 前置迁移、A 的页映射和 B 的线程创建。C 的调度与上下文切换待接入；原始框架保存在提交 `046550a`。

- [ABC 任务分工](report/资料/Lab4-任务分工.md)
- [前置代码迁移](report/资料/Lab4-前置代码迁移.md)
- [报告入口](report/report.md)
- [A：页映射与内存](report/资料/A-页映射与内存.md)
- [B：线程创建与初始化](report/资料/B-线程创建.md)
- [提示词记录](report/prompt.md)

配置好 RISC-V GCC/binutils、QEMU 和 Python 3 后，在 WSL 终端执行：

```bash
cd code
make check-a
python3 ../report/evidence/A-lab4/regress_b.py
```

`make check-a` 在临时内核中运行 64、128、256 MiB 三组页映射测试，保留教师 PMM 和 Process Create 检查。第二条命令复现已有 B 验证，并记录正式构建与评分状态。日志写入 `report/evidence/A-lab4/`。

正式 `make`、`make qemu` 当前缺少 `cpu_idle()`，`make grade` 还缺少 `schedule()`。C 完成后继续运行这三个入口。
