// SPDX-License-Identifier: MIT
// msm8916 global clock controller: the gpu's and its smmu's clocks and the oxili power domain (spec 2)

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pd_providers.h>

// register model: branch bit 0 enable, bit 31 clock off, rcg command bit 0 update;
// gdsc bit 0 collapse, bit 31 power on
#define CBCR_EN		(1u << 0)
#define CBCR_OFF	(1u << 31)
#define RCG_UPDATE	(1u << 0)
#define GDSC_COLLAPSE	(1u << 0)
#define GDSC_PWR_ON	(1u << 31)
#define APCS_VOTE	0x4500c

enum { K_BRANCH, K_VOTED, K_RCG };

// clock ids as the device tree's clocks lists use them
struct gcc_clk {
	u32	id;
	int	kind;
	u32	reg;		// cbcr, or the rcg's command register
	u32	vote_bit;	// voted branches: the bit in APCS_VOTE
	const char *name;
};

static const struct gcc_clk sClocks[] = {
	{ 14,  K_RCG,    0x59000, 0,  "gfx3d_clk_src" },
	{ 105, K_BRANCH, 0x59024, 0,  "oxili_gmem" },
	{ 117, K_BRANCH, 0x59028, 0,  "oxili_ahb" },
	{ 118, K_BRANCH, 0x59020, 0,  "oxili_gfx3d" },
	{ 129, K_VOTED,  0x12038, 12, "smmu_cfg" },
	{ 140, K_VOTED,  0x12020, 2,  "gfx_tcu" },
	{ 142, K_BRANCH, 0x31024, 0,  "bimc_gfx" },
	{ 143, K_BRANCH, 0x31040, 0,  "bimc_gpu" },
};

// gfx3d root settings seen working: source 3 (800 MHz) with two dividers
static const struct { unsigned long rate; u32 cfg; } sGfx3dRates[] = {
	{ 50000000, 0x31f },
	{ 400000000, 0x303 },
};

// power domain ids as power-domains lists use them
struct gcc_domain {
	u32	id;
	u32	reg;
};

static const struct gcc_domain sDomains[] = {
	{ 5, 0x5901c },		// oxili
};

static void __iomem *sGcc;

static void __iomem *
gcc_base(const struct of_phandle_args *spec)
{
	if (sGcc == NULL)
		sGcc = of_iomap(spec->np, 0);
	return (sGcc);
}

static bool
poll(u32 reg, u32 mask, u32 want, int us)
{
	for (int i = 0; i < us; i += 10) {
		if ((readl(sGcc + reg) & mask) == want)
			return (true);
		udelay(10);
	}
	printf("pd gcc8916: 0x%05x stuck at 0x%08x\n", reg, readl(sGcc + reg));
	return (false);
}

static void *
gcc_clk_xlate(void *priv, const struct of_phandle_args *spec)
{
	if (spec->args_count < 1 || gcc_base(spec) == NULL)
		return (NULL);
	for (int i = 0; i < nitems(sClocks); i++) {
		if (sClocks[i].id == spec->args[0])
			return ((void *)&sClocks[i]);
	}
	printf("pd gcc8916: clock %u is not described\n", spec->args[0]);
	return (NULL);
}

static int
gcc_clk_enable(void *hw)
{
	const struct gcc_clk *c = hw;

	switch (c->kind) {
	case K_BRANCH:
		writel(readl(sGcc + c->reg) | CBCR_EN, sGcc + c->reg);
		return (poll(c->reg, CBCR_OFF, 0, 2000) ? 0 : -ETIMEDOUT);
	case K_VOTED:
		writel(readl(sGcc + APCS_VOTE) | (1u << c->vote_bit), sGcc + APCS_VOTE);
		return (poll(c->reg, CBCR_OFF, 0, 2000) ? 0 : -ETIMEDOUT);
	default:
		// a root clock runs while any branch under it does
		return (0);
	}
}

static void
gcc_clk_disable(void *hw)
{
	const struct gcc_clk *c = hw;

	if (c->kind == K_BRANCH)
		writel(readl(sGcc + c->reg) & ~CBCR_EN, sGcc + c->reg);
	else if (c->kind == K_VOTED)
		writel(readl(sGcc + APCS_VOTE) & ~(1u << c->vote_bit), sGcc + APCS_VOTE);
}

static long
gcc_clk_round(void *hw, unsigned long rate)
{
	const struct gcc_clk *c = hw;
	unsigned long best = sGfx3dRates[0].rate;

	if (c->kind != K_RCG)
		return (0);
	for (int i = 0; i < nitems(sGfx3dRates); i++) {
		if (sGfx3dRates[i].rate <= rate)
			best = sGfx3dRates[i].rate;
	}
	return (best);
}

static int
gcc_clk_set(void *hw, unsigned long rate)
{
	const struct gcc_clk *c = hw;
	unsigned long r = gcc_clk_round(hw, rate);

	if (c->kind != K_RCG)
		return (0);
	for (int i = 0; i < nitems(sGfx3dRates); i++) {
		if (sGfx3dRates[i].rate == r) {
			writel(sGfx3dRates[i].cfg, sGcc + c->reg + 4);
			writel(readl(sGcc + c->reg) | RCG_UPDATE, sGcc + c->reg);
			return (poll(c->reg, RCG_UPDATE, 0, 2000) ? 0 : -ETIMEDOUT);
		}
	}
	return (-EINVAL);
}

static unsigned long
gcc_clk_rate(void *hw)
{
	const struct gcc_clk *c = hw;

	if (c->kind != K_RCG)
		return (0);
	for (int i = 0; i < nitems(sGfx3dRates); i++) {
		if (sGfx3dRates[i].cfg == (readl(sGcc + c->reg + 4) & 0x71f))
			return (sGfx3dRates[i].rate);
	}
	return (0);
}

static const struct pd_clk_ops sClkOps = {
	.xlate = gcc_clk_xlate,
	.enable = gcc_clk_enable,
	.disable = gcc_clk_disable,
	.set_rate = gcc_clk_set,
	.get_rate = gcc_clk_rate,
	.round_rate = gcc_clk_round,
};

static void *
gcc_pd_xlate(void *priv, const struct of_phandle_args *spec)
{
	if (spec->args_count < 1 || gcc_base(spec) == NULL)
		return (NULL);
	for (int i = 0; i < nitems(sDomains); i++) {
		if (sDomains[i].id == spec->args[0])
			return ((void *)&sDomains[i]);
	}
	printf("pd gcc8916: power domain %u is not described\n", spec->args[0]);
	return (NULL);
}

static int
gcc_pd_on(void *hw)
{
	u32 reg = ((const struct gcc_domain *)hw)->reg;

	writel(readl(sGcc + reg) & ~GDSC_COLLAPSE, sGcc + reg);
	return (poll(reg, GDSC_PWR_ON, GDSC_PWR_ON, 10000) ? 0 : -ETIMEDOUT);
}

static int
gcc_pd_off(void *hw)
{
	u32 reg = ((const struct gcc_domain *)hw)->reg;

	writel(readl(sGcc + reg) | GDSC_COLLAPSE, sGcc + reg);
	return (0);
}

static const struct pd_power_ops sPowerOps = {
	.xlate = gcc_pd_xlate,
	.power_on = gcc_pd_on,
	.power_off = gcc_pd_off,
};

void
pd_msm_gcc8916_register(void)
{
	pd_clk_provider_add("qcom,gcc-msm8916", &sClkOps, NULL);
	pd_power_provider_add("qcom,gcc-msm8916", &sPowerOps, NULL);
}
