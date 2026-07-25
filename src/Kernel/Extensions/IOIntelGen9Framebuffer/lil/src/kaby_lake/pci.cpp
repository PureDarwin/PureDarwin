#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/kaby_lake/pci.hpp"

namespace {

uint16_t SKYLAKE_IDS[12] = { 0x1902, 0x1912, 0x1932, 0x193A, 0x190B, 0x191B, 0x192B, 0x193B, 0x191D, 0x192D, 0x193D, 0 };
uint16_t SKYLAKE_ULT_IDS[8] = { 0x1906, 0x1916, 0x1926, 0x1927, 0x1913, 0x1923, 0x1921, 0 };
uint16_t SKYLAKE_ULX_IDS[3] = { 0x190E, 0x191E, 0 };

uint16_t KABY_LAKE_IDS[11] = { 0x5902, 0x5912, 0x5917, 0x5908, 0x590A, 0x591A, 0x590B, 0x591B, 0x593B, 0x591D, 0 };
uint16_t KABY_LAKE_ULT_IDS[8] = { 0x5906, 0x5916, 0x5926, 0x5913, 0x5923, 0x5921, 0x5927, 0 };
uint16_t KABY_LAKE_ULX_IDS[4] = { 0x5915, 0x590E, 0x591E, 0 };

uint16_t GEMINI_LAKE_ULX_IDS[3] = { 0x3184, 0x3185, 0 };

struct variant_desc {
	uint16_t *id_list;
	enum LilGpuGen gen;
	enum LilGpuSubGen subgen;
	enum LilGpuVariant variant;
} variants[] = {
	{ SKYLAKE_IDS, GEN_SKL, SUBGEN_NONE, H },
	{ SKYLAKE_ULT_IDS, GEN_SKL, SUBGEN_NONE, ULT },
	{ SKYLAKE_ULX_IDS, GEN_SKL, SUBGEN_NONE, ULX },
	{ KABY_LAKE_IDS, GEN_SKL, SUBGEN_KABY_LAKE, H },
	{ KABY_LAKE_ULT_IDS, GEN_SKL, SUBGEN_KABY_LAKE, ULT },
	{ KABY_LAKE_ULX_IDS, GEN_SKL, SUBGEN_KABY_LAKE, ULX },
	{ GEMINI_LAKE_ULX_IDS, GEN_SKL, SUBGEN_GEMINI_LAKE, ULX },
};

} // namespace

namespace kbl::pci {

void detect(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	uint16_t dev_id = gpu->pci_read<uint16_t>(2);

	for(size_t i = 0; i < (sizeof(variants)/sizeof(*variants)); i++) {
		struct variant_desc *desc = &variants[i];

		for(size_t j = 0; desc->id_list[j]; j++) {
			if(desc->id_list[j] == dev_id) {
				gpu->gen = desc->gen;
				gpu->subgen = desc->subgen;
				gpu->variant = desc->variant;
			}
		}
	}
}

} // namespace kbl::pci
