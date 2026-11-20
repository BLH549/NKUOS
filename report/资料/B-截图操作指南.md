# Lab4 分工 B 截图指南

柯云超（2413575）。全程使用 **一个 Ubuntu 终端窗口**。先构建并截 B-01，接着运行测试，再按编号截 B-02 到 B-05。

## 1. 打开 Ubuntu 终端

1. 在 Windows 按 **Win + R**，打开「运行」。
2. 输入下面这一行，按 **Enter**。

   ```text
   wsl.exe -d Ubuntu-22.04
   ```

3. 看到类似 `kyc@电脑名:~$` 的提示符后，接下来的命令都粘贴到这个窗口。若看到 `PS C:\...>`，先在里面输入上面那行命令并按 Enter。
4. 每次复制一个标有 `bash` 的完整代码块。点进终端，按 **Ctrl + Shift + V** 粘贴。也可以用终端的右键粘贴。若 Windows 提示即将粘贴多行，点「仍要粘贴」。最后一行还停在光标处时，按一下 Enter。

## 2. 构建并截 B-01

在刚打开的 Ubuntu 窗口里，复制下面整段，从 `cd` 到最后一个右括号，粘贴并执行。

```bash
cd /home/kyc/project/os/NKUOS-lab4 &&
export PATH="$HOME/.local/opt/riscv/toolchain-10.2.0/bin:$PATH" &&
export QEMU=/usr/bin/qemu-system-riscv64 &&
E=report/evidence/B-lab4 &&
mkdir -p "$HOME/.cache/nku-lab4-b" &&
WORK=$(mktemp -d "$HOME/.cache/nku-lab4-b/capture.XXXXXX") &&
(
    set -e
    if ! command -v riscv64-unknown-elf-gcc >/dev/null; then
        printf '找不到 riscv64-unknown-elf-gcc，请检查 RISC-V 工具链路径。\n' >&2
        exit 1
    fi
    printf '\n--- B-01 构建环境 ---\n'
    git status -sb | head -n 1
    git log -1 --format='Lab4 commit %h'
    riscv64-unknown-elf-gcc --version | head -n 1
    "$QEMU" --version | head -n 1
    gdb-multiarch --version | head -n 1
    bash "$E/verify.sh" prepare "$WORK"
)
```

等待命令执行完，看到 `lab4 injection complete` 和 `[B fixture] built .../code/bin/kernel`。中间的目录名每次构建都会变化。

**框选范围** 从 `--- B-01 构建环境 ---` 到 `[B fixture] built ...`。画面里应有 `lab4` 分支、GCC、QEMU、GDB 版本和构建成功行。文件名为 **B-01-build.png**。

### 每张图都这样截图和保存

1. 最大化终端，确认要截的几行都在屏幕内。
2. 按 **Win + Shift + S**。屏幕变暗后，在上方点「矩形截图」。鼠标从要截内容的左上方拖到右下方，再松开。
3. 点击右下角的截图通知打开预览。按 **Ctrl + S** 或点「保存」图标。如果没有看到通知，从开始菜单打开「截图工具」，在工具里点「新建」重新框选。
4. 在保存窗口顶部点地址栏，粘贴下面这行路径，按 **Enter**。

   ```text
   \\wsl.localhost\Ubuntu-22.04\home\kyc\project\os\NKUOS-lab4\report\evidence\B-lab4
   ```

5. 在「文件名」输入当前图的名字，例如 `B-01-build.png`，确认类型为 PNG，再点「保存」。回到 Ubuntu 窗口继续。

## 3. 用同一个窗口生成剩余四张图的输出

继续使用刚才的 Ubuntu 窗口。直接复制下面整段。`$E` 和 `$WORK` 沿用第 2 节的值，原样粘贴即可。命令会依次运行 QEMU 和 GDB，并自动结束调试。

```bash
(
    set -e
    if [[ -z "$WORK" || ! -f "$WORK/code/bin/kernel" ]]; then
        printf '请先在这个 Ubuntu 窗口完成第 2 节构建。\n' >&2
        exit 1
    fi
    printf '正在运行 QEMU 测试…\n'
    bash "$E/verify.sh" run "$WORK" >"$WORK/run-output.log" 2>&1 || {
        tail -n 25 "$WORK/run-output.log"
        exit 1
    }
    printf 'QEMU 测试通过，正在跟踪首次运行…\n'
    timeout 35s bash "$E/verify.sh" debug "$WORK" >"$WORK/debug-qemu.log" 2>&1 &
    qemu_pid=$!
    trap 'kill "$qemu_pid" 2>/dev/null || true' EXIT
    sleep 1
    if ! kill -0 "$qemu_pid" 2>/dev/null; then
        cat "$WORK/debug-qemu.log"
        exit 1
    fi
    timeout 20s gdb-multiarch -q -nx -batch "$WORK/code/bin/kernel" \
        -x "$E/trace.gdb" >"$WORK/gdb.log" 2>&1 || {
        tail -n 25 "$WORK/gdb.log"
        exit 1
    }
    wait "$qemu_pid" || {
        tail -n 25 "$WORK/debug-qemu.log"
        exit 1
    }
    trap - EXIT
    grep -F '[B trace] PASS' "$WORK/gdb.log"
    grep -F '[B result] PASS' "$WORK/debug-qemu.log"
    printf '日志已生成，可以开始截图 B-02 到 B-05。\n'
)
```

看到 `[B trace] PASS`、`[B result] PASS` 和「日志已生成」后，继续第 4 节。本次测试在临时副本中运行，截图记录分工 B 的验收结果。

## 4. 每运行一组命令，截一张图

四组命令按顺序在同一个 Ubuntu 窗口运行。一组跑完，先截图并保存，再粘贴下一组。屏幕内容太多时，可以先输入 `clear` 并按 Enter，再运行该组命令。

### B-02 教师检查、PCB 和内核栈

```bash
grep -F '[Test Process Create]' "$WORK/qemu.log"
grep -E '^\[B (PCB|frame)\]' "$WORK/gdb.log"
```

**框选范围** 保留命令，从 `[Test Process Create] PASS` 截到最后一行 `tf alignment remainder=0`。画面里应看到 idle 的 PID 是 0、init 的 PID 是 1，`current==idle 1`、`parent==idle 1`，以及 `size=288`。两组地址中 `tf` 等于 `context.sp`，`stack_top` 等于 `tf.sp`。地址的具体数值随构建变化。

**保存为** `B-02-pcb-frame.png`。

### B-03 四条失败路径的资源回滚

```bash
grep -F '[B rollback]' "$WORK/qemu.log"
```

**框选范围** 保留命令与下列四行，每行最右侧的 `pages_delta=0 count_delta=0` 也要截进去。

```text
[B rollback] PCB PASS ret=-4 pcb=1/0 stack=0/0 pages_delta=0 count_delta=0
[B rollback] kernel stack PASS ret=-4 pcb=1/1 stack=1/0 pages_delta=0 count_delta=0
[B rollback] copy_mm PASS ret=-3 pcb=1/1 stack=1/1 pages_delta=0 count_delta=0
[B rollback] process limit PASS ret=-5 pcb=0/0 stack=0/0 pages_delta=0 count_delta=0
```

`pcb` 和 `stack` 的斜杠左边是分配尝试次数，右边是释放次数。

**保存为** `B-03-rollback.png`。

### B-04 首次运行的 GDB 跟踪

```bash
grep -F '[B trace]' "$WORK/gdb.log"
```

**框选范围** 保留命令，从 `[B trace] forkret pid=2` 截到 `[B trace] PASS`。中间应看到 `forkret → forkrets → __trapret → kernel_thread_entry → b_worker`。`kernel_thread_entry` 那行应有 `a0=0 SIE=1`，`b_worker` 那行应有 `arg=2413575`。

**保存为** `B-04-first-entry.png`。

### B-05 多线程 PID 和各自收到的参数

```bash
grep -E '^\[B (create|first-run|result)\]' "$WORK/qemu.log"
```

**框选范围** 保留命令与下列四行，让两个 PID 及各自的参数都能看清。

```text
[B create] PIDs 2/3/4, PID hash, explicit SP and SIE restoration PASS
[B first-run] pid=2 arg=2413575 SIE=1 PASS
[B first-run] pid=3 arg=20261120 SIE=1 PASS
[B result] PASS
```

**保存为** `B-05-multi-thread.png`。

## 5. 检查截图和报告

1. 五张截图保存后，在 Ubuntu 终端运行下面的命令，确认五个文件名都出现。

   ```bash
   ls -1 /home/kyc/project/os/NKUOS-lab4/report/evidence/B-lab4/B-*.png
   ```

2. 打开 [B-线程创建.md](B-线程创建.md) 的 Markdown 预览，找到「四、验证结果与截图」。报告已引用这五个文件，逐张确认能加载并能读到关键输出。

## 卡住时看这里

| 你看到的情况 | 接下来怎么做 |
| --- | --- |
| 粘贴后出现单独的 `>`，一直没有结果 | 按 `Ctrl+C` 回到提示符，再完整复制代码块。第 2、3 节最后一个右括号也要复制。 |
| 第 2 节没有打印 `[B fixture] built` | 看最后几行的报错。若提示找不到编译器，检查 `$HOME/.local/opt/riscv/toolchain-10.2.0/bin` 下是否有 `riscv64-unknown-elf-gcc`。 |
| 第 3 节提示端口 `12340` 已被占用 | 若还有自己之前启动的 Lab4 调试窗口，到那个窗口按 `Ctrl+C`，再回当前窗口重跑第 3 节。 |
| 关了终端，或者提示找不到 `qemu.log`、`gdb.log` | 重新打开 Ubuntu，重跑第 2、3 节，再从缺的那张图继续。 |
| 截图存到了 Windows 的默认图片目录 | 按 `Win+E` 打开文件资源管理器，将第 2 节的 WSL 图片目录粘贴到地址栏。把已截的 PNG 移到该目录。 |
