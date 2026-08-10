#include <lil/intel.h>
#include <lil/imports.h>

#include "src/avi.hpp"

enum hdmi_infoframe_type {
	HDMI_INFOFRAME_TYPE_AVI = 0x82,
};

#define HDMI_AVI_INFOFRAME_SIZE 13

enum hdmi_colorspace {
	HDMI_COLORSPACE_RGB = 0b00,
	HDMI_COLORSPACE_YUV422 = 0b01,
	HDMI_COLORSPACE_YUV444 = 0b10,
	HDMI_COLORSPACE_YUV420 = 0b11,
};

enum hdmi_scan_mode {
	HDMI_SCAN_MODE_NONE,
	HDMI_SCAN_MODE_OVERSCAN,
	HDMI_SCAN_MODE_UNDERSCAN,
	HDMI_SCAN_MODE_RESERVED,
};

enum hdmi_active_bar_info {
	HDMI_ACTIVE_BAR_INFO_INVALID = 0b00,
	HDMI_ACTIVE_BAR_INFO_VERTICAL_VALID = 0b01,
	HDMI_ACTIVE_BAR_INFO_HORIZONTAL_VALID = 0b10,
	HDMI_ACTIVE_BAR_INFO_BOTH_VALID = 0b11,
};

enum hdmi_active_format_info {
	HDMI_ACTIVE_FORMAT_INFO_INVALID = 0,
	HDMI_ACTIVE_FORMAT_INFO_VALID = 1,
};

enum hdmi_active_aspect {
	HDMI_ACTIVE_ASPECT_16_9_TOP = 2,
	HDMI_ACTIVE_ASPECT_14_9_TOP = 3,
	HDMI_ACTIVE_ASPECT_16_9_CENTER = 4,
	HDMI_ACTIVE_ASPECT_PICTURE = 8,
	HDMI_ACTIVE_ASPECT_4_3 = 9,
	HDMI_ACTIVE_ASPECT_16_9 = 10,
	HDMI_ACTIVE_ASPECT_14_9 = 11,
	HDMI_ACTIVE_ASPECT_4_3_SP_14_9 = 13,
	HDMI_ACTIVE_ASPECT_16_9_SP_14_9 = 14,
	HDMI_ACTIVE_ASPECT_16_9_SP_4_3 = 15,
};

enum hdmi_nups {
	HDMI_NUPS_UNKNOWN,
	HDMI_NUPS_HORIZONTAL,
	HDMI_NUPS_VERTICAL,
	HDMI_NUPS_BOTH,
};

struct hdmi_avi_infoframe {
	uint8_t packet_type;
	uint8_t version;
	uint8_t length: 5;
	uint8_t zeroes0: 3;
	uint8_t checksum;
	uint8_t scan_info: 2;
	uint8_t bar_info_valid: 2;
	uint8_t active_info_valid: 1;
	uint8_t color_indicator: 2;
	uint8_t reserved0: 1;
	uint8_t active_format_aspect_ratio: 4;
	uint8_t picture_aspect_ratio: 2;
	uint8_t colorimetry: 2;
	uint8_t nups: 2;
	uint8_t reserved1: 6;
	uint8_t video_format: 7;
	uint8_t reserved2: 1;
	uint8_t pixel_repitition: 4;
	uint8_t reserved3: 4;
	uint16_t top_bar;
	uint16_t bottom_bar;
	uint16_t left_bar;
	uint16_t right_bar;
	uint8_t reserved4[14];
} __attribute__((packed));

static_assert(sizeof(struct hdmi_avi_infoframe) == 31, "HDMI AVI infoframe has incorrect size");

namespace hdmi::avi {

void infoframe_populate(LilCrtc *crtc, void *data) {
	memset(data, 0, 16);

	auto f = reinterpret_cast<hdmi_avi_infoframe *>(data);

	f->packet_type = HDMI_INFOFRAME_TYPE_AVI;
	f->version = 2;
	f->length = 0x0D;
	f->active_info_valid = HDMI_ACTIVE_FORMAT_INFO_INVALID;
	f->bar_info_valid = HDMI_ACTIVE_BAR_INFO_BOTH_VALID;
	f->scan_info = HDMI_SCAN_MODE_NONE;
	f->color_indicator = HDMI_COLORSPACE_RGB;
	f->nups = HDMI_NUPS_BOTH;
}

} // namespace hdmi::avi
