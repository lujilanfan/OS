.text
.globl kernel_thread_entry
kernel_thread_entry:
	/* sret 到达这里时，s0 是线程函数，s1 是参数，均来自初始 Trap frame。
	   按 RISC-V 调用约定把参数放入 a0，再通过 s0 间接调用线程函数。 */
	move a0, s1
	jalr s0

	/* 线程函数的返回值保留在 a0。do_exit 接收该值并切到其他可运行线程。 */
	jal do_exit
