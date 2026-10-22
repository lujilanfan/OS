# 操作系统实验报告

> 当前状态：Lab2 的物理页初始化、Best-Fit 分配器和本机 QEMU 启动适配已完成。当前 `code/` 可构建、启动并通过内核自检；本地 `make grade` 的 Page Init 与 Best-Fit 两项均为 OK，总分 20/20。以下执行记录来自本工作环境，不表示三名组员分别在各自电脑上完成了相同操作。

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab2：物理内存管理与 Best-Fit 连续页分配 |
| 小组成员 | 2412932-李潇健、2410718-姚钧、2410764-朱彦泽（沿用 Lab1 报告中的小组信息，提交前由成员核对） |
| 源码目录 | 本分支根目录的 `code/` |
| 报告目录 | 本分支根目录的 `report/` |
| 本稿修订日期 | 2026-10-22 |
| 最终完成日期 | 三名成员核对分工、个人操作记录和课程提交要求后填写 |
| 验证结果 | `make grade`：Page Init OK、Best-Fit OK，20/20 |

### 小组分工与证据范围

下表参照 Lab1 报告的答辩分工写法，沿用[Lab2 任务分工](../任务分工.md)中的模块安排，并给出提交前各人要核对的材料。它是工作与讲解任务的分配；当前截图和本机验证不能证明三人分别编写或运行过代码。个人实现、操作及 AI 使用情况须由成员本人确认。

| 成员 | 模块与答辩主责 | 提交前个人核对任务 | 共同必会 |
| --- | --- | --- | --- |
| 2412932-李潇健 | 物理页初始化；本报告“任务 1” | 逐行核对 `page_init()` 与图 2；解释设备树范围、`pages` 位置、保留页和空闲页边界；核对评分测试对初始化的实际检查项 | 启动到 `pmm_init()` 的流程、内核自检与两项评分结果 |
| 2410718-姚钧 | Best-Fit 管理器；本报告“任务 2” | 核对图 3—5 与 `best_fit_pmm.c`；讲清选块、拆分、按地址插入和前后合并；确认原有 `basic_check()`、`best_fit_check()` 未被改动 | 管理器六个回调、内核自检与两项评分结果 |
| 2410764-朱彦泽 | 管理器集成、启动适配与验证；本报告“启动兼容调整”和第五章 | 核对管理器选择行、图 6—11、`Makefile` 与 `grade.sh`；从最终源码复核构建、QEMU 自检、`make grade` 及失败修复记录；检查图片和提交材料 | 页初始化与 Best-Fit 原理、内核自检与两项评分结果 |

三人分别准备上述讲解，并共同检查报告结论与源码、截图是否一致。若实际代码编写或终端操作的人员与此分工不同，应如实更新记录，不能仅凭主责表推定个人贡献。

---

## 一、实验目的

1. 理解物理内存如何以固定大小的页为单位管理，以及页描述符与实际物理页的对应关系。
2. 从设备树读取可用物理内存范围，建立页描述符数组，保护内核及管理元数据占用的物理页。
3. 实现 Best-Fit 连续页分配：在所有足够大的空闲块中选择最小者，并维护分配、释放和合并后的空闲页计数。
4. 理解内核启动映射、管理器接口和自检之间的依赖，用编译、QEMU 输出和评分脚本验证实现。

本实验处理内核启动阶段的**物理页分配**。当前代码尚未实现进程地址空间、缺页处理或通用用户态内存分配，不能将这些功能写作本次成果。

---

## 二、实验环境

### 2.1 本机已验证环境

| 项目 | 实测值 | 用途 |
| --- | --- | --- |
| 宿主与执行环境 | Windows 上的 WSL Ubuntu | 在 Linux 终端中构建和运行实验 |
| 交叉编译器 | `riscv64-unknown-elf-gcc` 14.2.0 | 编译 RISC-V 内核 |
| QEMU | `qemu-system-riscv64` 10.2.1 | 运行 `virt` 机器及 OpenSBI |
| GNU Make | 4.4.1 | 构建内核、启动 QEMU、运行评分 |
| 其他工具 | Python 3、tar、timeout；RISC-V ld、objcopy、objdump | 注入评分测试、处理镜像和限时验证 |

本分支将 Lab2 源码放在根目录的 `code/`，命令在 WSL 中执行；报告截图拍摄于整理前的工作区路径 `lab2/code/`。当前 Makefile 生成 `bin/kernel`（ELF）和 `bin/ucore.img`（裸镜像）；QEMU 使用 `-kernel bin/ucore.img` 交给 OpenSBI 启动。

![WSL 实验目录及 GCC、QEMU、Make 版本](./images/1.png)

图 1：WSL 中的 `lab2/code` 路径，以及交叉编译器、QEMU 和 GNU Make 的版本输出。

### 2.2 AI 辅助与操作记录

当前工作会话使用 Codex 辅助阅读源码、实现代码、分析 QEMU 兼容问题并整理报告。用户提问原文及工作链见 [`prompt.md`](./prompt.md)；具体模型名称、各成员本人是否使用 AI，以及课程是否要求完整对话导出，须由实际使用者核对。本报告所附图片是用户保存到 `report/images/` 的源码和终端截图；它们证明图中所示操作与输出，不表示三名组员分别完成了相同操作。

---

## 三、实验整体逻辑分析

### 3.1 从启动到内存管理

```text
交叉编译 + 链接脚本 → bin/kernel → objcopy → bin/ucore.img
    ↓ QEMU -kernel，OpenSBI 将控制权交给内核
kern_entry：保存启动参数 → 启用 Sv39 启动页表 → 设置高地址内核栈 → 进入 kern_init
    ↓
dtb_init：从设备树取得 RAM 起点和大小
    ↓
pmm_init：选择 Best-Fit 管理器 → page_init 建立页描述符与空闲区
    ↓
check_alloc_page：执行 best_fit_check 的断言自检
    ↓
内核打印成功信息，随后进入常驻循环
```

启动页表同时映射 `0x80000000` 起的低地址窗口和对应的内核高地址窗口。写入 `satp` 后，PC 尚在低地址，必须能继续取指；跳转到高地址后，内核再通过高地址映射访问同一物理内存。该映射窗口的大小为 1 GiB；设备树在本次 QEMU 启动中实际报告的 RAM 为 128 MB，二者不能混为一谈。

### 3.2 页描述符与分配器接口

`struct Page` 记录一个物理页的引用计数、状态标记、空闲块长度和链表节点。`pages[i]` 对应从 `DRAM_BASE` 起第 `i` 个物理页；`page2pa()` 与 `pa2page()` 在页描述符和物理地址之间转换。`PG_reserved` 表示页不可分配，`PG_property` 仅标记空闲连续块的首个页，其 `property` 保存该块页数。

`pmm.c` 通过 `struct pmm_manager` 的六个回调使用分配器：`init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages` 和 `check`。当前 `init_pmm_manager()` 选择 `best_fit_pmm_manager`；因此上层的 `alloc_pages()`、`free_pages()` 调用会转交给 Best-Fit 实现。

---

## 四、实验内容与实现

### 任务 1：物理页初始化

**分工负责人：** 2412932-李潇健（负责核对本节并准备讲解；实际实现经历由本人确认）。

**对应源码：**[`code/kern/mm/pmm.c`](../code/kern/mm/pmm.c)、[`code/kern/driver/dtb.c`](../code/kern/driver/dtb.c)。

`dtb_init()` 解析设备树的内存节点，`page_init()` 通过 `get_memory_base()`、`get_memory_size()` 取得物理内存边界。本次启动输出为 `Base=0x80000000`、`Size=0x08000000`（128 MB），所以结束地址的下一字节是 `0x88000000`；`npage=PPN(0x88000000)=0x88000`。地址映射偏移量设为 `PHYSICAL_MEMORY_OFFSET`，供 `PADDR()` 等转换使用。

页描述符数组 `pages` 放在内核链接符号 `end` 之后，地址按 4 KiB 向上对齐：

```text
pages = ROUNDUP(end, PGSIZE)
page_count = npage - nbase
free_start = max(ROUNDUP(PADDR(pages + page_count), PGSIZE),
                 ROUNDUP(mem_base, PGSIZE))
free_end = ROUNDDOWN(mem_base + mem_size, PGSIZE)
```

先把 `page_count` 个页描述符全部标记为 `PG_reserved`，再将 `[free_start, free_end)` 对应的完整页交给 `init_memmap()`。因此固件所在低地址页、内核镜像、启动栈、页表和 `pages` 数组均不会从空闲链表分配出去。代码还断言内存范围有效、描述符数组位于设备树报告的内存中并且空间足够。

本次 ELF 的 `end=0xffffffffc0206078`，由此可**推算** `pages` 的起始虚拟地址为 `0xffffffffc0207000`、物理地址为 `0x80207000`。这些数组地址是根据符号与代码计算得到，并非 QEMU 单独打印的测量值。

![pmm.c 中的 page_init 实现](./images/3.png)

图 2：`pmm.c` 的 `page_init()`：从设备树读取内存范围，设置地址偏移与页数，在内核末尾对齐放置 `pages`，标记保留页并登记剩余空闲页。

### 任务 2：Best-Fit 连续页分配

**分工负责人：** 2410718-姚钧（负责核对本节并准备讲解；实际实现经历由本人确认）。

**对应源码：**[`code/kern/mm/best_fit_pmm.c`](../code/kern/mm/best_fit_pmm.c)。

分配器以 `free_area` 保存按地址升序排列的双向空闲链表和空闲**页数** `nr_free`。`init_memmap()` 清理将要释放的页描述符，再登记初始空闲块。登记或释放时，新块按地址插入；若前块的结束地址恰好等于新块起点，先与前块合并；若合并后的结束地址恰好等于后块起点，再与后块合并。只有带 `PG_property` 标记的空闲块首，其 `property` 长度才有效；合并后失去块首身份的页可能仍留有旧数值。

`alloc_pages(n)` 扫描链表，找出 `property >= n` 的候选块，选长度最小的一个；若大小相同，选择链表中先遇到的较低地址块。找到长度恰好为 `n` 的块即可结束扫描。若选中块更大，前 `n` 页交给调用者，余下连续页重新作为空闲块插回原位置；最后扣减 `nr_free`。如果总空闲页数不足，或虽有足够总量却没有一个足够大的连续块，函数返回 `NULL`。

![Best-Fit 分配时选择和拆分空闲块](./images/4.png)

图 3：`best_fit_alloc_pages()` 扫描候选块，选择满足请求的最小块，并在需要时拆分余块。

![按地址插入并合并相邻空闲块](./images/5.1.png)

图 4：`insert_free_block()` 按地址插入空闲块，分别检查并合并前后相邻块。

![释放物理页并登记空闲块](./images/5.2.png)

图 5：`best_fit_free_pages()` 清理被释放页的状态，再调用 `insert_free_block()` 登记和合并。

`best_fit_pmm_manager` 填齐六个管理器回调，`pmm.c` 将其设为当前管理器。框架原有的 `basic_check()`、`best_fit_check()` 保持不变。自检覆盖基础分配、空链表、不同大小块的选择、释放后的合并和空闲页计数等行为。

### 启动兼容调整

**分工负责人：** 2410764-朱彦泽（负责核对集成、启动与验证材料；实际操作经历由本人确认）。

原启动页表只有内核高地址映射。QEMU 在低地址执行到 `csrw satp` 后，下一条低地址指令无法继续取指。`entry.S` 为 `0x80000000` 起的地址窗口加入恒等映射，同时保留原高地址映射，使内核能够在启用分页后跳转到高地址代码。

原 Makefile 和评分脚本使用 `-device loader,file=bin/ucore.img,addr=0x80200000`。在本机 QEMU 10.2.1 中，OpenSBI 的 `Domain0 Next Address` 为零，未进入内核。将 `Makefile` 的 `qemu`、`debug`、`test` 目标及 `tools/grade.sh` 的装载选项改为 `-kernel bin/ucore.img` 后，OpenSBI 报告下一阶段入口为 `0x80200000`。评分项目和 `tools/test_lab2.c` 未修改。

![entry.S 启用 Sv39 页表并跳转到内核高地址](./images/6.1.png)

图 6：`entry.S` 写入 `satp`、刷新 TLB，随后跳转到高地址的 `kern_init`。

![entry.S 中的低地址和高地址启动映射](./images/6.2.png)

图 7：启动页表同时包含 `0x80000000` 起的恒等映射和内核高地址映射。

![Makefile 和评分脚本使用 QEMU 的 kernel 装载参数](./images/6.3.png)

图 8：`Makefile` 的 QEMU 目标与 `tools/grade.sh` 均使用 `-kernel` 装载镜像。

### 核心模块关系

| 模块 | 作用 | 可观察结果 |
| --- | --- | --- |
| `kern/init/entry.S`、`tools/kernel.ld` | 定义入口、栈与启动映射，确定高地址链接布局 | 内核入口符号 `0xffffffffc0200000`；QEMU 进入内核 |
| `kern/driver/dtb.c` | 解析设备树内存节点 | 输出 128 MB RAM 范围 |
| `kern/mm/pmm.c` | 建立描述符、标记保留页、选择分配器并启动自检 | 输出 `best_fit_pmm_manager` 与自检成功消息 |
| `kern/mm/best_fit_pmm.c` | 管理空闲块、分配和回收连续物理页 | 自检断言全部通过 |
| `tools/inject.py`、`tools/test_lab2.c`、`tools/grade.sh` | 在临时副本注入测试并核对输出 | 本地评分 20/20 |

---

## 五、测试与验证

### 5.1 本机执行命令

在 WSL 的课程目录中执行：

```bash
cd code
make obj/kern/mm/pmm.o
make obj/kern/mm/best_fit_pmm.o
make -j2
timeout 5s make qemu
make grade
```

单文件编译与完整 `make -j2` 均成功，生成 `bin/kernel` 和 `bin/ucore.img`。`make grade` 会清理开发目录的构建产物，并在 `/tmp/lab2_grade` 临时副本中注入测试、重新构建和启动；评分后若要再次执行 `make qemu`，须重新构建。内核初始化后进入常驻循环，所以上面的 QEMU 命令由 `timeout` 在 5 秒后停止，其退出码 124 是预期的限时停止。

![全量编译和内核镜像产物](./images/2.png)

图 9：`make -B -j2` 完成编译与链接，生成 `bin/kernel` 和 `bin/ucore.img`；符号表显示内核入口、启动页表和 `end` 地址。

| 验证项 | 实际结果 | 证据位置 |
| --- | --- | --- |
| 工具链与目录 | GCC 14.2.0、QEMU 10.2.1、Make 4.4.1；截图时工作目录为 `lab2/code`，本分支运行目录为 `code` | 图 1 |
| 两个模块单独编译 | `pmm.o` 与 `best_fit_pmm.o` 均成功 | [实验记录](../实验记录.md)；本次截图没有单独显示这两条命令 |
| 全量构建 | 生成 `bin/kernel`、`bin/ucore.img` | 图 9 |
| QEMU 自检 | 读取 128 MB RAM，选择 Best-Fit，打印自检成功 | 图 10 |
| 本地评分 | Page Init 与 Best-Fit 均为 `OK`，20/20 | 图 11 |

### 5.2 QEMU 启动与自检

最终 `make qemu` 的关键输出如下：

```text
DTB Init
HartID: 0
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0205000
satp physical address: 0x0000000080205000
```

`check_alloc_page() succeeded!` 只有在当前管理器的 `check` 回调，即 `best_fit_check()`，正常返回后才会打印。它证明现有自检路径完成，不能据此推断所有可能的分配序列均已被穷举测试。

![QEMU 中设备树内存范围与 Best-Fit 自检输出](./images/7.png)

图 10：QEMU 运行时打印设备树内存范围、当前管理器 `best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`。截图未包含启动命令和后续 `satp` 输出；上方文本记录了完整关键输出。

### 5.3 评分结果与测试覆盖

图 11 拍摄于原工作区 `lab2/code/`；2026-10-22 在本分支根目录的 `code/` 重新执行 `make grade`，得到相同的两项评分结果：

```text
lab2 injection complete
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

`test_page_init()` 检查地址偏移、设备树范围、`npage`、`pages` 的对齐与位置、空闲页数非零且不超过总页数、内核末尾页与描述符数组**起始页**的保留标记，以及页描述符与物理地址的往返转换。源码中的循环会把数组对应的全部页描述符先设为保留；注入测试只抽查上述两个保留标记，不能把本地评分写成逐页穷举验证。`test_best_fit()` 直接检查当前管理器指针是否为 `best_fit_pmm_manager`；具体分配行为还依赖启动时的 `best_fit_check()` 断言。QEMU 内部先输出两项 `PASS`，评分脚本匹配后在主机终端显示 `OK`。

![make grade 两项检查通过并得到 20 分](./images/8.png)

图 11：执行 `make grade` 后，Page Init 和 Best-Fit 两项均为 `OK`，总分为 `20/20`。

### 5.4 失败定位及修复

| 阶段 | 实际现象 | 定位与处理 |
| --- | --- | --- |
| 原始框架 | `best_fit_pmm.c` 的 `free_list`、`nr_free` 未定义；`pmm.c` 的 `page_init()` 未声明 | 实现两个模块后，单文件和完整构建成功 |
| 首次评分 | 两项均缺少 PASS，0/20；串口只有 OpenSBI | 原 `-device loader` 在当前 QEMU 中使下一阶段入口为零；改用 `-kernel` |
| 更换装载参数后 | OpenSBI 报告入口 `0x80200000`，但没有内核输出；指令轨迹停在低地址的 `csrw satp` | 结合原页表只有高地址映射，判断低地址后续取指失败；加入低地址恒等映射 |
| 最终验收 | 内核打印自检成功；评分两项 OK | 20/20 |

这里的“轨迹停在 `csrw satp`”是 QEMU 指令轨迹的观察；“低地址缺少映射导致后续取指失败”是结合页表内容作出的判断。改动后的启动输出和评分结果提供了修复有效的运行证据。

本报告已附用户保存的 11 张源码与终端截图。截图与操作步骤的对应关系见[本机操作与人工截图指南](./操作步骤与截图说明.md)；各成员的个人操作和截图归属仍需按实际情况确认。

---

## 六、实验总结与收获

1. **内存边界决定可分配范围。** 设备树给出 RAM 范围，链接符号 `end` 给出内核占用范围，页描述符数组还会消耗物理内存。只有这些区域之后的完整页才能加入空闲链表。
2. **Best-Fit 依赖准确的块元数据。** 块首长度、空闲页计数和按地址排序的链表必须同步维护；拆分或合并后若有一项错误，后续分配可能重叠、遗漏或错误报告可用页数。
3. **启动映射是内存管理的前提。** 内核尚未运行到 `page_init()` 时，页表切换和镜像装载就可能使执行中断。通过对照 OpenSBI 入口、QEMU 指令轨迹和页表内容，才能将启动故障与分配器故障区分开。
4. **自检与评分各有边界。** 内核断言验证给定分配序列，评分脚本检查特定初始化状态和输出；20/20 表示这套本地检查通过，不代表已经评估性能、并发访问或所有边界输入。

### AI 协作开发的经验

本次 AI 辅助工作涉及源码阅读、实现和故障分析。最需要核对的两类结论分别是初始化边界与启动故障：前者用 `pmm.c` 和注入测试逐项对照，后者用 `entry.S` 的页表项、OpenSBI 入口与 QEMU 自检结果交叉验证。报告中的事实以当前源码、实际命令输出及评分结果为准；涉及个人操作、提示词和课程提交格式的内容仍应由小组按真实情况核对。

## 提交前最终核对

- [x] `pmm.c` 选择 `best_fit_pmm_manager`；两个模块可以单独编译，完整内核可以构建。
- [x] QEMU 启动并打印 `check_alloc_page() succeeded!`。
- [x] 本地 `make grade` 的 Page Init 与 Best-Fit 均为 OK，总分 20/20。
- [ ] 小组核对成员信息、实际分工、个人操作和 AI 使用记录。
- [x] 将现有真实源码与终端截图放入报告对应位置，并核对图片路径。
- [ ] 若课程要求，补充完整对话导出、仓库提交号及指定模板内容；核对现有 `prompt.md` 的账号和模型信息。

个人复核任务：李潇健确认任务 1 的源码、图 2 和初始化测试解释；姚钧确认任务 2 的源码、图 3—5 和原有自检范围；朱彦泽确认启动与集成、图 6—11、构建及评分记录。三人共同核对姓名、真实贡献、AI 使用情况和最终提交要求，并在确认后填写最终完成日期。

## 依据与核对范围

本报告的结构参考 [Lab1 报告](https://github.com/lujilanfan/OS/blob/lab1/report/report.md)；Lab2 的技术事实依据当前 `code/` 源码、[`实验记录`](../实验记录.md)、本机 WSL/QEMU 输出及 `tools/test_lab2.c`、`tools/grade.sh`。Lab1 报告仅用作写作样式参考，其中的 Lab1 练习、环境版本和未完成事项未作为 Lab2 的要求。逐项核对与待确认问题见[修改说明](./修改说明.md)。本报告尚未核对课程是否另有 Lab2 指导书或新的提交公告；若存在，应以其正式要求补充核查。
