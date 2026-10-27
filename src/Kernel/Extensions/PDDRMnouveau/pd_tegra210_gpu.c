// SPDX-License-Identifier: MIT
// tegra210 clocks, reset, power domain and vdd_gpu for gm20b over FreeBSD's car, pmc and efuse code

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pd_providers.h>
#include <soc/tegra/fuse.h>

#include "tegra210_gpu.h"
#include "tegra_i2c_poll.h"

// handles the providers hand out, the clock and reset ids are the car's own peripheral indexes
enum { TCLK_PGATE, TCLK_PLLP_OUT5 };

struct tclk {
	int		kind;
	uint32_t	idx;
};

// pllP_out5 has no id in the car's numbering, the bindings below use this one
#define PD_TEGRA210_PLLP_OUT5	0x10000

// pllP_out0 runs at 408 MHz: freebsd's tegra210 clock init table puts the uarts on it at that rate
#define PLLP_OUT0_HZ		408000000UL

static struct tclk sGpuClk = { TCLK_PGATE, TEGRA210_CLK_GPU };
static struct tclk sRefClk = { TCLK_PGATE, TEGRA210_CLK_PLLG_REF };
static struct tclk sPwrClk = { TCLK_PLLP_OUT5, 0 };
static struct tclk sGpuRst = { TCLK_PGATE, TEGRA210_CLK_GPU };

static void __iomem *sCar;

// pmc words through the firmware's sip call: freebsd tegra210_pmc.c PMC_SMC, read 0xaa, write 0xbb
static uint32_t
pmc_read(uint32_t off)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xaa;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = 0;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	if (x0 != 0)
		printf("pd tegra210: pmc read 0x%x failed, %llu\n", off, (unsigned long long)x0);
	return (x0 == 0 ? (uint32_t)x1 : 0);
}

static void
pmc_write(uint32_t off, uint32_t val)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xbb;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = val;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	printf("pd tegra210: pmc write 0x%x = 0x%x, %llu\n", off, val, (unsigned long long)x0);
}

// a block whose clock gate is open and whose reset is released, else nothing is mapped
static void __iomem *
map_if_clocked(struct device_node *np, uint32_t idx)
{
	bool on = false;

	if (np == NULL)
		return (NULL);
	tegra210_pgate_get_gate(idx, &on);
	if (!on || tegra210_hwreset_is_asserted(idx) != 0) {
		printf("pd tegra210: %s not mapped, clock %u off or held in reset\n", np->full_name, idx);
		return (NULL);
	}
	return (of_iomap(np, 0));
}

// clocks

static void *
car_clk_xlate(void *priv, const struct of_phandle_args *spec)
{
	if (spec->args_count < 1)
		return (NULL);
	switch (spec->args[0]) {
	case TEGRA210_CLK_GPU:
		return (&sGpuClk);
	case TEGRA210_CLK_PLLG_REF:
		return (&sRefClk);
	case PD_TEGRA210_PLLP_OUT5:
		return (&sPwrClk);
	}
	printf("pd tegra210: clock %u has no handler\n", spec->args[0]);
	return (NULL);
}

static int
car_clk_enable(void *hw)
{
	struct tclk *c = hw;

	printf("pd tegra210: clock %u kind %d on\n", c->idx, c->kind);
	if (c->kind == TCLK_PLLP_OUT5)
		return (-tegra210_pllp_out5_set_gate(true));
	return (-tegra210_pgate_set_gate(c->idx, true));
}

static void
car_clk_disable(void *hw)
{
	struct tclk *c = hw;

	// pllP_out5 feeds more than the gpu, so it stays running
	if (c->kind == TCLK_PGATE)
		tegra210_pgate_set_gate(c->idx, false);
}

// gpu hangs off osc_div_clk and pllg_ref off clk_m (freebsd tegra210_clk_per.c gate table)
static unsigned long
car_clk_get_rate(void *hw)
{
	struct tclk *c = hw;

	if (c->kind == TCLK_PLLP_OUT5)
		return (tegra210_pllp_out5_get_freq(PLLP_OUT0_HZ));
	if (c->idx == TEGRA210_CLK_GPU)
		return (tegra210_osc_div_freq());
	return (tegra210_clk_m_freq());
}

static int
car_clk_set_rate(void *hw, unsigned long rate)
{
	struct tclk *c = hw;

	printf("pd tegra210: clock %u kind %d to %lu Hz\n", c->idx, c->kind, rate);
	if (c->kind != TCLK_PLLP_OUT5)
		return (car_clk_get_rate(hw) == rate ? 0 : -EINVAL);
	return (-tegra210_pllp_out5_set_freq(PLLP_OUT0_HZ, rate));
}

static long
car_clk_round_rate(void *hw, unsigned long rate)
{
	struct tclk *c = hw;

	// the 7.1 divider: twice the parent over a half-step count of at least two
	if (c->kind == TCLK_PLLP_OUT5 && rate != 0) {
		unsigned long halves = (2 * PLLP_OUT0_HZ + rate / 2) / rate;
		return (2 * PLLP_OUT0_HZ / (halves < 2 ? 2 : halves));
	}
	return (car_clk_get_rate(hw));
}

static const struct pd_clk_ops car_clk_ops = {
	.xlate = car_clk_xlate,
	.enable = car_clk_enable,
	.disable = car_clk_disable,
	.set_rate = car_clk_set_rate,
	.get_rate = car_clk_get_rate,
	.round_rate = car_clk_round_rate,
};

// resets

static void *
car_rst_xlate(void *priv, const struct of_phandle_args *spec)
{
	if (spec->args_count >= 1 && spec->args[0] == TEGRA210_CLK_GPU)
		return (&sGpuRst);
	printf("pd tegra210: reset %u has no handler\n", spec->args_count >= 1 ? spec->args[0] : 0);
	return (NULL);
}

static int
car_rst_assert(void *hw)
{
	printf("pd tegra210: reset %u asserted\n", ((struct tclk *)hw)->idx);
	return (-tegra210_hwreset_by_idx(((struct tclk *)hw)->idx, true));
}

// mc client hot reset: control at 0x970 and status at 0x974, gpu is bit 2 (tegra x1 trm 18.11.1.114)
#define MC_CLIENT_HOTRESET_CTRL_1	0x970
#define MC_CLIENT_HOTRESET_STATUS_1	0x974
#define MC_HOTRESET_GPU			(1u << 2)

static void __iomem *sMc;
extern void (*pd_flcn_mc_dump)(const char *);

// the last step of rail ungating lets the gpu's memory traffic through again (tegra x1 trm 12.2.7)
static void
gpu_mc_unflush(void)
{
	u32 ctrl;

	if (sMc == NULL)
		sMc = of_iomap(of_find_compatible_node(NULL, NULL, "nvidia,tegra210-smmu"), 0);
	if (sMc == NULL)
		return;
	ctrl = readl(sMc + MC_CLIENT_HOTRESET_CTRL_1);
	printf("pd tegra210: mc hotreset ctrl1 0x%08x status1 0x%08x\n", ctrl,
	    readl(sMc + MC_CLIENT_HOTRESET_STATUS_1));
	if (ctrl & MC_HOTRESET_GPU) {
		writel(ctrl & ~MC_HOTRESET_GPU, sMc + MC_CLIENT_HOTRESET_CTRL_1);
		printf("pd tegra210: mc gpu flush cleared, ctrl1 0x%08x\n", readl(sMc + MC_CLIENT_HOTRESET_CTRL_1));
	}
}

// mc error capture and the gpu's smmu asid words, all non-secure (tegra x1 trm 18.11.1.1-4, .135, .136)
#define MC_INTSTATUS		0x000
#define MC_ERR_STATUS		0x008
#define MC_ERR_ADR		0x00c
#define MC_SMMU_GPU_ASID	0xaa8
#define MC_SMMU_GPUB_ASID	0xaac

static void
pd_tegra210_mc_dump(const char *why)
{
	if (sMc == NULL)
		return;
	printf("pd tegra210: mc at %s: intstatus 0x%08x err_status 0x%08x err_adr 0x%08x gpu_asid 0x%08x gpub_asid 0x%08x\n",
	    why, readl(sMc + MC_INTSTATUS), readl(sMc + MC_ERR_STATUS), readl(sMc + MC_ERR_ADR),
	    readl(sMc + MC_SMMU_GPU_ASID), readl(sMc + MC_SMMU_GPUB_ASID));
}

static int
car_rst_deassert(void *hw)
{
	printf("pd tegra210: reset %u released\n", ((struct tclk *)hw)->idx);
	// rail ungating releases the gpu reset with its clock stopped, then restarts it (tegra x1 trm 12.2.7)
	if (((struct tclk *)hw)->idx == TEGRA210_CLK_GPU) {
		int error;

		udelay(10);
		tegra210_pgate_set_gate(TEGRA210_CLK_GPU, false);
		error = -tegra210_hwreset_by_idx(TEGRA210_CLK_GPU, false);
		udelay(10);
		tegra210_pgate_set_gate(TEGRA210_CLK_GPU, true);
		printf("pd tegra210: gpu out of reset, clock back on, gpu_rg_cntrl 0x%x\n", pmc_read(0x2d4));
		gpu_mc_unflush();
		return (error);
	}
	return (-tegra210_hwreset_by_idx(((struct tclk *)hw)->idx, false));
}

static const struct pd_reset_ops car_rst_ops = {
	.xlate = car_rst_xlate,
	.assert = car_rst_assert,
	.deassert = car_rst_deassert,
};

// the gpu-pd domain: the rail is vdd_gpu and the clamp is nouveau's own step, so this only reports
static void *
gpu_pd_xlate(void *priv, const struct of_phandle_args *spec)
{
	return (priv);
}

static int
gpu_pd_power_on(void *hw)
{
	printf("pd tegra210: gpu-pd, gpu_rg_cntrl 0x%x, the rail comes from vdd_gpu\n", pmc_read(0x2d4));
	return (0);
}

static const struct pd_power_ops gpu_pd_ops = {
	.xlate = gpu_pd_xlate,
	.power_on = gpu_pd_power_on,
};

// vdd_gpu: a pwm regulator on pwm@7000a000 channel 1 with its enable on max77620 gpio6

static struct device_node *sVddGpu;
static struct tegra_i2c_poll sPwrI2c;
static uint8_t sPmicAddr;
static void __iomem *sPwm;
static void __iomem *sPinmux;
static u32 sVddMin, sVddMax, sVddInit, sVddRamp;
static int sVddOn;

// max77620.h: output value bit 3, push-pull drive bit 0, alternate mode bits in AME_GPIO
#define MAX77620_REG_GPIO6		0x3c
#define MAX77620_REG_AME_GPIO		0x40
#define MAX77620_GPIO_OUT		(1u << 3)
#define MAX77620_GPIO_PUSHPULL		(1u << 0)

// pwm csr: enable bit 31, duty in n/256 at 24:16, frequency divider at 12:0 (tegra x1 trm 39.2.1)
#define PWM_CSR1			0x10
#define PWM_CSR_ENB			(1u << 31)
#define PWM_CSR_DUTY_SHIFT		16
#define PWM_DUTY_FULL			256

// car clk source pwm at 0x110: source at 31:29 (0 is pllP_out0), divisor n gives n/2+1 (tegra x1 trm 5.2.56)
#define CAR_CLK_SOURCE_PWM		0x110
// 408 MHz over 8.5 is 48 MHz, the pwm's device clock limit (tegra x1 trm ch. 39)
#define PWM_CLK_DIVISOR			15

// pinmux lcd_gpio2_pv4 at apb_misc 0x320c, second pinmux window in the dt (tegra x1 trm 9.15.129)
#define PINMUX_LCD_GPIO2		0x20c
#define PINMUX_PM_MASK			0x3
#define PINMUX_PM_PWM1			0x1
#define PINMUX_PUPD_MASK		(0x3u << 2)
#define PINMUX_TRISTATE			(1u << 4)
#define PINMUX_PARK			(1u << 5)
#define PINMUX_E_INPUT			(1u << 6)
#define PINMUX_LOCK			(1u << 7)

static int
pmic_read(uint8_t reg, uint8_t *val)
{
	if (sPwrI2c.regs == NULL)
		return (-ENODEV);
	return (-tegra_i2c_poll_exec(&sPwrI2c, sPmicAddr, &reg, 1, val, 1, true));
}

static int
pmic_write(uint8_t reg, uint8_t val)
{
	uint8_t buf[2] = { reg, val };

	if (sPwrI2c.regs == NULL)
		return (-ENODEV);
	return (-tegra_i2c_poll_exec(&sPwrI2c, sPmicAddr, NULL, 0, buf, 2, false));
}

// a linear map of the dt's voltage range onto the duty, 0/256 at the minimum and 256/256 at the maximum
static u32
vdd_duty(int uv)
{
	if (uv < (int)sVddMin)
		uv = sVddMin;
	if (uv > (int)sVddMax)
		uv = sVddMax;
	return (((u64)(uv - sVddMin) * PWM_DUTY_FULL + (sVddMax - sVddMin) / 2) / (sVddMax - sVddMin));
}

static int
vdd_get_voltage(void *hw)
{
	u32 duty;

	if (sPwm == NULL || (readl(sPwm + PWM_CSR1) & PWM_CSR_ENB) == 0)
		return (-ENODEV);
	duty = (readl(sPwm + PWM_CSR1) >> PWM_CSR_DUTY_SHIFT) & 0x1ff;
	return (sVddMin + (u64)duty * (sVddMax - sVddMin) / PWM_DUTY_FULL);
}

// frequency divider 0: the 48 MHz device clock over 256 is a 5.3 us period, the dt asks for 4.88 us
static int
vdd_set_voltage(void *hw, int min_uv, int max_uv)
{
	u32 duty = vdd_duty(min_uv);

	if (sPwm == NULL)
		return (-ENODEV);
	writel(PWM_CSR_ENB | (duty << PWM_CSR_DUTY_SHIFT), sPwm + PWM_CSR1);
	udelay(sVddRamp);
	printf("pd tegra210: vdd_gpu %d uV asked, duty %u/256, csr1 0x%08x\n", min_uv, duty, readl(sPwm + PWM_CSR1));
	return (0);
}

static bool
vdd_pinmux(void)
{
	u32 v;

	if (sPinmux == NULL)
		return (false);
	v = readl(sPinmux + PINMUX_LCD_GPIO2);
	printf("pd tegra210: lcd_gpio2_pv4 pinmux 0x%08x\n", v);
	if ((v & PINMUX_PM_MASK) == PINMUX_PM_PWM1 &&
	    (v & (PINMUX_TRISTATE | PINMUX_PARK | PINMUX_E_INPUT | PINMUX_PUPD_MASK)) == 0)
		return (true);
	if (v & PINMUX_LOCK)
		return (false);
	// the dt's pinmux for this pin: function pwm1, no pull, input off, not tristated
	v &= ~(PINMUX_PM_MASK | PINMUX_PUPD_MASK | PINMUX_TRISTATE | PINMUX_PARK | PINMUX_E_INPUT);
	writel(v | PINMUX_PM_PWM1, sPinmux + PINMUX_LCD_GPIO2);
	printf("pd tegra210: lcd_gpio2_pv4 pinmux now 0x%08x\n", readl(sPinmux + PINMUX_LCD_GPIO2));
	return (true);
}

static void *
vdd_xlate(void *priv, struct device_node *np)
{
	return (np == sVddGpu ? np : NULL);
}

static int
vdd_enable(void *hw)
{
	uint8_t gpio6 = 0, ame = 0;
	int error;

	if (sVddOn)
		return (0);
	if ((error = pmic_read(MAX77620_REG_GPIO6, &gpio6)) != 0 ||
	    (error = pmic_read(MAX77620_REG_AME_GPIO, &ame)) != 0) {
		printf("pd tegra210: vdd_gpu, max77620 not readable (%d)\n", error);
		return (error);
	}
	printf("pd tegra210: vdd_gpu gpio6 0x%02x ame 0x%02x\n", gpio6, ame);
	if (ame & (1u << 6)) {
		printf("pd tegra210: vdd_gpu, gpio6 is in an alternate mode, left alone\n");
		return (-ENXIO);
	}
	if (!vdd_pinmux()) {
		printf("pd tegra210: vdd_gpu, pwm1 pin locked to another function\n");
		return (-ENXIO);
	}

	// the pwm block: clock source and divisor, then the gate opens and the reset lifts
	writel((0u << 29) | PWM_CLK_DIVISOR, sCar + CAR_CLK_SOURCE_PWM);
	tegra210_pgate_set_gate(TEGRA210_CLK_PWM, true);
	udelay(2);
	tegra210_hwreset_by_idx(TEGRA210_CLK_PWM, false);
	udelay(2);
	if (sPwm == NULL)
		sPwm = of_iomap(of_find_compatible_node(NULL, NULL, "nvidia,tegra124-pwm"), 0);
	if (sPwm == NULL)
		return (-ENODEV);

	// the dt's initial voltage before the rail turns on
	vdd_set_voltage(hw, sVddInit, sVddInit);
	// gpio5 shares gpio6's dt pin group and reads 0x09 driven high, gpio6 and gpio7 read 0x02 from a
	// cold boot (gpu8), so bit 1, unnamed in freebsd's max77620.h, keeps a pin an input
	uint8_t gpio[8];
	for (int i = 0; i < 8; i++)
		pmic_read(0x36 + i, &gpio[i]);
	printf("pd tegra210: max77620 gpio0-7 %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    gpio[0], gpio[1], gpio[2], gpio[3], gpio[4], gpio[5], gpio[6], gpio[7]);
	if ((gpio[5] & MAX77620_GPIO_OUT) && (gpio[5] & 0x02) == 0 && (gpio6 & 0x02)) {
		printf("pd tegra210: gpio6 has bit 1 set where output gpio5 has it clear, clearing it\n");
		gpio6 &= ~0x02;
	}
	gpio6 = (gpio6 | MAX77620_GPIO_PUSHPULL | MAX77620_GPIO_OUT);
	if ((error = pmic_write(MAX77620_REG_GPIO6, gpio6)) != 0) {
		printf("pd tegra210: vdd_gpu, gpio6 write failed (%d)\n", error);
		return (error);
	}
	udelay(sVddRamp);
	pmic_read(MAX77620_REG_GPIO6, &gpio6);
	printf("pd tegra210: vdd_gpu on, gpio6 0x%02x, %d uV\n", gpio6, vdd_get_voltage(hw));
	sVddOn = 1;
	return (0);
}

static int
vdd_disable(void *hw)
{
	uint8_t gpio6 = 0;

	if (!sVddOn || pmic_read(MAX77620_REG_GPIO6, &gpio6) != 0)
		return (0);
	pmic_write(MAX77620_REG_GPIO6, gpio6 & ~MAX77620_GPIO_OUT);
	sVddOn = 0;
	printf("pd tegra210: vdd_gpu off\n");
	return (0);
}

static const struct pd_regulator_ops vdd_ops = {
	.xlate = vdd_xlate,
	.enable = vdd_enable,
	.disable = vdd_disable,
	.get_voltage = vdd_get_voltage,
	.set_voltage = vdd_set_voltage,
};

// vdd_gpu among the regulator-pwm nodes, which share one parent bus in the l4t tree
static struct device_node *
find_vdd_gpu(void)
{
	struct device_node *first = of_find_compatible_node(NULL, NULL, "regulator-pwm");
	struct device_node *bus = first != NULL ? of_get_parent(first) : NULL;
	struct device_node *np;
	const char *name;

	if (bus == NULL)
		return (NULL);
	for_each_child_of_node(bus, np) {
		if (of_device_is_compatible(np, "regulator-pwm") &&
		    of_property_read_string(np, "regulator-name", &name) == 0 && strcmp(name, "vdd_gpu") == 0)
			return (np);
	}
	return (NULL);
}

void
pd_tegra210_gpu_init(void)
{
	struct device_node *car = of_find_compatible_node(NULL, NULL, "nvidia,tegra210-car");
	struct device_node *pmic, *gpupd;
	struct tegra210_gpu_bus bus;
	struct tegra210_sku sku;
	u32 reg;

	if (car == NULL || (sCar = of_iomap(car, 0)) == NULL) {
		printf("pd tegra210: no car node mapped, gm20b providers not set up\n");
		return;
	}
	bzero(&bus, sizeof(bus));
	bus.car = sCar;
	bus.pmc_read = pmc_read;
	bus.pmc_write = pmc_write;
	tegra210_gpu_attach(&bus);
	bus.fuse = map_if_clocked(of_find_compatible_node(NULL, NULL, "nvidia,tegra210-efuse"),
	    TEGRA210_CLK_FUSE);
	tegra210_gpu_attach(&bus);
	printf("pd tegra210: osc %llu, osc_div %llu, clk_m %llu, pllP_out5 %llu Hz\n",
	    (unsigned long long)tegra210_osc_freq(), (unsigned long long)tegra210_osc_div_freq(),
	    (unsigned long long)tegra210_clk_m_freq(),
	    (unsigned long long)tegra210_pllp_out5_get_freq(PLLP_OUT0_HZ));

	bzero(&sku, sizeof(sku));
	if (tegra210_efuse_sku(&sku) == 0) {
		tegra_sku_info.gpu_speedo_id = sku.gpu_speedo_id;
		tegra_sku_info.gpu_speedo_value = sku.gpu_speedo_value;
		printf("pd tegra210: sku 0x%x, speedo rev %d, gpu speedo id %u value %u\n",
		    sku.sku_id, sku.speedo_rev, sku.gpu_speedo_id, sku.gpu_speedo_value);
	}

	pmic = of_find_compatible_node(NULL, NULL, "maxim,max77620");
	if (pmic != NULL && of_property_read_u32(pmic, "reg", &reg) == 0) {
		sPmicAddr = reg;
		sPwrI2c.regs = map_if_clocked(of_get_parent(pmic), TEGRA210_CLK_I2C5);
	}
	sVddGpu = find_vdd_gpu();
	sPinmux = of_iomap(of_find_compatible_node(NULL, NULL, "nvidia,tegra210-pinmux"), 1);
	// range, start value and settle time from the vdd_gpu node
	if (sVddGpu != NULL && (of_property_read_u32(sVddGpu, "regulator-min-microvolt", &sVddMin) != 0 ||
	    of_property_read_u32(sVddGpu, "regulator-max-microvolt", &sVddMax) != 0 || sVddMax <= sVddMin ||
	    of_property_read_u32(sVddGpu, "regulator-init-microvolt", &sVddInit) != 0)) {
		printf("pd tegra210: vdd_gpu node lacks its voltage range, not used\n");
		sVddGpu = NULL;
	}
	if (sVddGpu != NULL && of_property_read_u32(sVddGpu, "regulator-enable-ramp-delay", &sVddRamp) != 0)
		sVddRamp = 1000;
	if (sVddGpu != NULL)
		printf("pd tegra210: vdd_gpu %u..%u uV, start %u uV, ramp %u us\n", sVddMin, sVddMax, sVddInit, sVddRamp);

	// the mc's gpu hot reset state before the gpu is touched
	sMc = of_iomap(of_find_compatible_node(NULL, NULL, "nvidia,tegra210-smmu"), 0);
	printf("pd tegra210: mc mapped %s\n", sMc != NULL ? "yes" : "no");
	if (sMc != NULL)
		printf("pd tegra210: mc hotreset ctrl1 0x%08x status1 0x%08x\n",
		    readl(sMc + MC_CLIENT_HOTRESET_CTRL_1), readl(sMc + MC_CLIENT_HOTRESET_STATUS_1));
	if (sMc != NULL)
		pd_flcn_mc_dump = pd_tegra210_mc_dump;
	pd_clk_provider_add("nvidia,tegra210-car", &car_clk_ops, NULL);
	pd_reset_provider_add("nvidia,tegra210-car", &car_rst_ops, NULL);
	gpupd = of_find_compatible_node(NULL, NULL, "nvidia,tegra210-gpu-pd");
	if (gpupd != NULL)
		pd_power_provider_add("nvidia,tegra210-gpu-pd", &gpu_pd_ops, gpupd);
	pd_regulator_provider_add("regulator-pwm", &vdd_ops, NULL);

	// the l4t tree's gpu node names none of these, the car ids are freebsd's
	pd_consumer_binding_add("nvidia,gm20b", "clocks", "gpu", car, TEGRA210_CLK_GPU);
	pd_consumer_binding_add("nvidia,gm20b", "clocks", "ref", car, TEGRA210_CLK_PLLG_REF);
	pd_consumer_binding_add("nvidia,gm20b", "clocks", "pwr", car, PD_TEGRA210_PLLP_OUT5);
	pd_consumer_binding_add("nvidia,gm20b", "resets", "gpu", car, TEGRA210_CLK_GPU);
	if (sVddGpu != NULL)
		pd_consumer_binding_add("nvidia,gm20b", "vdd-supply", "vdd", sVddGpu, 0);
	printf("pd tegra210: gm20b providers up, vdd_gpu %s, pmic i2c %s\n",
	    sVddGpu != NULL ? sVddGpu->full_name : "missing", sPwrI2c.regs != NULL ? "mapped" : "unmapped");
}
