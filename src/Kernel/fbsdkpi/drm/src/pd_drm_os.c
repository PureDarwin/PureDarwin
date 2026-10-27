// SPDX-License-Identifier: MIT
// kernel services drm and amdgpu call that PureDarwin does not back yet
// each reports absent the way the Linux callers already handle
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sema.h>
#include <sys/rtprio.h>
#include <sys/power.h>

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/suspend.h>

#include <drm/drm_device.h>

#include "../../pd_fbsd_os.h"

// linuxkpi renames sema_init for struct semaphore, these are the bsd ones
#undef sema_init

MALLOC_DEFINE(M_DMABUF, "dmabuf", "dma-buf fences");

suspend_state_t pm_suspend_target_state;

// the haiku layer ticks at 1000 hz
int hz = 1000;

int
power_profile_get_state(void)
{
	return POWER_PROFILE_PERFORMANCE;
}

int
rtp_to_pri(struct rtprio *rtp, struct thread *td)
{
	return 0;
}

void
sema_init(struct sema *sema, int value, const char *description)
{
	mtx_init(&sema->sema_mtx, description, "sema", MTX_DEF);
	cv_init(&sema->sema_cv, description);
	sema->sema_value = value;
	sema->sema_waiters = 0;
}

void
sema_destroy(struct sema *sema)
{
	cv_destroy(&sema->sema_cv);
	mtx_destroy(&sema->sema_mtx);
}

void
_sema_post(struct sema *sema, const char *file, int line)
{
	mtx_lock(&sema->sema_mtx);
	sema->sema_value++;
	if (sema->sema_waiters != 0)
		cv_signal(&sema->sema_cv);
	mtx_unlock(&sema->sema_mtx);
}

void
_sema_wait(struct sema *sema, const char *file, int line)
{
	mtx_lock(&sema->sema_mtx);
	while (sema->sema_value == 0) {
		sema->sema_waiters++;
		cv_wait(&sema->sema_cv, &sema->sema_mtx);
		sema->sema_waiters--;
	}
	sema->sema_value--;
	mtx_unlock(&sema->sema_mtx);
}

// the acpi namespace is not reachable from drivers yet
acpi_handle
pd_acpi_device_handle(struct device *dev)
{
	return NULL;
}

acpi_handle
pd_acpi_dev_handle(struct acpi_device *adev)
{
	return NULL;
}

// amdgpu and radeon read the vbios out of VFCT, instances count from 1 as in acpica
acpi_status
acpi_get_table(const char *signature, uint32_t instance,
    struct acpi_table_header **table)
{
	void *data;
	size_t size;

	*table = NULL;
	if (pdos_acpi_get_table(signature, instance > 0 ? instance - 1 : 0,
	    &data, &size) != 0)
		return AE_NOT_FOUND;
	// a kmalloc copy, so acpi_put_table frees it without knowing the size
	*table = kmemdup(data, size, GFP_KERNEL);
	pdos_acpi_put_table(data, size);
	return *table != NULL ? AE_OK : AE_NO_MEMORY;
}

void
acpi_put_table(struct acpi_table_header *table)
{
	kfree(table);
}

acpi_status
acpi_get_handle(acpi_handle parent, const char *pathname, acpi_handle *ret)
{
	*ret = NULL;
	return AE_NOT_FOUND;
}

acpi_status
acpi_get_name(acpi_handle handle, uint32_t name_type, struct acpi_buffer *ret)
{
	return AE_NOT_FOUND;
}

acpi_status
acpi_evaluate_object(acpi_handle handle, const char *pathname,
    struct acpi_object_list *params, struct acpi_buffer *ret)
{
	return AE_NOT_FOUND;
}

union acpi_object *
acpi_evaluate_dsm_typed(acpi_handle handle, const guid_t *guid, uint64_t rev,
    uint64_t func, union acpi_object *argv4, acpi_object_type type)
{
	return NULL;
}

const char *
acpi_format_exception(acpi_status status)
{
	return status == AE_NOT_FOUND ? "AE_NOT_FOUND" : "AE_ERROR";
}

struct acpi_device *
acpi_dev_get_first_match_dev(const char *hid, const char *uid, int64_t hrv)
{
	return NULL;
}

void
acpi_dev_put(struct acpi_device *adev)
{
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	return 0;
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	return 0;
}

// no debugfs, callers ignore the returned dentries
struct dentry *
debugfs_create_dir(const char *name, struct dentry *parent)
{
	return ERR_PTR(-ENODEV);
}

struct dentry *
debugfs_create_file(const char *name, umode_t mode, struct dentry *parent,
    void *data, const struct file_operations *fops)
{
	return ERR_PTR(-ENODEV);
}

struct dentry *
debugfs_create_file_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data, const struct file_operations *fops)
{
	return ERR_PTR(-ENODEV);
}

void
debugfs_create_atomic_t(const char *name, umode_t mode, struct dentry *parent,
    atomic_t *value)
{
}

void
debugfs_create_bool(const char *name, umode_t mode, struct dentry *parent,
    bool *value)
{
}

void
debugfs_remove(struct dentry *dentry)
{
}

int
drm_sysctl_init(struct drm_device *dev)
{
	return 0;
}

int
drm_sysctl_cleanup(struct drm_device *dev)
{
	return 0;
}

// panel brightness quirks and hdcp are left out, see PROVENANCE
struct drm_edid;
struct drm_connector;
int drm_get_panel_min_brightness_quirk(const struct drm_edid *edid);
int drm_connector_attach_content_protection_property(
    struct drm_connector *connector, bool hdcp_content_type);
void drm_hdcp_update_content_protection(struct drm_connector *connector,
    u64 val);

int
drm_get_panel_min_brightness_quirk(const struct drm_edid *edid)
{
	return -EINVAL;
}

int
drm_connector_attach_content_protection_property(
    struct drm_connector *connector, bool hdcp_content_type)
{
	return 0;
}

void
drm_hdcp_update_content_protection(struct drm_connector *connector, u64 val)
{
}

// files and character devices arrive with /dev/dri
struct linux_file *
linux_file_alloc(void)
{
	struct linux_file *filp = kzalloc(sizeof(*filp), GFP_KERNEL);

	if (filp != NULL)
		filp->f_count = 1;
	return filp;
}

void
linux_file_free(struct linux_file *filp)
{
	kfree(filp);
}

struct linux_cdev *
linux_find_cdev(const char *name, unsigned major, unsigned minor)
{
	return NULL;
}

void
linux_poll_wait(struct linux_file *filp, wait_queue_head_t *wqh, poll_table *p)
{
}

void
__unregister_chrdev(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name)
{
}

// a file whose count already dropped to zero is on its way out and is not handed back
struct linux_file *
get_file_active(struct linux_file **f)
{
	struct linux_file *filp = *f;

	if (filp == NULL || !refcount_acquire_if_not_zero(&filp->f_count))
		return NULL;
	return filp;
}

// user addresses are checked when copyin and copyout touch them
int
linux_access_ok(const void *uaddr, size_t len)
{
	return 1;
}

void
vma_set_file(struct vm_area_struct *vma, struct linux_file *file)
{
	struct linux_file *old = vma->vm_file;

	vma->vm_file = get_file(file);
	if (old != NULL)
		fput(old);
}

int
zap_vma_ptes(struct vm_area_struct *vma, unsigned long address, unsigned long size)
{
	return 0;
}

// user mappings of buffers come with the device pager
void
lkpi_unmap_mapping_range(void *obj, loff_t const holebegin, loff_t const holelen,
    int even_cows)
{
}

vm_fault_t
lkpi_vmf_insert_pfn_prot_locked(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, pgprot_t prot)
{
	return VM_FAULT_SIGBUS;
}

// the console framebuffer handover from vt is not used
void
vt_freeze_main_vd(unsigned long base, unsigned long size)
{
}

void
vt_unfreeze_main_vd(void)
{
}
