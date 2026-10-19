// SPDX-License-Identifier: MIT
// dma-buf interface, written from what drm and amdgpu call and the public kernel-doc
#ifndef _PD_LINUX_DMA_BUF_H_
#define _PD_LINUX_DMA_BUF_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/iosys-map.h>
#include <linux/dma-fence.h>
#include <linux/wait.h>

struct device;
struct dma_buf;
struct dma_buf_attachment;
struct dma_resv;
struct file;
struct module;
struct vm_area_struct;

struct dma_buf_ops {
	bool cache_sgt_mapping;
	int (*attach)(struct dma_buf *, struct dma_buf_attachment *);
	void (*detach)(struct dma_buf *, struct dma_buf_attachment *);
	int (*pin)(struct dma_buf_attachment *);
	void (*unpin)(struct dma_buf_attachment *);
	struct sg_table *(*map_dma_buf)(struct dma_buf_attachment *,
	    enum dma_data_direction);
	void (*unmap_dma_buf)(struct dma_buf_attachment *, struct sg_table *,
	    enum dma_data_direction);
	void (*release)(struct dma_buf *);
	int (*begin_cpu_access)(struct dma_buf *, enum dma_data_direction);
	int (*end_cpu_access)(struct dma_buf *, enum dma_data_direction);
	int (*mmap)(struct dma_buf *, struct vm_area_struct *);
	int (*vmap)(struct dma_buf *, struct iosys_map *);
	void (*vunmap)(struct dma_buf *, struct iosys_map *);
};

struct dma_buf {
	size_t size;
	struct file *file;
	struct list_head attachments;
	const struct dma_buf_ops *ops;
	unsigned int vmapping_counter;
	struct iosys_map vmap_ptr;
	const char *exp_name;
	const char *name;
	struct module *owner;
	struct list_head list_node;
	void *priv;
	struct dma_resv *resv;
	struct mutex lock;
	// PureDarwin: export id until dma-buf fds exist (devfs route)
	int pd_id;
	int pd_refs;
};

struct dma_buf_attach_ops {
	bool allow_peer2peer;
	void (*move_notify)(struct dma_buf_attachment *);
};

struct dma_buf_attachment {
	struct dma_buf *dmabuf;
	struct device *dev;
	struct list_head node;
	struct sg_table *sgt;
	enum dma_data_direction dir;
	bool peer2peer;
	const struct dma_buf_attach_ops *importer_ops;
	void *importer_priv;
	void *priv;
};

struct dma_buf_export_info {
	const char *exp_name;
	struct module *owner;
	const struct dma_buf_ops *ops;
	size_t size;
	int flags;
	struct dma_resv *resv;
	void *priv;
};

#define DEFINE_DMA_BUF_EXPORT_INFO(name) \
	struct dma_buf_export_info name = { .exp_name = KBUILD_MODNAME, \
	    .owner = THIS_MODULE }

static inline bool
dma_buf_is_dynamic(struct dma_buf *db)
{
	return (db->ops->pin != NULL);
}

static inline bool
dma_buf_attachment_is_dynamic(struct dma_buf_attachment *dba)
{
	return (dba->importer_ops != NULL);
}

static inline void
get_dma_buf(struct dma_buf *db)
{
	__atomic_add_fetch(&db->pd_refs, 1, __ATOMIC_RELAXED);
}

struct dma_buf_attachment *dma_buf_attach(struct dma_buf *, struct device *);
struct dma_buf_attachment *dma_buf_dynamic_attach(struct dma_buf *,
    struct device *, const struct dma_buf_attach_ops *, void *);
void dma_buf_detach(struct dma_buf *, struct dma_buf_attachment *);
int dma_buf_pin(struct dma_buf_attachment *);
void dma_buf_unpin(struct dma_buf_attachment *);
bool is_dma_buf_file(struct file *filp);
struct dma_buf *dma_buf_export(const struct dma_buf_export_info *);
int dma_buf_fd(struct dma_buf *, int);
struct dma_buf *dma_buf_get(int);
void dma_buf_put(struct dma_buf *);
struct sg_table *dma_buf_map_attachment(struct dma_buf_attachment *,
    enum dma_data_direction);
struct sg_table *dma_buf_map_attachment_unlocked(struct dma_buf_attachment *,
    enum dma_data_direction);
void dma_buf_unmap_attachment(struct dma_buf_attachment *, struct sg_table *,
    enum dma_data_direction);
void dma_buf_unmap_attachment_unlocked(struct dma_buf_attachment *,
    struct sg_table *, enum dma_data_direction);
void dma_buf_move_notify(struct dma_buf *);
int dma_buf_begin_cpu_access(struct dma_buf *, enum dma_data_direction);
int dma_buf_end_cpu_access(struct dma_buf *, enum dma_data_direction);
int dma_buf_mmap(struct dma_buf *, struct vm_area_struct *, unsigned long);
int dma_buf_vmap(struct dma_buf *, struct iosys_map *);
void dma_buf_vunmap(struct dma_buf *, struct iosys_map *);
int dma_buf_vmap_unlocked(struct dma_buf *, struct iosys_map *);
void dma_buf_vunmap_unlocked(struct dma_buf *, struct iosys_map *);

#endif
