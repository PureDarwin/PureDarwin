
#include "src/pd_kext_prims.hpp"
#include <lil/fourcc.h>
#include <lil/imports.h>

#include "src/base.hpp"
#include "src/helpers.hpp"
#include "src/kaby_lake/plane.hpp"
#include "src/kaby_lake/watermarks.hpp"
#include "src/regs.hpp"

namespace {

void wait_for_vblank(LilGpu *gpu, LilPlane *plane) {
	REG(IIR(plane->pipe_id)) |= 1;

	if(!wait_for_bit_set(REG_PTR(IIR(plane->pipe_id)), 1, 500000, 1000))
		lil_log(WARNING, "timeout on wait_for_vblank\n");
}

} // namespace

namespace kbl::plane {

bool update_primary_surface(struct LilGpu* gpu, struct LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride) {
	if(surface_address & 0xFFF)
		return false;

    REG(PRI_SURFACE(plane->pipe_id)) = surface_address;

    return true;
}

void page_flip(LilGpu *gpu, LilCrtc *crtc) {
	REG(PRI_SURFACE(crtc->pipe_id)) = REG(PRI_SURFACE(crtc->pipe_id));
}

namespace {

uint32_t linetime_us(uint32_t pixel_clock, uint32_t htotal) {
	return div_u32_ceil(htotal * 1000, pixel_clock);
}

} // namespace

void enable(LilGpu *lil_gpu, LilCrtc *crtc, bool vblank_wait) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	uint32_t htotal = crtc->current_mode.htotal;
	uint32_t pixel_clock = crtc->current_mode.clock;

	uint8_t wm[8];
	wm[0] = (gpu->mem_latency_first_set >> 0) & 0xFF;
	wm[1] = (gpu->mem_latency_first_set >> 8) & 0xFF;
	wm[2] = (gpu->mem_latency_first_set >> 16) & 0xFF;
	wm[3] = (gpu->mem_latency_first_set >> 24) & 0xFF;
	wm[4] = (gpu->mem_latency_second_set >> 0) & 0xFF;
	wm[5] = (gpu->mem_latency_second_set >> 8) & 0xFF;
	wm[6] = (gpu->mem_latency_second_set >> 16) & 0xFF;
	wm[7] = (gpu->mem_latency_second_set >> 24) & 0xFF;

	lil_log(VERBOSE, "memory latency levels:\n");
	for(size_t level = 0; level < 8; level++) {
		lil_log(VERBOSE, "\tlevel %zu: %u us\n", level, wm[level]);
	}

	size_t valid_levels = 8;

	for(size_t level = 1; level < valid_levels; level++) {
		if(wm[level] == 0) {
			for(size_t i = level + 1; i < valid_levels; i++) {
				wm[i] = 0;
			}

			valid_levels = level;
			break;
		}
	}

	if(!wm[0]) {
		for(size_t level = 0; level < valid_levels; level++) {
			wm[level] += 2;
		}
	}

	lil_log(VERBOSE, "linetime: %u us\n", linetime_us(pixel_clock, htotal));
	for(size_t level = 0; level < valid_levels; level++) {
		auto [blocks, lines] = kbl::watermarks::calculate(pixel_clock, crtc->current_mode.hactive, htotal, wm[level], level).value();

		if(wm[level]) {
			REG(PLANE_WM(crtc->pipe_id, level)) = (1 << 31) | (blocks & 0x3FF) | ((lines & 0x1F) << 14);

			lil_log(VERBOSE, "[%ux%u] level %zu: %zu blocks %zu lines (level %u)\n", crtc->current_mode.hactive, crtc->current_mode.vactive, level, blocks, lines, wm[level]);
		}
	}

	uint32_t pixel_format = PLANE_CTL_SOURCE_PIXEL_FORMAT_RGB_8_8_8_8 | PLANE_CTL_COLOR_ORDER_BGRX;

	switch(crtc->planes[0].pixel_format) {
		case FORMAT_XBGR8888:
			pixel_format = PLANE_CTL_SOURCE_PIXEL_FORMAT_RGB_8_8_8_8 | PLANE_CTL_COLOR_ORDER_RGBX;
			break;
		case FORMAT_XRGB8888:
			pixel_format = PLANE_CTL_SOURCE_PIXEL_FORMAT_RGB_8_8_8_8 | PLANE_CTL_COLOR_ORDER_BGRX;
			break;
		default:
			lil_log(WARNING, "invalid/unsupported pixel format for plane; falling back to BGRX");
			break;
	}

	REG(PLANE_CTL(crtc->pipe_id)) |= PLANE_CTL_ENABLE | PLANE_CTL_INTERNAL_GAMMA_DISABLE | pixel_format;

	page_flip(gpu, crtc);

	if(vblank_wait)
		wait_for_vblank(gpu, &crtc->planes[0]);
}

void disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(PLANE_CTL(crtc->pipe_id)) &= ~PLANE_CTL_ENABLE;

	page_flip(gpu, crtc);
	wait_for_vblank(gpu, &crtc->planes[0]);

	for(size_t i = 0; i < 8; i++) {
		REG(PLANE_WM_1(crtc->pipe_id) + (4 * i)) = 0;
	}

	REG(WM_LINETIME(crtc->pipe_id)) = 0;
}

void size_set(LilGpu *gpu, LilCrtc *crtc) {
	REG(PLANE_SIZE(crtc->pipe_id)) = (crtc->current_mode.hactive - 1) | ((crtc->current_mode.vactive - 1) << 16);
}

lil::array<uint32_t, 2> supported_formats = {
	FORMAT_XBGR8888,
	FORMAT_XRGB8888,
};

uint32_t *get_formats(struct LilGpu *gpu, size_t *num) {
	*num = supported_formats.size();
	return supported_formats.data();
}

} // namespace kbl::plane
