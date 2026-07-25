#include "src/pd_kext_prims.hpp"
#include <stddef.h>
#include <stdint.h>

#include <lil/imports.h>
#include <lil/vbt.h>

#include "src/vbt/vbt.hpp"

const struct vbt_header *vbt_get_header(const void *vbt, size_t size) {
	for(size_t i = 0; i + 4 < size; i++) {
		auto header = reinterpret_cast<const struct vbt_header *>(uintptr_t(vbt) + i);

		if(lil::string_view{reinterpret_cast<const char *>(header->signature), 4} != VBT_HEADER_SIGNATURE)
			continue;

		return header;
	}

	return nullptr;
}

const struct bdb_header *vbt_get_bdb_header(const struct vbt_header *hdr) {
	auto bdb = reinterpret_cast<struct bdb_header *>(uintptr_t(hdr) + hdr->bdb_offset);

	if(lil::string_view{reinterpret_cast<const char *>(bdb->signature), 16} != BDB_HEADER_SIGNATURE)
		return nullptr;

	return bdb;
}

LilDdiId vbt_dvo_to_ddi(uint8_t dvo_id) {
	switch(dvo_id) {
		case 1:
			return DDI_B;
		case 2:
			return DDI_C;
		case 3:
			return DDI_D;
		case 7:
			return DDI_B;
		case 8:
			return DDI_C;
		case 9:
			return DDI_D;
	}

	if(dvo_id == 11 || dvo_id == 12)
		return DDI_D;

	return DDI_A;
}

#define DVO_PORT_HDMIA 0
#define DVO_PORT_HDMIB 1
#define DVO_PORT_HDMIC 2
#define DVO_PORT_HDMID 3
#define DVO_PORT_LVDS 4
#define DVO_PORT_TV 5
#define DVO_PORT_CRT 6
#define DVO_PORT_DPB 7
#define DVO_PORT_DPC 8
#define DVO_PORT_DPD 9
#define DVO_PORT_DPA 10
#define DVO_PORT_DPE 11
#define DVO_PORT_HDMIE 12
#define DVO_PORT_DPF 13
#define DVO_PORT_HDMIF 14
#define DVO_PORT_DPG 15
#define DVO_PORT_HDMIG 16
#define DVO_PORT_DPH 17
#define DVO_PORT_HDMIH 18
#define DVO_PORT_DPI 19
#define DVO_PORT_HDMII 20
#define DVO_PORT_MIPIA 21
#define DVO_PORT_MIPIB 22
#define DVO_PORT_MIPIC 23
#define DVO_PORT_MIPID 24
#define DVO_PORT_MAX DVO_PORT_MIPID

static struct {
	uint8_t dvo;
	const char *name;
} dvo_port_names[] = {
	{ DVO_PORT_CRT, "CRT" },
	{ DVO_PORT_DPA, "DP-A" },
	{ DVO_PORT_DPB, "DP-B" },
	{ DVO_PORT_DPC, "DP-C" },
	{ DVO_PORT_DPD, "DP-D" },
	{ DVO_PORT_DPE, "DP-E" },
	{ DVO_PORT_DPF, "DP-F" },
	{ DVO_PORT_DPG, "DP-G" },
	{ DVO_PORT_DPH, "DP-H" },
	{ DVO_PORT_DPI, "DP-I" },
	{ DVO_PORT_HDMIA, "HDMI-A" },
	{ DVO_PORT_HDMIB, "HDMI-B" },
	{ DVO_PORT_HDMIC, "HDMI-C" },
	{ DVO_PORT_HDMID, "HDMI-D" },
	{ DVO_PORT_HDMIE, "HDMI-E" },
	{ DVO_PORT_HDMIF, "HDMI-F" },
	{ DVO_PORT_HDMIG, "HDMI-G" },
	{ DVO_PORT_HDMIH, "HDMI-H" },
	{ DVO_PORT_HDMII, "HDMI-I" },
	{ DVO_PORT_LVDS, "LVDS" },
	{ DVO_PORT_MIPIA, "MIPI-A" },
	{ DVO_PORT_MIPIB, "MIPI-B" },
	{ DVO_PORT_MIPIC, "MIPI-C" },
	{ DVO_PORT_MIPID, "MIPI-D" },
	{ DVO_PORT_TV, "TV" },
};

const char *vbt_dvo_get_name(uint8_t dvo) {
	if(dvo > DVO_PORT_MAX)
		return nullptr;

	auto val = lil::ranges::find_if(dvo_port_names, [dvo](auto &m) {
		return m.dvo == dvo;
	});

	return (val != lil::end(dvo_port_names)) ? val->name : nullptr;
}
