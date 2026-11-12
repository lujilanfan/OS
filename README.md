# Lab4：虚拟内存与内核线程

本分支以课程提供的 Lab4 框架为基础，补齐前置的物理内存和 Trap 代码，并完成页表映射、内核线程创建、FIFO 调度与 RISC-V 上下文切换。`report/` 保存实验报告、提示词记录和测试结果图；根目录的 `任务分工.md` 与 `实验记录.md` 分别记录建议分工和验收过程。

在 WSL 的 Ubuntu 24.04 中运行：

```sh
cd /mnt/e/LiXiaojianPersonalFolders/大学/学业/课内/大三上/操作系统/lab/lab4/lab4
make -j2
make grade
make qemu
```

`make grade` 会把 `tools/test_lab4.c` 注入临时副本，不会在正式内核源码中保留测试调用。当前三项检查的结果为 PMM、Process Create、Schedule 全部通过，总分 `30/30`。具体实现、测试条件与结果解释见 [`report/report.md`](./report/report.md)。

`init_main` 输出完成后会进入框架中的 `do_exit` 占位函数并转入内核监视器；本实验要求的页表操作、内核线程创建和调度结果在此之前已经输出并由评分脚本检查。
