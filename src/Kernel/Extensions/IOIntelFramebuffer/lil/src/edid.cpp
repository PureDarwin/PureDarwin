#include <lil/imports.h>

#include "src/edid.hpp"

void edid_timing_to_mode(DisplayData* edid, DetailTiming timing, LilModeInfo* mode) {
    mode->clock = timing.pixelClock * 10;

    uint32_t horz_active = timing.horzActive | ((uint32_t)(timing.horzActiveBlankMsb >> 4) << 8);
    uint32_t horz_blank = timing.horzBlank | ((uint32_t)(timing.horzActiveBlankMsb & 0xF) << 8);
    uint32_t horz_sync_offset = timing.horzSyncOffset | ((uint32_t)(timing.syncMsb >> 6) << 8);
    uint32_t horz_sync_pulse = timing.horzSyncPulse | (((uint32_t)(timing.syncMsb >> 4) & 0x3) << 8);
    mode->hactive = horz_active;
    mode->hsyncStart = horz_active + horz_sync_offset;
    mode->hsyncEnd = horz_active + horz_sync_offset + horz_sync_pulse;
    mode->htotal = horz_active + horz_blank;

	uint32_t vert_active =  timing.vertActive | ((uint32_t)(timing.vertActiveBlankMsb >> 4) << 8);
	uint32_t vert_blank = timing.vertBlank | ((uint32_t)(timing.vertActiveBlankMsb & 0xF) << 8);
	uint32_t vert_sync_offset = (timing.vertSync >> 4) | (((uint32_t)(timing.syncMsb >> 2) & 0x3) << 4);
	uint32_t vert_sync_pulse = (timing.vertSync & 0xF) | ((uint32_t)(timing.syncMsb & 0x3) << 4);
	mode->vactive = vert_active;
	mode->vsyncStart = vert_active + vert_sync_offset;
	mode->vsyncEnd = vert_active + vert_sync_offset + vert_sync_pulse;
	mode->vtotal = vert_active + vert_blank;

	if(edid->structRevision < 4 || !(edid->inputParameters & (1 << 7)) || (((edid->inputParameters >> 4) & 0x7) == 0) || (((edid->inputParameters >> 4) & 0x7) == 7))
		mode->bpc = 5;
	else
		mode->bpc = 4 + 2 * ((edid->inputParameters >> 4) & 0x7);

	if(edid->structRevision >= 4 && (edid->inputParameters & 0x80) != 0) {
		uint32_t edid_bpp_val = (edid->inputParameters >> 4) & 7;
		uint32_t bpp = 0;

		switch(edid_bpp_val) {
			case 0:
				// EDID 1.4 "color bit depth undefined"; default to 8bpc.
				bpp = 24;
				break;
			case 1:
				bpp = 18;
				break;
			case 2:
				bpp = 24;
				break;
			case 3:
				bpp = 30;
				break;
			case 4:
				bpp = 36;
				break;
			case 5:
				bpp = 42;
				break;
			case 6:
				bpp = 48;
				break;
		}

		mode->bpp = bpp;
	} else {
		mode->bpp = 24;
	}

	mode->hsyncPolarity = 0;
	mode->vsyncPolarity = 0;

	if((timing.features & 0x10) && (timing.features & 8)) {
		mode->hsyncPolarity = 1;
		mode->vsyncPolarity = 1;

		if(timing.features & 2)
			mode->hsyncPolarity = 2;

		if(timing.features & 4)
			mode->vsyncPolarity = 2;
	}

	mode->horizontalMm = timing.dimensionHeight | ((timing.dimensionMsb & 0x0F) << 8);
	mode->horizontalMm = timing.dimensionWidth | ((timing.dimensionMsb & 0xF0) << 4);
}
