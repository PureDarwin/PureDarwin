#include "PDAmdGPU.h"

extern "C" {
#include "atom/atom.h"
}

// amdgpu_atombios_init's card callbacks: registers by dword index, mc and pll have no meaning on this asic
static uint32_t cardRegRead(struct card_info *info, uint32_t reg)
{
    return ((PDAmdGPU *)info->dev)->rregAny(reg);
}

static void cardRegWrite(struct card_info *info, uint32_t reg, uint32_t val)
{
    ((PDAmdGPU *)info->dev)->wregAny(reg, val);
}

static uint32_t cardBadRead(struct card_info *info, uint32_t reg)
{
    LOG("atom: mc or pll read 0x%x not supported\n", reg);
    return 0;
}

static void cardBadWrite(struct card_info *info, uint32_t reg, uint32_t val)
{
    LOG("atom: mc or pll write 0x%x not supported\n", reg);
}

#define kAtomFirmwareInfoIndex      4           // firmwareinfo in atom_master_list_of_data_tables_v2_1
#define kFirmwareInfoScratchStart   24          // atom_firmware_info_v3_1 bios_scratch_reg_startaddr
#define kAsicInitComplete           0x200       // ATOM_S7_ASIC_INIT_COMPLETE_MASK
#define kAtomScratchBytes           (20 * 1024)

// amdgpu_device_asic_init for gc 10.3: run the vbios asic init table when the scratch says the asic is not posted,
// or always after a reset, as amdgpu's has_hw_reset does
bool PDAmdGPU::asicPost(bool force)
{
    if (!vbios && !readVbios()) {
        LOG("atom: no video bios, cannot post\n");
        return false;
    }

    struct card_info card = {};
    card.dev = (struct drm_device *)this;
    card.reg_read = cardRegRead;
    card.reg_write = cardRegWrite;
    card.mc_read = cardBadRead;
    card.mc_write = cardBadWrite;
    card.pll_read = cardBadRead;
    card.pll_write = cardBadWrite;

    struct atom_context *ctx = amdgpu_atom_parse(&card, vbios);
    if (!ctx) {
        LOG("atom: video bios does not parse\n");
        return false;
    }
    ctx->scratch = (uint32_t *)kzalloc(kAtomScratchBytes, GFP_KERNEL);
    ctx->scratch_size_bytes = ctx->scratch ? kAtomScratchBytes : 0;

    uint16_t fwi = 0;
    uint32_t scratchBase = 0;
    if (amdgpu_atom_parse_data_header(ctx, kAtomFirmwareInfoIndex, NULL, NULL, NULL, &fwi))
        memcpy(&scratchBase, vbios + fwi + kFirmwareInfoScratchStart, 4);

    uint32_t s7 = scratchBase ? rregAny(scratchBase + 7) : 0;
    bool posted = (s7 & kAsicInitComplete) != 0;
    LOG("atom: bios scratch at 0x%x, BIOS_SCRATCH_7 0x%08x, %s%s\n", scratchBase, s7, posted ? "posted" : "not posted",
        force ? ", posting after reset" : "");

    int ret = 0;
    if (force || !posted) {
        ret = amdgpu_atom_asic_init(ctx);
        LOG("atom: asic init %s (%d), BIOS_SCRATCH_7 now 0x%08x\n", ret ? "failed" : "done", ret,
            scratchBase ? rregAny(scratchBase + 7) : 0);
    }

    kfree(ctx->scratch);
    amdgpu_atom_destroy(ctx);
    return ret == 0;
}
