# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab4：虚拟内存与内核线程 |
| 小组成员 | 2412932 李潇健、2410718 姚钧、2410764 朱彦泽 |
| 完成日期 | 2026-11-12 |

实验内容分为页表映射、进程创建和进程调度三个模块，三者依次构成从内存初始化到内核线程运行的完整路径。

## 一、实验目的

1. 在 Sv39 页表结构上实现物理页与虚拟地址之间的映射插入、替换和删除，理解页表项、引用计数与 TLB 的一致性要求。
2. 建立内核线程所需的进程控制块、内核栈、Trap frame 和执行上下文，完成 `idleproc` 与 `initproc` 的创建。
3. 实现可运行进程的循环选择和 RISC-V 上下文切换，使多个内核线程能够依次使用处理器。
4. 结合评分器和 QEMU 输出验证页表操作、进程初始化与调度路径。

## 二、实验环境

| 项目 | 版本或配置 |
| --- | --- |
| 宿主环境 | Windows + WSL2 |
| Linux 发行版 | Ubuntu 24.04 |
| 交叉编译器 | `riscv64-unknown-elf-gcc 13.2.0` |
| 模拟器 | QEMU 8.2.2，`virt` 机器，OpenSBI v1.3 |
| 构建工具 | GNU Make 4.3、Python 3.12.3 |
| AI 工具 | OpenAI Codex 桌面应用，GPT-5 |

## 三、实验整体逻辑分析

内核线程运行依赖一条连续的初始化路径。物理内存管理器先从设备树读取 128 MiB 内存范围，建立页描述符和空闲块；Sv39 根页表提供内核高地址映射，并在开启分页的过渡阶段保留低地址恒等映射。页表操作接口初始化完成后，SLOB 分配器才能为 `proc_struct` 和内核栈分配空间。

`proc_init` 先构造不经过 `do_fork` 的 `idleproc`，再由 `kernel_thread` 组织临时 Trap frame，并通过 `do_fork` 创建 `initproc`。子线程第一次被选中时，`switch_to` 恢复其 `context.ra` 和 `context.sp`，执行流进入 `forkret`，随后由 `forkrets` 和 `sret` 恢复 Trap frame，最终到达 `kernel_thread_entry` 调用线程函数。

调度器从当前进程之后开始遍历 `proc_list`，跳过不可运行进程并选择下一个 `PROC_RUNNABLE` 进程。`proc_run` 切换当前进程和 `satp`，汇编函数保存旧进程的 `ra`、`sp`、`s0` 至 `s11`，再恢复新进程的同组寄存器。三个模块的依赖顺序为“页表与内存分配 → 进程创建 → 调度与上下文切换”。

## 四、实验内容与实现

### 4.1 虚拟内存操作

涉及函数：

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

`page_insert` 先调用 `get_pte` 获取末级页表项。新物理页的引用计数在修改映射前增加，以便覆盖旧映射时统一处理。如果原页表项指向同一物理页，只撤销刚增加的一次引用并更新权限；如果指向另一物理页，则由 `page_remove_pte` 减少旧页引用，引用数归零时释放该页。新 PTE 写入后，`sfence.vma` 使对应地址的旧 TLB 项失效。

`page_remove` 在页表项存在时清除映射并维护引用计数；对未映射地址重复调用不会释放额外页面。`get_pte` 创建的两级中间页表项使用 `ptd_create`，只设置 Valid 位，末级叶子项再根据 `perm` 设置读、写、执行或用户权限。

### 4.2 进程创建与初始化

涉及函数：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t stack,
                        struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

`alloc_proc` 将新 PCB 置为 `PROC_UNINIT`，PID 置为 -1，并清零运行次数、调度标志、内存描述、上下文和名称，同时初始化进程链表与 PID 哈希链表节点。内核线程共享内核地址空间，因此 `copy_mm` 在本实验中只检查 `current->mm == NULL`，不复制用户页表。

`kernel_thread` 把线程函数和参数分别放入 Trap frame 的 `s0`、`s1`，设置 S 态返回状态和 `kernel_thread_entry`。`copy_thread` 将 Trap frame 放在子进程内核栈顶，并把新上下文的返回地址设为 `forkret`。`do_fork` 依次分配 PCB、内核栈并构造上下文；PID 分配和全局链表插入在关中断区间完成，失败路径分别释放已经取得的资源。

`proc_init` 直接建立 PID 为 0 的 `idleproc`，再创建 PID 为 1、父进程为 `idleproc` 的 `initproc`。注入测试检查了两个进程的状态、父子关系、内核栈和上下文字段。

### 4.3 调度与上下文切换

涉及函数：

```c
void cpu_idle(void);
void schedule(void);
void proc_run(struct proc_struct *proc);
void switch_to(struct context *from, struct context *to);
```

`cpu_idle` 在 `need_resched` 置位时调用调度器。`schedule` 关闭本地中断，从当前进程之后循环扫描 `proc_list`；遇到链表头时继续回绕，只接受 `PROC_RUNNABLE` 状态，找不到其他候选时选择 `idleproc`。选中进程的 `runs` 增加，目标与当前进程不同时调用 `proc_run`。

`proc_run` 更新 `current`，以 Sv39 模式和页表物理页号组成 `satp`，刷新 TLB 后进入 `switch_to`。RISC-V 调用约定要求被调用者保持 `s0` 至 `s11`、`sp` 和返回地址，因此 `switch_to` 按 `struct context` 的字段顺序保存旧值并恢复新值；`ret` 使用新进程的 `ra` 继续执行。

### 4.4 Challenge：本地中断状态保存与恢复

`local_intr_save(intr_flag)` 读取 `sstatus.SIE`。如果进入临界区前中断已开启，函数关闭中断并把 `intr_flag` 记为 1；如果本来已关闭，则记为 0。`local_intr_restore(intr_flag)` 只在标志为 1 时重新开启中断。因此嵌套临界区中的内层调用不会提前打开外层已经关闭的中断。该方法保护单核上的短临界区，不能代替多核环境中的自旋锁。

### 4.5 Challenge：分页层次与 `get_pte`

Sv32 使用两级页表，每级索引 10 位；Sv39 和 Sv48 分别使用三级、四级页表，每级索引 9 位，页内偏移均为 12 位。当前 Sv39 `get_pte` 中两段相似代码分别处理根页表到中间页表、中间页表到末级页表：读取当前级索引，检查 Valid；需要创建时分配一页、清零并写入非叶子 PTE。它们相似的原因是多级页表在每一级都执行相同的“索引—检查—下降”操作，差别只在层数和移位值。

查找与按需分配合在 `get_pte(pgdir, la, create)` 中，调用者可以用同一接口完成只查找和查找并创建，适合本实验固定的 Sv39 结构。若继续支持 Sv32、Sv48 或需要细化失败回收，可拆成“下降一级”和“取得末级 PTE”两个函数，再用层数循环复用；代价是接口和错误路径更复杂。对当前规模而言保留单一入口较直接，但中间页表项的标志必须集中处理，避免把叶子权限写入非叶子项。

## 五、测试与验证

执行命令：

```sh
make clean
make -j2
make grade
make qemu
```

评分脚本在临时副本中注入 PMM、进程创建和调度测试。三项检查均返回 `OK`，总分为 `30/30`。

![make grade 实际输出片段](./images/grade.png)

普通启动读取到 128 MiB 物理内存，物理页、页目录、SLOB 与 VMA 自检依次完成。调度启动后，PID 为 1 的 `init` 内核线程打印了预设消息。

![make qemu 实际输出片段](./images/qemu.png)

测试覆盖了映射替换、重复删除、PCB 字段检查以及多个线程获得运行机会等场景。上述结论以当前实验框架、工具版本和测试输入为准。

## 六、实验总结

页表映射不仅是写入物理页号，还要同步维护页引用计数和 TLB 状态。覆盖同一页、覆盖不同页和删除空映射的处理顺序不同，任何一步遗漏都会造成重复释放或旧地址转换继续生效。Sv39 中间页表项与叶子项的权限含义也不同，非叶子项只承担指向下一级页表的作用。

内核线程的创建和运行由 PCB、内核栈、Trap frame 与 `context` 共同完成。`do_fork` 建立静态结构，`schedule` 决定运行对象，`switch_to` 恢复最小上下文，`sret` 再恢复完整 Trap frame。将这几段执行路径连起来后，可以解释新线程为何从 `kernel_thread_entry` 开始，以及为什么只保存 callee-saved 寄存器即可完成协作式切换。

本实验未涉及独立用户地址空间、用户程序装载、系统调用、进程等待与资源回收，也没有实现多核调度和锁。它建立的是后续用户进程和更完整调度机制所需的内核线程基础。
