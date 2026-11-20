#include <riscv.h>

    .macro SAVE_ALL
    .align 2

    /* 陷入时先保存原 sp，再在当前内核栈分配 Trap frame。
       32 个通用寄存器槽加 4 个 CSR 槽，布局和 trap.h 一致。 */
    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save gprs
    STORE  x0,0*REGBYTES(sp)
    STORE  x1,1*REGBYTES(sp)
    STORE  x3,3*REGBYTES(sp)
    STORE  x4,4*REGBYTES(sp)
    STORE  x5,5*REGBYTES(sp)
    STORE  x6,6*REGBYTES(sp)
    STORE  x7,7*REGBYTES(sp)
    STORE  x8,8*REGBYTES(sp)
    STORE  x9,9*REGBYTES(sp)
    STORE  x10,10*REGBYTES(sp)
    STORE  x11,11*REGBYTES(sp)
    STORE  x12,12*REGBYTES(sp)
    STORE  x13,13*REGBYTES(sp)
    STORE  x14,14*REGBYTES(sp)
    STORE  x15,15*REGBYTES(sp)
    STORE  x16,16*REGBYTES(sp)
    STORE  x17,17*REGBYTES(sp)
    STORE  x18,18*REGBYTES(sp)
    STORE  x19,19*REGBYTES(sp)
    STORE  x20,20*REGBYTES(sp)
    STORE  x21,21*REGBYTES(sp)
    STORE  x22,22*REGBYTES(sp)
    STORE  x23,23*REGBYTES(sp)
    STORE  x24,24*REGBYTES(sp)
    STORE  x25,25*REGBYTES(sp)
    STORE  x26,26*REGBYTES(sp)
    STORE  x27,27*REGBYTES(sp)
    STORE  x28,28*REGBYTES(sp)
    STORE  x29,29*REGBYTES(sp)
    STORE  x30,30*REGBYTES(sp)
    STORE  x31,31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Fetch the pre-trap stack pointer and clear sscratch so a nested trap is
    # identifiable as a kernel trap.
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL
    /* sp 此时是 Trap frame 基址。槽位 32 保存 sstatus，槽位 33 保存 sepc。 */
    LOAD  s1,32*REGBYTES(sp)
    LOAD  s2,33*REGBYTES(sp)
    csrw  sstatus, s1
    csrw  sepc, s2

    LOAD  x1,1*REGBYTES(sp)
    LOAD  x3,3*REGBYTES(sp)
    LOAD  x4,4*REGBYTES(sp)
    LOAD  x5,5*REGBYTES(sp)
    LOAD  x6,6*REGBYTES(sp)
    LOAD  x7,7*REGBYTES(sp)
    LOAD  x8,8*REGBYTES(sp)
    LOAD  x9,9*REGBYTES(sp)
    LOAD  x10,10*REGBYTES(sp)
    LOAD  x11,11*REGBYTES(sp)
    LOAD  x12,12*REGBYTES(sp)
    LOAD  x13,13*REGBYTES(sp)
    LOAD  x14,14*REGBYTES(sp)
    LOAD  x15,15*REGBYTES(sp)
    LOAD  x16,16*REGBYTES(sp)
    LOAD  x17,17*REGBYTES(sp)
    LOAD  x18,18*REGBYTES(sp)
    LOAD  x19,19*REGBYTES(sp)
    LOAD  x20,20*REGBYTES(sp)
    LOAD  x21,21*REGBYTES(sp)
    LOAD  x22,22*REGBYTES(sp)
    LOAD  x23,23*REGBYTES(sp)
    LOAD  x24,24*REGBYTES(sp)
    LOAD  x25,25*REGBYTES(sp)
    LOAD  x26,26*REGBYTES(sp)
    LOAD  x27,27*REGBYTES(sp)
    LOAD  x28,28*REGBYTES(sp)
    LOAD  x29,29*REGBYTES(sp)
    LOAD  x30,30*REGBYTES(sp)
    LOAD  x31,31*REGBYTES(sp)
    /* 最后才恢复 x2（sp），前面的 LOAD 都需要用当前 sp 找到 Trap frame。 */
    LOAD  x2,2*REGBYTES(sp)
    .endm

    .globl __alltraps
__alltraps:
    SAVE_ALL

    /* 真正发生 Trap 时把栈上的现场传给 C 层。新线程的 forkrets 跳过这段保存与分发。 */
    move a0, sp
    call trap
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    /* sret 按 sepc 恢复执行位置，并用 SPP、SPIE 恢复特权级和中断状态。 */
    sret

    .globl forkrets
forkrets:
    /* forkret 通过 a0 传入 current->tf。把它设为栈基址，复用 Trap 返回流程，
       新线程的 sepc 已指向 kernel_thread_entry。 */
    move sp, a0
    j __trapret
