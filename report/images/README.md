# 截图目录说明

本目录当前只保留说明文件，没有由 AI 代做的截图。三位成员应按照 `../操作步骤与截图说明.md` 在各自电脑实际运行后，将 PNG 放入本目录，再由报告整合负责人在 `../report.md` 第五节引用。

每张图应显示真实命令和关键输出，并在图注写清：成员、系统、QEMU/GDB 版本、操作日期和 Git 提交号。建议文件名：

```text
姓名-01-versions.png
姓名-02-build.png
姓名-03-elf.png
姓名-04-qemu.png
姓名-05-reset.png
姓名-06-kernel-entry.png
姓名-07-stack-tail.png
```

三人都要提供构建、QEMU 启动和 GDB 基本连接证据；主责成员再提供对应练习的详细截图。当前缺少官方 `tools/grade.sh`，不要创建或提交伪造的 `grade_result.png`。
