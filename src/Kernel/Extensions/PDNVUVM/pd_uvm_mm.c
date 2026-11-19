// pd_uvm_mm.c - nvidia-uvm's mappings of /dev/nvidia-uvm: the VMA each mmap makes, found by address as Linux's find_vma
// does, and the pages nvidia-uvm inserts into it, which become the mapping's pages
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

// a VMA of /dev/nvidia-uvm and the page frame nvidia-uvm put at each of its pages, 0 for none yet
struct pd_uvm_vma {
    struct list_head link;
    struct vm_area_struct vma;
    unsigned long npages;
    unsigned long *pfns;
};

// every live mapping, for all processes; few are live at once
static LIST_HEAD(pd_uvm_vmas);
static DEFINE_SPINLOCK(pd_uvm_vmas_lock);

// the lowest VMA of mm that ends above addr
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
    struct vm_area_struct *best = NULL;
    struct pd_uvm_vma *v;

    spin_lock(&pd_uvm_vmas_lock);
    list_for_each_entry(v, &pd_uvm_vmas, link) {
        struct vm_area_struct *vma = &v->vma;
        if (vma->vm_mm == mm && addr < vma->vm_end && (!best || vma->vm_start < best->vm_start))
            best = vma;
    }
    spin_unlock(&pd_uvm_vmas_lock);
    return best;
}

struct vm_area_struct *find_vma_intersection(struct mm_struct *mm, unsigned long start, unsigned long end)
{
    struct vm_area_struct *vma = find_vma(mm, start);

    return (vma && end > vma->vm_start) ? vma : NULL;
}

// nvidia-uvm putting a page into one of its mappings: the page is kept for the mapping, which is made of these pages
int vm_insert_page(struct vm_area_struct *vma, unsigned long addr, struct page *page)
{
    struct pd_uvm_vma *v = container_of(vma, struct pd_uvm_vma, vma);
    unsigned long i;

    // linuxkpi's PAGE_MASK is Linux's, the page's bits: the offset within a page is these
    if (addr < vma->vm_start || addr >= vma->vm_end || (addr & (PAGE_SIZE - 1)) != 0)
        return -EINVAL;
    i = (addr - vma->vm_start) >> PAGE_SHIFT;
    if (i >= v->npages)
        return -EINVAL;
    v->pfns[i] = page_to_pfn(page);
    return 0;
}

// the mapping's end, when its last user is gone: nvidia-uvm closes it, then it is forgotten
static void pd_uvm_vma_release(void *arg)
{
    struct pd_uvm_vma *v = arg;

    linux_set_current(curthread);
    if (v->vma.vm_ops != NULL && v->vma.vm_ops->close != NULL)
        v->vma.vm_ops->close(&v->vma);
    spin_lock(&pd_uvm_vmas_lock);
    list_del(&v->link);
    spin_unlock(&pd_uvm_vmas_lock);
    kfree(v->pfns);
    kfree(v);
}

// the page behind every part of a mapping nvidia-uvm put nothing in: a semaphore pool's range is its virtual size,
// only its first pages are memory. Linux leaves the rest unmapped, CUDA never touches it
static struct page *pd_uvm_scratch;

// an mmap of /dev/nvidia-uvm: CUDA maps at an address equal to the offset it passes, which nvidia-uvm checks, so the VMA
// starts at the offset. nvidia-uvm's mmap inserts the pages of what lies there (a semaphore pool); each of them is
// handed back as the mapping's page, the rest of the range is a scratch page
int pd_uvm_mmap(void *file, unsigned long offset, unsigned long size, int prot,
    uint32_t *pages, void (**release)(void *), void **release_arg)
{
    struct linux_file *filp = file;
    struct pd_uvm_vma *v;
    unsigned long i;
    int ret;

    if (filp->f_op->mmap == NULL)
        return ENODEV;
    v = kzalloc(sizeof(*v), GFP_KERNEL);
    if (v == NULL)
        return ENOMEM;
    v->npages = size >> PAGE_SHIFT;
    v->pfns = kcalloc(v->npages, sizeof(*v->pfns), GFP_KERNEL);
    if (v->pfns == NULL) {
        kfree(v);
        return ENOMEM;
    }
    linux_set_current(curthread);
    v->vma.vm_start = offset;
    v->vma.vm_end = offset + size;
    v->vma.vm_pgoff = offset >> PAGE_SHIFT;
    v->vma.vm_flags = VM_SHARED | (prot & VM_PROT_READ ? VM_READ : 0) | (prot & VM_PROT_WRITE ? VM_WRITE : 0);
    v->vma.vm_page_prot = 0;
    v->vma.vm_mm = current->mm;
    v->vma.vm_file = filp;

    // the VMA is findable while nvidia-uvm sets it up, as Linux has it in the mm before calling mmap
    spin_lock(&pd_uvm_vmas_lock);
    list_add(&v->link, &pd_uvm_vmas);
    spin_unlock(&pd_uvm_vmas_lock);

    ret = filp->f_op->mmap(filp, &v->vma);
    if (ret == 0 && pd_uvm_scratch == NULL)
        pd_uvm_scratch = alloc_page(GFP_KERNEL | __GFP_ZERO);
    for (i = 0; ret == 0 && i < v->npages; i++) {
        if (v->pfns[i] == 0 && pd_uvm_scratch == NULL)
            ret = -ENOMEM;
        pages[i] = (uint32_t)(v->pfns[i] != 0 ? v->pfns[i] : page_to_pfn(pd_uvm_scratch));
    }
    if (ret != 0) {
        printf("PDNVUVM: mmap of 0x%lx bytes at 0x%lx: %d\n", size, offset, ret);
        if (v->vma.vm_ops != NULL && v->vma.vm_ops->close != NULL)
            v->vma.vm_ops->close(&v->vma);
        spin_lock(&pd_uvm_vmas_lock);
        list_del(&v->link);
        spin_unlock(&pd_uvm_vmas_lock);
        kfree(v->pfns);
        kfree(v);
        return ret < 0 ? -ret : ret;
    }
    *release = pd_uvm_vma_release;
    *release_arg = v;
    return 0;
}
