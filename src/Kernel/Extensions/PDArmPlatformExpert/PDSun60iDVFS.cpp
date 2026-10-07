// the A733's cluster clocks, CPU rails and thermal sensors. pddvfs=1 reads them back and throttles on Linux's trips,
// pddvfs=2 also writes today's settings back once, and pda76mhz=/pda55mhz=MHZ then raise a cluster to an OPP

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include "PDSun60iDVFS.h"
#include "PDArmCPU.h"
#include <arm/machine_routines.h>

extern "C" int cpu_number(void);
extern "C" kern_return_t cpu_xcall(int cpu, void (*func)(void *), void *param);

extern "C" int pd_sync_bounded(void);

// the CPU PLL block, 26 MHz reference: freq = 26 MHz * N (P = M0 = M1 = 1), CLK_REG [26:24] 3 = PLL, [17:16] post-divider
#define PLL_BASE        0x08870000ULL
#define PLL_L_CTRL      0x1000
#define PLL_L_CLK       0x101C
#define PLL_B_CTRL      0x2000
#define PLL_B_CLK       0x201C
#define PLL_DSU_CTRL    0x3000
#define PLL_DSU_CLK     0x301C
#define PLL_REF_KHZ     26000

// the AXP8191 sits on S_TWI0 at 0x36: DCDC5 (0x16) feeds the A55s, DCDC3 (0x14) the A76s, 0.5 V + 10 mV * (v & 0x7f)
#define TWI_BASE        0x07083000ULL
#define TWI_DATA        0x08
#define TWI_CNTR        0x0C
#define TWI_STAT        0x10
#define TWI_CNTR_BUS_EN 0x40
#define TWI_CNTR_M_STA  0x20
#define TWI_CNTR_M_STP  0x10
#define TWI_CNTR_INT    0x08
#define TWI_CNTR_A_ACK  0x04
#define AXP_ADDR        0x36
#define AXP_DCDC3       0x14
#define AXP_DCDC5       0x16

// sensors: 0 CPUB, 1 DDR, 2 NPU, 3 CPUL, 4 GPU. raw counts fall as it warms: T = 165 C - 57 mC * raw,
// fitted on this board against Linux's readings (within 1.5 C)
#define CCU_BASE        0x02002000ULL
#define CCU_THS0_BGR    0x0FE4
#define THS_BASE        0x02522000ULL
#define THS_EN          0x04
#define THS_PER         0x08
#define THS_FILTER      0x30
#define THS_CDATA0      0xA0
#define THS_DATA0       0xC0

// the trips Linux uses: 90 C passive (OPP steps down, back up below 80 C), 110 C critical
#define TRIP_THROTTLE_MC  90000
#define TRIP_RELEASE_MC   80000
#define TRIP_CRITICAL_MC  110000

static volatile uint8_t *sPll, *sTwi, *sThs;
static thread_call_t sThermalCall;
static int sMode, sCriticalTicks;
static uint32_t sTicks;
static int32_t sPeakB, sPeakL;
static int32_t sCriticalMc = TRIP_CRITICAL_MC;
static int32_t sPassiveMc = TRIP_THROTTLE_MC, sReleaseMc = TRIP_RELEASE_MC;

static inline uint32_t rd(volatile uint8_t *b, uint32_t o) { return *(volatile uint32_t *)(b + o); }
static inline void wr(volatile uint8_t *b, uint32_t o, uint32_t v) { *(volatile uint32_t *)(b + o) = v; }

static volatile uint8_t *
pd_map(uint64_t phys, uint64_t len)
{
	IOMemoryDescriptor *d = IOMemoryDescriptor::withPhysicalAddress(phys, len, kIODirectionInOut);
	IOMemoryMap *m = d != NULL ? d->map(kIOMapInhibitCache) : NULL;

	if (d != NULL)
		d->release();
	return m != NULL ? (volatile uint8_t *)m->getVirtualAddress() : NULL;
}

// wait for the controller's interrupt flag, then check the status it reached
static bool
twi_step(uint32_t want)
{
	for (int i = 0; i < 2000; i++) {
		if (rd(sTwi, TWI_CNTR) & TWI_CNTR_INT)
			return (rd(sTwi, TWI_STAT) & 0xff) == want;
		IODelay(5);
	}
	return false;
}

// hand the next byte to the bus by clearing the interrupt flag (written as 1 on this controller)
static void
twi_go(uint32_t extra)
{
	wr(sTwi, TWI_CNTR, TWI_CNTR_BUS_EN | TWI_CNTR_INT | extra);
}

static void
twi_stop(void)
{
	wr(sTwi, TWI_CNTR, TWI_CNTR_BUS_EN | TWI_CNTR_M_STP | TWI_CNTR_INT);
	for (int i = 0; i < 2000 && (rd(sTwi, TWI_CNTR) & TWI_CNTR_M_STP); i++) {
		IODelay(5);
	}
}

static bool
axp_read(uint8_t reg, uint8_t *val)
{
	bool ok;

	wr(sTwi, TWI_CNTR, TWI_CNTR_BUS_EN | TWI_CNTR_M_STA);
	ok = twi_step(0x08);
	if (ok) {
		wr(sTwi, TWI_DATA, AXP_ADDR << 1);
		twi_go(0);
		ok = twi_step(0x18);
	}
	if (ok) {
		wr(sTwi, TWI_DATA, reg);
		twi_go(0);
		ok = twi_step(0x28);
	}
	if (ok) {
		twi_go(TWI_CNTR_M_STA);
		ok = twi_step(0x10);
	}
	if (ok) {
		wr(sTwi, TWI_DATA, (AXP_ADDR << 1) | 1);
		twi_go(0);
		ok = twi_step(0x40);
	}
	if (ok) {
		// one byte: no acknowledge, the slave stops sending
		twi_go(0);
		ok = twi_step(0x58);
		if (ok)
			*val = (uint8_t)rd(sTwi, TWI_DATA);
	}
	twi_stop();
	return ok;
}

static bool
axp_write(uint8_t reg, uint8_t val)
{
	bool ok;

	wr(sTwi, TWI_CNTR, TWI_CNTR_BUS_EN | TWI_CNTR_M_STA);
	ok = twi_step(0x08);
	if (ok) {
		wr(sTwi, TWI_DATA, AXP_ADDR << 1);
		twi_go(0);
		ok = twi_step(0x18);
	}
	if (ok) {
		wr(sTwi, TWI_DATA, reg);
		twi_go(0);
		ok = twi_step(0x28);
	}
	if (ok) {
		wr(sTwi, TWI_DATA, val);
		twi_go(0);
		ok = twi_step(0x28);
	}
	twi_stop();
	return ok;
}

static uint32_t
pll_khz(uint32_t ctrl, uint32_t clk)
{
	uint32_t n = (ctrl >> 8) & 0xff, p = ((ctrl >> 16) & 0xf) + 1, m0 = ((ctrl >> 20) & 3) + 1, m1 = (ctrl & 0xf) + 1;
	uint32_t divp = 1U << ((clk >> 16) & 3);

	if (((clk >> 24) & 7) != 3)
		return 0;	// not on its PLL
	return PLL_REF_KHZ * n / p / (m0 * m1) / divp;
}

// raw 0 is a sensor that has not sampled yet, not 165 C
#define THS_NONE        (-999000)

static int32_t
ths_mc(int sensor)
{
	uint32_t raw = rd(sThs, THS_DATA0 + 4 * sensor) & 0xfff;

	if (raw == 0)
		return THS_NONE;
	return 165000 - 57 * (int32_t)raw;
}

static void
pd_dvfs_readback(const char *when)
{
	uint8_t v3 = 0, v5 = 0;
	bool ok3 = axp_read(AXP_DCDC3, &v3), ok5 = axp_read(AXP_DCDC5, &v5);

	IOLog("PDSun60iDVFS: %s: A55 %u MHz, A76 %u MHz, DSU %u MHz (L %08x/%08x B %08x/%08x); "
	    "A55 rail %s%u mV, A76 rail %s%u mV; CPUB %d C CPUL %d C GPU %d C\n", when,
	    pll_khz(rd(sPll, PLL_L_CTRL), rd(sPll, PLL_L_CLK)) / 1000, pll_khz(rd(sPll, PLL_B_CTRL), rd(sPll, PLL_B_CLK)) / 1000,
	    pll_khz(rd(sPll, PLL_DSU_CTRL), rd(sPll, PLL_DSU_CLK)) / 1000, rd(sPll, PLL_L_CTRL), rd(sPll, PLL_L_CLK),
	    rd(sPll, PLL_B_CTRL), rd(sPll, PLL_B_CLK), ok5 ? "" : "unread ", ok5 ? 500 + 10 * (v5 & 0x7f) : 0,
	    ok3 ? "" : "unread ", ok3 ? 500 + 10 * (v3 & 0x7f) : 0, ths_mc(0) / 1000, ths_mc(3) / 1000, ths_mc(4) / 1000);
}

// the one write: today's PLL words without the update bit (no relock) and today's rail codes, then read back
static void
pd_dvfs_nochange_write(void)
{
	uint32_t l = rd(sPll, PLL_L_CTRL), b = rd(sPll, PLL_B_CTRL);
	uint8_t v3 = 0, v5 = 0;

	wr(sPll, PLL_L_CTRL, l & ~(1U << 26));
	wr(sPll, PLL_B_CTRL, b & ~(1U << 26));
	if (axp_read(AXP_DCDC3, &v3) && axp_read(AXP_DCDC5, &v5)) {
		bool w3 = axp_write(AXP_DCDC3, v3), w5 = axp_write(AXP_DCDC5, v5);
		uint8_t r3 = 0, r5 = 0;

		axp_read(AXP_DCDC3, &r3);
		axp_read(AXP_DCDC5, &r5);
		IOLog("PDSun60iDVFS: no-change write: PLL L %08x -> %08x, B %08x -> %08x; DCDC3 %02x -> %s %02x, "
		    "DCDC5 %02x -> %s %02x\n", l, rd(sPll, PLL_L_CTRL), b, rd(sPll, PLL_B_CTRL), v3, w3 ? "ok" : "FAILED", r3,
		    v5, w5 ? "ok" : "FAILED", r5);
	} else {
		IOLog("PDSun60iDVFS: no-change write: rails unread, not written\n");
	}
}

struct pd_opp { uint32_t mhz, mv; };

// this chip's operating points (Linux's OPP tables for its bin), lowest first. 1014 MHz is U-Boot's clock and the floor
static const struct pd_opp kA55Opp[] = {
	{ 1014, 800 }, { 1196, 800 }, { 1404, 840 }, { 1508, 880 }, { 1612, 920 }, { 1716, 960 }, { 1794, 1000 },
};
static const struct pd_opp kA76Opp[] = {
	{ 1014, 800 }, { 1404, 800 }, { 1508, 820 }, { 1612, 840 }, { 1716, 870 },
	{ 1794, 920 }, { 1898, 970 }, { 1950, 1000 }, { 2002, 1050 },
};

// one cluster: its PLL, its rail, its OPPs, and the cluster type whose cores relock it (never its own)
struct pd_cluster {
	const char *name;
	uint32_t ctrlReg, clkReg;
	uint8_t rail;
	const struct pd_opp *opp;
	unsigned nOpp;
	cluster_type_t relockOn;
	int cur, top;		// OPP indices, cur -1 = clock not on the table (left alone)
	bool noRaise;		// a failed change stops raises, steps down still happen
	uint32_t downs, ups;
	uint32_t lastDown;	// tick of the last step down
};

static struct pd_cluster sA55 = { "A55", PLL_L_CTRL, PLL_L_CLK, AXP_DCDC5, kA55Opp, sizeof(kA55Opp) / sizeof(kA55Opp[0]),
	CLUSTER_TYPE_P, -1, -1, false, 0, 0, 0 };
static struct pd_cluster sA76 = { "A76", PLL_B_CTRL, PLL_B_CLK, AXP_DCDC3, kA76Opp, sizeof(kA76Opp) / sizeof(kA76Opp[0]),
	CLUSTER_TYPE_E, -1, -1, false, 0, 0, 0 };

static bool
on_cluster(cluster_type_t type)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	int me = cpu_number();

	for (unsigned i = 0; topo != NULL && i < topo->num_cpus; i++) {
		if ((int)topo->cpus[i].cpu_id == me)
			return topo->cpus[i].cluster_type == type;
	}
	return false;
}

// relock with interrupts off on a core of the other cluster, the cluster parked on the 26 MHz crystal meanwhile
// as Linux's sunxi mux notifier does
static bool
pll_relock_here(const struct pd_cluster *c, uint32_t ctrl)
{
	uint32_t clk = rd(sPll, c->clkReg);
	bool locked = false;
	boolean_t intr = ml_set_interrupts_enabled(FALSE);

	if (!on_cluster(c->relockOn)) {
		ml_set_interrupts_enabled(intr);
		return false;
	}
	wr(sPll, c->clkReg, clk & ~(7U << 24));
	IODelay(1);
	wr(sPll, c->ctrlReg, ctrl | (1U << 26));
	for (int i = 0; i < 1000 && !locked; i++) {
		IODelay(10);
		locked = (rd(sPll, c->ctrlReg) & (1U << 28)) != 0 && (rd(sPll, c->ctrlReg) & (1U << 26)) == 0;
	}
	IODelay(20);
	wr(sPll, c->clkReg, clk);
	ml_set_interrupts_enabled(intr);
	return locked;
}

struct relock_req {
	const struct pd_cluster *c;
	uint32_t ctrl;
	volatile int done;
	volatile int locked;
	volatile int cpu;
};

static void
pll_relock_xcall(void *p)
{
	struct relock_req *r = (struct relock_req *)p;

	r->cpu = cpu_number();
	r->locked = pll_relock_here(r->c, r->ctrl);
	__sync_synchronize();
	r->done = 1;
}

// here when this thread already runs on the other cluster, else cross-called to its first core
static bool
pll_relock(const struct pd_cluster *c, uint32_t ctrl, int *ranOn)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	struct relock_req r = { c, ctrl, 0, 0, -1 };
	int target = -1;

	*ranOn = cpu_number();
	if (on_cluster(c->relockOn))
		return pll_relock_here(c, ctrl);
	for (unsigned i = 0; topo != NULL && i < topo->num_cpus && target < 0; i++) {
		if (topo->cpus[i].cluster_type == c->relockOn)
			target = (int)topo->cpus[i].cpu_id;
	}
	if (target < 0 || cpu_xcall(target, pll_relock_xcall, &r) != KERN_SUCCESS) {
		IOLog("PDSun60iDVFS: %s relock: no core outside the cluster to run it on (cpu %d)\n", c->name, target);
		return false;
	}
	for (int i = 0; i < 2000 && !r.done; i++) {
		IODelay(100);
	}
	if (!r.done) {
		// the request lives on this stack: wait for the other core however long it takes
		IOLog("PDSun60iDVFS: %s relock: cpu %d has not answered in 200 ms, waiting\n", c->name, target);
		while (!r.done) {
			IODelay(1000);
		}
	}
	*ranOn = r.cpu;
	return r.locked;
}

static uint8_t
rail_code(uint32_t mv)
{
	return (uint8_t)(0x80 | ((mv - 500) / 10));
}

// rail write, settle, read back. on a mismatch the old code goes back
static bool
rail_set(const struct pd_cluster *c, uint8_t code, uint8_t old)
{
	uint8_t r = 0;

	if (code == old)
		return true;
	if (axp_write(c->rail, code)) {
		IODelay(2000);
		if (axp_read(c->rail, &r) && r == code)
			return true;
	}
	IOLog("PDSun60iDVFS: %s rail write %02x read back %02x: restored %02x\n", c->name, code, r, old);
	axp_write(c->rail, old);
	return false;
}

// move a cluster to OPP idx. up: voltage first, then frequency. down: frequency first, then voltage.
// any mismatch puts the old PLL word and rail code back and stops raises on this cluster
static bool
cluster_set_opp(struct pd_cluster *c, int idx, int32_t t)
{
	uint32_t oldCtrl = rd(sPll, c->ctrlReg), n = c->opp[idx].mhz / 26;
	uint32_t newCtrl = (oldCtrl & ~(0xffU << 8) & ~(1U << 26)) | (n << 8);
	uint8_t old = 0, now = 0, code = rail_code(c->opp[idx].mv);
	bool up = idx > c->cur;
	int ranOn = -1;

	if (!axp_read(c->rail, &old)) {
		IOLog("PDSun60iDVFS: %s to %u MHz: rail unread, nothing changed\n", c->name, c->opp[idx].mhz);
		c->noRaise = true;
		return false;
	}
	// a rail already above the target (a failed step down) is only ever lowered after the clock
	if (up && (code & 0x7f) > (old & 0x7f) && !rail_set(c, code, old)) {
		c->noRaise = true;
		return false;
	}
	if (!pll_relock(c, newCtrl, &ranOn) || ((rd(sPll, c->ctrlReg) >> 8) & 0xff) != n) {
		uint32_t got = rd(sPll, c->ctrlReg);
		bool back = pll_relock(c, oldCtrl & ~(1U << 26), &ranOn);

		if (up)
			axp_write(c->rail, old);
		IOLog("PDSun60iDVFS: %s PLL %08x did not lock at N %u (read %08x): old word %s, rail %02x\n", c->name,
		    newCtrl, n, got, back ? "relocked" : "NOT relocked", old);
		c->noRaise = true;
		return false;
	}
	if (!up && (code & 0x7f) < (old & 0x7f) && !rail_set(c, code, old))
		c->noRaise = true;	// the higher rail stays, safe at the lower clock
	IOLog("PDSun60iDVFS: %d C: %s %u -> %u MHz, rail %u -> %u mV (relock on cpu %d)\n", t / 1000, c->name,
	    c->cur >= 0 ? c->opp[c->cur].mhz : 0, pll_khz(rd(sPll, c->ctrlReg), rd(sPll, c->clkReg)) / 1000,
	    500 + 10 * (old & 0x7f), axp_read(c->rail, &now) ? 500 + 10 * (now & 0x7f) : 0, ranOn);
	c->cur = idx;
	return true;
}

// the table entry the cluster runs at now, -1 when its clock is not on the table
static int
cluster_find_opp(const struct pd_cluster *c)
{
	uint32_t mhz = pll_khz(rd(sPll, c->ctrlReg), rd(sPll, c->clkReg)) / 1000;

	for (unsigned i = 0; i < c->nOpp; i++) {
		if (c->opp[i].mhz == mhz)
			return (int)i;
	}
	return -1;
}

// pdXXmhz=MHZ (with pddvfs=2): straight to that OPP at boot, which then is the top the thermal steps return to
static void
cluster_boot_raise(struct pd_cluster *c, const char *arg)
{
	uint32_t mhz = 0;

	c->cur = c->top = cluster_find_opp(c);
	if (c->cur < 0) {
		IOLog("PDSun60iDVFS: %s clock not on the OPP table, left alone\n", c->name);
		return;
	}
	if (sMode < 2 || !PE_parse_boot_argn(arg, &mhz, sizeof(mhz)) || mhz == 0)
		return;
	for (unsigned i = 0; i < c->nOpp; i++) {
		if (c->opp[i].mhz == mhz && (int)i > c->cur) {
			if (cluster_set_opp(c, (int)i, ths_mc(0)))
				c->top = (int)i;
			return;
		}
	}
	IOLog("PDSun60iDVFS: %s raise to %u MHz refused (not an OPP above today's)\n", c->name, mhz);
}

// thermal emergency: one log line, a sync capped at sync_timeout_seconds, then psci SYSTEM_OFF (what
// Linux uses here). the AXP8191 off control is a fallback only when pdaxpoff=0xRRVV names it
static void
pd_thermal_poweroff(int32_t t)
{
	uint32_t axp = 0;

	IOLog("PDSun60iDVFS: %d C for 10 s: syncing and powering off\n", t / 1000);
	uint64_t t0 = mach_absolute_time(), ns;
	int err = pd_sync_bounded();

	absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
	IOLog("PDSun60iDVFS: sync %s after %llu ms (%d), SYSTEM_OFF\n", err == 0 ? "finished" : "timed out",
	    ns / 1000000ULL, err);
	IODelay(50000);	// let the line drain to the uart before power goes
	PDArmCPU::systemReset(true);
	if (PE_parse_boot_argn("pdaxpoff", &axp, sizeof(axp)) && axp != 0) {
		IOLog("PDSun60iDVFS: SYSTEM_OFF returned, AXP8191 reg 0x%02x <- 0x%02x\n", (axp >> 8) & 0xff, axp & 0xff);
		axp_write((uint8_t)(axp >> 8), (uint8_t)axp);
		IODelay(500000);
	}
	panic("PDSun60iDVFS: CPU at %d C for 10 s and power-off returned", t / 1000);
}

// step_wise as Linux runs it, both clusters bound to the hotter CPU sensor: at 90 C and above each cluster drops one
// OPP per second while the temperature still rises (every 5 s if it holds), below 80 C each climbs one OPP every 3 s
#define STEP_UP_TICKS   3
#define STEP_HOLD_TICKS 5

static int32_t sLastMc = THS_NONE;

static void
cluster_thermal_step(struct pd_cluster *c, int32_t t)
{
	if (c->cur < 0)
		return;
	if (t >= sPassiveMc && (t > sLastMc || sTicks - c->lastDown >= STEP_HOLD_TICKS) && c->cur > 0) {
		c->lastDown = sTicks;
		if (cluster_set_opp(c, c->cur - 1, t))
			c->downs++;
	} else if (t < sReleaseMc && c->cur < c->top && !c->noRaise && sTicks % STEP_UP_TICKS == 0) {
		if (cluster_set_opp(c, c->cur + 1, t))
			c->ups++;
	}
}

static void
pd_thermal_tick(thread_call_param_t, thread_call_param_t)
{
	int32_t t = ths_mc(0) > ths_mc(3) ? ths_mc(0) : ths_mc(3);
	uint64_t deadline;

	// first tick and once a minute after: shows the tick runs and what it sees (-999 = no sample)
	if (ths_mc(0) > sPeakB) sPeakB = ths_mc(0);
	if (ths_mc(3) > sPeakL) sPeakL = ths_mc(3);
	if (sTicks++ % 60 == 0)
		IOLog("PDSun60iDVFS: tick %u: CPUB %d C CPUL %d C GPU %d C DDR %d C NPU %d C, peak CPUB %d CPUL %d, "
		    "A55 %u MHz (down %u up %u), A76 %u MHz (down %u up %u)\n",
		    sTicks - 1, ths_mc(0) / 1000, ths_mc(3) / 1000, ths_mc(4) / 1000, ths_mc(1) / 1000, ths_mc(2) / 1000,
		    sPeakB / 1000, sPeakL / 1000, pll_khz(rd(sPll, PLL_L_CTRL), rd(sPll, PLL_L_CLK)) / 1000, sA55.downs,
		    sA55.ups, pll_khz(rd(sPll, PLL_B_CTRL), rd(sPll, PLL_B_CLK)) / 1000, sA76.downs, sA76.ups);
	if (t == THS_NONE) {
		sCriticalTicks = 0;
		clock_interval_to_deadline(1, kSecondScale, &deadline);
		thread_call_enter_delayed(sThermalCall, deadline);
		return;
	}

	cluster_thermal_step(&sA76, t);
	cluster_thermal_step(&sA55, t);
	sLastMc = t;
	sCriticalTicks = t >= sCriticalMc ? sCriticalTicks + 1 : 0;
	if (sCriticalTicks == 1)
		IOLog("PDSun60iDVFS: %d C: critical, powering off in 10 s unless it cools\n", t / 1000);
	if (sCriticalTicks >= 10) {
		pd_thermal_poweroff(t);
		return;
	}
	clock_interval_to_deadline(1, kSecondScale, &deadline);
	thread_call_enter_delayed(sThermalCall, deadline);
}

// the board carries an allwinner,sun60iw2p1-ths node
static bool
pd_is_a733(void)
{
	IORegistryIterator *it = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
	bool found = false;
	IORegistryEntry *e;

	if (it == NULL)
		return false;
	while (!found && (e = it->getNextObject()) != NULL) {
		OSData *c = OSDynamicCast(OSData, e->getProperty("compatible"));

		found = c != NULL && c->getLength() >= 24 &&
		    strncmp((const char *)c->getBytesNoCopy(), "allwinner,sun60iw2p1-ths", 24) == 0;
	}
	it->release();
	return found;
}

void
PDSun60iDVFS_start(void)
{
	uint32_t mode = 0;
	uint64_t deadline;

	if (!PE_parse_boot_argn("pddvfs", &mode, sizeof(mode)) || mode == 0 || !pd_is_a733())
		return;
	sMode = (int)mode;
	// pdthermtrip=MC lowers the critical trip for a power-off test without heat
	if (PE_parse_boot_argn("pdthermtrip", &mode, sizeof(mode)) && mode != 0) {
		sCriticalMc = (int32_t)mode;
		IOLog("PDSun60iDVFS: critical trip lowered to %d mC for a test\n", sCriticalMc);
	}
	// pdpassivetrip=MC lowers the step-down trip (release 10 C below it) to exercise the OPP steps
	if (PE_parse_boot_argn("pdpassivetrip", &mode, sizeof(mode)) && mode != 0) {
		sPassiveMc = (int32_t)mode;
		sReleaseMc = sPassiveMc - 10000;
		IOLog("PDSun60iDVFS: passive trip lowered to %d mC for a test\n", sPassiveMc);
	}
	sPll = pd_map(PLL_BASE, 0x4000);
	sTwi = pd_map(TWI_BASE, 0x1000);
	sThs = pd_map(THS_BASE, 0x1000);
	volatile uint8_t *ccu = pd_map(CCU_BASE + (CCU_THS0_BGR & ~0xfffU), 0x1000);

	if (sPll == NULL || sTwi == NULL || sThs == NULL || ccu == NULL) {
		IOLog("PDSun60iDVFS: register mapping failed\n");
		return;
	}
	// the sensors as Linux runs them, when firmware left them off. calibration: this board's fused values
	if ((rd(sThs, THS_EN) & 0x1f) != 0x1f) {
		wr(ccu, CCU_THS0_BGR & 0xfff, rd(ccu, CCU_THS0_BGR & 0xfff) | (1U << 16) | 1U);
		IODelay(10);
		if (rd(sThs, THS_CDATA0) == 0) {
			wr(sThs, THS_CDATA0, 0x07db07d5);
			wr(sThs, THS_CDATA0 + 4, 0x07df07da);
			wr(sThs, THS_CDATA0 + 8, 0x000007d2);
		}
		wr(sThs, THS_PER, 0x0001c000);
		wr(sThs, THS_FILTER, 0x5);
		wr(sThs, THS_EN, 0x1f);
	}
	// wait for the first sample of both CPU sensors, up to 1 s
	for (int i = 0; i < 100 && (ths_mc(0) == THS_NONE || ths_mc(3) == THS_NONE); i++) {
		IODelay(10000);
	}
	pd_dvfs_readback("at start");
	if (sMode >= 2) {
		pd_dvfs_nochange_write();
		pd_dvfs_readback("after write");
	}
	cluster_boot_raise(&sA76, "pda76mhz");
	cluster_boot_raise(&sA55, "pda55mhz");
	if (sMode >= 2)
		pd_dvfs_readback("after raise");
	sThermalCall = thread_call_allocate(pd_thermal_tick, NULL);
	if (sThermalCall != NULL) {
		clock_interval_to_deadline(1, kSecondScale, &deadline);
		thread_call_enter_delayed(sThermalCall, deadline);
	}
}
