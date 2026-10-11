#include <stdint.h>

// the imagination firmware image, unmodified, linked in because the kernel collection carries
// no bundle resources. its licence is firmware/LICENSE.powervr, installed with the kext
__asm__(
	".section __TEXT,__const\n"
	".p2align 4\n"
	".private_extern _pd_pvr_fw_start\n"
	"_pd_pvr_fw_start:\n"
	".incbin \"" PDPOWERVR_FW_PATH "\"\n"
	".private_extern _pd_pvr_fw_end\n"
	"_pd_pvr_fw_end:\n"
	".text\n");

extern const uint8_t pd_pvr_fw_start[];
extern const uint8_t pd_pvr_fw_end[];

const uint8_t *
pd_pvr_fw_blob(uint32_t *size)
{
	*size = (uint32_t)(pd_pvr_fw_end - pd_pvr_fw_start);
	return pd_pvr_fw_start;
}
