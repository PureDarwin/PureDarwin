#pragma once

#include <stdint.h>

#define OPREGION_SIGNATURE "IntelGraphicsMem"
#define OPREGION_SIZE (8 * 1024)

#define OPREGION_ASLE_OFFSET 0x300
#define OPREGION_VBT_OFFSET 0x400
#define OPREGION_ASLE_EXT_OFFSET 0x1C00

#define MBOX_ASLE (1 << 2)
#define MBOX_VBT (1 << 3)
#define MBOX_ASLE_EXT (1 << 4)

struct opregion_header {
	char signature[16];
	uint32_t size;
	struct {
		uint8_t reserved;
		uint8_t revision;
		uint8_t minor;
		uint8_t major;
	} __attribute__((packed)) over;
	char sver[32];
	char vver[16];
	char gver[16];
	uint32_t mbox;
	uint32_t dmod;
	uint32_t pcon;
	char dver[32];
	uint8_t reserved[124];
} __attribute__((packed));

static_assert(sizeof(struct opregion_header) == 256, "OpRegion header size is incorrect");

struct opregion_asle {
	uint32_t ardy;
	uint32_t aslc;
	uint32_t tche;
	uint32_t alsi;
	uint32_t bclp;
	uint32_t pfit;
	uint32_t cblv;
	uint16_t bclm[20];
	uint32_t cpfm;
	uint32_t epfm;
	uint8_t plut[74];
	uint32_t pfmb;
	uint32_t cddv;
	uint32_t pcft;
	uint32_t srot;
	uint32_t iuer;
	uint64_t fdss;
	uint32_t fdsp;
	uint32_t stat;
	uint64_t rvda;
	uint32_t rvds;
	uint8_t reserved[58];
} __attribute__((packed));
