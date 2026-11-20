#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// Lab4 直接用 current 区分哪个可运行线程正在 CPU 上执行。
enum proc_state
{
    PROC_UNINIT = 0, // PCB 已分配，创建流程尚未把它置为可运行。
    PROC_SLEEPING,   // 等待事件，调度器会跳过。
    PROC_RUNNABLE,   // 可运行，包含就绪和已经在 CPU 上运行两种情形。
    PROC_ZOMBIE,     // 线程已退出，调度器跳过，PCB 和内核栈留待回收。
};

// switch.S 依照这里的顺序处理 14 个寄存器槽位。新线程的 ra 和 sp 由 copy_thread 设置。
struct context
{
    uintptr_t ra; // ret 使用的地址。旧线程存调用后的续执行位置，新线程存 forkret。
    uintptr_t sp; // 线程切换时的栈指针。
    uintptr_t s0; // s0 至 s11 为 RISC-V 调用约定中由被调用者保存的寄存器。
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    enum proc_state state;       // 调度状态，由 schedule 用于筛选。
    int pid;                     // 进程编号。-1 表示未分配，0 专用于 idle。
    int runs;                    // 被 schedule 选中的次数，包括继续运行同一线程。
    uintptr_t kstack;            // 独立内核栈的起始虚拟地址，idle 使用 bootstack。
    volatile bool need_resched;  // 当前执行流是否请求重新调度。
    struct proc_struct *parent;  // 创建者的 PCB，idle 无父进程。
    struct mm_struct *mm;        // 独立地址空间的描述符。Lab4 的内核线程为 NULL。
    struct context context;      // switch_to 使用的调度上下文。
    struct trapframe *tf;        // 通用寄存器及 Trap 相关 CSR 的现场，新线程也用它启动。
    uintptr_t pgdir;             // 根页表物理地址，内核线程用 boot_pgdir_pa。
    uint32_t flags;              // 进程标志，alloc_proc 初始置 0。
    char name[PROC_NAME_LEN + 1]; // 最多 15 个名称字节，另外预留一个结束符。
    list_entry_t list_link;      // 挂在全局 proc_list 中，供遍历和调度。
    list_entry_t hash_link;      // 挂在 PID 哈希桶内，供按 PID 查找。
};

// 从 PCB 内嵌的链表节点地址减去成员偏移，取回 PCB 的地址。
#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
