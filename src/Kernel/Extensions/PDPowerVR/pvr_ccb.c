// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// the kernel and firmware ccb paths of pvr_ccb.c, the power commands of pvr_power.c and the
// cache flush of pvr_mmu.c. mutexes are iokit locks, wait queues pd_pvr_wait/pd_pvr_wake

#include "pvr_device.h"
#include "pvr_free_list.h"
#include "pvr_queue.h"

#define RESERVE_SLOT_TIMEOUT_US 1000000
#define RESERVE_SLOT_MIN_RETRIES 10

#define POWER_SYNC_TIMEOUT_US (1000000) /* 1s */

#define WRITE_ONCE(x, v) (*(volatile __typeof__(x) *)&(x) = (v))
#define READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#define mb() __asm__ volatile ("dsb sy" ::: "memory")
#define wmb() __asm__ volatile ("dsb st" ::: "memory")

#define PVR_MMU_SYNC_LEVEL_0_FLAGS (ROGUE_FWIF_MMUCACHEDATA_FLAGS_PT | \
				    ROGUE_FWIF_MMUCACHEDATA_FLAGS_INTERRUPT | \
				    ROGUE_FWIF_MMUCACHEDATA_FLAGS_TLB)
#define PVR_MMU_SYNC_LEVEL_1_FLAGS (PVR_MMU_SYNC_LEVEL_0_FLAGS | ROGUE_FWIF_MMUCACHEDATA_FLAGS_PD)
#define PVR_MMU_SYNC_LEVEL_2_FLAGS (PVR_MMU_SYNC_LEVEL_1_FLAGS | ROGUE_FWIF_MMUCACHEDATA_FLAGS_PC)

int
pvr_kccb_init_locks(struct pvr_device *pvr_dev)
{
	pvr_dev->kccb.slot_count = 1 << ROGUE_FWIF_KCCB_NUMCMDS_LOG2_DEFAULT;
	pvr_dev->kccb.reserved_count = 0;
	pvr_dev->kccb.lock = pd_pvr_lock_alloc();
	pvr_dev->fwccb_lock = pd_pvr_lock_alloc();
	return (pvr_dev->kccb.lock && pvr_dev->fwccb_lock) ? 0 : -ENOMEM;
}

// pvr_fw_mips.c
bool
pvr_fw_irq_pending(struct pvr_device *pvr_dev)
{
	return pvr_cr_read32(pvr_dev, ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS) &
	       ROGUE_CR_MIPS_WRAPPER_IRQ_STATUS_EVENT_EN;
}

void
pvr_fw_irq_clear(struct pvr_device *pvr_dev)
{
	pvr_cr_write32(pvr_dev, ROGUE_CR_MIPS_WRAPPER_IRQ_CLEAR,
		       ROGUE_CR_MIPS_WRAPPER_IRQ_CLEAR_EVENT_EN);
}

static void
pvr_fw_mts_schedule(struct pvr_device *pvr_dev, u32 val)
{
	/* Ensure memory is flushed before kicking MTS. */
	wmb();

	pvr_cr_write32(pvr_dev, ROGUE_CR_MTS_SCHEDULE, val);

	/* Ensure the MTS kick goes through before continuing. */
	mb();
}

static bool
pvr_ccb_slot_available_locked(struct pvr_ccb *pvr_ccb, u32 *write_offset)
{
	struct rogue_fwif_ccb_ctl *ctrl = pvr_ccb->ctrl;
	u32 next_write_offset = (READ_ONCE(ctrl->write_offset) + 1) & READ_ONCE(ctrl->wrap_mask);

	if (READ_ONCE(ctrl->read_offset) != next_write_offset) {
		if (write_offset)
			*write_offset = next_write_offset;
		return true;
	}

	return false;
}

static void
process_fwccb_command(struct pvr_device *pvr_dev, struct rogue_fwif_fwccb_cmd *cmd)
{
	pvr_dev->fwccb_count++;

	switch (cmd->cmd_type) {
	case ROGUE_FWIF_FWCCB_CMD_REQUEST_GPU_RESTART:
		// the reset path of pvr_power.c is not ported yet
		pd_pvr_log("firmware requests a gpu restart\n");
		break;

	case ROGUE_FWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION:
		pvr_free_list_process_reconstruct_req(pvr_dev,
						      &cmd->cmd_data.cmd_freelists_reconstruction);
		break;

	case ROGUE_FWIF_FWCCB_CMD_FREELIST_GROW:
		pvr_free_list_process_grow_req(pvr_dev, &cmd->cmd_data.cmd_free_list_gs);
		break;

	case ROGUE_FWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION:
		pvr_context_reset_notify(pvr_dev, &cmd->cmd_data.cmd_context_reset_notification);
		break;

	case ROGUE_FWIF_FWCCB_CMD_CONTEXT_FW_PF_NOTIFICATION:
		pd_pvr_log("firmware page fault at 0x%llx\n", (u64)cmd->cmd_data.cmd_fw_pagefault.fw_fault_addr);
		break;

	default:
		pd_pvr_log("received unknown fwccb command 0x%x\n", cmd->cmd_type);
		break;
	}
}

void
pvr_fwccb_process(struct pvr_device *pvr_dev)
{
	struct rogue_fwif_fwccb_cmd *fwccb = (struct rogue_fwif_fwccb_cmd *)pvr_dev->fwccb.ccb;
	struct rogue_fwif_ccb_ctl *ctrl = pvr_dev->fwccb.ctrl;
	u32 read_offset;

	pd_pvr_lock(pvr_dev->fwccb_lock);

	while ((read_offset = READ_ONCE(ctrl->read_offset)) != READ_ONCE(ctrl->write_offset)) {
		struct rogue_fwif_fwccb_cmd cmd = fwccb[read_offset];

		WRITE_ONCE(ctrl->read_offset, (read_offset + 1) & READ_ONCE(ctrl->wrap_mask));

		/* Drop FWCCB lock while we process command. */
		pd_pvr_unlock(pvr_dev->fwccb_lock);

		process_fwccb_command(pvr_dev, &cmd);

		pd_pvr_lock(pvr_dev->fwccb_lock);
	}

	pd_pvr_unlock(pvr_dev->fwccb_lock);
}

static u32
pvr_kccb_capacity(struct pvr_device *pvr_dev)
{
	/* Capacity is the number of slot minus one to cope with the wrapping
	 * mechanisms. If we were to use all slots, we might end up with
	 * read_offset == write_offset, which the FW considers as a KCCB-is-empty
	 * condition.
	 */
	return pvr_dev->kccb.slot_count - 1;
}

static u32
pvr_kccb_used_slot_count_locked(struct pvr_device *pvr_dev)
{
	struct pvr_ccb *pvr_ccb = &pvr_dev->kccb.ccb;
	struct rogue_fwif_ccb_ctl *ctrl = pvr_ccb->ctrl;
	u32 wr_offset = READ_ONCE(ctrl->write_offset);
	u32 rd_offset = READ_ONCE(ctrl->read_offset);
	u32 used_count;

	if (wr_offset >= rd_offset)
		used_count = wr_offset - rd_offset;
	else
		used_count = wr_offset + pvr_dev->kccb.slot_count - rd_offset;

	return used_count;
}

void
pvr_kccb_send_cmd_reserved_powered(struct pvr_device *pvr_dev,
				   struct rogue_fwif_kccb_cmd *cmd,
				   u32 *kccb_slot)
{
	struct pvr_ccb *pvr_ccb = &pvr_dev->kccb.ccb;
	struct rogue_fwif_kccb_cmd *kccb = (struct rogue_fwif_kccb_cmd *)pvr_ccb->ccb;
	struct rogue_fwif_ccb_ctl *ctrl = pvr_ccb->ctrl;
	u32 old_write_offset;
	u32 new_write_offset;

	pd_pvr_lock(pvr_dev->kccb.lock);

	if (!pvr_dev->kccb.reserved_count)
		goto out_unlock;

	old_write_offset = READ_ONCE(ctrl->write_offset);

	/* We reserved the slot, we should have one available. */
	if (!pvr_ccb_slot_available_locked(pvr_ccb, &new_write_offset))
		goto out_unlock;

	__builtin_memcpy(&kccb[old_write_offset], cmd, sizeof(struct rogue_fwif_kccb_cmd));
	if (kccb_slot) {
		*kccb_slot = old_write_offset;
		/* Clear return status for this slot. */
		WRITE_ONCE(pvr_dev->kccb.rtn[old_write_offset],
			   ROGUE_FWIF_KCCB_RTN_SLOT_NO_RESPONSE);
	}
	mb(); /* memory barrier */
	WRITE_ONCE(ctrl->write_offset, new_write_offset);
	pvr_dev->kccb.reserved_count--;

	/* Kick MTS */
	pvr_fw_mts_schedule(pvr_dev,
			    PVR_FWIF_DM_GP & ~ROGUE_CR_MTS_SCHEDULE_DM_CLRMSK);

out_unlock:
	pd_pvr_unlock(pvr_dev->kccb.lock);
}

static bool
pvr_kccb_try_reserve_slot(struct pvr_device *pvr_dev)
{
	bool reserved = false;
	u32 used_count;

	pd_pvr_lock(pvr_dev->kccb.lock);

	used_count = pvr_kccb_used_slot_count_locked(pvr_dev);
	if (pvr_dev->kccb.reserved_count < pvr_kccb_capacity(pvr_dev) - used_count) {
		pvr_dev->kccb.reserved_count++;
		reserved = true;
	}

	pd_pvr_unlock(pvr_dev->kccb.lock);

	return reserved;
}

int
pvr_kccb_reserve_slot_sync(struct pvr_device *pvr_dev)
{
	u64 deadline = pd_pvr_now_us() + RESERVE_SLOT_TIMEOUT_US;
	bool reserved = false;
	u32 retries = 0;

	while (pd_pvr_now_us() < deadline || retries < RESERVE_SLOT_MIN_RETRIES) {
		reserved = pvr_kccb_try_reserve_slot(pvr_dev);
		if (reserved)
			break;

		pd_pvr_udelay(20);
		retries++;
	}

	return reserved ? 0 : -EBUSY;
}

int
pvr_kccb_send_cmd(struct pvr_device *pvr_dev, struct rogue_fwif_kccb_cmd *cmd,
		  u32 *kccb_slot)
{
	int err;

	if (pvr_dev->lost)
		return -EIO;

	err = pvr_kccb_reserve_slot_sync(pvr_dev);
	if (err)
		return err;

	pvr_kccb_send_cmd_reserved_powered(pvr_dev, cmd, kccb_slot);
	return 0;
}

int
pvr_kccb_wait_for_completion(struct pvr_device *pvr_dev, u32 slot_nr,
			     u32 timeout_ms, u32 *rtn_out)
{
	u64 deadline = pd_pvr_now_us() + (u64)timeout_ms * 1000;

	for (;;) {
		u32 gen = pd_pvr_wait_gen();
		u32 rtn = READ_ONCE(pvr_dev->kccb.rtn[slot_nr]);

		if (rtn & ROGUE_FWIF_KCCB_RTN_SLOT_CMD_EXECUTED) {
			if (rtn_out)
				*rtn_out = rtn;
			return 0;
		}
		if (pd_pvr_now_us() >= deadline)
			return -ETIMEDOUT;
		pd_pvr_wait_since(pvr_dev, gen, 10);
	}
}

bool
pvr_kccb_is_idle(struct pvr_device *pvr_dev)
{
	struct rogue_fwif_ccb_ctl *ctrl = pvr_dev->kccb.ccb.ctrl;
	bool idle;

	pd_pvr_lock(pvr_dev->kccb.lock);

	idle = (READ_ONCE(ctrl->write_offset) == READ_ONCE(ctrl->read_offset));

	pd_pvr_unlock(pvr_dev->kccb.lock);

	return idle;
}

int
pvr_kccb_health_check(struct pvr_device *pvr_dev, u32 *rtn_out)
{
	struct rogue_fwif_kccb_cmd cmd;
	u32 slot_nr;
	int err;

	__builtin_memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_type = ROGUE_FWIF_KCCB_CMD_HEALTH_CHECK;

	err = pvr_kccb_send_cmd(pvr_dev, &cmd, &slot_nr);
	if (err)
		return err;

	// the firmware leaves a health check's return slot alone; it is done once the kccb moves past it
	{
		u64 deadline = pd_pvr_now_us() + 1000000;
		struct rogue_fwif_ccb_ctl *ctrl = pvr_dev->kccb.ccb.ctrl;

		while (READ_ONCE(ctrl->read_offset) == slot_nr) {
			if (pd_pvr_now_us() >= deadline)
				return -ETIMEDOUT;
			pd_pvr_wait(pvr_dev, 10);
		}
	}
	if (rtn_out)
		*rtn_out = READ_ONCE(pvr_dev->kccb.rtn[slot_nr]);
	return 0;
}

// pvr_mmu.c
void
pvr_mmu_flush_request_all(struct pvr_device *pvr_dev)
{
	__atomic_fetch_or(&pvr_dev->mmu_flush_cache_flags, PVR_MMU_SYNC_LEVEL_2_FLAGS,
			  __ATOMIC_SEQ_CST);
}

int
pvr_mmu_flush_exec(struct pvr_device *pvr_dev, bool wait)
{
	struct rogue_fwif_kccb_cmd cmd_mmu_cache;
	struct rogue_fwif_mmucachedata *cmd_mmu_cache_data =
		&cmd_mmu_cache.cmd_data.mmu_cache_data;
	int err = 0;
	u32 slot;

	__builtin_memset(&cmd_mmu_cache, 0, sizeof(cmd_mmu_cache));

	/* Can't flush MMU if the firmware hasn't booted yet. */
	if (!pvr_dev->fw_dev.booted)
		return 0;

	cmd_mmu_cache_data->cache_flags =
		__atomic_exchange_n(&pvr_dev->mmu_flush_cache_flags, 0, __ATOMIC_SEQ_CST);

	if (!cmd_mmu_cache_data->cache_flags)
		return 0;

	cmd_mmu_cache.cmd_type = ROGUE_FWIF_KCCB_CMD_MMUCACHE;

	pvr_fw_object_get_fw_addr(pvr_dev->fw_dev.mem.mmucache_sync_obj,
				  &cmd_mmu_cache_data->mmu_cache_sync_fw_addr);
	cmd_mmu_cache_data->mmu_cache_sync_update_value = 0;

	err = pvr_kccb_send_cmd(pvr_dev, &cmd_mmu_cache, &slot);
	if (err)
		goto err_lost;

	if (!wait)
		return 0;

	err = pvr_kccb_wait_for_completion(pvr_dev, slot, 1000, NULL);
	if (err)
		goto err_lost;

	return 0;

err_lost:
	// the hard reset and retry of the linux driver is not ported yet
	pd_pvr_log("mmu cache flush failed (%d), firmware looks locked up\n", err);
	pvr_dev->lost = true;
	return err;
}

// pvr_power.c
static int
pvr_power_send_command(struct pvr_device *pvr_dev, struct rogue_fwif_kccb_cmd *pow_cmd)
{
	struct pvr_fw_device *fw_dev = &pvr_dev->fw_dev;
	u64 deadline;
	u32 slot_nr;
	int err;

	WRITE_ONCE(*fw_dev->power_sync, 0);

	err = pvr_kccb_send_cmd(pvr_dev, pow_cmd, &slot_nr);
	if (err)
		return err;

	/* Wait for FW to acknowledge. */
	deadline = pd_pvr_now_us() + POWER_SYNC_TIMEOUT_US;
	while (READ_ONCE(*fw_dev->power_sync) == 0) {
		if (pd_pvr_now_us() >= deadline)
			return -ETIMEDOUT;
		pd_pvr_udelay(100);
	}
	return 0;
}

static int
pvr_power_request_idle(struct pvr_device *pvr_dev)
{
	struct rogue_fwif_kccb_cmd pow_cmd;

	__builtin_memset(&pow_cmd, 0, sizeof(pow_cmd));

	/* Send FORCED_IDLE request to FW. */
	pow_cmd.cmd_type = ROGUE_FWIF_KCCB_CMD_POW;
	pow_cmd.cmd_data.pow_data.pow_type = ROGUE_FWIF_POW_FORCED_IDLE_REQ;
	pow_cmd.cmd_data.pow_data.power_req_data.pow_request_type = ROGUE_FWIF_POWER_FORCE_IDLE;

	return pvr_power_send_command(pvr_dev, &pow_cmd);
}

static int
pvr_power_request_pwr_off(struct pvr_device *pvr_dev)
{
	struct rogue_fwif_kccb_cmd pow_cmd;

	__builtin_memset(&pow_cmd, 0, sizeof(pow_cmd));

	/* Send POW_OFF request to firmware. */
	pow_cmd.cmd_type = ROGUE_FWIF_KCCB_CMD_POW;
	pow_cmd.cmd_data.pow_data.pow_type = ROGUE_FWIF_POW_OFF_REQ;
	pow_cmd.cmd_data.pow_data.power_req_data.forced = true;

	return pvr_power_send_command(pvr_dev, &pow_cmd);
}

int
pvr_power_fw_disable(struct pvr_device *pvr_dev, bool hard_reset)
{
	if (!hard_reset) {
		int err;

		err = pvr_power_request_idle(pvr_dev);
		if (err) {
			pd_pvr_log("forced idle request failed (%d)\n", err);
			return err;
		}

		err = pvr_power_request_pwr_off(pvr_dev);
		if (err) {
			pd_pvr_log("power off request failed (%d)\n", err);
			return err;
		}
	}

	return pvr_fw_stop(pvr_dev);
}

int
pvr_power_fw_enable(struct pvr_device *pvr_dev)
{
	int err;

	err = pvr_fw_start(pvr_dev);
	if (err)
		return err;

	err = pvr_wait_for_fw_boot(pvr_dev);
	if (err) {
		pd_pvr_log("firmware failed to boot\n");
		pvr_fw_stop(pvr_dev);
		return err;
	}

	return 0;
}

// pvr_fw.c
int
pvr_fw_structure_cleanup(struct pvr_device *pvr_dev, u32 type, struct pvr_fw_object *fw_obj,
			 u32 offset)
{
	struct rogue_fwif_kccb_cmd cmd;
	u32 slot_nr;
	int err;
	u32 rtn;

	struct rogue_fwif_cleanup_request *cleanup_req = &cmd.cmd_data.cleanup_data;

	__builtin_memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_type = ROGUE_FWIF_KCCB_CMD_CLEANUP;
	cmd.kccb_flags = 0;
	cleanup_req->cleanup_type = type;

	switch (type) {
	case ROGUE_FWIF_CLEANUP_FWCOMMONCONTEXT:
		pvr_fw_object_get_fw_addr_offset(fw_obj, offset,
						 &cleanup_req->cleanup_data.context_fw_addr);
		break;
	case ROGUE_FWIF_CLEANUP_HWRTDATA:
		pvr_fw_object_get_fw_addr_offset(fw_obj, offset,
						 &cleanup_req->cleanup_data.hwrt_data_fw_addr);
		break;
	case ROGUE_FWIF_CLEANUP_FREELIST:
		pvr_fw_object_get_fw_addr_offset(fw_obj, offset,
						 &cleanup_req->cleanup_data.freelist_fw_addr);
		break;
	default:
		return -EINVAL;
	}

	err = pvr_kccb_send_cmd(pvr_dev, &cmd, &slot_nr);
	if (err)
		return err;

	err = pvr_kccb_wait_for_completion(pvr_dev, slot_nr, 1000, &rtn);
	if (err)
		return err;

	if (rtn & ROGUE_FWIF_KCCB_RTN_SLOT_CLEANUP_BUSY)
		err = -EBUSY;

	return err;
}
