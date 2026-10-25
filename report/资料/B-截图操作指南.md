# Lab2 B 逐步截图操作指南

适用人：**柯云超，2413575**。编写日期：2026-10-23；截图核对更新：2026-10-24（北京时间）。

本指南只需一个 WSL 终端，按顺序覆盖七项截图内容。2026-10-24 已完成截图核对：现有六张图片已齐全，其中 B-04 与 B-05 合并为一张，见 [核对清单](B-截图核对清单.md)。下列步骤保留用于后续复现；如重新采集，应实际运行命令并保留对应日志。

## 0. 目录、终端与截图准备

本轮已从 GitHub 拉取最新 `lab2` 基线 `7e9a242`，并创建本地 `lab2` 分支的独立工作目录。原 `NKUOS` 目录仍保留 lab1 及其未提交内容。

- Linux 源码目录：`/home/kyc/project/os/NKUOS-lab2/code`。
- Windows 图片目录：`\\wsl.localhost\Ubuntu-22.04\home\kyc\project\os\NKUOS-lab2\report\images`。
- B 报告：`/home/kyc/project/os/NKUOS-lab2/report/资料/B-Buddy与SLUB.md`。
- 本人重新运行的日志请使用 `B-manual-*` 名称，保留已保存的 `evidence/B-20261023/` 原始自动验证证据。

### 步骤 0.1：打开正确终端

1. 打开 Windows Terminal，进入 Ubuntu-22.04 配置；或在 PowerShell 输入：

```powershell
wsl -d Ubuntu-22.04
```

2. 后面的所有 shell 命令都在 Ubuntu 终端执行，不要继续在 PowerShell 提示符中执行。

### 步骤 0.2：设置路径和工具链

逐行执行：

```sh
cd /home/kyc/project/os/NKUOS-lab2/code
source ~/.local/opt/riscv/env.sh
export ARCH=rv64imac
set -o pipefail
mkdir -p ../report/images ../report/evidence
pwd
git branch --show-current
```

应看到目录末尾是 `NKUOS-lab2/code`，分支是 `lab2`。本机 GCC 10.2 使用 `ARCH=rv64imac`；每次新开终端都需重新 source 和 export。

### 步骤 0.3：截图方法

1. 把终端拉大，建议至少约 120 列、35 行；用清晰的等宽字体。
2. 每张图保留执行命令和关键输出，不必截整个桌面；输出太长时滚动到相关区域。
3. 在 Windows 按 `Win+Shift+S`，矩形截取终端区域。
4. 点击截图通知，另存为 PNG 到上面的图片目录，严格采用下文文件名。若截图工具不能直接写 WSL 网络目录，先保存到本地，再用资源管理器移动进去。
5. 截图前可以用 `clear` 清屏，但不要清掉尚未截取的必要结果。

## 1. B-01：环境、身份与分支

执行：

```sh
clear
printf 'Lab2 B | 柯云超 | 学号 2413575\n'
pwd
git branch --show-current
git log -1 --oneline
uname -r
riscv64-unknown-elf-gcc --version | head -n 1
qemu-system-riscv64 --version | head -n 1
gcc --version | head -n 1
make --version | head -n 1
python3 --version
printf 'ARCH=%s\n' "$ARCH"
```

**必须截到：** 姓名学号、`NKUOS-lab2/code` 路径、lab2 分支、基线提交、GCC/QEMU 版本及 `ARCH=rv64imac`。未提交本轮改动时 `git log` 仍显示 A 的基线提交，这是正常现象。

**保存为：** `B-01-environment.png`。

## 2. B-02：Buddy 全量构建

执行：

```sh
clear
printf 'Lab2 B | 柯云超 | 2413575 | Buddy build\n'
make clean
make -j2 PMM=buddy ARCH=rv64imac | tee ../report/evidence/B-manual-buddy-build.log
file bin/kernel
ls -lh bin/kernel bin/ucore.img
```

`make clean` 只清理生成的 obj/bin 等构建文件，不删除报告。

**必须截到：** 构建命令，`kern/mm/buddy_pmm.c` 与 `kern/mm/slub.c` 被编译，`+ ld bin/kernel`、objcopy 成功，以及 ELF 为 RISC-V。并行编译的行顺序可能不同。

**保存为：** `B-02-buddy-build.png`。

## 3. B-03：Buddy 与 SLUB 在 QEMU 中运行

执行：

```sh
clear
make PMM=buddy ARCH=rv64imac qemu | tee ../report/evidence/B-manual-buddy-boot.log
```

稍等至出现：

```text
memory management: buddy_pmm_manager
physical memory: ...
free memory: ...
[Buddy] rounding/alignment/split/merge/count: PASS
check_alloc_page() succeeded!
satp virtual address: ...
satp physical address: ...
[SLUB] classes/alignment/data independence: PASS
[SLUB] multi-slab/reuse/invalid input/page return: PASS
[SLUB] all checks passed; free pages restored: ...
```

**必须截到：** `buddy_pmm_manager`、Buddy PASS、两个 SLUB PASS 和最终恢复页数。向上滚动或适当调小字体，让这些行在同一画面。

**保存为：** `B-03-buddy-slub-boot.png`。

**截图后退出：** 按住 Ctrl 再按 A，全部松开，再按 X。不要一直按着 Ctrl+A 再按 X。终端回到 shell 才继续后续步骤。

内核检查后不再输出、光标停住是预期的等待循环，不表示运行失败。页数随编译器和内核大小变化，不能要求恰好等于旧日志里的 31929。

## 4. B-04：Buddy 独立模型和边界测试

执行一次完整主机测试：

```sh
clear
printf 'Lab2 B | 柯云超 | 2413575 | Host tests\n'
make check-b | tee ../report/evidence/B-manual-host.log
```

此测试直接编译真实 C 源码，启用 ASan/UBSan。完整输出可以分为 B-04、B-05 两张截图，也可在文字清晰的前提下合并为一张；本次已采用合并图。

**B-04 对应内容必须截到：**

```text
[Buddy host] unaligned 63-page arena/exhaustion: PASS
[Buddy host] rounded release/multi-level merge/non-buddies: PASS
[Buddy host] out-of-order adjacent bank registration: PASS
[Buddy host] 20000 independent bitmap-model operations: PASS
```

**本次归档为：** `B-04-05-host-checks.png`，同图还包含下一节的 B-05 内容。

若测试结束后输出滚动过多，可回看刚才本人实际生成的日志：

```sh
sed -n '1,13p' ../report/evidence/B-manual-host.log
```

截取时保留该回看命令，明确它是在展示刚运行的真实测试日志。

## 5. B-05：SLUB 三种底层与异常释放

若上一节的合并图已完整包含以下内容，无需再截一张。本次六张图中的主机测试合并图已经满足。若以后选择分开截图，可在同一次 `make check-b` 的输出中滚动到末尾，或执行：

```sh
sed -n '9,40p' ../report/evidence/B-manual-host.log
```

**必须截到：**

- `buddy_pmm_manager + SLUB: PASS (ASan + UBSan)`。
- `best_fit_pmm_manager + SLUB: PASS (ASan + UBSan)`。
- `default_pmm_manager + SLUB: PASS (ASan + UBSan)`。
- `5000 object operations/data/overlap/OOM/recovery: PASS`。
- `Buddy invalid free size/tail/double: PASS (expected panic)`。
- 最后的 `B host checks: PASS (ASan + UBSan)`。

**仅分开截图时保存为：** `B-05-slub-host.png`。当前报告使用 `B-04-05-host-checks.png`，不缺少独立 B-05 文件。

`expected panic` 是测试在子进程中有意传错参数并确认被拒绝，不是整个测试失败。若看到 ASan 错误、Traceback 或 make Error，则应先排查，不能作为通过截图。

## 6. B-06：默认 Best-Fit 回归与 SLUB

执行：

```sh
clear
make PMM=best_fit ARCH=rv64imac qemu | tee ../report/evidence/B-manual-best-fit-boot.log
```

**必须截到：**

```text
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
[SLUB] classes/alignment/data independence: PASS
[SLUB] multi-slab/reuse/invalid input/page return: PASS
[SLUB] all checks passed; free pages restored: ...
```

有些原始 Best-Fit 检查不单独打印 PASS，因此以完整运行至 `check_alloc_page() succeeded!` 和 SLUB 结束为准。切换管理器会触发 pmm.c 重编译，不需手动改源码。

**保存为：** `B-06-best-fit-boot.png`。截完依次按 Ctrl+A、X 退出。

## 7. B-07：教师评分

执行：

```sh
clear
printf 'Lab2 B | 柯云超 | 2413575 | Teacher grade\n'
make grade PMM=best_fit ARCH=rv64imac | tee ../report/evidence/B-manual-grade.log
```

**必须截到：** 评分命令和以下三行：

```text
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

**保存为：** `B-07-grade.png`。

评分必须使用 Best-Fit；在 Buddy 下运行同一脚本，Best-Fit 身份检查会失败。教师评分没有覆盖 Buddy 与 SLUB，前面的独立测试和内核截图才是 B 的验证证据。

评分会清理本地构建目录，截图后恢复默认可运行镜像：

```sh
make -j2 PMM=best_fit ARCH=rv64imac
```

## 8. 可选：一次运行完整回归矩阵

需要向 A 展示整合结果时可执行：

```sh
python3 tools/check_b_qemu.py --arch rv64imac --output ../report/evidence/B-manual-matrix
```

预期依次显示 First-Fit 128 MiB、Buddy 64/128/256 MiB、Best-Fit 128 MiB 全部 PASS，以及教师 20/20。脚本会自动结束 QEMU，不需要手工 Ctrl+A、X。可另存 `B-08-regression-matrix.png`，这是补充图，不占前述七张的位置。

该脚本把命令、版本、日志和源码/镜像哈希保存到指定目录，不覆盖本轮原始证据。256 MiB 是机器提供的 RAM，内核仍按框架上限管理至 `0x88000000`，并非宣称管理了全部 256 MiB。

## 9. 当前报告的图片引用

`report/资料/B-Buddy与SLUB.md` 第 8 节已经替换完全部占位，当前使用以下六张图片：

```markdown
![B-01 环境与身份](images/B-01-environment.png)
![B-02 Buddy 构建](images/B-02-buddy-build.png)
![B-03 Buddy 与 SLUB 运行](images/B-03-buddy-slub-boot.png)
![B-04 与 B-05 合并：Buddy 和 SLUB 完整主机测试](images/B-04-05-host-checks.png)
![B-06 Best-Fit 与 SLUB 回归](images/B-06-best-fit-boot.png)
![B-07 教师评分](images/B-07-grade.png)
```

已逐张查看图片并核对相应日志，在分工清单中勾选“B 本人截图采集与亲自复现”。B 的代码、报告、六张截图和日志纳入本次 lab2 提交；完整流程演示、答辩和全组最终验收仍待完成。

## 10. 常见问题

| 现象 | 处理方法 |
| --- | --- |
| `make: command not found` 或 Linux 路径无法进入 | 确认已运行 `wsl -d Ubuntu-22.04`，不是仍在 PowerShell |
| 找不到交叉 GCC/QEMU | 执行 `source ~/.local/opt/riscv/env.sh`，用 `command -v riscv64-unknown-elf-gcc qemu-system-riscv64` 确认 |
| ISA 字符串含 `zicsr/zifencei` 时旧汇编器报错 | 本机执行 `export ARCH=rv64imac`，并按指南显式传 `ARCH=rv64imac` |
| 更换终端后又报工具不存在 | source 和 export 只影响当前 shell，新终端需要重新执行 |
| QEMU 输出结束但没有返回命令行 | 内核设计为等待循环；先截图，再依次 Ctrl+A、X |
| 输出显示错误管理器 | 检查当前目录和 `PMM=` 参数，退出 QEMU 后重新执行对应命令 |
| 截图字体太小或输出不全 | 拉大窗口、适当调整字体或滚动；B-04/B-05 可分两张，也可清晰合为一张 |
| `make check-b` 编译失败 | 核对主机 `gcc --version`，保留完整错误；不要把 `expected panic` 之外的异常当作通过 |
| 修改代码后旧截图与输出不同 | 重新运行、重截对应证据，并使用新目录保存回归矩阵，避免混用版本 |
