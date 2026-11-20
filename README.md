# Lab4：虚拟内存与内核线程

本分支以课程提供的 Lab4 框架为基础，补齐前置的物理内存和 Trap 代码，并完成页表映射、内核线程创建、循环调度与 RISC-V 上下文切换。[实验报告](./report/report.md) 包含整体逻辑、各练习的 spec 与 prompt、核心函数分析、问题修复、测试结果及 OS 原理知识对照。[任务分工](./任务分工.md) 列出三人的模块责任，[实验记录](./实验记录.md) 保存构建与调试过程。

在 WSL 的 Ubuntu 24.04 中运行：

```sh
cd /mnt/e/LiXiaojianPersonalFolders/大学/学业/课内/大三上/操作系统/lab/lab4/lab4
make -j2
make grade
make qemu
```

`make grade` 会把 `tools/test_lab4.c` 注入临时副本，不会在正式内核源码中保留测试调用。当前三项检查的结果为 PMM、Process Create、Schedule 全部通过，总分 `30/30`。具体实现、测试条件与结果解释见 [`report/report.md`](./report/report.md)。

`init_main` 正常返回后，`do_exit` 打印退出码并将线程置为 `PROC_ZOMBIE`。普通启动会切回 idle，随后由原有的定时器逻辑自动关闭 QEMU。
