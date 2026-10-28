// pd_msm_a3xx.c - the a3xx generation: register init, microcode, ring, submissions and fences (spec 4-6)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/jiffies.h>

#include "pd_msm.h"

// gpu registers, dword offsets, named as in the freedreno register database
#define RBBM_HW_VERSION			0x0000
#define RBBM_CLOCK_CTL			0x0010
#define RBBM_SP_HYST_CNT		0x0012
#define RBBM_AHB_CTL0			0x0020
#define RBBM_AHB_CTL1			0x0021
#define RBBM_STATUS			0x0030
#define RBBM_WAIT_IDLE_CLOCKS_CTL	0x0033
#define RBBM_INTERFACE_HANG_INT_CTL	0x0050
#define RBBM_INT_CLEAR_CMD		0x0061
#define RBBM_INT_0_MASK			0x0063
#define RBBM_INT_0_STATUS		0x0064
#define RBBM_PERFCTR_CTL		0x0080
#define RBBM_GPU_BUSY_MASKED		0x0088
#define RBBM_RBBM_CTL			0x0100
#define CP_RB_BASE			0x01c0
#define CP_RB_CNTL			0x01c1
#define CP_RB_RPTR			0x01c4
#define CP_RB_WPTR			0x01c5
#define CP_PFP_UCODE_ADDR		0x01c9
#define CP_PFP_UCODE_DATA		0x01ca
#define CP_QUEUE_THRESHOLDS		0x01d5
#define CP_ME_CNTL			0x01f6
#define CP_ME_RAM_WADDR			0x01f8
#define CP_ME_RAM_DATA			0x01fa
#define CP_DEBUG			0x01fc
#define CP_PROTECT_CTRL			0x045e
#define CP_PROTECT0			0x0460
#define CP_SCRATCH_REG2			0x057a
#define UCHE_CACHE_MODE_CONTROL_REG	0x0e82
#define SP_PERFCOUNTER6_SELECT		0x0eca
#define SP_PERFCOUNTER7_SELECT		0x0ecb
#define VBIF_OUT_RD_LIM_CONF0		0x3034
#define VBIF_OUT_WR_LIM_CONF0		0x3035
#define VBIF_ROUND_ROBIN_QOS_ARB	0x3049

// packet headers and opcodes from the freedreno pm4 database
#define PKT0(reg, n)	((((u32)(n) - 1) << 16) | ((reg) & 0x7fff))
#define PKT2		0x80000000u
#define PKT3(op, n)	((3u << 30) | (((u32)(n) - 1) << 16) | ((u32)(op) << 8))
#define CP_WAIT_FOR_IDLE	0x26
#define CP_INDIRECT_BUFFER_PFD	0x37
#define CP_EVENT_WRITE		0x46
#define CP_ME_INIT		0x48
#define EV_CACHE_FLUSH_TS	0x04
#define EV_HLSQ_FLUSH		0x07
#define EV_IRQ			(1u << 31)

#define RING_BYTES	(32 * 1024)
#define RING_DWORDS	(RING_BYTES / 4)
#define RING_CNTL	0x0800020cu	// 32 KB, rptr read from the register only

// observed init values
static const u32 kProtect[13] = {
	0x63000040, 0x62000080, 0x600000cc, 0x60000108, 0x64000140, 0x66000400, 0x65000700,
	0x610007d8, 0x620007e0, 0x61001178, 0x64001180, 0x60003300, 0x6b00c000,
};
static const u32 kMeInit[17] = {
	0x000003f7, 0, 0, 0, 0x00000080, 0x00000100, 0x00000180, 0x00006600,
	0x00000150, 0x0000014e, 0x00000154, 0x00000001, 0, 0, 0, 0, 0,
};

static u32
gpu_read(struct pd_msm_gpu *g, u32 reg)
{
	return (readl((u8 __iomem *)g->mmio + reg * 4));
}

static void
gpu_write(struct pd_msm_gpu *g, u32 reg, u32 v)
{
	writel(v, (u8 __iomem *)g->mmio + reg * 4);
}

// free dwords in the ring, one stays empty so a full ring never reads as empty
static u32
ring_space(struct pd_msm_gpu *g)
{
	u32 rptr = gpu_read(g, CP_RB_RPTR);

	return ((rptr + RING_DWORDS - g->wptr - 1) % RING_DWORDS);
}

static int
ring_reserve(struct pd_msm_gpu *g, u32 n)
{
	for (int i = 0; i < 100000; i++) {
		if (ring_space(g) >= n)
			return (0);
		udelay(10);
	}
	printf("pd msm: ring full: rptr %u wptr %u status 0x%08x\n", gpu_read(g, CP_RB_RPTR),
	    g->wptr, gpu_read(g, RBBM_STATUS));
	return (-EBUSY);
}

static void
ring_emit(struct pd_msm_gpu *g, u32 w)
{
	u32 *ring = g->ring->cpu;

	ring[g->wptr] = w;
	g->wptr = (g->wptr + 1) % RING_DWORDS;
}

static void
ring_kick(struct pd_msm_gpu *g)
{
	wmb();
	gpu_write(g, CP_RB_WPTR, g->wptr);
}

static int
ring_wait_idle(struct pd_msm_gpu *g, int ms)
{
	for (int i = 0; i < ms * 100; i++) {
		if (gpu_read(g, CP_RB_RPTR) == g->wptr)
			return (0);
		udelay(10);
	}
	printf("pd msm: ring stuck: rptr %u wptr %u status 0x%08x int0 0x%08x\n",
	    gpu_read(g, CP_RB_RPTR), g->wptr, gpu_read(g, RBBM_STATUS), gpu_read(g, RBBM_INT_0_STATUS));
	if (g->iommu->dump_fault != NULL)
		g->iommu->dump_fault(g->iommu_priv);
	return (-ETIMEDOUT);
}

// spec 4: vbif, rbbm, interrupts, ring, protection, then the microcode
static void
hw_init(struct pd_msm_gpu *g, const struct firmware *pm4, const struct firmware *pfp)
{
	const u32 *w;

	gpu_write(g, VBIF_ROUND_ROBIN_QOS_ARB, 0x3);
	gpu_write(g, VBIF_OUT_RD_LIM_CONF0, 0xa);
	gpu_write(g, VBIF_OUT_WR_LIM_CONF0, 0xa);
	gpu_write(g, RBBM_GPU_BUSY_MASKED, 0xffffffff);
	gpu_write(g, RBBM_SP_HYST_CNT, 0x10);
	gpu_write(g, RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);
	gpu_write(g, RBBM_AHB_CTL0, 0x1);
	gpu_write(g, RBBM_AHB_CTL1, 0xa6ffffff);
	gpu_write(g, RBBM_RBBM_CTL, 0x30000);
	gpu_write(g, RBBM_INTERFACE_HANG_INT_CTL, 0x10fff);
	gpu_write(g, UCHE_CACHE_MODE_CONTROL_REG, 0x1);
	gpu_write(g, RBBM_CLOCK_CTL, 0xaaaaaaaa);
	gpu_write(g, RBBM_PERFCTR_CTL, 0x1);
	gpu_write(g, SP_PERFCOUNTER6_SELECT, 0x1d);
	gpu_write(g, SP_PERFCOUNTER7_SELECT, 0xe);
	gpu_write(g, RBBM_INT_0_MASK, 0x0231ef22);

	g->wptr = 0;
	gpu_write(g, CP_RB_CNTL, RING_CNTL);
	gpu_write(g, CP_RB_BASE, (u32)g->ring->iova);

	gpu_write(g, CP_PROTECT_CTRL, 0x7);
	for (int i = 0; i < nitems(kProtect); i++)
		gpu_write(g, CP_PROTECT0 + i, kProtect[i]);
	gpu_write(g, CP_DEBUG, 0x0a000000);

	// word 0 of each firmware file is a header and is not uploaded
	w = (const u32 *)pm4->data;
	gpu_write(g, CP_ME_RAM_WADDR, 0);
	for (size_t i = 1; i < pm4->size / 4; i++)
		gpu_write(g, CP_ME_RAM_DATA, w[i]);
	w = (const u32 *)pfp->data;
	gpu_write(g, CP_PFP_UCODE_ADDR, 0);
	for (size_t i = 1; i < pfp->size / 4; i++)
		gpu_write(g, CP_PFP_UCODE_DATA, w[i]);

	gpu_write(g, CP_QUEUE_THRESHOLDS, 0x000e0602);
}

// spec 5: release the micro engine and feed it its init packet
static int
cp_start(struct pd_msm_gpu *g)
{
	gpu_write(g, CP_ME_CNTL, 0);
	ring_emit(g, PKT3(CP_ME_INIT, nitems(kMeInit)));
	for (int i = 0; i < nitems(kMeInit); i++)
		ring_emit(g, kMeInit[i]);
	ring_kick(g);
	return (ring_wait_idle(g, 1000));
}

// probe time: the hardware answers, the ring and the fence page exist
static int
a3xx_init(struct pd_msm_dev *mdev)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	int error;

	dev_info(&mdev->pdev->dev, "hw version 0x%08x\n", gpu_read(g, RBBM_HW_VERSION));
	g->ring = pd_msm_bo_new(&mdev->drm, RING_BYTES, PD_MSM_BO_KERNEL);
	g->memptrs = pd_msm_bo_new(&mdev->drm, PAGE_SIZE, PD_MSM_BO_KERNEL);
	if (IS_ERR(g->ring) || IS_ERR(g->memptrs))
		return (-ENOMEM);
	if ((error = pd_msm_bo_iova(mdev, g->ring)) != 0 ||
	    (error = pd_msm_bo_iova(mdev, g->memptrs)) != 0)
		return (error);
	return (0);
}

// microcode from /Library/Firmware, register init, then the cp
static int
a3xx_start(struct pd_msm_dev *mdev)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	struct device *dev = &mdev->pdev->dev;
	const struct firmware *pm4 = NULL, *pfp = NULL;
	int error;

	if ((error = request_firmware(&pm4, "qcom/a300_pm4.fw", dev)) != 0 ||
	    (error = request_firmware(&pfp, "qcom/a300_pfp.fw", dev)) != 0) {
		dev_err(dev, "firmware: %d\n", error);
		if (pm4 != NULL)
			release_firmware(pm4);
		return (error);
	}
	hw_init(g, pm4, pfp);
	release_firmware(pm4);
	release_firmware(pfp);
	error = cp_start(g);
	if (error == 0)
		dev_info(dev, "cp running, ring at 0x%08llx, fences at 0x%08llx\n",
		    (unsigned long long)g->ring->iova, (unsigned long long)g->memptrs->iova);
	return (error);
}

static u32
a3xx_completed(struct pd_msm_dev *mdev)
{
	rmb();
	return (READ_ONCE(*(volatile u32 *)mdev->gpu.memptrs->cpu));
}

// spec 6: each indirect buffer, then the fence seqno into scratch and memory with an interrupt.
// a3xx addresses are 32 bits, the va space keeps every iova below 4 GB
static int
a3xx_submit(struct pd_msm_dev *mdev, struct drm_file *file, const u64 *ib_iova,
    const u32 *ib_dwords, int nib, u32 seqno)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	int error;

	if ((error = ring_reserve(g, nib * 3 + 12)) != 0)
		return (error);
	for (int i = 0; i < nib; i++) {
		ring_emit(g, PKT3(CP_INDIRECT_BUFFER_PFD, 2));
		ring_emit(g, (u32)ib_iova[i]);
		ring_emit(g, ib_dwords[i]);
	}
	ring_emit(g, PKT2);
	ring_emit(g, PKT0(CP_SCRATCH_REG2, 1));
	ring_emit(g, seqno);
	ring_emit(g, PKT3(CP_EVENT_WRITE, 1));
	ring_emit(g, EV_HLSQ_FLUSH);
	ring_emit(g, PKT3(CP_WAIT_FOR_IDLE, 1));
	ring_emit(g, 0);
	ring_emit(g, PKT3(CP_EVENT_WRITE, 3));
	ring_emit(g, EV_IRQ | EV_CACHE_FLUSH_TS);
	ring_emit(g, (u32)g->memptrs->iova);
	ring_emit(g, seqno);
	ring_kick(g);
	return (0);
}

// the fence interrupt is masked off at the gic, ack it here so the status stays readable
static void
a3xx_fence_ack(struct pd_msm_dev *mdev)
{
	u32 st = gpu_read(&mdev->gpu, RBBM_INT_0_STATUS);

	if (st != 0)
		gpu_write(&mdev->gpu, RBBM_INT_CLEAR_CMD, st);
}

const struct pd_msm_gpu_funcs pd_msm_a3xx_funcs = {
	.init = a3xx_init,
	.start = a3xx_start,
	.submit = a3xx_submit,
	.completed = a3xx_completed,
	.fence_ack = a3xx_fence_ack,
};
