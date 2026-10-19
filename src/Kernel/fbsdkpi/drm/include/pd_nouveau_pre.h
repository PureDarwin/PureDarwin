// SPDX-License-Identifier: MIT
// nouveau's kconfig values and the few kernel interfaces linuxkpi leaves out
#ifndef _PD_NOUVEAU_PRE_H_
#define _PD_NOUVEAU_PRE_H_

#define CONFIG_NOUVEAU_DEBUG		5
#define CONFIG_NOUVEAU_DEBUG_DEFAULT	3

#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/pci.h>
#include <linux/maple_tree.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <drm/drm_encoder_slave.h>

// haiku's posix unistd.h turns every ioctl( into a function-like macro, nvif has an ioctl member
#include <unistd.h>
#undef ioctl

// i2c clients nouveau probes for fan and temperature chips, PD has no i2c client drivers
struct i2c_board_info {
	char type[20];
	unsigned short flags;
	unsigned short addr;
	void *platform_data;
};

#define I2C_BOARD_INFO(dev_type, dev_addr) .type = dev_type, .addr = (dev_addr)

static inline struct i2c_client *
i2c_new_client_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
	return ERR_PTR(-ENODEV);
}

static inline void i2c_unregister_device(struct i2c_client *client) { }

struct regulator;
static inline int regulator_get_voltage(struct regulator *r) { return -ENODEV; }
static inline int regulator_set_voltage(struct regulator *r, int min_uv, int max_uv) { return -ENODEV; }


// request_module ends in a FreeBSD kld load, PD loads nothing on request
struct thread;
int kern_kldload(struct thread *td, const char *file, int *fileid);

static inline void
list_rotate_left(struct list_head *head)
{
	if (!list_empty(head))
		list_move_tail(head->next, head);
}

// true with the mutex held when the count drops to zero, as linux does
static inline bool
refcount_dec_and_mutex_lock(refcount_t *r, struct mutex *lock)
{
	if (atomic_add_unless(r, -1, 1))
		return false;
	mutex_lock(lock);
	if (!atomic_dec_and_test(r)) {
		mutex_unlock(lock);
		return false;
	}
	return true;
}

// the expansion rom decode bit in the rom base address register
static inline int
pci_enable_rom(struct pci_dev *pdev)
{
	u32 v;

	pci_read_config_dword(pdev, 0x30 /* expansion rom base */, &v);
	if ((v & ~1u) == 0)
		return -ENOENT;
	pci_write_config_dword(pdev, 0x30 /* expansion rom base */, v | 1);
	return 0;
}

static inline void
pci_disable_rom(struct pci_dev *pdev)
{
	u32 v;

	pci_read_config_dword(pdev, 0x30 /* expansion rom base */, &v);
	pci_write_config_dword(pdev, 0x30 /* expansion rom base */, v & ~1u);
}

static inline bool
acpi_has_method(acpi_handle handle, const char *name)
{
	acpi_handle h;

	return ACPI_SUCCESS(acpi_get_handle(handle, name, &h));
}

// only nouveau's tegra paths use these, PD has no smmu driver for them yet
struct iommu_domain;
static inline size_t iommu_unmap(struct iommu_domain *d, unsigned long iova, size_t size) { return 0; }
struct device;
static inline bool tegra_dev_iommu_get_stream_id(struct device *dev, u32 *sid) { return false; }

#define WQ_MAX_ACTIVE	512
#define unrcu_pointer(p)	(p)

static inline int pm_runtime_get(struct device *dev) { return 1; }

#define DL_FLAG_STATELESS	0x1
struct device_link;
static inline struct device_link *device_link_add(struct device *consumer, struct device *supplier, u32 flags) { return (struct device_link *)1; }
static inline void device_link_remove(void *consumer, struct device *supplier) { }

static inline int iommu_map(struct iommu_domain *d, unsigned long iova, phys_addr_t paddr, size_t size, int prot, gfp_t gfp) { return -ENODEV; }

// the i2c slave encoders behind nv04 tv-out, PD has no i2c encoder drivers
struct drm_encoder;
struct drm_connector;
struct drm_display_mode;
static inline int drm_i2c_encoder_init(struct drm_device *dev, struct drm_encoder_slave *encoder, struct i2c_adapter *adap, const struct i2c_board_info *info) { return -ENODEV; }
static inline bool drm_i2c_encoder_mode_fixup(struct drm_encoder *encoder, const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode) { return false; }
static inline enum drm_connector_status drm_i2c_encoder_detect(struct drm_encoder *encoder, struct drm_connector *connector) { return connector_status_disconnected; }
static inline void drm_i2c_encoder_save(struct drm_encoder *encoder) { }
static inline void drm_i2c_encoder_restore(struct drm_encoder *encoder) { }

#define IOMMU_READ	(1 << 0)
#define IOMMU_WRITE	(1 << 1)

// nouveau walks the gpu's acpi children for _DOD, PD's acpi glue has no namespace walk yet
#ifndef ACPI_TYPE_DEVICE
#define ACPI_TYPE_DEVICE	6
#endif
static inline acpi_status
acpi_get_next_object(acpi_object_type type, acpi_handle parent, acpi_handle child, acpi_handle *next)
{
	return AE_NOT_FOUND;
}
#define acpi_handle_info(handle, fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

// hda audio binds to the gpu through the component framework, PD has no hda client for it
struct component_ops {
	int (*bind)(struct device *comp, struct device *master, void *data);
	void (*unbind)(struct device *comp, struct device *master, void *data);
};
static inline int component_add(struct device *dev, const struct component_ops *ops) { return 0; }
static inline void component_del(struct device *dev, const struct component_ops *ops) { }

// sleeps for *expires nanoseconds, rounded up to a tick
static inline int
schedule_hrtimeout(ktime_t *expires, int mode)
{
	long t = nsecs_to_jiffies(ktime_to_ns(*expires));

	schedule_timeout(t > 0 ? t : 1);
	return 0;
}

// the top of the user address space a gpu vm may mirror
#define TASK_SIZE	0x00007ffffffff000ULL

#define PCI_VENDOR_ID_NVIDIA		0x10de
#define PCI_VENDOR_ID_NVIDIA_SGS	0x12d2

static inline void mmu_notifier_synchronize(void) { }

// the 64-bit elf headers the gsp firmware image is packed in
struct elf64_hdr {
	unsigned char e_ident[16];
	u16 e_type;
	u16 e_machine;
	u32 e_version;
	u64 e_entry;
	u64 e_phoff;
	u64 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
};

struct elf64_shdr {
	u32 sh_name;
	u32 sh_type;
	u64 sh_flags;
	u64 sh_addr;
	u64 sh_offset;
	u64 sh_size;
	u32 sh_link;
	u32 sh_info;
	u64 sh_addralign;
	u64 sh_entsize;
};

#endif
