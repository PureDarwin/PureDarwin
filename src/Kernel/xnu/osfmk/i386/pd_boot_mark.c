/*
 * Early boot progress bars on the framebuffer.
 */
#include <stdint.h>
#include <pexpert/boot.h>
#include <i386/pmap.h>
#include <i386/postcode.h>
#include <i386/proc_reg.h>

uint64_t pd_boot_mark_base;             /* framebuffer physical base */
uint32_t pd_boot_mark_rowbytes;
uint32_t pd_boot_mark_width;
uint32_t pd_boot_mark_height;
int pd_boot_mark_physmap;
// set once the boot cpu is through i386_init: the other cores redraw the same bands and must not each wait
int pd_boot_mark_nohold;
uint64_t pd_boot_mark_boot_cr3;          /* boot page tables: framebuffer is 1:1 */
uint64_t pd_boot_mark_physmap_cr3;       /* IdlePML4: reach it through the physmap */

/* See PD_BAND_SPIN: hold each band long enough to be read off a screen. */
static void
pd_boot_mark_hold(void)
{
	volatile uint32_t spin = PD_BAND_SPIN;

	if (pd_boot_mark_nohold) {
		return;
	}

	while (spin-- != 0) {
		__asm__ volatile ("rep; nop");
	}
}

void pd_boot_mark_init(boot_args *args);
void pd_boot_mark(uint8_t code);
void pd_boot_mark_direct(uint32_t band, boot_args *args);
void pd_boot_mark_band(uint32_t band);
uintptr_t pd_boot_mark_fb_va(void);

void
pd_boot_mark_direct(uint32_t band, boot_args *args)
{
	uint32_t y0, x, y, colour;
	volatile uint32_t *row;

	if (args == NULL || args->Video.v_baseAddr == 0) {
		return;
	}
	// a graphical boot (no -v) keeps the logo screen clean
	if (args->Video.v_display == GRAPHICS_MODE) {
		return;
	}
	if (args->Video.v_depth != 32 || args->Video.v_rowBytes == 0) {
		return;
	}
	y0 = band * 16;
	if (y0 + 16 > args->Video.v_height) {
		return;
	}
	colour = (uint32_t)PD_BAND_COLOUR(band);

	for (y = y0; y < y0 + 16; y++) {
		row = (volatile uint32_t *)(uintptr_t)(args->Video.v_baseAddr +
		    (uint64_t)y * args->Video.v_rowBytes);
		for (x = 0; x < args->Video.v_width; x++) {
			row[x] = colour;
		}
	}
	pd_boot_mark_hold();
}

void
pd_boot_mark_init(boot_args *args)
{
	if (args == NULL || args->Video.v_baseAddr == 0) {
		return;
	}
	if (args->Video.v_display == GRAPHICS_MODE) {
		return;
	}
	/*
	 * Same bounds the assembly bands insist on, and for the same reason: a
	 * band is written straight to physical memory, so anything that does not
	 * look like a framebuffer is not written to at all.
	 */
	if (args->Video.v_depth != 32 || args->Video.v_rowBytes == 0) {
		return;
	}
	if (args->Video.v_baseAddr < PD_BAND_MIN_BASE) {
		return;
	}
	// above 4GB PD_MAP_HIGH_FB maps it, below 512GB in BootPML4[0], higher through BootFBPDPT

	if (args->Video.v_baseAddr >= (256ULL << 40)) {
		return;
	}
	if (args->Video.v_width == 0 || args->Video.v_width > PD_BAND_MAX_DIM ||
	    args->Video.v_height == 0 || args->Video.v_height > PD_BAND_MAX_DIM) {
		return;
	}
	if (args->Video.v_rowBytes > PD_BAND_MAX_ROWBYTES ||
	    args->Video.v_rowBytes < args->Video.v_width * 4) {
		return;
	}
	pd_boot_mark_boot_cr3 = get_cr3_raw();
	// the loader tags a physical framebuffer with bit 0
	pd_boot_mark_base     = args->Video.v_baseAddr & ~3ULL;
	pd_boot_mark_rowbytes = args->Video.v_rowBytes;
	pd_boot_mark_width    = args->Video.v_width;
	pd_boot_mark_height   = args->Video.v_height;
}

uintptr_t
pd_boot_mark_fb_va(void)
{
	extern uint64_t physmap_base, physmap_max;

	if (pd_boot_mark_base == 0) {
		return 0;
	}
	/*
	 * Which regime this CPU is in is a *per-CPU* fact, not a global one:
	 * vstart() runs on every processor, and while the boot CPU has already
	 * switched to IdlePML4, each AP still enters it on the boot page tables.
	 */
	uint64_t cr3 = get_cr3_raw();

	if (pd_boot_mark_physmap && cr3 == pd_boot_mark_physmap_cr3) {
		if (pd_boot_mark_base < (physmap_max - physmap_base)) {
			return (uintptr_t)PHYSMAP_PTOV(pd_boot_mark_base);
		}
		// above the physmap, Idle_PTs_init carried BootFBPD into the kernel PDPT
		if (pd_boot_mark_base >= (4ULL << 30) && ((pd_boot_mark_base >> 30) & 0x1ff) >= 4) {
			return (uintptr_t)(KERNEL_BASE + (pd_boot_mark_base & ((512ULL << 30) - 1)));
		}
		return 0;
	}
	if (cr3 == pd_boot_mark_boot_cr3) {
		/* BootPML4[0] identity-maps the low 4GB. */
		// above that, PD_MAP_HIGH_FB mapped it 1:1 from start.s
		return (uintptr_t)pd_boot_mark_base;
	}
	/* Some other page-table regime - refuse rather than guess an address.
	 * A wrong guess here faults, and a fault here reboots the machine. */
	return 0;
}

void
pd_boot_mark(uint8_t code)
{
	pd_boot_mark_band((uint32_t)PD_BAND_FOR_CODE(code));
}

void
pd_boot_mark_band(uint32_t band)
{
	uint32_t y0, x, y, colour;
	uintptr_t base;

	if (band == PD_BAND_NONE || pd_boot_mark_base == 0) {
		return;
	}
	y0 = band * 16;
	if (y0 + 16 > pd_boot_mark_height) {
		return;
	}
	colour = (uint32_t)PD_BAND_COLOUR(band);

	base = pd_boot_mark_fb_va();
	if (base == 0) {
		return;
	}


	for (y = y0; y < y0 + 16; y++) {
		volatile uint32_t *row =
		    (volatile uint32_t *)(base + (uintptr_t)y * pd_boot_mark_rowbytes);
		for (x = 0; x < pd_boot_mark_width; x++) {
			row[x] = colour;
		}
	}
	pd_boot_mark_hold();
}

// one more block along a band per call, 32 to a row, so a count of blocks names the last step reached
void
pd_boot_mark_tick(uint32_t band)
{
	static uint32_t ticks[64];
	uint32_t n, x0, x1, x, y;
	uintptr_t base;

	if (band >= 64 || pd_boot_mark_base == 0 || band * 16 + 16 > pd_boot_mark_height) {
		return;
	}
	base = pd_boot_mark_fb_va();
	if (base == 0) {
		return;
	}
	n = ticks[band]++ % 32;
	x0 = n * pd_boot_mark_width / 32;
	x1 = (n + 1) * pd_boot_mark_width / 32 - 2;
	for (y = band * 16 + 2; y < band * 16 + 14; y++) {
		volatile uint32_t *row = (volatile uint32_t *)(base + (uintptr_t)y * pd_boot_mark_rowbytes);
		for (x = x0; x < x1; x++) {
			row[x] = (n & 1) ? 0xffffffffu : 0xff20c0ffu;
		}
	}
}
