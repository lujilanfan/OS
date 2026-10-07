# 真实截图交接清单

本目录目前没有 PNG。请在各自电脑完成操作后截取真实终端/GDB 画面，不要把本机已验证记录改写成其他成员的截图。截图应能看见命令、关键输出、操作环境或配套文字日志；敏感个人信息先遮盖，**不要改动测试结果**。最终把实际 PNG 放进本目录，并在 `../report.md` 第五节逐张引用和解释。

| 建议文件名 | 由谁提供 | 必须显示什么 |
|---|---|---|
| `lixiaojian_versions_build.png` | 李潇健 | 机器/工具版本与本人 `make` 的编译结果 |
| `lixiaojian_gdb_reset_firmware.png` | 李潇健 | GDB 的 `0x1000`、`0x80000000` 停止点，最好保留 PC 寄存器 |
| `lixiaojian_gdb_kernel_stack.png` | 李潇健 | `0x80200000` 与 `kern_init` 断点，以及 SP 从固件栈到 `bootstacktop` 的变化 |
| `yaojun_versions_build.png` | 姚均 | 本人版本、`make`、产物产生情况 |
| `yaojun_elf_image.png` | 姚均 | `readelf` 的 ELF 入口与 `file`/镜像格式，配套文本日志可写更多地址 |
| `yaojun_qemu_compare.png` | 姚均 | 原 loader 模式与实际可用启动模式的输出，标清 QEMU 版本和参数；若课程 QEMU 4.1.1 行为不同则如实记录 |
| `zhuyanze_versions_build.png` | 朱彦泽 | 本人版本、`make` 和至少一次 GDB 连接 |
| `zhuyanze_kernel_output.png` | 朱彦泽 | 本人运行的 OpenSBI 下一阶段地址及内核消息 |
| `grade_result.png` | 负责验收者 | **仅在**官方提供评分脚本且实际运行后截取；当前不能生成“通过”图 |

每位成员另外提交一份纯文本操作记录：日期、系统和工具版本、仓库提交号、完整命令、退出码、实测输出、是否与报告一致。QEMU 中内核停在无限循环，截图后用正常中断终止即可；外部超时不等于启动失败。若截图数量需要精简，可合并画面，但每个关键观察点必须仍清晰可读。
