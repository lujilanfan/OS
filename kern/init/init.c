#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <proc.h>
#include <kmonitor.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init(); // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    pmm_init(); // init physical memory management

    pic_init(); // init interrupt controller
    idt_init(); // init interrupt descriptor table

    vmm_init();  // 检查 VMA 链表的插入、排序和按地址查找。
    proc_init(); // 初始化进程链表和 PID 哈希桶。登记 idle，创建 init，但此时还未运行 init_main。

    clock_init();  // 设置定时器并允许 S 态定时器中断源。
    intr_enable(); // 置位本 hart 的 sstatus.SIE，打开 S 态全局中断。

    cpu_idle(); // 进入 idle 循环。idle 的 need_resched 已置位，接下来会调用 schedule。
}

static void
lab1_print_cur_status(void)
{
    static int round = 0;
    round++;
}
