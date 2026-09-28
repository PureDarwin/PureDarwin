/*
 * Copyright (c) 2000-2017, 2024 Apple Inc. All rights reserved.
 *
 *    riscv platform expert initialization.
 */
#include <sys/types.h>
#include <sys/kdebug.h>
#include <mach/vm_param.h>
#include <pexpert/protos.h>
#include <pexpert/pexpert.h>
#include <pexpert/boot.h>
#include <pexpert/device_tree.h>
#include <pexpert/pe_images.h>
#include <kern/sched_prim.h>
#include <kern/socd_client.h>
#include <machine/atomic.h>
#include <machine/machine_routines.h>
#include <console/serial_protos.h>
#include <riscv/caches_internal.h>
#include <kern/debug.h>
#include <libkern/section_keywords.h>
#include <os/overflow.h>

#include <pexpert/riscv/board_config.h>

/* extern references */
extern void     pe_identify_machine(boot_args *bootArgs);

/* static references */
static void     pe_prepare_images(void);

/* private globals */
SECURITY_READ_ONLY_LATE(PE_state_t) PE_state;
TUNABLE_DT(uint32_t, PE_esdm_fuses, "/chosen", "esdm-fuses", "", 0, TUNABLE_DT_NONE);
TUNABLE_DT(uint32_t, PE_vmm_present, "/defaults", "vmm-present", "", 0, TUNABLE_DT_NONE);

#define FW_VERS_LEN 128

// the loader's /chosen firmware-version, the counterpart of iboot's version string
char iBoot_version[FW_VERS_LEN];

// Set once when the BSP starts. Lives in __DATA_CONST, which is RW early in boot
// and becomes RO once the kernel is locked down.
SECURITY_READ_ONLY_LATE(volatile uint32_t) debug_enabled = FALSE;

// Page protection security policy in use, mostly for debugging.
SECURITY_READ_ONLY_LATE(ml_page_protection_t) page_protection_type;

uint8_t         gPlatformECID[8];
uint32_t        gPlatformMemoryID;
static boolean_t vc_progress_initialized = FALSE;
uint64_t    last_hwaccess_thread = 0;
uint8_t last_hwaccess_type = 0;
uint8_t last_hwaccess_size = 0;
uint64_t last_hwaccess_paddr = 0;
char     gTargetTypeBuffer[16];
char     gUniqueDeviceTargetTypeBuffer[16];   /* Holds "sub-product-type" from product entry */
char     gUniqueDeviceModelTypeBuffer[32];    /* Holds "unique-model" from product entry */

/* Clock Frequency Info */
clock_frequency_info_t gPEClockFrequencyInfo;

vm_offset_t gPanicBase = 0;
unsigned int gPanicSize;
struct embedded_panic_header *panic_info = NULL;

/* Maximum size of panic log excluding headers, in bytes */
static unsigned int panic_text_len;

/* Whether a console is standing by for panic logging */
static boolean_t panic_console_available = FALSE;

/* socd trace ram attributes */
static SECURITY_READ_ONLY_LATE(vm_offset_t) socd_trace_ram_base = 0;
static SECURITY_READ_ONLY_LATE(vm_size_t) socd_trace_ram_size = 0;

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

void PE_slide_devicetree(vm_offset_t);

static void
pe_init_fill_buffer_from_property(DTEntry entry, const char *property_name, char *buffer, size_t buffer_size)
{
	void const *prop;
	unsigned int size;

	buffer[0] = '\0';  // Initialize buffer to empty string

	if (kSuccess == SecureDTGetProperty(entry, property_name, &prop, &size)) {
		if (size > buffer_size) {
			size = buffer_size;
		}
		if (size > 0) {
			bcopy(prop, buffer, size);
			buffer[size - 1] = '\0';
		}
	}
}

static void
check_for_panic_log(void)
{
	DTEntry entry, chosen;
	unsigned int size;
	uintptr_t const *reg_prop;
	uint32_t const *panic_region_length;

	// a pram node's reg gives the region that survives a warm reset
	// /chosen embedded-panic-log-size caps how much of it the log uses
	if (kSuccess != SecureDTLookupEntry(0, "pram", &entry)) {
		return;
	}

	if (kSuccess != SecureDTGetProperty(entry, "reg", (void const **)&reg_prop, &size)) {
		return;
	}

	if (kSuccess != SecureDTLookupEntry(0, "/chosen", &chosen)) {
		return;
	}

	if (kSuccess != SecureDTGetProperty(chosen, "embedded-panic-log-size", (void const **) &panic_region_length, &size)) {
		return;
	}

	gPanicBase = ml_io_map_wcomb(reg_prop[0], panic_region_length[0]);

	/* Deduct the size of the panic header from the panic region size */
	panic_text_len = panic_region_length[0] - sizeof(struct embedded_panic_header);
	gPanicSize = panic_region_length[0];

	panic_info = (struct embedded_panic_header *)gPanicBase;

	/* Check if a shared memory console is running in the panic buffer */
	if (panic_info->eph_magic == 'SHMC') {
		panic_console_available = TRUE;
		return;
	}

	/* Check if there's a boot profile in the panic buffer */
	if (panic_info->eph_magic == 'BTRC') {
		return;
	}

	// Check for a panic (FUNK) left in VRAM from the last boot.
	if (panic_info->eph_magic == EMBEDDED_PANIC_MAGIC) {
		printf("the loader didn't extract the panic log from the previous session\n");
	}

	/* Clear panic region */
	bzero((void *)gPanicBase, gPanicSize);
	clean_dcache(gPanicBase, gPanicSize, FALSE);
}

int
PE_initialize_console(PE_Video * info, int op)
{
	static int last_console = -1;

	if (info && (info != &PE_state.video)) {
		info->v_scale = PE_state.video.v_scale;
	}

	switch (op) {
	case kPEDisableScreen:
		initialize_screen(info, op);
		last_console = switch_to_serial_console();
		kprintf("kPEDisableScreen %d\n", last_console);
		break;

	case kPEEnableScreen:
		initialize_screen(info, op);
		if (info) {
			PE_state.video = *info;
		}
		kprintf("kPEEnableScreen %d\n", last_console);
		if (last_console != -1) {
			switch_to_old_console(last_console);
		}
		break;

	case kPEReleaseScreen:
		// no progress indicator on boot, but show it afterwards.
		if (!vc_progress_initialized) {
			default_progress.dx = 0;
			default_progress.dy = 0;
			vc_progress_initialize(&default_progress,
			    default_progress_data1x,
			    default_progress_data2x,
			    default_progress_data3x,
			    (unsigned char *) appleClut8);
			vc_progress_initialized = TRUE;
		}
		initialize_screen(info, op);
		break;

	default:
		initialize_screen(info, op);
		break;
	}

	return 0;
}

void
PE_init_iokit(void)
{
	DTEntry         entry;
	unsigned int    size, scale;
	unsigned long   display_size;
	void const * const *map;
	unsigned int    show_progress;
	int             *delta, image_size, flip;
	uint32_t        start_time_value = 0;
	uint32_t        debug_wait_start_value = 0;
	uint32_t        load_kernel_start_value = 0;
	uint32_t        populate_registry_time_value = 0;

	PE_init_printf(TRUE);

	printf("firmware version: %s\n", iBoot_version);

	if (kSuccess == SecureDTLookupEntry(0, "/chosen/memory-map", &entry)) {
		boot_progress_element const *bootPict;

		if (kSuccess == SecureDTGetProperty(entry, "BootCLUT", (void const **) &map, &size)) {
			bcopy(map[0], appleClut8, sizeof(appleClut8));
		}

		if (kSuccess == SecureDTGetProperty(entry, "Pict-FailedBoot", (void const **) &map, &size)) {
			bootPict = (boot_progress_element const *) map[0];
			default_noroot.width = bootPict->width;
			default_noroot.height = bootPict->height;
			default_noroot.dx = 0;
			default_noroot.dy = bootPict->yOffset;
			default_noroot_data = &bootPict->data[0];
		}
	}

	pe_prepare_images();

	scale = PE_state.video.v_scale;
	flip = 1;

	show_progress = FALSE;
	PE_parse_boot_argn("-progress", &show_progress, sizeof(show_progress));
	if (show_progress && PE_state.video.v_display) {
		/* Rotation: 0:normal, 1:right 90, 2:left 180, 3:left 90 */
		switch (PE_state.video.v_rotate) {
		case 2:
			flip = -1;
			OS_FALLTHROUGH;
		case 0:
			display_size = PE_state.video.v_height;
			image_size = default_progress.height;
			delta = &default_progress.dy;
			break;
		case 1:
			flip = -1;
			OS_FALLTHROUGH;
		case 3:
		default:
			display_size = PE_state.video.v_width;
			image_size = default_progress.width;
			delta = &default_progress.dx;
		}
		assert(*delta >= 0);
		while (((unsigned)(*delta + image_size)) >= (display_size / 2)) {
			*delta -= 50 * scale;
			assert(*delta >= 0);
		}
		*delta *= flip;

		/* Check for DT-defined progress y delta */
		PE_get_default("progress-dy", &default_progress.dy, sizeof(default_progress.dy));

		vc_progress_initialize(&default_progress,
		    default_progress_data1x,
		    default_progress_data2x,
		    default_progress_data3x,
		    (unsigned char *) appleClut8);
		vc_progress_initialized = TRUE;
	}

	if (kdebug_enable && kdebug_debugid_enabled(IOKDBG_CODE(DBG_BOOTER, 0))) {
		/* Trace iBoot-provided timing information. */
		if (kSuccess == SecureDTLookupEntry(0, "/chosen/iBoot", &entry)) {
			uint32_t const * value_ptr;

			if (kSuccess == SecureDTGetProperty(entry, "start-time", (void const **)&value_ptr, &size)) {
				if (size == sizeof(start_time_value)) {
					start_time_value = *value_ptr;
				}
			}

			if (kSuccess == SecureDTGetProperty(entry, "debug-wait-start", (void const **)&value_ptr, &size)) {
				if (size == sizeof(debug_wait_start_value)) {
					debug_wait_start_value = *value_ptr;
				}
			}

			if (kSuccess == SecureDTGetProperty(entry, "load-kernel-start", (void const **)&value_ptr, &size)) {
				if (size == sizeof(load_kernel_start_value)) {
					load_kernel_start_value = *value_ptr;
				}
			}

			if (kSuccess == SecureDTGetProperty(entry, "populate-registry-time", (void const **)&value_ptr, &size)) {
				if (size == sizeof(populate_registry_time_value)) {
					populate_registry_time_value = *value_ptr;
				}
			}
		}

		KDBG_RELEASE(IOKDBG_CODE(DBG_BOOTER, 0), start_time_value, debug_wait_start_value, load_kernel_start_value, populate_registry_time_value);
	}

	InitIOKit(PE_state.deviceTreeHead);
	ConfigureIOKit();
}

void
PE_lockdown_iokit(void)
{
	// as on arm64, general iokit matching waits for machine_lockdown()
	zalloc_iokit_lockdown();
	StartIOKitMatching();
}

void
PE_slide_devicetree(vm_offset_t slide)
{
	assert(PE_state.initialized);
	PE_state.deviceTreeHead = (void *)((uintptr_t)PE_state.deviceTreeHead + slide);
	SecureDTInit(PE_state.deviceTreeHead, PE_state.deviceTreeSize);
}

void
PE_init_platform(boolean_t vm_initialized, void *args)
{
	DTEntry         entry;
	unsigned int    size;
	void * const    *prop;
	boot_args      *boot_args_ptr = (boot_args *) args;

	if (PE_state.initialized == FALSE) {
		page_protection_type = ml_page_protection_type();
		PE_state.initialized = TRUE;
		PE_state.bootArgs = boot_args_ptr;
		PE_state.deviceTreeHead = boot_args_ptr->deviceTreeP;
		PE_state.deviceTreeSize = boot_args_ptr->deviceTreeLength;
		PE_state.video.v_baseAddr = boot_args_ptr->Video.v_baseAddr;
		PE_state.video.v_rowBytes = boot_args_ptr->Video.v_rowBytes;
		PE_state.video.v_width = boot_args_ptr->Video.v_width;
		PE_state.video.v_height = boot_args_ptr->Video.v_height;
		PE_state.video.v_depth = (boot_args_ptr->Video.v_depth >> kBootVideoDepthDepthShift) & kBootVideoDepthMask;
		PE_state.video.v_rotate = (
			((boot_args_ptr->Video.v_depth >> kBootVideoDepthRotateShift) & kBootVideoDepthMask) +    // rotation
			((boot_args_ptr->Video.v_depth >> kBootVideoDepthBootRotateShift)  & kBootVideoDepthMask) // add extra boot rotation
			) % 4;
		PE_state.video.v_scale = ((boot_args_ptr->Video.v_depth >> kBootVideoDepthScaleShift) & kBootVideoDepthMask) + 1;
		PE_state.video.v_display = boot_args_ptr->Video.v_display;
		strlcpy(PE_state.video.v_pixelFormat, "BBBBBBBBGGGGGGGGRRRRRRRR", sizeof(PE_state.video.v_pixelFormat));
	}
	if (!vm_initialized) {
		// Set up the Device Tree routines so the console and the right I/O space can be found.
		SecureDTInit(PE_state.deviceTreeHead, PE_state.deviceTreeSize);
		pe_identify_machine(boot_args_ptr);
	} else {
		pe_riscv_init_interrupts(args);
	}

	if (!vm_initialized) {
		if (kSuccess == (SecureDTFindEntry("name", "device-tree", &entry))) {
			pe_init_fill_buffer_from_property(entry, "target-type", gTargetTypeBuffer, sizeof(gTargetTypeBuffer));
			// a converted fdt has no target-type, its first compatible string names the board
			if (gTargetTypeBuffer[0] == '\0') {
				pe_init_fill_buffer_from_property(entry, "compatible", gTargetTypeBuffer, sizeof(gTargetTypeBuffer));
			}
		}
		if (kSuccess == (SecureDTFindEntry("name", "product", &entry))) {
			pe_init_fill_buffer_from_property(entry, "unique-model", gUniqueDeviceTargetTypeBuffer, sizeof(gUniqueDeviceTargetTypeBuffer));
			pe_init_fill_buffer_from_property(entry, "sub-product-type", gUniqueDeviceModelTypeBuffer, sizeof(gUniqueDeviceModelTypeBuffer));
		}
		if (kSuccess == SecureDTLookupEntry(NULL, "/chosen", &entry)) {
			if (kSuccess == SecureDTGetProperty(entry, "debug-enabled",
			    (void const **) &prop, &size)) {
				// Intentionally writes a const variable. It gets locked down later by a trusted
				// monitor or the page table mappings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
				boolean_t *modify_debug_enabled = (boolean_t *) &debug_enabled;
				if (size > sizeof(uint32_t)) {
					size = sizeof(uint32_t);
				}
				bcopy(prop, modify_debug_enabled, size);
#pragma clang diagnostic pop
			}
			if (kSuccess == SecureDTGetProperty(entry, "firmware-version", (void const **) &prop, &size)) {
				if (size > sizeof(iBoot_version)) {
					size = sizeof(iBoot_version);
				}
				if (size > 0) {
					bcopy(prop, iBoot_version, size);
					iBoot_version[size - 1] = '\0';
				}
			}
			if (kSuccess == SecureDTGetProperty(entry, "unique-chip-id",
			    (void const **) &prop, &size)) {
				if (size > sizeof(gPlatformECID)) {
					size = sizeof(gPlatformECID);
				}
				bcopy(prop, gPlatformECID, size);
			}
			if (kSuccess == SecureDTGetProperty(entry, "dram-vendor-id",
			    (void const **) &prop, &size)) {
				if (size > sizeof(gPlatformMemoryID)) {
					size = sizeof(gPlatformMemoryID);
				}
				bcopy(prop, &gPlatformMemoryID, size);
			}
		}
		pe_init_debug();
	}
}

void
PE_create_console(void)
{
	// Check the head of VRAM for a panic log from the last panic, before VRAM is trashed.
	check_for_panic_log();

	if (PE_state.video.v_display) {
		// Boot_Video uses 1 for a splash/graphics boot and 2 for a verbose text boot.
		// Only the former suppresses character painting when the screen is acquired.
		PE_initialize_console(&PE_state.video,
		    PE_state.video.v_display == 1 ? kPEGraphicsMode : kPETextMode);
	} else {
		PE_initialize_console(&PE_state.video, kPETextMode);
	}
}

int
PE_current_console(PE_Video * info)
{
	*info = PE_state.video;
	return 0;
}

void
PE_display_icon(__unused unsigned int flags, __unused const char *name)
{
	if (default_noroot_data) {
		vc_display_icon(&default_noroot, default_noroot_data);
	}
}

extern          boolean_t
PE_get_hotkey(__unused unsigned char key)
{
	return FALSE;
}

static timebase_callback_func gTimebaseCallback;

void
PE_register_timebase_callback(timebase_callback_func callback)
{
	gTimebaseCallback = callback;

	PE_call_timebase_callback();
}

void
PE_call_timebase_callback(void)
{
	struct timebase_freq_t timebase_freq;

	timebase_freq.timebase_num = gPEClockFrequencyInfo.timebase_frequency_hz;
	timebase_freq.timebase_den = 1;

	if (gTimebaseCallback) {
		gTimebaseCallback(&timebase_freq);
	}
}

// The default PE_poll_input handler.
int
PE_stub_poll_input(__unused unsigned int options, char *c)
{
	*c = (char)uart_getc();
	return 0; /* 0 for success, 1 for unsupported */
}

// 1 when the loader marked the boot debuggable through /chosen debug-enabled
// debug_flags gets the debug= boot-arg so kexts don't parse it themselves
uint32_t
PE_i_can_has_debugger(uint32_t *debug_flags)
{
	if (debug_flags) {
#if DEVELOPMENT || DEBUG
		assert(startup_phase >= STARTUP_SUB_TUNABLES);
#endif
		if (debug_enabled) {
			*debug_flags = debug_boot_arg;
		} else {
			*debug_flags = 0;
		}
	}
	return debug_enabled;
}

// TRUE if the device is configured with panic debugging enabled.
boolean_t
PE_panic_debugging_enabled()
{
	return panicDebugging;
}

void
PE_update_panic_crc(unsigned char *buf, unsigned int *size)
{
	if (!panic_info || !size) {
		return;
	}

	if (!buf) {
		*size = panic_text_len;
		return;
	}

	if (*size == 0) {
		return;
	}

	*size = *size > panic_text_len ? panic_text_len : *size;
	if (panic_info->eph_magic != EMBEDDED_PANIC_MAGIC) {
		printf("Error!! Current Magic 0x%X, expected value 0x%x\n", panic_info->eph_magic, EMBEDDED_PANIC_MAGIC);
	}

	/* CRC everything after the CRC itself - starting with the panic header version */
	panic_info->eph_crc = crc32(0L, &panic_info->eph_version, (panic_text_len +
	    sizeof(struct embedded_panic_header) - offsetof(struct embedded_panic_header, eph_version)));
}

uint32_t
PE_get_offset_into_panic_region(char *location)
{
	assert(gPanicBase != 0);
	assert(location >= (char *) gPanicBase);
	assert((unsigned int)(location - gPanicBase) < gPanicSize);

	return (uint32_t)(uintptr_t)(location - gPanicBase);
}

void
PE_init_panicheader()
{
	if (!panic_info) {
		return;
	}

	bzero(panic_info, sizeof(struct embedded_panic_header));

	// The panic log starts right after the header. Debugger sync and others may log here
	// before we become the exclusive panicking CPU and initialize the header.
	panic_info->eph_panic_log_offset = debug_buf_base ? PE_get_offset_into_panic_region(debug_buf_base) : 0;

	panic_info->eph_magic = EMBEDDED_PANIC_MAGIC;
	panic_info->eph_version = EMBEDDED_PANIC_HEADER_CURRENT_VERSION;

	return;
}

// Keeps the panic header consistent on nested panics.
// It does not detect or correct corruption of the panic region.
void
PE_update_panicheader_nestedpanic()
{
	// bail if the panic header pointer is bogus (e.g. someone stomped on it).
	if (!panic_info) {
		/* if this happens in development then blow up bigly */
		assert(panic_info);
		return;
	}

	// Re-init the panic header if the log offset is unset. Only possible if someone zeroed
	// the header, by now something should have been appended to the log.
	if (panic_info->eph_panic_log_offset == 0) {
		PE_init_panicheader();
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_NESTED_PANIC;
		return;
	}

	panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_NESTED_PANIC;

	// If the panic log length is unset, close it at the current debug_buf_ptr.
	if (panic_info->eph_panic_log_len == 0) {
		panic_info->eph_panic_log_len = PE_get_offset_into_panic_region(debug_buf_ptr);

		/* indicative of corruption in the panic region, consumer beware */
		if ((panic_info->eph_other_log_offset == 0) &&
		    (panic_info->eph_other_log_len == 0)) {
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_INCOHERENT_PANICLOG;
		}
	}

	/* likely indicative of corruption in the panic region, consumer beware */
	if (((panic_info->eph_stackshot_offset == 0) && (panic_info->eph_stackshot_len == 0)) || ((panic_info->eph_stackshot_offset != 0) && (panic_info->eph_stackshot_len != 0))) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_INCOHERENT_PANICLOG;
	}

	// If the other log isn't set up yet, start it at the current debug_buf_ptr.
	if (panic_info->eph_other_log_offset == 0) {
		panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);

		/* indicative of corruption in the panic region, consumer beware */
		if (panic_info->eph_other_log_len == 0) {
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_INCOHERENT_PANICLOG;
		}
	}

	return;
}

boolean_t
PE_reboot_on_panic(void)
{
	uint32_t debug_flags;

	if (PE_i_can_has_debugger(&debug_flags)
	    && (debug_flags & DB_NMI)) {
		/* kernel debugging is active */
		return FALSE;
	} else {
		return TRUE;
	}
}

void
PE_sync_panic_buffers(void)
{
	// push the write-combined panic region out to memory before the reset
	if (gPanicBase) {
		clean_dcache(gPanicBase, gPanicSize, FALSE);
	}
}

static void
pe_prepare_images(void)
{
	if ((1 & PE_state.video.v_rotate) != 0) {
		// Only square square images with radial symmetry are supported
		// No need to actually rotate the data

		// Swap the dx and dy offsets
		uint32_t tmp = default_progress.dx;
		default_progress.dx = default_progress.dy;
		default_progress.dy = tmp;
	}
}

void
PE_mark_hwaccess(uint64_t thread)
{
	last_hwaccess_thread = thread;
	__asm__ volatile ("fence rw, rw" ::: "memory");
}

void
PE_mark_hwaccess_data(uint8_t type, uint8_t size, uint64_t paddr)
{
	last_hwaccess_type = type;
	last_hwaccess_size = size;
	last_hwaccess_paddr = paddr;
	__asm__ volatile ("fence rw, rw" ::: "memory");
}

__startup_func
vm_size_t
PE_init_socd_client(void)
{
	DTEntry entry;
	uintptr_t const *reg_prop;
	unsigned int size;

	// Only cold boot can initialize, after lockdown the page tables are read-only.
	if (startup_phase >= STARTUP_SUB_EARLY_BOOT) {
		return 0;
	}

	if (kSuccess != SecureDTLookupEntry(0, "socd-trace-ram", &entry)) {
		return 0;
	}

	if (kSuccess != SecureDTGetProperty(entry, "reg", (void const **)&reg_prop, &size)) {
		return 0;
	}

	if (size < 2 * sizeof(uintptr_t)) {
		return 0;
	}

	socd_trace_ram_size = (vm_size_t)reg_prop[1];
	if (socd_trace_ram_size == 0) {
		return 0;
	}

	socd_trace_ram_base = ml_io_map(reg_prop[0], socd_trace_ram_size);

	return socd_trace_ram_size;
}

// see comments in PE_write_socd_client_buffer.
void
PE_read_socd_client_buffer(vm_offset_t offset, void *out_buff, vm_size_t size)
{
	volatile uint32_t *client_buff = (volatile uint32_t *)(socd_trace_ram_base + offset);
	vm_size_t len = size / sizeof(client_buff[0]);

	assert(out_buff);
	assert3u((offset + size), <=, socd_trace_ram_size);

	/* Perform 4 byte aligned accesses */
	if ((offset % 4 != 0) || (size % 4 != 0)) {
		panic("unaligned read of 0x%lu bytes from socd trace ram address 0x%lu", size, offset);
	}

	for (vm_size_t i = 0; i < len; i++) {
		((uint32_t *)out_buff)[i] = client_buff[i];
	}
}

// Never trusts values read back from the socd client buffer, and only uses 4-byte stores
// since some platforms back that buffer with SRAM that needs 4-byte writes.
void
PE_write_socd_client_buffer(vm_offset_t offset, const void *in_buff, vm_size_t size)
{
	volatile uint32_t *client_buff = (volatile uint32_t *)(socd_trace_ram_base + offset);
	vm_size_t len = size / sizeof(client_buff[0]);

	assert(in_buff);
	assert3u((offset + size), <=, socd_trace_ram_size);

	/* Perform 4 byte aligned accesses */
	if ((offset % 4 != 0) || (size % 4 != 0)) {
		panic("unaligned write of 0x%lu bytes to socd trace ram address 0x%lu", size, offset);
	}

	for (vm_size_t i = 0; i < len; i++) {
		client_buff[i] = ((const uint32_t *)in_buff)[i];
	}
}

boolean_t
PE_device_is_simulated(void)
{
	return strnstr(gTargetTypeBuffer, "sim", sizeof(gTargetTypeBuffer)) != NULL;
}

// no hardware trace or cross trigger hooks on riscv yet
void
PE_panic_hook(const char *str __unused)
{
}

void
PE_init_cpu(void)
{
}

void
PE_singlestep_hook(void)
{
}
