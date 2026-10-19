// SPDX-License-Identifier: MIT
// msm8916's qcom,msm-iommu-v1: the gpu's context banks on one lpae table, and their secure setup (spec 3)

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "pd_fbsd_os.h"
#include "pd_msm.h"

// stage-1 context bank registers, arm smmu architecture v2
#define CB_SCTLR	0x000
#define CB_TCR2		0x010
#define CB_TTBR0	0x020
#define CB_TTBR1	0x028
#define CB_TCR		0x030
#define CB_MAIR0	0x038
#define CB_MAIR1	0x03c
#define CB_FSR		0x058
#define CB_FAR		0x060
#define CB_FSYNR0	0x068
#define CB_TLBIASID	0x610
#define CB_TLBSYNC	0x7f0
#define CB_TLBSTATUS	0x7f4

// the values the banks were seen programmed with, spec 3.2
static const u32 kSctlrOn = 0x000010e7;
static const u32 kTcr = 0x80802020;
static const u32 kTcr2 = 0x00038002;
static const u32 kMair0 = 0xf404ff44;
static const u32 kMair1 = 0xffffffff;
static const u32 kFsrAll = 0xc00001fe;

// vmsav8-64 descriptors, 4 kb granule
#define DESC_TABLE	0x3ull
#define DESC_AP_RW	(1ull << 6)
#define DESC_SH_OUTER	(2ull << 8)
#define DESC_AF		(1ull << 10)
#define DESC_NG		(1ull << 11)
#define DESC_ADDR	0x0000fffffffff000ull

// secure calls, smc32 with x0 the function and x1 the argument descriptor, spec 3.1
#define SCM_PTBL_SIZE	0x02000c03u
#define SCM_PTBL_INIT	0x02000c04u
#define SCM_RESTORE_CFG	0x02000c02u
#define SECURE_PTBL_SIZE	0x17000u
// a no-map carve-out in the device tree, the os never touches it after the handover
#define SECURE_PTBL_PHYS	0x9bf00000u

struct pd_msm_table {
	u64	*va;
	u64	phys;
	void	*alloc;
};

struct pd_msm_smmu {
	// one mapping per context bank, from the smmu node's iommu-ctx children
	void __iomem		*cb[8];
	u32			banks[4];
	int			nbanks;
	struct pd_msm_table	tabs[64];
	int			ntabs;
	struct mutex		lock;
};

static u64
scm_call(u64 fn, u64 a1, u64 a2, u64 a3, u64 *res1)
{
	register u64 x0 __asm__("x0") = fn;
	register u64 x1 __asm__("x1") = a1;
	register u64 x2 __asm__("x2") = a2;
	register u64 x3 __asm__("x3") = a3;

	__asm__ volatile("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : :
	    "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
	    "x16", "x17", "memory");
	if (res1 != NULL)
		*res1 = x1;
	return (x0);
}

static u32
cb_read(struct pd_msm_smmu *s, u32 bank, u32 off)
{
	return (readl((u8 __iomem *)s->cb[bank] + off));
}

static void
cb_write(struct pd_msm_smmu *s, u32 bank, u32 off, u32 v)
{
	writel(v, (u8 __iomem *)s->cb[bank] + off);
}

static void
cb_write64(struct pd_msm_smmu *s, u32 bank, u32 off, u64 v)
{
	writeq(v, (u8 __iomem *)s->cb[bank] + off);
}

static struct pd_msm_table *
table_new(struct pd_msm_smmu *s)
{
	struct pd_msm_table *t;

	if (s->ntabs == nitems(s->tabs))
		return (NULL);
	t = &s->tabs[s->ntabs];
	t->va = pd_msm_dma_alloc(PAGE_SIZE, &t->phys, &t->alloc);
	if (t->va == NULL)
		return (NULL);
	s->ntabs++;
	return (t);
}

// the next-level table below parent[index], made on first use
static u64 *
table_at(struct pd_msm_smmu *s, u64 *parent, u32 index)
{
	u64 d = parent[index];

	if ((d & 1) != 0) {
		for (int i = 0; i < s->ntabs; i++) {
			if (s->tabs[i].phys == (d & DESC_ADDR))
				return (s->tabs[i].va);
		}
		return (NULL);
	}
	struct pd_msm_table *t = table_new(s);
	if (t == NULL)
		return (NULL);
	parent[index] = (t->phys & DESC_ADDR) | DESC_TABLE;
	return (t->va);
}

// the leaf entry for a 32-bit iova: level 1 from bits 31:30, level 2 from 29:21, level 3 from 20:12
static u64 *
leaf(struct pd_msm_smmu *s, u32 iova)
{
	u64 *l2 = table_at(s, s->tabs[0].va, (iova >> 30) & 0x3);
	u64 *l3 = l2 != NULL ? table_at(s, l2, (iova >> 21) & 0x1ff) : NULL;

	return (l3 != NULL ? &l3[(iova >> 12) & 0x1ff] : NULL);
}

static void
flush(struct pd_msm_smmu *s)
{
	for (int i = 0; i < s->nbanks; i++) {
		u32 b = s->banks[i];
		cb_write(s, b, CB_TLBIASID, b);
		cb_write(s, b, CB_TLBSYNC, 0);
		for (int us = 0; us < 10000 && (cb_read(s, b, CB_TLBSTATUS) & 1) != 0; us++)
			udelay(1);
	}
}

static int
v1_map_pages(struct pd_msm_smmu *s, u32 iova, u64 phys, size_t size)
{
	int error = 0;

	mutex_lock(&s->lock);
	for (size_t off = 0; off < size; off += PAGE_SIZE) {
		u64 *e = leaf(s, iova + off);
		if (e == NULL) {
			error = -ENOMEM;
			break;
		}
		// attribute index 0: normal non-cacheable
		*e = ((phys + off) & DESC_ADDR) | DESC_TABLE | DESC_AP_RW | DESC_SH_OUTER |
		    DESC_AF | DESC_NG;
	}
	wmb();
	flush(s);
	mutex_unlock(&s->lock);
	return (error);
}

static void
v1_unmap_pages(struct pd_msm_smmu *s, u32 iova, size_t size)
{
	mutex_lock(&s->lock);
	for (size_t off = 0; off < size; off += PAGE_SIZE) {
		u64 *e = leaf(s, iova + off);
		if (e != NULL)
			*e = 0;
	}
	wmb();
	flush(s);
	mutex_unlock(&s->lock);
}

static void
v1_dump_fault(void *priv)
{
	struct pd_msm_smmu *s = priv;

	for (int i = 0; i < s->nbanks; i++) {
		u32 b = s->banks[i];
		printf("pd msm: smmu bank %u fsr 0x%x far 0x%08x%08x fsynr0 0x%x\n", b,
		    cb_read(s, b, CB_FSR), cb_read(s, b, CB_FAR + 4), cb_read(s, b, CB_FAR),
		    cb_read(s, b, CB_FSYNR0));
	}
}

// once per boot: the secure world takes a page-table buffer, then restores each bank's config
static int
secure_setup(struct pd_msm_smmu *s, u32 secure_id)
{
	static int done;
	u64 r1 = 0, ret;
	u32 phys = (u32)pdos_bootarg_int("pdadrenosec", SECURE_PTBL_PHYS);

	if (!done) {
		ret = scm_call(SCM_PTBL_SIZE, 1, 0, 0, &r1);
		printf("pd msm: secure ptbl size: ret 0x%llx size 0x%llx\n", ret, r1);
		if (ret != 0)
			return (-EIO);
		ret = scm_call(SCM_PTBL_INIT, 0x23, phys, SECURE_PTBL_SIZE, &r1);
		printf("pd msm: secure ptbl at 0x%x: ret 0x%llx\n", phys, ret);
		if (ret != 0)
			return (-EIO);
		done = 1;
	}
	for (int i = 0; i < s->nbanks; i++) {
		ret = scm_call(SCM_RESTORE_CFG, 2, secure_id, s->banks[i], &r1);
		printf("pd msm: restore sec cfg 0x%x bank %u: ret 0x%llx\n", secure_id, s->banks[i], ret);
		if (ret != 0)
			return (-EIO);
	}
	return (0);
}

static void
bank_on(struct pd_msm_smmu *s, u32 b)
{
	cb_write(s, b, CB_SCTLR, 0);
	cb_write64(s, b, CB_FAR, 0);
	cb_write(s, b, CB_FSR, kFsrAll);
	cb_write64(s, b, CB_TTBR0, ((u64)b << 48) | s->tabs[0].phys);
	cb_write64(s, b, CB_TTBR1, 0);
	cb_write(s, b, CB_TCR2, kTcr2);
	cb_write(s, b, CB_TCR, kTcr);
	cb_write(s, b, CB_MAIR0, kMair0);
	cb_write(s, b, CB_MAIR1, kMair1);
	cb_write(s, b, CB_SCTLR, kSctlrOn);
	printf("pd msm: smmu bank %u on, sctlr 0x%x tcr 0x%x\n", b, cb_read(s, b, CB_SCTLR),
	    cb_read(s, b, CB_TCR));
}

static void *
v1_attach(struct device *dev)
{
	struct pd_msm_smmu *s;
	struct device_node *np = NULL;
	struct of_phandle_args args;
	u32 secure_id = 0;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	mutex_init(&s->lock);
	for (int i = 0; s->nbanks < nitems(s->banks) &&
	    of_parse_phandle_with_args(dev->of_node, "iommus", "#iommu-cells", i, &args) == 0; i++) {
		np = args.np;
		s->banks[s->nbanks++] = args.args_count > 0 ? args.args[0] : 0;
	}
	if (np == NULL) {
		dev_err(dev, "no iommus in the device tree\n");
		goto fail;
	}
	of_property_read_u32(np, "qcom,iommu-secure-id", &secure_id);
	dev_info(dev, "smmu %s, %d banks (first %u), secure id 0x%x\n", np->full_name, s->nbanks,
	    s->banks[0], secure_id);
	// the smmu node has only ranges, each bank is a child whose reg is the bank's offset
	for (int i = 0; i < s->nbanks; i++) {
		struct device_node *child;
		u32 off;

		if (s->banks[i] >= nitems(s->cb)) {
			dev_err(dev, "smmu bank %u out of range\n", s->banks[i]);
			goto fail;
		}
		for_each_child_of_node(np, child) {
			if (of_property_read_u32(child, "reg", &off) == 0 && off == s->banks[i] * 0x1000)
				s->cb[s->banks[i]] = of_iomap(child, 0);
		}
		if (s->cb[s->banks[i]] == NULL) {
			dev_err(dev, "cannot map smmu bank %u\n", s->banks[i]);
			goto fail;
		}
	}
	for (int i = 0; ; i++) {
		struct clk *c = of_clk_get(np, i);
		if (IS_ERR(c)) {
			if (i == 0)
				dev_err(dev, "smmu has no clocks: %ld\n", PTR_ERR(c));
			break;
		}
		if (clk_prepare_enable(c) != 0)
			dev_err(dev, "smmu clock %d did not start\n", i);
	}
	if (table_new(s) == NULL) {
		dev_err(dev, "no memory for the smmu table\n");
		goto fail;
	}
	if (secure_setup(s, secure_id) != 0) {
		dev_err(dev, "secure smmu setup failed\n");
		goto fail;
	}
	for (int i = 0; i < s->nbanks; i++)
		bank_on(s, s->banks[i]);
	return (s);
fail:
	kfree(s);
	return (NULL);
}

// the table walk covers 32-bit iovas only, so does this iommu
static int
v1_map(void *priv, u64 iova, u64 phys, size_t size)
{
	if (iova + size > 0x100000000ull)
		return (-EINVAL);
	return (v1_map_pages(priv, (u32)iova, phys, size));
}

static void
v1_unmap(void *priv, u64 iova, size_t size)
{
	if (iova + size <= 0x100000000ull)
		v1_unmap_pages(priv, (u32)iova, size);
}

const struct pd_msm_iommu pd_msm_iommu_v1 = {
	.compatible = "qcom,msm-iommu-v1",
	.attach = v1_attach,
	.map = v1_map,
	.unmap = v1_unmap,
	.dump_fault = v1_dump_fault,
};
