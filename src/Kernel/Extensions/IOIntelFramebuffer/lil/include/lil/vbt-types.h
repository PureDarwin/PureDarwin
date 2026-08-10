#pragma once

#include <stdint.h>

struct vbt_header {
	uint8_t signature[20];
	uint16_t version;
	uint16_t header_size;
	uint16_t vbt_size;
	uint8_t vbt_checksum;
	uint8_t reserved0;
	uint32_t bdb_offset;
	uint32_t aim_offset[4];
} __attribute__((packed));
