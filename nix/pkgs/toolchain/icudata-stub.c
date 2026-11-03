// empty icu common data package: a valid header with no entries, so icu loads and every
// lookup fails with a missing resource error instead of pulling in 32 MB of locale data
#include <stdint.h>

struct pd_icu_empty {
	uint16_t headerSize;
	uint8_t magic1, magic2;
	struct {
		uint16_t size, reservedWord;
		uint8_t isBigEndian, charsetFamily, sizeofUChar, reservedByte;
		uint8_t dataFormat[4], formatVersion[4], dataVersion[4];
	} info;
	char padding[8];
	uint32_t count, reserved;
};

__attribute__((visibility("default"), aligned(16)))
const struct pd_icu_empty icudt76_dat = {
	32, 0xda, 0x27,
	{ 20, 0, 0, 0, 2, 0, { 'C', 'm', 'n', 'D' }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
	{ 0 }, 0, 0
};
