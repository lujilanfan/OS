# 任务二：Best-Fit 实现提示词

```text
请阅读初始 kern/mm/best_fit_pmm.c 的 lab2 function2-2 YOUR CODE 标记、kern/mm/default_pmm.c、pmm.h 的 pmm_manager 接口和 memlayout.h 的 Page/free_area_t。实现 Best-Fit 的 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 回调及 best_fit_pmm_manager，并在 pmm.c 中切换管理器。
空闲块按地址排序，nr_free 记录页数，块首 property 记录长度并带 PG_property。分配时扫描足够大的块并选最小者，按需拆分；释放时按地址插入并与前后相邻块合并。不得修改 basic_check()、best_fit_check() 和评分测试。先解释不变量，再给最小改动和可运行的编译/QEMU 命令；未执行的测试不得写成通过。
```

# 任务三：Buddy System 实现与测试提示词

**A. 模块生成与接入提示词：**

```text
请参考现有 uCore 的 pmm.h、memlayout.h、default_pmm.c、best_fit_pmm.c 和 Makefile，从零生成 Buddy System 分配模块。初始框架没有 Buddy 文件。空闲块大小为 2^order 页；用各阶链表和页描述符元数据管理任意长度、非二次幂的初始可用区。申请 n 页时选择最小可容纳阶，必要时逐级拆分；释放时依据分配时记录的实际阶数回收，并且只合并同阶、空闲、未越界的真正伙伴。
请先说明 alloc_pages(n)/free_pages(base,n) 接口中请求页数与实际占用整阶页数如何对应、内部碎片如何计数，以及多段初始化和元数据生命周期，再给出最小代码与构建改动。不要改已有检查函数或评分脚本。提供单独选择 Buddy、自检及恢复 Best-Fit 为默认管理器的步骤。只报告真正运行过的输出。
```

**B. 独立测试生成提示词：**

```text
请针对上一步生成的 Buddy 模块写独立断言测试，不要修改 basic_check()、best_fit_check()、tools/test_lab2.c 或评分匹配条件。先列出测试的初始状态、操作序列、预期地址/阶数/空闲页计数及失败定位信息，再实现测试函数。
至少覆盖非二次幂初始区间与对齐边界、1 页和整块及跨阶申请、连续拆分、真正伙伴释放后的逐级合并、相邻但非伙伴不得合并、空闲不足和无法形成足够大连续块的失败、重复释放与越界防护、多轮操作后页数恢复。解释各用例能发现的错误以及未覆盖的并发和性能风险。给出构建与 QEMU 命令；未运行时不得宣称通过。
```

# 任务一：First-Fit 阅读提示词

```text
请只阅读、不修改 kern/mm/default_pmm.c，并结合 memlayout.h 的 Page/free_area_t 和 libs/list.h 的双向链表，分别解释 default_init、default_init_memmap、default_alloc_pages、default_free_pages。用两个不同大小的空闲块示例追踪 free_list、块首 property、PG_property、nr_free 的变化，说明为何 First-Fit 取第一个足够大的块，以及按地址排列如何帮助回收合并。最后分析查找成本、外部碎片和可改进方案，区分代码事实与推测。
```

# Lab2 实际对话记录


本节参照 [Lab1 的 `prompt.md`](https://github.com/lujilanfan/OS/blob/lab1/report/prompt.md) 编排，记录**本次对话中实际出现的用户提问**，并简述 Codex 随后的处理和验证。下列 `P1`—`P13` 引用块保留用户文字原貌，包括原有笔误与路径称呼；“目的”“结果”等字段及末尾工作链是整理说明，**不是助手答复的逐字转录**。消息的精确发送时间未保存；本节最初编写于 2026-10-21。文中 `lab2/code/`、`lab2/report/` 是当时工作区的历史路径，本分支现将对应内容放在根目录的 `code/`、`report/`。

本次使用的是 Codex 桌面应用，具体底层模型名称应由实际使用者从当时的界面或对话导出核对。

## P1：读取 Lab2 并识别任务

- 目的：了解提供的 `lab2.zip` 与展开后的源码，识别具体需要完成的内容。
- 结果：检查到 `page_init()` 尚未定义、`best_fit_pmm.c` 缺少分配器实现，评分脚本分别检查 Page Init 和 Best-Fit。用户原文中的“人物”按上下文理解为“任务”，引文保持原字。

> 读取lab2中的内容，说明要具体完成的人物

## P2：分阶段、模块化规划

- 目的：将实验拆成可并行的开发模块和依赖明确的集成、验收阶段。
- 结果：提出物理页初始化与 Best-Fit 分配器并行开发，验证准备同步进行，最后统一切换管理器、构建、启动和评分。

> 把这整个实验拆分成不同的阶段与任务，要求尽量模块化，最好可以并发进行

## P3：核对 Lab1 前置依赖

- 目的：确认是否必须先复制或完成 Lab1 的代码和报告。
- 结果：对照 Lab1 与 Lab2 目录，确认 Lab2 自带入口、控制台、链接与构建框架；Lab1 启动流程知识有帮助，但不需要把 Lab1 同名源码覆盖到 Lab2。

> 这里需要一个前置的lab1所完成内容吗

## P4：接口和编译环境基线

- 目的：核对 `pmm_manager` 接口、设备树内存接口、Makefile 与实际工具链。
- 结果：WSL 中查到 RISC-V GCC 14.2.0、QEMU 10.2.1、GNU Make 4.4.1 等工具。原始源码编译失败，分别报 `best_fit_pmm.c` 的 `free_list`、`nr_free` 未定义，以及 `pmm.c` 的 `page_init()` 未声明；这给后续实现提供了基线。

> 先确认一下现有接口和编译环境

## P5：三人任务分工文件

- 目的：把开发、集成与验证职责写成可交接的 Markdown 文档。
- 结果：建立当前的 [`任务分工.md`](../任务分工.md)，沿用 Lab1 材料中的李潇健、姚钧、朱彦泽三人姓名，将 `pmm.c`、`best_fit_pmm.c` 与集成验证分别列为主责。姓名和个人实际工作须由成员本人确认。

> 将任务分划为3个人，写入一个md中

## P6：实现物理页初始化

- 目的：完成设备树内存范围、页描述符、保留区与可用区的初始化。
- 结果：在当前 [`code/kern/mm/pmm.c`](../code/kern/mm/pmm.c) 中实现 `page_init()`，单独执行 `make obj/kern/mm/pmm.o` 成功，原先的隐式声明错误消失。

> 先完成物理页的初始化，从设备树读取内存范围；设置 `va_pa_offset`、`npage`；在内核末尾之后按页对齐放置 `pages`；标记内核和页描述符数组占用的页；调用 `init_memmap()` 登记剩余可用页。单独编译 `pmm.o` 不再出现 `page_init` 错误。

## P7：实现 Best-Fit 并接入管理器

- 目的：实现最佳适应连续页分配器及其回调接口。
- 结果：在 [`code/kern/mm/best_fit_pmm.c`](../code/kern/mm/best_fit_pmm.c) 中增加空闲链表、选块、拆分、按地址释放与相邻块合并，定义 `best_fit_pmm_manager`；`basic_check()`、`best_fit_check()` 保持原样。`make obj/kern/mm/best_fit_pmm.o` 与完整构建均成功；`pmm.c` 选择新管理器。随后运行评分时，发现另有启动兼容问题。

> 适配pmm.c， 定义 `free_area`、`free_list`、`nr_free` 和 `best_fit_pmm_manager`；实现 `init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`；分配时选能容纳请求的最小空闲块，释放时按地址插入并合并相邻块。不得改动已有的 `basic_check()`、`best_fit_check()`。单独编译 `best_fit_pmm.o` 无错误。

## P8：修复启动兼容问题

- 目的：让当前 QEMU 10.2.1 正确进入内核，并使页表启用后的代码继续运行。
- 结果：原 `-device loader` 参数下，OpenSBI 显示下一阶段入口为 0；改为 `-kernel` 后能转到 `0x80200000`，但执行到 `satp` 后无内核输出。给 [`code/kern/init/entry.S`](../code/kern/init/entry.S) 的启动页表增加低地址恒等映射，并同步修改 Makefile 与 `tools/grade.sh` 的装载参数后，QEMU 打印 `check_alloc_page() succeeded!`，本地评分达到 20/20。评分断言和 `tools/test_lab2.c` 未改动。

> 处理一下启动兼容问题

## P9：整理集成与验证记录

- 目的：核对测试脚本、编译和运行命令，保存实际结果及排错过程。
- 结果：写成当前的 [`实验记录.md`](../实验记录.md)，列出接口、测试清单、QEMU 输出、首次 0/20 的原因和修复后的 20/20。该文档最初写在当时的 `lab2_new/` 目录中；之后工作区源码现为 `lab2/code/`，记录已同步到当前路径。

> 接下来进行`lab2_new/` 下的实验记录文档；集成阶段接管 `pmm.c` 的管理器选择行 | 开发期间核对 `tools/test_lab2.c`、`tools/grade.sh` 和构建命令，整理测试清单。两份实现完成后，将 `init_pmm_manager()` 切换到 `best_fit_pmm_manager`，运行编译、QEMU 自检和 `make grade`；记录命令、结果、失败定位及修复过程。

## P10：重新验证集成与内核自检

- 目的：在已接入 Best-Fit 的当前源码上重新执行完整编译和 QEMU 自检。
- 结果：确认 `pmm.c` 已选用 `best_fit_pmm_manager`；`make -B -j2` 强制重编译成功。短时运行 `make qemu` 后观察到设备树 128 MB 内存、管理器名称和 `check_alloc_page() succeeded!`。限时停止的退出码 124 来自 `timeout`。

> 尝试进行集成，在pmm.c中切换至best_fit_pmm_manager，编译并处理接口问题。直到内核成功编译启动，并完成自检

## P11：核对评分两项输出

- 目的：重新运行 `make grade`，确认 Page Init 与 Best-Fit 的验收结果。
- 结果：最初的命令指向已不在原位置的 `lab2_new/`，WSL 返回路径错误；检查工作区后发现源码当前位于 `lab2/code/`。在该目录运行 `make grade`，两项均为 OK，总分 20/20。随后同步修正分工与实验记录中的旧路径。

> 运行 `make grade`，核对两项输出  

## P12：按 Lab1 样式完成实验报告

- 参考文件：`OS-lab1/OS-lab1/report/report.md`，仅作为章节结构与写法参考。
- 目的：在 `code/` 同级建立 `report/` 并撰写 Lab2 报告。
- 结果：建立 [`report.md`](./report.md)，写入目的、环境、整体逻辑、物理页初始化、Best-Fit、启动兼容调整、测试结果和总结；个人操作、截图与 AI 信息的未核对部分据实标明。

> 仿照这份的样式，完成实验报告，和code同级另开一个文件夹并命名为report

## P13：保存本次对话的提示词与结果

- 目的：按 Lab1 `prompt.md` 的提交样式保存本次对话中实际出现的用户提问及处理脉络。
- 结果：形成本文件 `lab2/report/prompt.md`。本条的助手答复和之后可能出现的追问尚不属于编写时已完成的对话记录；若课程需要全量逐字内容，请以实际会话导出为准。

> 把我们在这个对话的内容写到一个md中，也放到report，命名为prompt。要上传的格式部分参考lab1中的prompt.md

## 实际分析—实现—测试—修正链

1. 读取给定 Lab2 源码与评分脚本，确定 `page_init()` 和 Best-Fit 管理器是两个缺失模块；核对 `pmm_manager` 回调与设备树接口。
2. 在 `pmm.c` 建立页描述符与保留区；在 `best_fit_pmm.c` 建立按地址排序的空闲链表、最小可用块选择、拆分和相邻块合并。两个对象文件及完整内核均编译成功。
3. 首次 `make grade` 得到 0/20，评分输出缺少两项 PASS。串口仅有 OpenSBI，`Domain0 Next Address` 为 0，说明内核尚未运行到测试，而非两项断言已执行后失败。
4. 改用 QEMU `-kernel` 后，OpenSBI 下一阶段入口变为 `0x80200000`。指令轨迹显示在低地址写入 `satp` 后无后续内核指令；对照页表发现只有高地址映射，于是补充低地址恒等映射。
5. 再次构建并启动，看到 `DTB Init`、128 MB RAM、`memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`。最终 `make grade` 输出：

   ```text
   -check page init:                          OK
   -check best fit:                           OK
   Total Score: 20/20
   ```

6. 工作区目录后来从 `lab2/lab2_new/` 变为 `lab2/code/`，实验记录移至 `lab2/`，报告置于 `lab2/report/`。核对当前目录后再次运行 `make grade`，仍为 20/20；所有现用文档中的命令路径已更新。

以上工作链是根据本会话可见记录、当前源码和终端输出整理的摘要。用户引文之外的说明不应冒充逐字助手答复；本文件也不证明三位成员分别编写过代码、使用过 AI 或在不同电脑上复现实验。正式提交时由实际使用者核对账号、模型、原始对话和课程要求。
