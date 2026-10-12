#include "PDAmdGPU.h"

// firmware stays an unmodified binary, LICENSE.amdgpu is installed with the kext
#define FW_BLOB(sym, file) \
    ".p2align 4\n.private_extern _pd_amd_" #sym "_start\n_pd_amd_" #sym "_start:\n" \
    ".incbin \"" PDAMDGPU_FW_DIR "/" file "\"\n" \
    ".private_extern _pd_amd_" #sym "_end\n_pd_amd_" #sym "_end:\n"

__asm__(".section __TEXT,__const\n"
        FW_BLOB(gr_toc, "psp_13_0_5_toc.bin")
        FW_BLOB(gr_sdma, "sdma_5_2_6.bin")
        FW_BLOB(gr_ce, "gc_10_3_6_ce.bin")
        FW_BLOB(gr_pfp, "gc_10_3_6_pfp.bin")
        FW_BLOB(gr_me, "gc_10_3_6_me.bin")
        FW_BLOB(gr_mec, "gc_10_3_6_mec.bin")
        FW_BLOB(gr_rlc, "gc_10_3_6_rlc.bin")
        FW_BLOB(gr_dmcub, "dcn_3_1_5_dmcub.bin")
        FW_BLOB(mdc_toc, "psp_13_0_8_toc.bin")
        FW_BLOB(mdc_sdma, "sdma_5_2_7.bin")
        FW_BLOB(mdc_ce, "gc_10_3_7_ce.bin")
        FW_BLOB(mdc_pfp, "gc_10_3_7_pfp.bin")
        FW_BLOB(mdc_me, "gc_10_3_7_me.bin")
        FW_BLOB(mdc_mec, "gc_10_3_7_mec.bin")
        FW_BLOB(mdc_rlc, "gc_10_3_7_rlc.bin")
        FW_BLOB(mdc_dmcub, "dcn_3_1_6_dmcub.bin")
        ".text\n");

#define FW_DECL(sym) extern "C" const uint8_t pd_amd_##sym##_start[], pd_amd_##sym##_end[];
FW_DECL(gr_toc) FW_DECL(gr_sdma) FW_DECL(gr_ce) FW_DECL(gr_pfp) FW_DECL(gr_me) FW_DECL(gr_mec) FW_DECL(gr_rlc)
FW_DECL(gr_dmcub)
FW_DECL(mdc_toc) FW_DECL(mdc_sdma) FW_DECL(mdc_ce) FW_DECL(mdc_pfp) FW_DECL(mdc_me) FW_DECL(mdc_mec) FW_DECL(mdc_rlc)
FW_DECL(mdc_dmcub)

#define FW_IMAGE(sym) { pd_amd_##sym##_start, pd_amd_##sym##_end }

// psp 13.0.5, gc 10.3.6, sdma 5.2.6, dcn 3.1.5
const AmdFirmware kAmdFirmwareGraniteRidge = {
    FW_IMAGE(gr_toc), FW_IMAGE(gr_sdma), FW_IMAGE(gr_ce), FW_IMAGE(gr_pfp), FW_IMAGE(gr_me), FW_IMAGE(gr_mec),
    FW_IMAGE(gr_rlc), FW_IMAGE(gr_dmcub),
};

// psp 13.0.8, gc 10.3.7, sdma 5.2.7, dcn 3.1.6, mec2 is the mec image on both
const AmdFirmware kAmdFirmwareMendocino = {
    FW_IMAGE(mdc_toc), FW_IMAGE(mdc_sdma), FW_IMAGE(mdc_ce), FW_IMAGE(mdc_pfp), FW_IMAGE(mdc_me), FW_IMAGE(mdc_mec),
    FW_IMAGE(mdc_rlc), FW_IMAGE(mdc_dmcub),
};
