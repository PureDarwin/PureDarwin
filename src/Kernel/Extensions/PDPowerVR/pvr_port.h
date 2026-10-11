/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_device, pvr_fw_device and the register helpers from pvr_device.h and pvr_fw.h,
// cut down to what booting the mips firmware needs and moved onto iokit

#ifndef _PDPOWERVR_PVR_PORT_H
#define _PDPOWERVR_PVR_PORT_H

#include <linux/types.h>
#include <linux/pd_kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "pvr_device_info.h"
#include "pvr_fw_info.h"
#include "pvr_rogue_cr_defs.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_heap_config.h"
#include "pvr_rogue_mips.h"
#include "pvr_rogue_fwif_stream.h"
#include "pvr_stream.h"

#ifndef EINVAL
#define EINVAL		22
#endif
#ifndef ENOMEM
#define ENOMEM		12
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT	60
#endif
#ifndef EBUSY
#define EBUSY		16
#endif
#ifndef EIO
#define EIO		5
#endif
#ifndef ENOSPC
#define ENOSPC		28
#endif

#define PVR_BO_FW_FLAGS_DEVICE_UNCACHED	(1u << 0)
#define PVR_BO_FW_NO_CLEAR_ON_RESET	(1u << 1)

// a render target dataset takes about four, and airjitd keeps up to 256 framebuffers in flight
#define PVR_FW_MAX_OBJECTS	2048

#define ROGUE_FWIF_FWCCB_NUMCMDS_LOG2 5
#define ROGUE_FWIF_KCCB_NUMCMDS_LOG2_DEFAULT 7

// one firmware allocation: physically contiguous, mapped write-combined for the cpu
struct pvr_fw_object {
	// the iokit side: buffer descriptor and its write-combined mapping
	void *bmd;
	void *map;
	u8 *cpu;
	u64 phys;
	u32 size;
	u32 flags;
	// fw heap range, the drm_mm node of the linux driver
	u64 fw_start;
	u32 fw_addr_offset;
	void (*init)(void *cpu_ptr, void *priv);
	void *init_priv;
	struct pvr_device *pvr_dev;
	// the creating call site, for the pdpvrslots dump
	void *caller;
	bool in_use;
};

struct pvr_gpu_id {
	u16 b, v, n, c;
};

struct pvr_fw_heap_info {
	u64 gpu_addr;
	u32 log2_size;
	u32 reserved_size;
	u32 raw_size;
	u32 offset_mask;
	u32 config_offset;
	u32 size;
};

#define PVR_MIPS_PT_PAGE_COUNT	ROGUE_MIPSFW_MAX_NUM_PAGETABLE_PAGES

struct pvr_fw_mips_data {
	struct pvr_fw_object *pt_obj;
	u32 *pt;
	u64 pt_dma_addr[PVR_MIPS_PT_PAGE_COUNT];
	u64 boot_code_dma_addr;
	u64 boot_data_dma_addr;
	u64 exception_code_dma_addr;
	u32 cache_policy;
	u32 pfn_mask;
};

struct pvr_ccb {
	struct pvr_fw_object *ctrl_obj;
	struct pvr_fw_object *ccb_obj;
	struct rogue_fwif_ccb_ctl *ctrl;
	u8 *ccb;
	u32 ctrl_fw_addr;
	u32 ccb_fw_addr;
	u32 num_cmds;
	u32 cmd_size;
};

struct pvr_fw_trace_buffer {
	struct pvr_fw_object *buf_obj;
	u32 *buf;
	struct rogue_fwif_tracebuf_space *tracebuf_space;
};

struct pvr_fw_trace {
	struct pvr_fw_object *tracebuf_ctrl_obj;
	struct rogue_fwif_tracebuf *tracebuf_ctrl;
	struct pvr_fw_trace_buffer buffers[ROGUE_FW_THREAD_MAX];
	u32 group_mask;
};

struct pvr_fw_mem {
	struct pvr_fw_object *code_obj, *data_obj, *core_code_obj, *core_data_obj;
	u8 *code, *data, *core_code, *core_data;
	u32 code_alloc_size, data_alloc_size, core_code_alloc_size, core_data_alloc_size;
	struct pvr_fw_object *fwif_connection_ctl_obj;
	struct pvr_fw_object *osinit_obj, *sysinit_obj, *osdata_obj, *sysdata_obj;
	struct pvr_fw_object *hwrinfobuf_obj, *power_sync_obj, *mmucache_sync_obj;
	struct pvr_fw_object *fault_page_obj, *gpu_util_fwcb_obj, *runtime_cfg_obj;
};

struct pvr_fw_device {
	const u8 *image;
	u32 image_size;
	const struct pvr_fw_info_header *header;
	const struct pvr_fw_layout_entry *layout_entries;
	struct pvr_fw_heap_info fw_heap_info;
	u64 fw_mm_base;
	struct pvr_fw_mips_data mips_data;
	struct pvr_fw_mem mem;
	struct pvr_fw_trace fw_trace;

	struct rogue_fwif_connection_ctl *fwif_connection_ctl;
	struct rogue_fwif_osinit *fwif_osinit;
	struct rogue_fwif_sysinit *fwif_sysinit;
	struct rogue_fwif_osdata *fwif_osdata;
	struct rogue_fwif_sysdata *fwif_sysdata;
	struct rogue_fwif_hwrinfobuf *hwrinfobuf;
	u32 *power_sync;

	// every object, for teardown and the heap allocator
	struct pvr_fw_object objs[PVR_FW_MAX_OBJECTS];
	u32 num_objs;
	bool booted;
};

struct pvr_device {
	volatile u8 *regs;
	u64 regs_phys;
	u32 core_clock_hz;
	struct pvr_gpu_id gpu_id;
	struct pvr_device_features features;
	struct pvr_device_quirks quirks;
	struct pvr_device_enhancements enhancements;
	struct pvr_fw_device fw_dev;
	struct {
		struct pvr_ccb ccb;
		struct pvr_fw_object *rtn_obj;
		u32 *rtn;
		u32 slot_count;
		u32 reserved_count;
		// iokit lock for the ccb, standing in for pvr_ccb::lock
		void *lock;
	} kccb;
	struct pvr_ccb fwccb;
	void *fwccb_lock;
	// pending mmu cache flush flags, the atomic of the linux driver
	volatile u32 mmu_flush_cache_flags;
	// interrupts taken and fwccb commands seen, for the logs
	volatile u32 irq_count;
	volatile u32 fwccb_count;
	bool lost;
	struct {
		u16 major, minor;
	} fw_version;
	// handle tables and queue lists of pvr_device.h
	struct xarray ctx_ids;
	struct xarray free_list_ids;
	struct xarray job_ids;
	struct {
		struct list_head active;
		struct list_head idle;
		struct mutex lock;
	} queues;
	spinlock_t ctx_list_lock;
	// one submission at a time, standing in for the drm_sched entities
	struct mutex submit_lock;
	struct mutex submit_lock_hwrt;
	/** @stream_musthave_quirks: Bit array of "must-have" quirks for stream commands. */
	u32 stream_musthave_quirks[PVR_STREAM_TYPE_MAX][PVR_STREAM_EXTHDR_TYPE_MAX];
};

#define PVR_HAS_FEATURE(pvr_dev, feature) ((pvr_dev)->features.has_##feature)
#define PVR_HAS_QUIRK(pvr_dev, quirk) ((pvr_dev)->quirks.has_brn##quirk)
#define PVR_HAS_ENHANCEMENT(pvr_dev, enhancement) ((pvr_dev)->enhancements.has_ern##enhancement)

#define PVR_FEATURE_VALUE(pvr_dev, feature, value_out)             \
	({                                                         \
		struct pvr_device *_pvr_dev = pvr_dev;             \
		int _ret = -EINVAL;                                \
		if (_pvr_dev->features.has_##feature) {            \
			*(value_out) = _pvr_dev->features.feature; \
			_ret = 0;                                  \
		}                                                  \
		_ret;                                              \
	})

#define GENMASK_ULL(h, l) ((~0ULL << (l)) & (~0ULL >> (63 - (h))))
#define FIELD_GET(mask, val) (((val) & (mask)) >> __builtin_ctzll(mask))
#define PVR_CR_FIELD_GET(val, field) FIELD_GET(~ROGUE_CR_##field##_CLRMSK, val)

#define PVR_PACKED_BVNC(b, v, n, c) \
	((((u64)(b) & GENMASK_ULL(15, 0)) << 48) | \
	 (((u64)(v) & GENMASK_ULL(15, 0)) << 32) | \
	 (((u64)(n) & GENMASK_ULL(15, 0)) << 16) | \
	 (((u64)(c) & GENMASK_ULL(15, 0)) <<  0))

static inline u64
pvr_gpu_id_to_packed_bvnc(struct pvr_gpu_id *gpu_id)
{
	return PVR_PACKED_BVNC(gpu_id->b, gpu_id->v, gpu_id->n, gpu_id->c);
}

static inline void
packed_bvnc_to_pvr_gpu_id(u64 bvnc, struct pvr_gpu_id *gpu_id)
{
	gpu_id->b = (bvnc & GENMASK_ULL(63, 48)) >> 48;
	gpu_id->v = (bvnc & GENMASK_ULL(47, 32)) >> 32;
	gpu_id->n = (bvnc & GENMASK_ULL(31, 16)) >> 16;
	gpu_id->c = bvnc & GENMASK_ULL(15, 0);
}

static inline u32
pvr_cr_read32(struct pvr_device *pvr_dev, u32 reg)
{
	return *(volatile u32 *)(pvr_dev->regs + reg);
}

static inline u64
pvr_cr_read64(struct pvr_device *pvr_dev, u32 reg)
{
	return *(volatile u64 *)(pvr_dev->regs + reg);
}

static inline void
pvr_cr_write32(struct pvr_device *pvr_dev, u32 reg, u32 val)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	*(volatile u32 *)(pvr_dev->regs + reg) = val;
}

static inline void
pvr_cr_write64(struct pvr_device *pvr_dev, u32 reg, u64 val)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	*(volatile u64 *)(pvr_dev->regs + reg) = val;
}

int pvr_cr_poll_reg32(struct pvr_device *pvr_dev, u32 reg_addr, u32 reg_value,
    u32 reg_mask, u64 timeout_usec);
int pvr_cr_poll_reg64(struct pvr_device *pvr_dev, u32 reg_addr, u64 reg_value,
    u64 reg_mask, u64 timeout_usec);

void pvr_device_info_set_quirks(struct pvr_device *pvr_dev, const u64 *bitmask, u32 bitmask_len);
void pvr_device_info_set_enhancements(struct pvr_device *pvr_dev, const u64 *bitmask, u32 bitmask_len);
int pvr_device_info_set_features(struct pvr_device *pvr_dev, const u64 *features, u32 features_size,
    u32 feature_param_size);

void pvr_load_gpu_id(struct pvr_device *pvr_dev);
int pvr_fw_validate_init_device_info(struct pvr_device *pvr_dev);
int pvr_fw_init(struct pvr_device *pvr_dev);
void pvr_fw_fini(struct pvr_device *pvr_dev);
int pvr_fw_stop(struct pvr_device *pvr_dev);
int pvr_fw_start(struct pvr_device *pvr_dev);
int pvr_wait_for_fw_boot(struct pvr_device *pvr_dev);
// pvr_fw.c's object interface
void pvr_fw_object_get_fw_addr_offset(struct pvr_fw_object *fw_obj, u32 offset, u32 *fw_addr_out);
#define pvr_fw_object_get_fw_addr(obj, out) pvr_fw_object_get_fw_addr_offset(obj, 0, out)
u64 pvr_fw_obj_get_gpu_addr(struct pvr_fw_object *fw_obj);
int pvr_fw_object_create(struct pvr_device *pvr_dev, size_t size, u32 flags,
    void (*init)(void *cpu_ptr, void *priv), void *init_priv, struct pvr_fw_object **fw_obj_out);
void *pvr_fw_object_create_and_map(struct pvr_device *pvr_dev, size_t size, u32 flags,
    void (*init)(void *cpu_ptr, void *priv), void *init_priv, struct pvr_fw_object **fw_obj_out);
void pvr_fw_object_destroy(struct pvr_fw_object *fw_obj);
#define pvr_fw_object_unmap_and_destroy(obj) pvr_fw_object_destroy(obj)
#define pvr_fw_object_vmap(obj) ((void *)(obj)->cpu)
#define pvr_fw_object_vunmap(obj) do { } while (0)
static inline int
pvr_fw_object_get_dma_addr(struct pvr_fw_object *fw_obj, u32 offset, u64 *dma_addr_out)
{
	if (offset >= fw_obj->size)
		return -22;
	*dma_addr_out = fw_obj->phys + offset;
	return 0;
}

// pvr_ccb.c, pvr_power.c and the mmu flush of pvr_mmu.c
bool pvr_fw_irq_pending(struct pvr_device *pvr_dev);
void pvr_fw_irq_clear(struct pvr_device *pvr_dev);
void pvr_fwccb_process(struct pvr_device *pvr_dev);
int pvr_kccb_init_locks(struct pvr_device *pvr_dev);
int pvr_kccb_send_cmd(struct pvr_device *pvr_dev, struct rogue_fwif_kccb_cmd *cmd, u32 *kccb_slot);
int pvr_kccb_wait_for_completion(struct pvr_device *pvr_dev, u32 slot_nr, u32 timeout_ms, u32 *rtn_out);
bool pvr_kccb_is_idle(struct pvr_device *pvr_dev);
void pvr_mmu_flush_request_all(struct pvr_device *pvr_dev);
int pvr_mmu_flush_exec(struct pvr_device *pvr_dev, bool wait);
int pvr_power_fw_disable(struct pvr_device *pvr_dev, bool hard_reset);
int pvr_power_fw_enable(struct pvr_device *pvr_dev);
int pvr_kccb_health_check(struct pvr_device *pvr_dev, u32 *rtn_out);
int pvr_kccb_reserve_slot_sync(struct pvr_device *pvr_dev);
void pvr_kccb_send_cmd_reserved_powered(struct pvr_device *pvr_dev, struct rogue_fwif_kccb_cmd *cmd,
    u32 *kccb_slot);
int pvr_fw_structure_cleanup(struct pvr_device *pvr_dev, u32 type, struct pvr_fw_object *fw_obj,
    u32 offset);

// provided by the iokit side
int pd_pvr_alloc(u32 size, u64 *phys, u8 **cpu, void **bmd, void **map);
void pd_pvr_free(void *bmd, void *map);
void pd_pvr_udelay(u32 us);
u64 pd_pvr_now_us(void);
void pd_pvr_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// pdpvrslots=1: fw object slot table growth and a dump of its owners when it fills
int pd_pvr_slot_diag(void);
// pdpvrslots counters: hwrt datasets created, destroy ioctls, releases; jobs created, released
extern u32 pd_diag_hwrt_created, pd_diag_hwrt_destroyed, pd_diag_hwrt_released, pd_diag_jobs_created, pd_diag_jobs_released;
extern u32 pd_diag_process_started, pd_diag_process_finished;
void *pd_pvr_lock_alloc(void);
void pd_pvr_lock(void *lock);
void pd_pvr_unlock(void *lock);
// sleep until pd_pvr_wake() or the timeout; callers recheck their condition
void pd_pvr_wait(struct pvr_device *pvr_dev, u32 timeout_ms);
// the race-free form: take the generation before checking the condition, then sleep only if no
// wake came since, so a completion between the check and the sleep is not missed
u32 pd_pvr_wait_gen(void);
void pd_pvr_wait_since(struct pvr_device *pvr_dev, u32 gen, u32 timeout_ms);
void pd_pvr_wake(struct pvr_device *pvr_dev);

#ifdef __cplusplus
}
#endif

#endif
