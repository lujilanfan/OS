#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <intr.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// 普通进程的循环双向链表。proc_list 是哨兵头，调度时从这里遍历。idleproc 单独保存。
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// PID 哈希表的每个元素都是一个桶的链表头，桶内用 hash_link 串起哈希值相同的 PCB。
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

// 总数包括 idleproc，即使它没有挂入普通进程链表或 PID 哈希桶。
static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// lab4 function2 2410718
// 分配并初始化一块 PCB。调用者随后分配 PID 和内核栈，填写父进程并登记到全局链表。
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;       // 此时调度器还不能选择它。
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;                 // Lab4 的内核线程无需单独的用户地址空间描述符。
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->pgdir = boot_pgdir_pa;     // 这里保存内核根页表的物理地址，供构造 satp。
        proc->flags = 0;
        memset(proc->name, 0, sizeof(proc->name));
        // 两个节点分别加入不同的链表，初始化时各自前后自指，均未挂到全局表。
        list_init(&(proc->list_link));
        list_init(&(proc->hash_link));
    }
    return proc;
}

// 名称缓冲区有 PROC_NAME_LEN + 1 个字节。清零后复制前 PROC_NAME_LEN 个字节，
// 末字节保持为 '\0'。memcpy 会固定复制 15 字节，短源字符串存在越界读取风险。
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// 把 PCB 名称复制到静态缓冲区，末字节保留 '\0'。下一次调用会覆盖该缓冲区。
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// 在 [1, MAX_PID) 中寻找未被占用的 PID。0 留给 idleproc。
// last_pid 记录上次分配值。next_safe 记住已占用的下一个较大 PID，避免每次都扫描。
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        // 首次调用也走此分支，从 1 开始，并用进程链表检查冲突。
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                // 当前候选已占用，继续试下一个。超过已知区间时从表头重新检查。
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// lab4 function3 2410764
// 接收 schedule 选中的 PCB，将当前执行流交给它。目标已是 current 时无需实际切换。
void
proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            // satp 的 MODE 字段使用 Sv39，PPN 字段使用根页表物理地址右移 12 位的结果。
            // 当前内核线程共享同一根页表，仍通过这一接口设置页表并清理旧地址转换。
            write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                                (next->pgdir >> RISCV_PGSHIFT));
            flush_tlb();
            // a0 对应旧线程的 context，a1 对应新线程的 context。
            // 旧线程下次被调度回来时，从此调用之后继续，再恢复它保存的中断状态。
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}


// 新线程第一次被切换到时，switch_to 用预设的 ra 返回到这里。
// 把当前线程栈上的 Trap frame 传给汇编，恢复寄存器并按预设的 sepc 开始执行。
static void
forkret(void)
{
    forkrets(current->tf);
}

// 由 PID 算出哈希桶索引，再把 PCB 内嵌的 hash_link 插到对应桶的头结点后面。
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// 只查目标 PID 对应的桶。不同 PID 可以落入同一个桶，遍历时仍需比较完整 PID。
// idle 由 idleproc 单独访问，所以这里的有效 PID 不包括 0。
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// 分配 KSTACKPAGE 个连续物理页作为子线程的内核栈。
// kstack 保存栈底的内核虚拟地址，栈顶在 kstack + KSTACKSIZE，sp 从高地址向低地址增长。
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// 由内核栈地址找到第一页的描述符，把创建时分配的整段内核栈归还给分配器。
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// lab4 function2 2410718
// 构造新内核线程的临时 Trap frame。do_fork 中的 copy_thread 会将它复制到子线程栈。
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));

    // 恢复 Trap frame 后，入口汇编用 s0 保存的地址调用函数，用 s1 传递参数。
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    // SPP 使 sret 返回 S 态。SPIE 让 sret 之后开启中断，SIE 在现场恢复期间保持清零。
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) &
                ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    // CLONE_VM 表示共享地址空间，0 表示让 copy_thread 从子线程自己的内核栈开始。
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// lab4 function2 2410718
// 本实验只创建内核线程，current->mm 应为空。子线程保留 alloc_proc 预设的内核根页表。
// clone_flags 和 proc 保留为创建接口的参数，在这个实现中无需复制或创建 mm_struct。
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    assert(current->mm == NULL);
    (void)clone_flags;
    (void)proc;
    return 0;
}

// lab4 function2 2410718
// 把初始 Trap frame 放到子线程独立的内核栈顶，并设置首次调度时的返回地址和栈指针。
static void
copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE -
                                    sizeof(struct trapframe));
    *(proc->tf) = *tf;
    // 子线程的 fork 返回寄存器预置为 0。gpr.sp 是恢复 Trap frame 时装入的栈指针。
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (stack == 0) ? (uintptr_t)proc->tf : stack;
    // context.sp 供 switch_to 使用。ra 预置为 forkret，之后由它进入 Trap 返回路径。
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

// lab4 function2 2410718
// 创建子线程：分配资源、准备现场、登记 PCB、置为可运行。成功返回子 PID，失败返回负错误码。
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;

    // 超过容量或 PCB 分配失败时没有需要回收的子线程资源。
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }

    ret = -E_NO_MEM;
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }
    proc->parent = current;     // 创建 init 时，current 正是 idleproc。
    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);

    // PCB 和初始现场就绪后再登记。单 hart 上关中断保护 PID 分配和链表等共享状态。
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link)); // 插到全局进程表的哨兵之后。
        nr_process++;
    }
    local_intr_restore(intr_flag);

    // 登记好之后才把状态从 PROC_UNINIT 改为 PROC_RUNNABLE，让 schedule 可以选中它。
    wakeup_proc(proc);
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    // 内核栈已分配时先还栈，再接着释放 PCB。分配栈失败可直接进入下一标签。
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}


// 内核线程返回后停止调度。此时仍在该线程的内核栈上，保留 PCB 与栈供后续回收。
int do_exit(int error_code)
{
    assert(current != NULL && current != idleproc && current->mm == NULL);
    intr_disable();
    current->state = PROC_ZOMBIE;
    cprintf("kernel thread %s (pid %d) exited with code %d\n",
            current->name, current->pid, error_code);

    // 调度器只选择 PROC_RUNNABLE。没有其他就绪线程时，切回 idle 的启动栈。
    schedule();
    panic("do_exit: exited thread resumed");
}

// init 内核线程的入口。打印线程名、PID 和由 kernel_thread 传入的字符串后返回。
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

// lab4 function2 2410718
// 初始化全局进程结构。把当前启动执行流登记为 idle，再由 idle 创建第一个可被选中的子线程。
void
proc_init(void)
{
    list_init(&proc_list); // 建立供 schedule 遍历的空循环双向链表。
    // HASH_LIST_SIZE 为 1 << 10，即 1024 个桶。hash_list + i 等价于 &hash_list[i]。
    // 每次只初始化一个桶的头结点，list_init 本身没有循环。
    for (int i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }
    // idle 复用启动内核栈，直接成为 current，并请求 cpu_idle 进行第一次调度。
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process++;
    current = idleproc;

    // do_fork 将 current 记为新线程的 parent。此时它是 idle，第一次分配 PID 得到 1。
    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }
    // kernel_thread 返回的是 PID，通过 PID 哈希表找到对应 PCB 后再命名。
    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// lab4 function3 2410764
// idle 的执行循环。引导结束后首次进入时，proc_init 已经把 idle 的 need_resched 置为 1。
void
cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}
