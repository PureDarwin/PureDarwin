// SPDX-License-Identifier: MIT
// shmem files for drm objects, wired pages filled on first use

#include <linux/err.h>
#include <linux/file.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct vm_object {
	struct mtx lock;
	unsigned long npages;
	struct page **pages;
};

static void
pd_shmem_release(struct vm_object *obj)
{
	for (unsigned long i = 0; i < obj->npages; i++) {
		if (obj->pages[i] != NULL)
			__free_page(obj->pages[i]);
	}
	kvfree(obj->pages);
	mtx_destroy(&obj->lock);
	kfree(obj);
}

static int
pd_shmem_file_release(struct inode *inode, struct linux_file *filp)
{
	if (filp->f_shmem != NULL)
		pd_shmem_release(filp->f_shmem);
	filp->f_shmem = NULL;
	return 0;
}

static const struct file_operations pd_shmem_fops = {
	.release = pd_shmem_file_release,
};

struct linux_file *
linux_shmem_file_setup(const char *name, loff_t size, unsigned long flags)
{
	struct linux_file *filp;
	struct vm_object *obj;

	if (size <= 0)
		return ERR_PTR(-EINVAL);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL)
		return ERR_PTR(-ENOMEM);
	obj->npages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	obj->pages = kvzalloc(obj->npages * sizeof(*obj->pages), GFP_KERNEL);
	if (obj->pages == NULL) {
		kfree(obj);
		return ERR_PTR(-ENOMEM);
	}
	mtx_init(&obj->lock, "pdshmem", NULL, MTX_DEF);

	filp = linux_file_alloc();
	if (filp == NULL) {
		pd_shmem_release(obj);
		return ERR_PTR(-ENOMEM);
	}
	filp->f_shmem = obj;
	filp->f_op = &pd_shmem_fops;
	return filp;
}

struct linux_file *
linux_shmem_file_setup_with_mnt(struct vfsmount *mount, const char *name,
    loff_t size, unsigned long flags)
{
	return linux_shmem_file_setup(name, size, flags);
}

// the caller gets its own reference, dropped with put_page
struct page *
linux_shmem_read_mapping_page_gfp(vm_object_t obj, int pindex, gfp_t gfp)
{
	struct page *page, *fresh = NULL;

	if (obj == NULL || pindex < 0 || (unsigned long)pindex >= obj->npages)
		return ERR_PTR(-EINVAL);

	mtx_lock(&obj->lock);
	page = obj->pages[pindex];
	mtx_unlock(&obj->lock);
	if (page == NULL) {
		fresh = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (fresh == NULL)
			return ERR_PTR(-ENOMEM);
		mtx_lock(&obj->lock);
		page = obj->pages[pindex];
		if (page == NULL) {
			obj->pages[pindex] = fresh;
			page = fresh;
			fresh = NULL;
		}
		mtx_unlock(&obj->lock);
		if (fresh != NULL)
			__free_page(fresh);
	}
	get_page(page);
	return page;
}

void
linux_shmem_truncate_range(vm_object_t obj, loff_t lstart, loff_t lend)
{
	unsigned long first, last;

	if (obj == NULL)
		return;
	first = (lstart + PAGE_SIZE - 1) >> PAGE_SHIFT;
	last = lend == (loff_t)-1 ? obj->npages : ((unsigned long)lend + 1) >> PAGE_SHIFT;
	if (last > obj->npages)
		last = obj->npages;

	mtx_lock(&obj->lock);
	for (unsigned long i = first; i < last; i++) {
		struct page *page = obj->pages[i];

		obj->pages[i] = NULL;
		if (page != NULL) {
			mtx_unlock(&obj->lock);
			__free_page(page);
			mtx_lock(&obj->lock);
		}
	}
	mtx_unlock(&obj->lock);
}
