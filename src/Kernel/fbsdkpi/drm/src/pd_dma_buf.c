// SPDX-License-Identifier: MIT
// dma-buf, written from the contract drm_prime.c and amdgpu_dma_buf.c use
// buffers go to userspace as file descriptors, one linux file per exported buffer

// file.h renames struct file, so it comes before the dma-buf header uses it
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

// a dma_buf and, when the exporter brings none, its reservation object
struct pd_dma_buf {
	struct dma_buf db;
	struct dma_resv resv;
};

struct dma_buf *
dma_buf_export(const struct dma_buf_export_info *info)
{
	struct pd_dma_buf *pdb;
	struct dma_buf *db;

	if (info->ops == NULL || info->ops->map_dma_buf == NULL ||
	    info->ops->unmap_dma_buf == NULL || info->ops->release == NULL)
		return ERR_PTR(-EINVAL);
	// pin and unpin come as a pair, and cached mappings rule out pinning
	if ((info->ops->pin == NULL) != (info->ops->unpin == NULL))
		return ERR_PTR(-EINVAL);
	if (info->ops->cache_sgt_mapping && info->ops->pin != NULL)
		return ERR_PTR(-EINVAL);

	pdb = kzalloc(sizeof(*pdb), GFP_KERNEL);
	if (pdb == NULL)
		return ERR_PTR(-ENOMEM);
	db = &pdb->db;
	db->ops = info->ops;
	db->size = info->size;
	db->exp_name = info->exp_name;
	db->owner = info->owner;
	db->priv = info->priv;
	if (info->resv != NULL) {
		db->resv = info->resv;
	} else {
		dma_resv_init(&pdb->resv);
		db->resv = &pdb->resv;
	}
	mutex_init(&db->lock);
	INIT_LIST_HEAD(&db->attachments);
	db->pd_refs = 1;
	return db;
}

static void
pd_dma_buf_release(struct dma_buf *db)
{
	struct pd_dma_buf *pdb = container_of(db, struct pd_dma_buf, db);

	WARN_ON(!list_empty(&db->attachments));
	WARN_ON(db->vmapping_counter != 0);
	db->ops->release(db);
	if (db->resv == &pdb->resv)
		dma_resv_fini(&pdb->resv);
	mutex_destroy(&db->lock);
	kfree(pdb);
}

void
dma_buf_put(struct dma_buf *db)
{
	if (db == NULL)
		return;
	if (__atomic_sub_fetch(&db->pd_refs, 1, __ATOMIC_ACQ_REL) == 0)
		pd_dma_buf_release(db);
}

// the last descriptor is gone, so is the file's buffer reference
static int
pd_dma_buf_file_release(struct inode *inode, struct linux_file *filp)
{
	struct dma_buf *db = filp->private_data;

	mutex_lock(&db->lock);
	if (db->file == filp)
		db->file = NULL;
	mutex_unlock(&db->lock);
	dma_buf_put(db);
	return 0;
}

// DMA_BUF_IOCTL_SYNC brackets cpu access, a no-op for coherent buffers
static long
pd_dma_buf_file_ioctl(struct linux_file *filp, unsigned int cmd, unsigned long arg)
{
	return cmd == _IOW('b', 0, uint64_t) ? 0 : -ENOTTY;
}

// libdrm sizes an imported buffer with lseek(SEEK_END)
static off_t
pd_dma_buf_file_llseek(struct linux_file *filp, off_t offset, int whence)
{
	struct dma_buf *db = filp->private_data;

	if (offset != 0)
		return -EINVAL;
	if (whence == SEEK_END)
		return db->size;
	if (whence == SEEK_SET)
		return 0;
	return -EINVAL;
}

static const struct file_operations pd_dma_buf_fops = {
	.release = pd_dma_buf_file_release,
	.llseek = pd_dma_buf_file_llseek,
	.unlocked_ioctl = pd_dma_buf_file_ioctl,
};

bool
is_dma_buf_file(struct linux_file *filp)
{
	return filp->f_op == &pd_dma_buf_fops;
}

// the descriptor takes over the caller's reference on the buffer
int
dma_buf_fd(struct dma_buf *db, int flags)
{
	struct linux_file *filp;
	int fd;

	if (db == NULL)
		return -EINVAL;
	fd = get_unused_fd_flags(flags);
	if (fd < 0)
		return fd;
	mutex_lock(&db->lock);
	filp = db->file;
	if (filp != NULL) {
		// the existing file already holds a buffer reference
		refcount_acquire(&filp->f_count);
		mutex_unlock(&db->lock);
		dma_buf_put(db);
	} else {
		filp = alloc_file(FMODE_READ | FMODE_WRITE, &pd_dma_buf_fops);
		if (filp == NULL) {
			mutex_unlock(&db->lock);
			put_unused_fd(fd);
			return -ENOMEM;
		}
		filp->private_data = db;
		db->file = filp;
		mutex_unlock(&db->lock);
	}
	fd_install(fd, filp);
	return fd;
}

struct dma_buf *
dma_buf_get(int fd)
{
	struct linux_file *filp = fget(fd);
	struct dma_buf *db;

	if (filp == NULL)
		return ERR_PTR(-EBADF);
	if (!is_dma_buf_file(filp)) {
		fput(filp);
		return ERR_PTR(-EINVAL);
	}
	db = filp->private_data;
	get_dma_buf(db);
	fput(filp);
	return db;
}

struct dma_buf_attachment *
dma_buf_dynamic_attach(struct dma_buf *db, struct device *dev,
    const struct dma_buf_attach_ops *importer_ops, void *importer_priv)
{
	struct dma_buf_attachment *dba;
	int ret;

	if (db == NULL || dev == NULL)
		return ERR_PTR(-EINVAL);
	if (importer_ops != NULL && importer_ops->move_notify == NULL)
		return ERR_PTR(-EINVAL);

	dba = kzalloc(sizeof(*dba), GFP_KERNEL);
	if (dba == NULL)
		return ERR_PTR(-ENOMEM);
	dba->dev = dev;
	dba->dmabuf = db;
	dba->importer_ops = importer_ops;
	dba->importer_priv = importer_priv;
	if (importer_ops != NULL)
		dba->peer2peer = importer_ops->allow_peer2peer;

	if (db->ops->attach != NULL) {
		ret = db->ops->attach(db, dba);
		if (ret != 0) {
			kfree(dba);
			return ERR_PTR(ret);
		}
	}
	dma_resv_lock(db->resv, NULL);
	list_add(&dba->node, &db->attachments);
	dma_resv_unlock(db->resv);
	return dba;
}

struct dma_buf_attachment *
dma_buf_attach(struct dma_buf *db, struct device *dev)
{
	return dma_buf_dynamic_attach(db, dev, NULL, NULL);
}

void
dma_buf_detach(struct dma_buf *db, struct dma_buf_attachment *dba)
{
	if (db == NULL || dba == NULL)
		return;
	dma_resv_lock(db->resv, NULL);
	if (dba->sgt != NULL) {
		db->ops->unmap_dma_buf(dba, dba->sgt, dba->dir);
		if (dma_buf_is_dynamic(db))
			db->ops->unpin(dba);
		dba->sgt = NULL;
	}
	list_del(&dba->node);
	dma_resv_unlock(db->resv);
	if (db->ops->detach != NULL)
		db->ops->detach(db, dba);
	kfree(dba);
}

int
dma_buf_pin(struct dma_buf_attachment *dba)
{
	struct dma_buf *db = dba->dmabuf;

	dma_resv_assert_held(db->resv);
	return db->ops->pin != NULL ? db->ops->pin(dba) : 0;
}

void
dma_buf_unpin(struct dma_buf_attachment *dba)
{
	struct dma_buf *db = dba->dmabuf;

	dma_resv_assert_held(db->resv);
	if (db->ops->unpin != NULL)
		db->ops->unpin(dba);
}

struct sg_table *
dma_buf_map_attachment(struct dma_buf_attachment *dba,
    enum dma_data_direction dir)
{
	struct dma_buf *db = dba->dmabuf;
	struct sg_table *sgt;
	int ret;

	dma_resv_assert_held(db->resv);
	// a cached mapping is handed out again as long as the direction matches
	if (dba->sgt != NULL) {
		if (dba->dir != dir && dba->dir != DMA_BIDIRECTIONAL)
			return ERR_PTR(-EBUSY);
		return dba->sgt;
	}
	// static importers of a dynamic exporter keep the buffer pinned
	if (dma_buf_is_dynamic(db) && !dma_buf_attachment_is_dynamic(dba)) {
		ret = dma_buf_pin(dba);
		if (ret != 0)
			return ERR_PTR(ret);
	}
	sgt = db->ops->map_dma_buf(dba, dir);
	if (sgt == NULL)
		sgt = ERR_PTR(-ENOMEM);
	if (IS_ERR(sgt)) {
		if (dma_buf_is_dynamic(db) && !dma_buf_attachment_is_dynamic(dba))
			dma_buf_unpin(dba);
		return sgt;
	}
	if (db->ops->cache_sgt_mapping) {
		dba->sgt = sgt;
		dba->dir = dir;
	}
	return sgt;
}

struct sg_table *
dma_buf_map_attachment_unlocked(struct dma_buf_attachment *dba,
    enum dma_data_direction dir)
{
	struct sg_table *sgt;

	dma_resv_lock(dba->dmabuf->resv, NULL);
	sgt = dma_buf_map_attachment(dba, dir);
	dma_resv_unlock(dba->dmabuf->resv);
	return sgt;
}

void
dma_buf_unmap_attachment(struct dma_buf_attachment *dba, struct sg_table *sgt,
    enum dma_data_direction dir)
{
	struct dma_buf *db = dba->dmabuf;

	dma_resv_assert_held(db->resv);
	// cached mappings stay until detach
	if (dba->sgt == sgt)
		return;
	db->ops->unmap_dma_buf(dba, sgt, dir);
	if (dma_buf_is_dynamic(db) && !dma_buf_attachment_is_dynamic(dba))
		dma_buf_unpin(dba);
}

void
dma_buf_unmap_attachment_unlocked(struct dma_buf_attachment *dba,
    struct sg_table *sgt, enum dma_data_direction dir)
{
	dma_resv_lock(dba->dmabuf->resv, NULL);
	dma_buf_unmap_attachment(dba, sgt, dir);
	dma_resv_unlock(dba->dmabuf->resv);
}

void
dma_buf_move_notify(struct dma_buf *db)
{
	struct dma_buf_attachment *dba;

	dma_resv_assert_held(db->resv);
	list_for_each_entry(dba, &db->attachments, node) {
		if (dba->importer_ops != NULL)
			dba->importer_ops->move_notify(dba);
	}
}

int
dma_buf_begin_cpu_access(struct dma_buf *db, enum dma_data_direction dir)
{
	return db->ops->begin_cpu_access != NULL ?
	    db->ops->begin_cpu_access(db, dir) : 0;
}

int
dma_buf_end_cpu_access(struct dma_buf *db, enum dma_data_direction dir)
{
	return db->ops->end_cpu_access != NULL ?
	    db->ops->end_cpu_access(db, dir) : 0;
}

int
dma_buf_mmap(struct dma_buf *db, struct vm_area_struct *vma,
    unsigned long pgoff)
{
	return -EOPNOTSUPP;
}

int
dma_buf_vmap(struct dma_buf *db, struct iosys_map *map)
{
	struct iosys_map ptr;
	int ret;

	iosys_map_clear(map);
	if (db->ops->vmap == NULL)
		return -EINVAL;
	dma_resv_assert_held(db->resv);
	if (db->vmapping_counter != 0) {
		db->vmapping_counter++;
		*map = db->vmap_ptr;
		return 0;
	}
	ret = db->ops->vmap(db, &ptr);
	if (ret != 0)
		return ret;
	db->vmap_ptr = ptr;
	db->vmapping_counter = 1;
	*map = ptr;
	return 0;
}

void
dma_buf_vunmap(struct dma_buf *db, struct iosys_map *map)
{
	dma_resv_assert_held(db->resv);
	if (WARN_ON(db->vmapping_counter == 0))
		return;
	if (--db->vmapping_counter == 0) {
		if (db->ops->vunmap != NULL)
			db->ops->vunmap(db, map);
		iosys_map_clear(&db->vmap_ptr);
	}
}

int
dma_buf_vmap_unlocked(struct dma_buf *db, struct iosys_map *map)
{
	int ret;

	dma_resv_lock(db->resv, NULL);
	ret = dma_buf_vmap(db, map);
	dma_resv_unlock(db->resv);
	return ret;
}

void
dma_buf_vunmap_unlocked(struct dma_buf *db, struct iosys_map *map)
{
	dma_resv_lock(db->resv, NULL);
	dma_buf_vunmap(db, map);
	dma_resv_unlock(db->resv);
}
