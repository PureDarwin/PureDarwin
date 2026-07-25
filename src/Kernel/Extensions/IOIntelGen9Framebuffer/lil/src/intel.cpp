#include <lil/intel.h>
#include <lil/imports.h>

#include "src/gemini_lake/glk.hpp"
#include "src/ivy_bridge/ivb.hpp"
#include "src/kaby_lake/kbl.hpp"
#include "src/pch.hpp"
#include "src/pci.hpp"
#include "src/base.hpp"
#include "src/vbt/vbt.hpp"

bool lil_init_gpu(LilGpu **ret, void *device, uint16_t pch_id) {
	auto gpu = new Gpu{device, pch_id};

	auto config_0 = gpu->pci_read<uint32_t>(PCI_HDR_VENDOR_ID);
	if (config_0 == 0xffffffff)
		return false;

	uint16_t device_id = (uint16_t) (config_0 >> 16);
	uint16_t vendor_id = (uint16_t) config_0;
	if (vendor_id != 0x8086)
		return false;

	uint16_t pci_class = gpu->pci_read<uint16_t>(PCI_HDR_SUBCLASS);
	if (pci_class != 0x300)
		return false;

	pch::get_gen(gpu);
	gpu->subgen = SUBGEN_NONE;

	switch (device_id) {
		case 0x0116:
		case 0x0166: {
			gpu->gen = GEN_IVB;
			lil_init_ivb_gpu(gpu);
			*ret = gpu;
			return true;
		}

		case 0x3184:
		case 0x3185: {
			vbt_init(gpu);

			uint8_t prog_if = gpu->pci_read<uint8_t>(PCI_HDR_PROG_IF);
			lil_assert(!prog_if);
			glk::init_gpu(gpu);
			*ret = gpu;
			return true;
		}

		case 0x5916:
		case 0x3E9B:
		case 0x5917: {
			vbt_init(gpu);

			uint8_t prog_if = gpu->pci_read<uint8_t>(PCI_HDR_PROG_IF);
			lil_assert(!prog_if);
			kbl::init_gpu(gpu);
			*ret = gpu;
			return true;
		}

		default:
			return false;
	}
}

