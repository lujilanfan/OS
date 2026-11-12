# Lab4 提示词记录

## 1. 初始任务

```text
请根据目录下的实验内容与实验要求、实验代码，认真完成本次实验。最后提交到 https://github.com/lujilanfan/OS 中，在这个链接中创建 lab4 分支。你可以参考一下 lab3 都是怎样提交的，需要哪些内容，仿照 lab3 的提交文件进行提交。
```

## 2. 功能模块一：虚拟内存操作

```text
阅读 kern/mm/pmm.c、pmm.h、mmu.h 和 tools/test_lab4.c。实现 page_insert、page_remove 及所需的内部辅助函数。插入映射时先获得或创建末级页表项，维护物理页引用计数；覆盖旧映射时区分“同一物理页更新权限”和“替换为另一物理页”两种情况；删除映射后清空 PTE，并对相应虚拟地址执行 TLB 失效。Sv39 的非叶子页表项只设置 Valid，不设置 R/W/X/U。不得把评分测试调用写进正式内核源码。
```

## 3. 功能模块二：进程创建与初始化

```text
根据 proc.h 的 struct proc_struct 和 tools/test_lab4.c，实现 alloc_proc、kernel_thread、copy_mm、copy_thread、do_fork 与 proc_init。初始化每个 PCB 字段和链表节点；内核线程通过 trapframe 保存入口函数与参数，经 forkret、forkrets 和 sret 进入 kernel_thread_entry；do_fork 要处理 PCB、内核栈和内存复制失败时的资源回收，并在关中断区间分配 PID、加入哈希表和进程链表；idleproc 的 PID 为 0，initproc 的 PID 为 1，且 initproc 的父进程为 idleproc。
```

## 4. 功能模块三：调度与上下文切换

```text
实现 cpu_idle、schedule、proc_run 和 switch_to。schedule 在关中断区间从 current 之后循环扫描 proc_list，只选择 PROC_RUNNABLE 进程；没有其他候选时回到 idleproc。proc_run 更新 current，按 Sv39 格式写 satp、刷新 TLB，再切换上下文。switch_to 的保存/恢复顺序必须和 struct context 一致，保存 ra、sp、s0-s11，不保存 caller-saved 寄存器。用注入测试创建的两个内核线程验证它们都获得运行机会。
```

## 5. 集成与报告

```text
先运行未修改骨架确认评分失败，再按依赖顺序实现并反复执行 make -j2、make grade 和 make qemu。若出现构建或启动问题，读取完整错误与 QEMU/OpenSBI 输出，使用指令或异常日志定位根因。报告只写实际运行得到的环境、命令和输出；把未确认的成员贡献放入独立任务分工文档，不写成正式报告事实。回答 local_intr_save/local_intr_restore 的中断状态保存机制，以及 Sv32、Sv39、Sv48 页表层数与 get_pte 重复代码、函数拆分的关系。
```
