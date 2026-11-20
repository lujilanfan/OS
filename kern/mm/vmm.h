#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

//pre define
struct mm_struct;

// 一个 VMA 描述连续虚拟地址区间 [vm_start, vm_end)，便于按区间管理地址空间。
struct vma_struct {
    struct mm_struct *vm_mm; // 所属地址空间，由 insert_vma_struct 设置。
    uintptr_t vm_start;      // 区间起点，包含在 VMA 内。
    uintptr_t vm_end;        // 区间终点，不包含在 VMA 内。
    uint32_t vm_flags;       // 区间的读、写、执行等属性。
    list_entry_t list_link;  // 插入所属 mm 的有序 VMA 链表时使用的节点。
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004

// 一个地址空间的软件描述，记录其虚拟区间以及根页表指针。
struct mm_struct {
    list_entry_t mmap_list;        // VMA 循环双向链表的哨兵头，按 vm_start 递增排列。
    struct vma_struct *mmap_cache; // 最近一次 find_vma 查到的区间，用于复用相邻地址查询。
    pde_t *pgdir;                  // 根页表的内核虚拟指针，mm_create 初始设为 NULL。
    int map_count;                 // 已挂入链表的 VMA 数量。
    void *sm_priv;                 // 交换管理器的私有数据指针，本实验初始化为空。
};

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

void vmm_init(void);

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

extern volatile unsigned int pgfault_num;
extern struct mm_struct *check_mm_struct;
#endif /* !__KERN_MM_VMM_H__ */
