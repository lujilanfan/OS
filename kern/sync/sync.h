#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

// 读取本 hart 的 S 态全局中断使能位。若原来打开，关掉并返回 1。
// 原来关闭时返回 0，让嵌套的内层临界区结束时继续保持关闭状态。
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

// flag 是进入该层临界区前的状态。只为原先已开启的情形重新开中断。
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

// 调用方用局部变量保存标志。同一层 save/restore 使用同一个变量。
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
