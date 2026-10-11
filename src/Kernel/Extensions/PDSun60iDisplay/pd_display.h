#ifndef _PD_DISPLAY_H
#define _PD_DISPLAY_H

// the scanout interface of PDSun60iDisplay, reached through callPlatformFunction so that its users
// (PDPowerVR's flip) need no link-time dependency on it

#include <stdint.h>

#define PD_DISPLAY_CLASS		"PDSun60iDisplay"

// p1: struct pd_display_info *
#define PD_DISPLAY_FN_INFO		"PDDisplayInfo"
// p1: physical address, p2: pitch in bytes, p3: struct pd_display_flip * (out), returns once the
// new buffer is latched, so the previous one is no longer read
#define PD_DISPLAY_FN_FLIP		"PDDisplayFlip"
// back to the buffer the firmware left on screen (the console)
#define PD_DISPLAY_FN_RESTORE		"PDDisplayRestore"

struct pd_display_info {
	uint32_t width, height;
	uint32_t pitch;			// of the boot buffer
	uint32_t format;		// drm fourcc of the layer, argb8888
	uint64_t boot_address;
	uint64_t current_address;
	uint32_t refresh_mhz;		// measured, millihertz
	uint32_t total_lines;
	uint64_t vblanks;
};

struct pd_display_flip {
	uint64_t sequence;		// vblank count at which the buffer was latched
	uint64_t time_ns;		// mach absolute time of that vblank, in ns
};

#endif
