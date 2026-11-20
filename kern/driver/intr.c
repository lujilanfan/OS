#include <intr.h>
#include <riscv.h>

/* 置位当前 hart 的 sstatus.SIE。各中断源还须通过 sie 各自的使能位。 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* 清除 sstatus.SIE，关 S 态全局中断。保留 sstatus 的其他位和 sie 里的中断源配置。 */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
