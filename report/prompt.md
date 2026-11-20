# Lab4 实现规格与提示词设计

各练习先确定接口、正常行为、边界情况和检查方法，再在提示词中指定源码范围及实现约束。以下按虚拟内存、进程创建、调度切换和 Challenge 组织规格与提示词。

## 1. 初始任务

```text
请根据目录下的实验内容与实验要求、实验代码，认真完成本次实验。最后提交到 https://github.com/lujilanfan/OS 中，在这个链接中创建 lab4 分支。你可以参考一下 lab3 都是怎样提交的，需要哪些内容，仿照 lab3 的提交文件进行提交。
```

## 2. 功能模块一：虚拟内存操作

### Spec

- 输入为页表、物理页、虚拟地址和权限；插入成功返回 0，无法取得末级页表项时返回 `-E_NO_MEM`。
- 首次映射增加引用，同页覆盖仅更新权限，异页覆盖减少旧页引用并增加新页引用。
- 删除映射清空 PTE，引用归零时释放物理页；重复删除不重复释放。
- 非叶子 PTE 使用 `ptd_create`，修改映射后使旧 TLB 转换失效。
- 通过 `tools/test_lab4.c` 的映射目标、引用计数和空闲页数量断言检查上述行为。

### Prompt

```text
阅读 kern/mm/pmm.c、pmm.h、mmu.h 和 tools/test_lab4.c。实现 page_insert、page_remove 及所需的内部辅助函数。插入映射时先获得或创建末级页表项，维护物理页引用计数；覆盖旧映射时区分“同一物理页更新权限”和“替换为另一物理页”两种情况；删除映射后清空 PTE，并对相应虚拟地址执行 TLB 失效。Sv39 的非叶子页表项只设置 Valid，不设置 R/W/X/U。不得把评分测试调用写进正式内核源码。
```

## 3. 功能模块二：进程创建与初始化

### Spec

- `alloc_proc` 初始化全部 PCB 字段、内核页表地址和链表节点，新对象状态为 `PROC_UNINIT`。
- `kernel_thread` 和 `copy_thread` 建立线程函数、参数、Trap frame、内核栈和首次切换入口；线程函数类型为 `int (*)(void *)`。
- `copy_mm` 面向共享内核地址空间的线程，不复制用户页表。
- `do_fork` 成功时返回子进程 PID，失败时返回负错误码并释放已分配资源；PID 与全局链表修改位于关中断临界区。
- `proc_init` 建立 PID 0 的 idle 和 PID 1 的 init，后者的父进程为前者；以 PCB 字段及父子关系断言验证。

### Prompt

```text
根据 proc.h 的 struct proc_struct 和 tools/test_lab4.c，实现 alloc_proc、kernel_thread、copy_mm、copy_thread、do_fork 与 proc_init。初始化每个 PCB 字段和链表节点；内核线程通过 trapframe 保存入口函数与参数，经 forkret、forkrets 和 sret 进入 kernel_thread_entry；do_fork 要处理 PCB、内核栈和内存复制失败时的资源回收，并在关中断区间分配 PID、加入哈希表和进程链表；idleproc 的 PID 为 0，initproc 的 PID 为 1，且 initproc 的父进程为 idleproc。
```

## 4. 功能模块三：调度与上下文切换

### Spec

- `cpu_idle` 在 `need_resched` 置位时调用调度器。
- `schedule` 从当前进程之后循环扫描，只选择 `PROC_RUNNABLE` 对象；无可运行候选时选择 idle，目标不同于当前对象时切换。
- `proc_run` 保存中断状态，更新 `current` 和 Sv39 `satp`，刷新 TLB 并调用 `switch_to`。
- `switch_to` 的字段顺序与 `struct context` 相同，保存与恢复 `ra`、`sp`、`s0`—`s11`。
- 两个测试线程主动调用 `schedule`，断言二者均运行过。

### Prompt

```text
实现 cpu_idle、schedule、proc_run 和 switch_to。schedule 在关中断区间从 current 之后循环扫描 proc_list，只选择 PROC_RUNNABLE 进程；没有可运行候选时回到 idleproc。proc_run 更新 current，按 Sv39 格式写 satp、刷新 TLB，再切换上下文。switch_to 的保存/恢复顺序必须和 struct context 一致，保存 ra、sp、s0-s11，不保存 caller-saved 寄存器。用注入测试创建的两个内核线程验证它们都获得运行机会。
```

## 5. 集成与报告

```text
先运行未修改骨架确认评分失败，再按依赖顺序实现并执行 make -j2、make grade 和 make qemu。若出现构建或启动问题，读取完整错误与 QEMU/OpenSBI 输出，使用指令或异常日志定位根因。报告按小组模块分工编写，说明整体实现顺序、每个练习的 spec 与 prompt、核心函数、问题修复和测试结果。列出实验知识点与 OS 原理的对应关系及区别，并讨论原理中重要但本实验没有覆盖的内容。
```

## 6. Challenge 1 保存与恢复中断状态

### Spec

解释 `sstatus.SIE`、保存标志和恢复动作，说明嵌套临界区为何不提前开启中断，并区分单核临界区保护与多核互斥。

### Prompt

```text
阅读 kern/sync/sync.h 的 __intr_save、__intr_restore 及两个宏。
分别分析进入临界区前中断开启和关闭两种情况，
以两层嵌套调用解释 intr_flag 的取值与恢复结果。
说明该机制能防止哪些本地交错执行，以及为什么不能代替多核锁。
```

## 7. Challenge 2 深入理解不同分页模式的工作原理（思考题）

### Spec

说明 Sv32、Sv39、Sv48 如何拆分地址并逐级查表，比较三种模式的地址宽度、层数和每级索引。结合 Lab4 的 Sv39 实现解释 `get_pte` 的两次下降，并讨论查找与按需分配接口的取舍。

### Prompt

```text
结合 mmu.h 的 PDX1、PDX0、PTX 和 pmm.c 的 get_pte，
比较 Sv32、Sv39、Sv48 的页表结构，解释两段相似代码各自处理哪一级。
分析 create 参数怎样区分查找与创建；
讨论抽取下降一级函数或拆分查找、创建接口的收益和资源回收代价。
```
