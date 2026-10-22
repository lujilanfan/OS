# Lab2：物理内存管理

本分支只保留 Lab2 内容：`code/` 为 RISC-V 内核与评分脚本，`report/` 为实验报告和截图，根目录的 [`任务分工.md`](./任务分工.md)、[`实验记录.md`](./实验记录.md)记录分工与验证过程。

在配置好 `riscv64-unknown-elf-` 工具链与 `qemu-system-riscv64` 的 WSL 中，从仓库根目录运行：

```bash
cd code
make -j2
make grade
```

2026-10-22 从此目录运行 `make grade`，Page Init 和 Best-Fit 两项均为 `OK`，总分 `20/20`。实现与测试边界详见 [`report/report.md`](./report/report.md)。
