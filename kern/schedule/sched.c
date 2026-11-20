#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

// 创建时由 do_fork 调用。只有将 PCB 改为 PROC_RUNNABLE，调度器才能选中它。
void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

// lab4 function3 2410764
// 在当前 hart 上关中断，扫描循环进程链表，再按选择结果切换或继续运行。
void
schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;

    local_intr_save(intr_flag);
    {
        current->need_resched = 0; // 本轮已响应当前线程的调度请求。
        // last 固定为本轮起点。idle 没加入普通进程链表，从哨兵头开始。
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last; // le 是移动的游标。先向后走一步，再看那个节点。
        do
        {
            if ((le = list_next(le)) != &proc_list)
            {
                // 哨兵没有所属 PCB，只对普通节点做 le2proc 转换。
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE)
                {
                    break;
                }
            }
        } while (le != last); // 回到起点即扫过一圈，普通进程的起点本身也会被检查。

        // next 可能仍为 NULL，也可能是最后一个检查过但不可运行的 PCB。
        if (next == NULL || next->state != PROC_RUNNABLE)
        {
            next = idleproc;
        }
        next->runs++;
        // 选中自己表示继续原执行流，选中其他线程才保存和恢复上下文。
        if (next != current)
        {
            proc_run(next);
        }
    }
    // 有切换时，原线程再次获得 CPU 后才会执行这一行。
    local_intr_restore(intr_flag);
}
