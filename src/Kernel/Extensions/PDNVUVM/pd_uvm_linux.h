// pd_uvm_linux.h - Linux kernel interfaces nvidia-uvm uses that linuxkpi does not provide, written for PD
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_UVM_LINUX_H
#define PD_UVM_LINUX_H

#include <sys/unistd.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/smp.h>

// PD has no procfs: nvidia-uvm's /proc/driver/nvidia-uvm files are left out
#undef CONFIG_PROC_FS

// PD has no procfs: nvidia-uvm's procfs entries are made as a placeholder that stands for every one of them, so its
// GPU setup, which treats NULL as a failure, goes on, and removing one does nothing
struct proc_dir_entry;
extern char pd_uvm_proc_entry;
#define NV_CREATE_PROC_DIR(name, parent) ((struct proc_dir_entry *)&pd_uvm_proc_entry)
#define NV_CREATE_PROC_FILE(filename, parent, name, data) ((struct proc_dir_entry *)&pd_uvm_proc_entry)
#define proc_remove(entry) ((void)(entry))

#ifndef RFSTOPPED
#define RFSTOPPED (1 << 17)
#endif
#ifndef EBADFD
#define EBADFD EBADF
#endif
#ifndef __read_mostly
#define __read_mostly
#endif
#define rounddown_pow_of_two(n) (1UL << (fls64((uint64_t)(n)) - 1))

// Linux's bitmap_and and bitmap_andnot say whether any bit is left; linuxkpi's return nothing
static inline bool pd_uvm_bitmap_and(unsigned long *d, const unsigned long *a, const unsigned long *b, unsigned int n)
{
    bitmap_and(d, a, b, n);
    return !bitmap_empty(d, n);
}
static inline bool pd_uvm_bitmap_andnot(unsigned long *d, const unsigned long *a, const unsigned long *b, unsigned int n)
{
    bitmap_andnot(d, a, b, n);
    return !bitmap_empty(d, n);
}
#define bitmap_and(d, a, b, n) pd_uvm_bitmap_and((d), (a), (b), (n))
#define bitmap_andnot(d, a, b, n) pd_uvm_bitmap_andnot((d), (a), (b), (n))

// Linux's struct cpumask is linuxkpi's cpumask_t, FreeBSD's cpuset
#define cpumask _cpuset
#define for_each_cpu(cpu, mask) for ((cpu) = 0; (cpu) < MAXCPU; (cpu)++) if (CPU_ISSET((cpu), (mask)))

// clang's blocks extension takes __block, which nvidia-uvm uses as a local's name
#undef __block

// FreeBSD's MODULE_VERSION takes a module name and a number, Linux's only a version string
#include <linux/module.h>
#include <sys/module.h>
#undef MODULE_VERSION
#define MODULE_VERSION(version)

// nv-mm.h passes Linux's trailing locked argument, which linuxkpi's calls do not take: the mmap lock is never dropped
static inline long pd_uvm_get_user_pages_remote(struct task_struct *t, struct mm_struct *mm, unsigned long start,
    unsigned long n, unsigned int flags, struct page **pages, struct vm_area_struct **vmas, int *locked)
{
    if (locked)
        *locked = 1;
    return get_user_pages_remote(t, mm, start, n, flags, pages, vmas);
}
static inline long pd_uvm_pin_user_pages_remote(struct task_struct *t, struct mm_struct *mm, unsigned long start,
    unsigned long n, unsigned int flags, struct page **pages, struct vm_area_struct **vmas, int *locked)
{
    if (locked)
        *locked = 1;
    return pin_user_pages_remote(t, mm, start, n, flags, pages, vmas);
}
#define get_user_pages_remote pd_uvm_get_user_pages_remote
#define pin_user_pages_remote pd_uvm_pin_user_pages_remote

// nvidia-uvm's character device: PD's cdev_add has no devfs, PDNVUVM makes /dev/nvidia-uvm itself (pd_uvm_glue.c)
int pd_uvm_cdev_add(struct linux_cdev *cdev, dev_t dev, unsigned count);
void pd_uvm_cdev_del(struct linux_cdev *cdev);
#define cdev_add pd_uvm_cdev_add
// nvidia-uvm's own descriptors are not the shared layer's, its fget finds them (pd_uvm_glue.c)
#include <linux/file.h>
struct linux_file *pd_uvm_fget(unsigned int fd);
#undef fget
#define fget(fd) pd_uvm_fget(fd)
#define cdev_del pd_uvm_cdev_del

#define proc_symlink(name, parent, dest) ((struct proc_dir_entry *)&pd_uvm_proc_entry)
#define UVM_DEFINE_SINGLE_PROCFS_FILE(name) struct pd_uvm_no_procfs_##name

#define for_each_clear_bit_from(bit, addr, size) \
    for ((bit) = find_next_zero_bit((addr), (size), (bit)); (bit) < (size); \
         (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

// PD maps DMA without an IOMMU, a bus address is the physical one
#define dma_to_phys(dev, dma) ((phys_addr_t)(dma))

// a task's saved user instruction pointer, which nvidia-uvm only prints
#define KSTK_EIP(task) 0UL

#define THREAD_SIZE (4 * PAGE_SIZE)
#define get_current() (current)
#define bitcount64(x) ((unsigned int)__builtin_popcountll(x))
#define DECLARE_COMPLETION_ONSTACK(name) struct completion name = { .done = 0 }

// shifts a bitmap of nbits bits up by shift, filling with zeroes
static inline void bitmap_shift_left(unsigned long *dst, const unsigned long *src, unsigned int shift, unsigned int nbits)
{
    unsigned int words = BITS_TO_LONGS(nbits), off = shift / BITS_PER_LONG, rem = shift % BITS_PER_LONG;
    int k;

    for (k = (int)words - 1; k >= 0; k--) {
        unsigned long upper = 0, lower = 0;
        if ((unsigned int)k >= off) {
            upper = src[k - off] << rem;
            if (rem && (unsigned int)k > off)
                lower = src[k - off - 1] >> (BITS_PER_LONG - rem);
        }
        dst[k] = upper | lower;
    }
    if (nbits % BITS_PER_LONG)
        dst[words - 1] &= (1UL << (nbits % BITS_PER_LONG)) - 1;
}

// per-CPU variables are arrays indexed by CPU
#define DEFINE_PER_CPU(type, name) __typeof__(type) name[MAXCPU]
#define DECLARE_PER_CPU(type, name) extern __typeof__(type) name[MAXCPU]
#define per_cpu(var, cpu) ((var)[(cpu)])
#define per_cpu_ptr(ptr, cpu) (&(*(ptr))[(cpu)])
#define this_cpu_ptr(ptr) (&(*(ptr))[smp_processor_id()])
#define get_cpu() ({ preempt_disable(); smp_processor_id(); })
#define put_cpu() preempt_enable()

static inline bool cpumask_empty(const cpumask_t *m) { return CPU_EMPTY(m); }
static inline bool cpumask_subset(const cpumask_t *a, const cpumask_t *b) { return CPU_SUBSET(b, a); }
#define cpumask_of_node(node) (&cpu_online_mask)

// page state nvidia-uvm keeps on CPU pages: dirty in a PD flag bit, references in ref_count
#define PG_PD_UVM_DIRTY 0x1000
#define PageDirty(p) (((p)->flags & PG_PD_UVM_DIRTY) != 0)
#define SetPageDirty(p) ((p)->flags |= PG_PD_UVM_DIRTY)
#define ClearPageDirty(p) ((p)->flags &= ~PG_PD_UVM_DIRTY)
static inline int TestClearPageDirty(struct page *p) { int was = PageDirty(p); ClearPageDirty(p); return was; }
#define page_count(p) ((p)->ref_count)

// the kernel address of a physical address, through its page
static inline void *phys_to_virt(phys_addr_t pa)
{
    return (char *)page_address(PHYS_TO_VM_PAGE(pa & ~(phys_addr_t)(PAGE_SIZE - 1))) + (pa & (PAGE_SIZE - 1));
}
#define virt_addr_valid(addr) (!is_vmalloc_addr((const void *)(addr)))

// a file's page cache, which nvidia-uvm only keeps for unmap_mapping_range
struct address_space_operations;
struct address_space {
    struct inode *host;
    const struct address_space_operations *a_ops;
};
static inline void address_space_init_once(struct address_space *m) { memset(m, 0, sizeof(*m)); }

// VMAs come from PDNVUVM's own mmap, which keeps them (pd_uvm_mm.c)
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr);
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm, unsigned long start, unsigned long end);
int vm_insert_page(struct vm_area_struct *vma, unsigned long addr, struct page *page);

// kernel threads made stopped, run on wake_up_process (pd_uvm_kthread.c)
struct task_struct *pd_uvm_kthread_create(int (*fn)(void *), void *data, const char *fmt, ...) __printflike(3, 4);
bool pd_uvm_wake_up_process(struct task_struct *task);
#define kthread_create(fn, data, fmt, ...) pd_uvm_kthread_create((fn), (data), fmt, ##__VA_ARGS__)
#define kthread_create_on_node(fn, data, node, fmt, ...) pd_uvm_kthread_create((fn), (data), fmt, ##__VA_ARGS__)
#undef wake_up_process
#define wake_up_process(task) pd_uvm_wake_up_process(task)

// the top of an x86-64 process's address space with 4-level paging
#ifndef TASK_SIZE
#define TASK_SIZE 0x00007ffffffff000UL
#endif

// takes a bit as a sleeping lock: sets it, or waits for it to clear and tries again
static inline int wait_on_bit_lock(unsigned long *word, int bit, unsigned int state)
{
    while (test_and_set_bit(bit, word)) {
        int r = wait_on_bit(word, bit, state);
        if (r)
            return r;
    }
    return 0;
}

// the last component of a path
static inline const char *kbasename(const char *path)
{
    const char *tail = strrchr(path, '/');
    return tail ? tail + 1 : path;
}

#define __is_aligned(x, a) IS_ALIGNED((x), (a))
#define spin_is_locked(l) mtx_owned(l)

// one CPU's copy of a per-CPU variable, with preemption held off while it is used
#define get_cpu_var(var) (*({ preempt_disable(); &(var)[smp_processor_id()]; }))
#define put_cpu_var(var) preempt_enable()

// PD is one NUMA node, node 0, which holds all memory and every CPU
#ifndef MAX_NUMNODES
#define MAX_NUMNODES 1
#endif
typedef struct { unsigned long bits[1]; } nodemask_t;
static const nodemask_t pd_uvm_node_mask_one = { { 1UL } };
#define node_possible_map pd_uvm_node_mask_one
#define node_online_map pd_uvm_node_mask_one

enum pd_uvm_node_states { N_POSSIBLE, N_ONLINE, N_NORMAL_MEMORY, N_HIGH_MEMORY = N_NORMAL_MEMORY, N_MEMORY, N_CPU };
static inline int node_state(int node, int state) { (void)state; return node == 0; }

#define node_isset(node, mask) (((node) >= 0 && (node) < MAX_NUMNODES) && ((mask).bits[0] >> (node)) & 1UL)
#define node_set(node, mask) ((mask).bits[0] |= 1UL << (node))
#define node_clear(node, mask) ((mask).bits[0] &= ~(1UL << (node)))
#define nodes_clear(mask) ((mask).bits[0] = 0)
#define nodes_empty(mask) ((mask).bits[0] == 0)
static inline int __nodes_weight(const nodemask_t *m, unsigned int n) { (void)n; return __builtin_popcountl(m->bits[0]); }
#define nodes_weight(mask) __nodes_weight(&(mask), MAX_NUMNODES)
static inline int pd_uvm_next_node(int n, const nodemask_t *m)
{
    for (n++; n < MAX_NUMNODES; n++)
        if ((m->bits[0] >> n) & 1UL)
            return n;
    return MAX_NUMNODES;
}
#define next_node(n, mask) pd_uvm_next_node((n), &(mask))
#define first_node(mask) pd_uvm_next_node(-1, &(mask))
#define for_each_node_mask(node, mask) \
    for ((node) = first_node(mask); (node) < MAX_NUMNODES; (node) = next_node((node), (mask)))
#define for_each_node_state(node, state) for_each_node_mask((node), node_possible_map)
#define for_each_online_node(node) for_each_node_mask((node), node_online_map)
#define for_each_node(node) for_each_node_mask((node), node_possible_map)
#define num_possible_nodes() 1
#define num_online_nodes() 1
#define node_distance(a, b) ((a) == (b) ? 10 : 20)
#define page_to_nid(page) 0
#define cpu_to_node(cpu) 0
#define numa_mem_id() 0

// pages are never device pages on PD
struct dev_pagemap;
#define node_start_pfn(node) 0UL
#define node_end_pfn(node) (~0UL)
static inline struct dev_pagemap *page_pgmap(const struct page *page) { (void)page; return NULL; }

#endif
