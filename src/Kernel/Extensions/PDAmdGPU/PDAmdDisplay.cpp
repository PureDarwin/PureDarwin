#include "PDAmdGPU.h"
#include "AmdDcn315Regs.h"
#include "AmdDcn315Dump.h"
#include "AmdDcnDpRegs.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

// one hdmi stream on otg0 from one linear argb8888 plane on hubp0/dpp0, the dcn31 hwseq order:
// clocks, then the hdmi encoder, then timing, then the pipe from opp back to hubp, then unblank

struct CeaMode {
    uint32_t pixKhz;
    uint16_t hActive, hFront, hSync, hBack;
    uint16_t vActive, vFront, vSync, vBack;
};

// cea-861 vic 16, 1920x1080p60
static const CeaMode k1080p60 = { 148500, 1920, 88, 44, 148, 1080, 4, 5, 36 };

// the LEN156FHD panel's edid timing, 2230x1110 at 148.51 MHz
static const CeaMode kEdp1080p60 = { 148510, 1920, 48, 32, 230, 1080, 3, 6, 21 };

struct RegValue { uint32_t reg, value; };

// the fetch schedule and arbitration amdgpu's dml computed for this mode, as the reference vm left them;
// a single-pipe dml calculation replaces this table
static const RegValue kDmlFor1080p60[] = {
    { dcnHUBP0_DCHUBP_REQ_SIZE_CONFIG, 0x033d1b40 },
    { dcnHUBPREQ0_DCN_EXPANSION_MODE, 0x00000056 },
    { dcnHUBPREQ0_DCN_TTU_QOS_WM, 0x058e0000 },
    { dcnHUBPREQ0_DCN_GLOBAL_TTU_CNTL, 0xe00037d0 },
    { dcnHUBPREQ0_DCN_SURF0_TTU_CNTL0, 0x0800295f },
    { dcnHUBPREQ0_DCN_SURF0_TTU_CNTL1, 0x0000148f },
    { dcnHUBPREQ0_DCN_SURF1_TTU_CNTL0, 0x08000000 },
    { dcnHUBPREQ0_DCN_CUR0_TTU_CNTL0, 0x08000000 },
    { dcnHUBPREQ0_BLANK_OFFSET_0, 0x0029001f },
    { dcnHUBPREQ0_BLANK_OFFSET_1, 0x00001184 },
    { dcnHUBPREQ0_DST_DIMENSIONS, 0x0001638e },
    { dcnHUBPREQ0_DST_AFTER_SCALER, 0x00000021 },
    { dcnHUBPREQ0_PREFETCH_SETTINGS, 0x0d200000 },
    { dcnHUBPREQ0_PREFETCH_SETTINGS_C, 0x00080000 },
    { dcnHUBPREQ0_VBLANK_PARAMETERS_0, 0x00000403 },
    { dcnHUBPREQ0_VBLANK_PARAMETERS_1, 0x00000058 },
    { dcnHUBPREQ0_VBLANK_PARAMETERS_3, 0x00000163 },
    { dcnHUBPREQ0_VBLANK_PARAMETERS_4, 0x00000163 },
    { dcnHUBPREQ0_FLIP_PARAMETERS_0, 0x00000403 },
    { dcnHUBPREQ0_FLIP_PARAMETERS_1, 0x00000058 },
    { dcnHUBPREQ0_FLIP_PARAMETERS_2, 0x00000163 },
    { dcnHUBPREQ0_NOM_PARAMETERS_0, 0x00000200 },
    { dcnHUBPREQ0_NOM_PARAMETERS_1, 0x00002c71 },
    { dcnHUBPREQ0_NOM_PARAMETERS_4, 0x00000020 },
    { dcnHUBPREQ0_NOM_PARAMETERS_5, 0x00000b1c },
    { dcnHUBPREQ0_PER_LINE_DELIVERY_PRE, 0x0000009a },
    { dcnHUBPREQ0_PER_LINE_DELIVERY, 0x00000136 },
    { dcnHUBPREQ0_REF_FREQ_TO_PIX_FREQ, 0x00014afd },
    { dcnHUBPREQ0_DST_Y_DELTA_DRQ_LIMIT, 0x00003fff },
    { dcnDCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A, 0x00000074 },
    { dcnDCHUBBUB_ARB_REFCYC_PER_TRIP_TO_MEMORY_A, 0x00000060 },
    { dcnDCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A, 0x00000187 },
    { dcnDCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A, 0x0000013f },
    { dcnDCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_A, 0x00000c8c },
    { dcnDCHUBBUB_ARB_FRAC_URG_BW_NOM_A, 0x00000057 },
    { dcnDCHUBBUB_ARB_FRAC_URG_BW_FLIP_A, 0x00000085 },
    { dcnDCHUBBUB_DET0_CTRL, 0x00000303 },
    { dcnDCHUBBUB_COMPBUF_CTRL, 0x00040d0d },
};

// registers the firmware or amdgpu sets and a dark start leaves at reset values, amdgpu's values from the reference vm
static const RegValue kAmdgpuSettings[] = {
    { 0x0554e, 0x0000001f },     // DIG0_DIG_CLOCK_PATTERN
    { 0x05566, 0x00001000 },     // DIG0_HDMI_DB_CONTROL
    { 0x0555a, 0x00000003 },     // DIG0_HDMI_GENERIC_PACKET_CONTROL0
    { 0x0555b, 0x00000000 },     // DIG0_HDMI_GENERIC_PACKET_CONTROL6
    { 0x0539e, 0x00000001 },     // DIO_MEM_PWR_CTRL
    { 0x03b2d, 0x00000000 },     // HUBPRET0_HUBPRET_MEM_PWR_CTRL
    { 0x0900c, 0x00000034 },     // MPCC0_MPCC_MEM_PWR_CTRL
    { 0x04f1e, 0x00000000 },     // OPP_TOP_CLK_CONTROL
    { 0x053d8, 0x00a00032 },     // HPD0_DC_HPD_TOGGLE_FILT_CNTL
    { 0x05049, 0x0000014d },     // OTG0_OTG_VREADY_PARAM
    { 0x05042, 0x02020000 },     // OTG0_OTG_STATIC_SCREEN_CONTROL
    { 0x039e1, 0xf080203a },     // DCHUBBUB_ARB_HOSTVM_CNTL
    { 0x03aae, 0x04380780 },     // HUBP0_DCSURF_SEC_VIEWPORT_DIMENSION
    { 0x03ab0, 0x04380780 },     // HUBP0_DCSURF_SEC_VIEWPORT_DIMENSION_C
};

static void writeTable(PDAmdGPU *g, const RegValue *t, size_t n)
{
    for (size_t i = 0; i < n; i++) g->wreg(t[i].reg, t[i].value);
}

// dcn31_hubp_pg_control: hubp0 and dpp0 sit in power domain 0, gated until the pipe is first used
bool PDAmdGPU::pipePowerUp(void)
{
    // the pg registers only take writes while IP_REQUEST_EN is set
    wreg(dcnDC_IP_REQUEST_CNTL, 1);
    wreg(dcnDOMAIN0_PG_CONFIG, 0);                  // DOMAIN_POWER_GATE off
    for (int i = 0; i < 1000; i++) {
        if ((rreg(dcnDOMAIN0_PG_STATUS) >> 30) == 0) {
            wreg(dcnDC_IP_REQUEST_CNTL, 0);
            return true;
        }
        IODelay(10);
    }
    wreg(dcnDC_IP_REQUEST_CNTL, 0);
    LOG("disp: domain 0 stays gated, PG_STATUS 0x%08x\n", rreg(dcnDOMAIN0_PG_STATUS));
    return false;
}

// dcn31_hubp_pg_control power_on false: gate domain 0 and wait for the fsm to report it off
void PDAmdGPU::pipePowerDown(void)
{
    if ((rreg(dcnDOMAIN0_PG_STATUS) >> 30) == 2) return;
    wreg(dcnDC_IP_REQUEST_CNTL, 1);
    wreg(dcnDOMAIN0_PG_CONFIG, 0x100);              // DOMAIN_POWER_GATE on
    for (int i = 0; i < 1000 && (rreg(dcnDOMAIN0_PG_STATUS) >> 30) != 2; i++) IODelay(10);
    wreg(dcnDC_IP_REQUEST_CNTL, 0);
    LOG("disp: domain 0 PG_STATUS 0x%08x after gating\n", rreg(dcnDOMAIN0_PG_STATUS));
}

// dccg31: dppclk at the full dispclk rate for dpp0, symclk a on for the hdmi phy, otg0 clocked from its phy pll
void PDAmdGPU::dccgSetup(void)
{
    wreg(dcnDPPCLK0_DTO_PARAM, 0x00ff00ff);         // phase 255 of modulo 255
    wreg(dcnDPPCLK_DTO_CTRL, 0x00000001);           // dto enable for dpp0
    wreg(dcnSYMCLKA_CLOCK_ENABLE, 0x00000001);      // symclk a on, sourced from phy a
    wreg(dcnOTG0_PIXEL_RATE_CNTL, 0x80000000);      // otg0 pixel rate from the phy pll, as dccg31_set_pixel_rate leaves it
    wreg(dcnOTG0_OTG_CLOCK_CONTROL, 0x00010103);    // otg clock on and enabled
}

// dcn31_dio_link_encoder and dio stream encoder for tmds/hdmi on dig0, rgb, 8 bpc, no audio yet
void PDAmdGPU::digSetup(void)
{
    wreg(dcnDIG0_DIG_BE_CNTL, 0x00030100);          // back end in hdmi mode, fed from front end 0
    wreg(dcnDIG0_DIG_BE_EN_CNTL, 0x00000101);
    wreg(dcnDIG0_TMDS_CNTL, 0x00000001);
    wreg(dcnDIG0_TMDS_CTL_BITS, 0x00000001);
    wreg(dcnDIG0_TMDS_DCBALANCER_CONTROL, 0x00000001);
    wreg(dcnDIG0_TMDS_SYNC_DCBALANCE_CHAR, 0x00aa0355);
    wreg(dcnDIG0_HDMI_CONTROL, 0x00010019);
    wreg(dcnDIG0_HDMI_GC, 0x00000004);
    wreg(dcnDIG0_HDMI_VBI_PACKET_CONTROL, 0x00000031);
    wreg(dcnDIG0_DIG_FE_CNTL, 0x01000100);          // front end on, source otg0
}

// optc31_program_timing for otg0, computed from the cea mode
void PDAmdGPU::otgSetup(const CeaMode &m)
{
    uint32_t hTotal = m.hActive + m.hFront + m.hSync + m.hBack;
    uint32_t vTotal = m.vActive + m.vFront + m.vSync + m.vBack;

    // blank start is the end of the front porch measured from sync start, blank end the start of active
    uint32_t hBlankEnd = m.hSync + m.hBack, hBlankStart = hBlankEnd + m.hActive;
    uint32_t vBlankEnd = m.vSync + m.vBack, vBlankStart = vBlankEnd + m.vActive;

    wreg(dcnOTG0_OTG_H_TOTAL, hTotal - 1);
    wreg(dcnOTG0_OTG_V_TOTAL, vTotal - 1);
    wreg(dcnOTG0_OTG_H_BLANK_START_END, hBlankStart | (hBlankEnd << 16));
    wreg(dcnOTG0_OTG_V_BLANK_START_END, vBlankStart | (vBlankEnd << 16));
    wreg(dcnOTG0_OTG_H_SYNC_A, m.hSync << 16);
    wreg(dcnOTG0_OTG_V_SYNC_A, m.vSync << 16);
    wreg(dcnOTG0_OTG_VSTARTUP_PARAM, 0x0000002c);
    wreg(dcnOTG0_OTG_VUPDATE_PARAM, 0x00fb0226);
    wreg(dcnOTG0_OTG_DOUBLE_BUFFER_CONTROL, 0x02000000);
    wreg(dcnODM0_OPTC_DATA_SOURCE_SELECT, 0xfff00000);  // opp0 into the otg, no odm combine
    wreg(dcnODM0_OPTC_INPUT_CLOCK_CONTROL, 0x00000007);
    LOG("disp: otg0 %ux%u total %ux%u, h blank 0x%08x v blank 0x%08x\n", m.hActive, m.vActive, hTotal, vTotal,
        rreg(dcnOTG0_OTG_H_BLANK_START_END), rreg(dcnOTG0_OTG_V_BLANK_START_END));
}

// opp0 on, mpcc0 blending dpp0 alone into opp0, dpp0 in bypass at 1:1, rgb out with no output csc
void PDAmdGPU::pipeSetup(const CeaMode &m)
{
    uint32_t size = m.hActive | (m.vActive << 16);

    wreg(dcnOPP_PIPE0_OPP_PIPE_CONTROL, 0x00000003);
    wreg(dcnFMT0_FMT_CLAMP_CNTL, 0x00000001);
    wreg(dcnFMT0_FMT_BIT_DEPTH_CONTROL, 0x00608900);   // spatial and temporal dither to 8 bpc, as amdgpu sets it

    wreg(dcnMPCC0_MPCC_TOP_SEL, 0);                  // dpp0
    wreg(dcnMPCC0_MPCC_BOT_SEL, 0x0000000f);         // nothing below
    wreg(dcnMPCC0_MPCC_OPP_ID, 0);
    wreg(dcnMPCC0_MPCC_CONTROL, 0xffff0422);
    wreg(dcnMPCC0_MPCC_TOP_GAIN, 0x0001f000);
    wreg(dcnMPCC0_MPCC_BOT_GAIN_INSIDE, 0x0001f000);
    wreg(dcnMPCC0_MPCC_BOT_GAIN_OUTSIDE, 0x0001f000);
    wreg(dcnMPCC0_MPCC_UPDATE_LOCK_SEL, 0);          // mpcc0 updates latch with otg0
    wreg(dcnMPC_OUT0_MUX, 0x00000100);               // mpcc0 to opp0, rate control off
    wreg(dcnMPC_OUT0_CSC_MODE, 0);                   // rgb out, no ycbcr conversion

    wreg(dcnCM0_CM_MEM_PWR_CTRL, 0x00000033);
    wreg(dcnDPP_TOP0_DPP_CONTROL, 0x70000010);
    wreg(dcnCNVC_CFG0_CNVC_SURFACE_PIXEL_FORMAT, 0x00000008);   // argb8888
    wreg(dcnCNVC_CFG0_FORMAT_CONTROL, 0x24000101);
    wreg(dcnDSCL0_DSCL_2TAP_CONTROL, 0x01000100);
    wreg(dcnDSCL0_SCL_HORZ_FILTER_INIT, 0x01000000);
    wreg(dcnDSCL0_SCL_VERT_FILTER_INIT, 0x01000000);
    wreg(dcnDSCL0_RECOUT_SIZE, size);
    wreg(dcnDSCL0_MPC_SIZE, size);
    wreg(dcnDSCL0_LB_MEMORY_CTRL, 0x08083f00);
    wreg(dcnDSCL0_OBUF_CONTROL, 0x000000e0);
}

// hubp0 scanning a linear argb8888 surface, fetch schedule from the dml table
void PDAmdGPU::hubpSetup(const CeaMode &m, uint64_t surfMc)
{
    uint32_t size = m.hActive | (m.vActive << 16);

    wreg(dcnHUBP0_HUBP_CLK_CNTL, 0x00f00001);
    wreg(dcnHUBP0_DCSURF_SURFACE_CONFIG, 0x00000008);      // argb8888
    wreg(dcnHUBP0_DCSURF_ADDR_CONFIG, 0x00001002);
    wreg(dcnHUBP0_DCSURF_TILING_CONFIG, 0x00000080);       // linear
    wreg(dcnHUBP0_DCSURF_PRI_VIEWPORT_DIMENSION, size);
    wreg(dcnHUBP0_DCSURF_PRI_VIEWPORT_DIMENSION_C, size);
    wreg(dcnHUBPREQ0_DCSURF_SURFACE_PITCH, m.hActive - 1);
    wreg(dcnHUBPREQ0_DCN_VM_SYSTEM_APERTURE_LOW_ADDR, (uint32_t)(fbStart >> 18));
    wreg(dcnHUBPREQ0_DCN_VM_SYSTEM_APERTURE_HIGH_ADDR, (uint32_t)((fbStart + vramSize - 1) >> 18));
    wreg(dcnHUBPREQ0_DCN_VM_MX_L1_TLB_CNTL, 0x00000019);
    wreg(dcnHUBPRET0_HUBPRET_CONTROL, 0x00e40000);
    writeTable(this, kDmlFor1080p60, sizeof(kDmlFor1080p60) / sizeof(kDmlFor1080p60[0]));
    wreg(dcnHUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH, (uint32_t)(surfMc >> 32));
    wreg(dcnHUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS, (uint32_t)surfMc);
    wreg(dcnHUBP0_DCHUBP_CNTL, 0x000f0400);
}

// a test pattern through the vram bar: colour bars, a white border and a grey ramp along the bottom
void PDAmdGPU::fillTestPattern(const CeaMode &m, uint64_t vramOff)
{
    if (!vramMap) vramMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0, kIOMapWriteCombineCache);
    if (!vramMap || vramOff + (uint64_t)m.hActive * m.vActive * 4 > vramMap->getLength()) {
        LOG("disp: vram bar too small or unmapped for the framebuffer\n");
        return;
    }
    static const uint32_t bars[8] = { 0xffffffff, 0xffffff00, 0xff00ffff, 0xff00ff00, 0xffff00ff, 0xffff0000, 0xff0000ff, 0xff000000 };
    volatile uint32_t *fb = (volatile uint32_t *)(vramMap->getVirtualAddress() + vramOff);
    for (uint32_t y = 0; y < m.vActive; y++) {
        for (uint32_t x = 0; x < m.hActive; x++) {
            uint32_t c = bars[x * 8 / m.hActive];
            if (y >= m.vActive * 3 / 4) {
                uint32_t g = x * 255 / m.hActive;
                c = 0xff000000 | (g << 16) | (g << 8) | g;
            }
            if (x < 4 || y < 4 || x >= m.hActive - 4u || y >= m.vActive - 4u) c = 0xffffffff;
            fb[y * m.hActive + x] = c;
        }
    }
    flushHdp();
}

// same format as the reference vm's regdump.sh so the two diff directly
static void dumpDcn(PDAmdGPU *g)
{
    for (size_t i = 0; i < sizeof(kDcnDumpList) / sizeof(kDcnDumpList[0]); i++) {
        kprintf("DCNDUMP %05x %08x %s\n", kDcnDumpList[i].reg, g->rreg(kDcnDumpList[i].reg), kDcnDumpList[i].name);
    }
}

// the previous run leaves the stream up, so take it down in amdgpu's disable order before the phy pll is touched again
void PDAmdGPU::displayQuiesce(void)
{
    LOG("disp: before quiesce OTG_CONTROL 0x%08x DIG_BE_EN_CNTL 0x%08x DIG_FE_CNTL 0x%08x\n", rreg(dcnOTG0_OTG_CONTROL),
        rreg(dcnDIG0_DIG_BE_EN_CNTL), rreg(dcnDIG0_DIG_FE_CNTL));

    // amdgpu's commit of zero streams: black from the pattern generator, then the timing generator and its clocks off
    wreg(dcnDPG0_DPG_COLOUR_R_CR, 0);
    wreg(dcnDPG0_DPG_COLOUR_G_Y, 0);
    wreg(dcnDPG0_DPG_COLOUR_B_CB, 0);
    wreg(dcnDPG0_DPG_CONTROL, 0x41);
    wreg(dcnOTG0_OTG_CONTROL, rreg(dcnOTG0_OTG_CONTROL) & ~1u);
    IOSleep(20);
    wreg(dcnOTG0_OTG_CLOCK_CONTROL, 0x100);
    wreg(dcnODM0_OPTC_INPUT_CLOCK_CONTROL, 0x4);
    wreg(dcnDPPCLK_DTO_CTRL, 0);

    // link_set_dpms_off: av mute, the back end loses its front end, then the transmitter goes down
    wreg(dcnDIG0_HDMI_GC, rreg(dcnDIG0_HDMI_GC) | 1u);
    wreg(dcnDIG0_DIG_FE_CNTL, rreg(dcnDIG0_DIG_FE_CNTL) & ~0x01000000u);    // DIG_SYMCLK_FE_ON off
    wreg(dcnDIG0_DIG_BE_CNTL, rreg(dcnDIG0_DIG_BE_CNTL) & ~0x100u);
    // dcn10_init_hw disables a link only when its dig reports enabled
    bool digOn = rreg(dcnDIG0_DIG_BE_EN_CNTL) & 1;
    wreg(dcnDIG0_DIG_BE_EN_CNTL, 0);
    if (digOn && edp) dmubTransmitter(0, 0, 0, kEdpLanes, kEdpLinkRate10khz, 1, 0, 0x14);
    else if (digOn) hdmiLinkDown();
    else LOG("disp: dig already off, no transmitter disable\n");
    dmubNotifyStreamMask(0);

    // dcn20_program_front_end_for_ctx with no streams: det0 to no segments, then the plane's power domain gated again
    wreg(dcnHUBP0_DCHUBP_CNTL, rreg(dcnHUBP0_DCHUBP_CNTL) | 1u);     // HUBP_BLANK_EN
    wreg(dcnDCHUBBUB_DET0_CTRL, 0);
    pipePowerDown();
}

// the scanout surface: 16 MiB into vram, inside the cpu aperture
static const uint64_t fbOff = 0x1000000;

bool PDAmdGPU::scanout(uint64_t *phys, uint32_t *w, uint32_t *h, uint32_t *rowBytes)
{
    if (!displayUp || !vramMap) return false;
    *phys = vramMap->getPhysicalAddress() + fbOff;
    *w = k1080p60.hActive;
    *h = k1080p60.vActive;
    *rowBytes = k1080p60.hActive * 4;
    return true;
}

void PDAmdGPU::displayInit(void)
{
    const CeaMode &m = edp ? kEdp1080p60 : k1080p60;

    if (!pipePowerUp()) return;
    fillTestPattern(m, fbOff);
    writeTable(this, kAmdgpuSettings, sizeof(kAmdgpuSettings) / sizeof(kAmdgpuSettings[0]));
    if (edp) {
        // edpLinkUp already set the dp dto and the dig, only the dpp and otg clocks remain
        wreg(dcnDPPCLK0_DTO_PARAM, 0x00ff00ff);
        wreg(dcnDPPCLK_DTO_CTRL, 0x00000001);
        wreg(dcnOTG0_OTG_CLOCK_CONTROL, 0x00010103);
    } else {
        dccgSetup();
        digSetup();
    }
    otgSetup(m);
    if (edp) {
        wreg(dcnOTG0_OTG_VSTARTUP_PARAM, 0x0000001d);
        wreg(dcnOTG0_OTG_VUPDATE_PARAM, 0x00f7022e);
    }
    pipeSetup(m);
    hubpSetup(m, fbStart + fbOff);
    wreg(dcnOTG0_OTG_CONTROL, (rreg(dcnOTG0_OTG_CONTROL) & ~0x300u) | 0x200u | 1u);   // disable point 2, master enable
    if (edp) {
        wreg(dcnDP0_DP_VID_STREAM_CNTL, 0x00000201);          // stream on, disable deferred to the next blank
        IOSleep(20);
        LOG("dp: VID_STREAM_CNTL 0x%08x LINK_CNTL 0x%08x\n", rreg(dcnDP0_DP_VID_STREAM_CNTL), rreg(dcnDP0_DP_LINK_CNTL));
    }
    dmubNotifyStreamMask(1);
    IOSleep(100);
    LOG("disp: DPG_CONTROL 0x%08x DPG_STATUS 0x%08x, clearing the pattern generator\n", rreg(dcnDPG0_DPG_CONTROL), rreg(dcnDPG0_DPG_STATUS));
    wreg(dcnDPG0_DPG_CONTROL, rreg(dcnDPG0_DPG_CONTROL) & ~1u);
    IOSleep(100);
    LOG("disp: otg0 CONTROL 0x%08x STATUS 0x%08x frame count %u, DIG_FIFO_STATUS 0x%08x\n", rreg(dcnOTG0_OTG_CONTROL),
        rreg(dcnOTG0_OTG_STATUS), rreg(dcnOTG0_OTG_STATUS_FRAME_COUNT), rreg(dcnDIG0_DIG_FIFO_STATUS));
    if (edp) edpBacklight(true);

    // pdamdgpu_dcndump=1: every named dcn register and a few framebuffer words, to diff against amdgpu's dump
    int dump = 0;
    PE_parse_boot_argn("pdamdgpu_dcndump", &dump, sizeof(dump));
    if (dump) {
        dumpDcn(this);
        if (vramMap) {
            volatile uint32_t *fb = (volatile uint32_t *)(vramMap->getVirtualAddress() + fbOff);
            kprintf("DCNFB %08x %08x %08x %08x\n", fb[0], fb[10 * m.hActive + 10], fb[540 * m.hActive + 300], fb[900 * m.hActive + 1500]);
        }
    }
}
