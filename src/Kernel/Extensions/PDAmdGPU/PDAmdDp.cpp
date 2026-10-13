#include "PDAmdGPU.h"
#include "AmdDcn315Regs.h"
#include "AmdDcnDpRegs.h"

#include <IOKit/IOLib.h>

// the internal panel on uniphy a: power sequencer, aux channel 0, 8b/10b link training, then the sst stream.
// register values that are not computed here are the ones amdgpu leaves on the 7520U's LEN156FHD panel

// aux engine fields (dcn_3_1_6_sh_mask.h)
enum {
    kAuxEn              = 1u << 0,
    kAuxReset           = 1u << 4,
    kAuxResetDone       = 1u << 5,
    kAuxSwGo            = 1u << 0,
    kAuxSwWrBytesShift  = 16,
    kAuxUseRegReq       = 1u << 16,
    kAuxDoneUsingReg    = 1u << 17,
    kAuxSwDoneAck       = 1u << 1,
    kAuxSwDone          = 1u << 0,
    kAuxSwDataRw        = 1u << 0,
    kAuxSwIndexShift    = 16,
    kAuxSwAutoIncDis    = 1u << 31,
};

// aux status bits that end a transaction as an error
#define kAuxErrMask     ((1u << 7) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 12) | (1u << 14) | (1u << 17) | \
                         (1u << 18) | (1u << 19) | (1u << 20) | (1u << 22) | (1u << 23))

// native aux commands
enum { kAuxNativeWrite = 0x8, kAuxNativeRead = 0x9 };

// one native aux transfer of up to 16 bytes, retried while the sink defers
bool PDAmdGPU::auxXfer(bool read, uint32_t addr, uint8_t *buf, uint8_t len)
{
    for (int attempt = 0; attempt < 7; attempt++) {
        // an engine coming up from reset is enabled and reset before software asks for it
        if (!(rreg(dcnDP_AUX0_AUX_CONTROL) & kAuxEn)) {
            wreg(dcnDP_AUX0_AUX_CONTROL, rreg(dcnDP_AUX0_AUX_CONTROL) | kAuxEn | kAuxReset);
            for (int r = 0; r < 11 && !(rreg(dcnDP_AUX0_AUX_CONTROL) & kAuxResetDone); r++) IODelay(1);
            wreg(dcnDP_AUX0_AUX_CONTROL, rreg(dcnDP_AUX0_AUX_CONTROL) & ~kAuxReset);
            for (int r = 0; r < 11 && (rreg(dcnDP_AUX0_AUX_CONTROL) & kAuxResetDone); r++) IODelay(1);
        }
        // the engine belongs to software while AUX_REG_RW_CNTL_STATUS reads 2
        wreg(dcnDP_AUX0_AUX_ARB_CONTROL, rreg(dcnDP_AUX0_AUX_ARB_CONTROL) | kAuxUseRegReq);
        int i = 0;
        for (; i < 1000 && ((rreg(dcnDP_AUX0_AUX_ARB_CONTROL) >> 2) & 3) != 2; i++) IODelay(10);
        if (i == 1000) {
            LOG("aux: engine not granted, ARB 0x%08x\n", rreg(dcnDP_AUX0_AUX_ARB_CONTROL));
            return false;
        }

        wreg(dcnDP_AUX0_AUX_INTERRUPT_CONTROL, rreg(dcnDP_AUX0_AUX_INTERRUPT_CONTROL) | kAuxSwDoneAck);
        for (i = 0; i < 100 && (rreg(dcnDP_AUX0_AUX_SW_STATUS) & kAuxSwDone); i++) IODelay(10);

        // header: command and address, then the length, then any data
        uint8_t req[20];
        uint32_t n = 0;
        req[n++] = (uint8_t)(((read ? kAuxNativeRead : kAuxNativeWrite) << 4) | ((addr >> 16) & 0xf));
        req[n++] = (uint8_t)(addr >> 8);
        req[n++] = (uint8_t)addr;
        req[n++] = (uint8_t)(len - 1);
        if (!read) {
            for (uint8_t k = 0; k < len; k++) req[n++] = buf[k];
        }
        wreg(dcnDP_AUX0_AUX_SW_CONTROL, (rreg(dcnDP_AUX0_AUX_SW_CONTROL) & ~(0x1fu << kAuxSwWrBytesShift)) |
             (n << kAuxSwWrBytesShift));
        wreg(dcnDP_AUX0_AUX_SW_DATA, kAuxSwAutoIncDis | ((uint32_t)req[0] << 8));
        for (uint32_t k = 1; k < n; k++) wreg(dcnDP_AUX0_AUX_SW_DATA, (uint32_t)req[k] << 8);
        wreg(dcnDP_AUX0_AUX_SW_CONTROL, rreg(dcnDP_AUX0_AUX_SW_CONTROL) | kAuxSwGo);

        uint32_t st = 0;
        for (i = 0; i < 1000; i++) {
            st = rreg(dcnDP_AUX0_AUX_SW_STATUS);
            if (st & kAuxSwDone) break;
            IODelay(10);
        }
        bool ok = (st & kAuxSwDone) && !(st & kAuxErrMask);
        uint8_t reply = 0xff, got = 0;
        if (ok) {
            uint32_t count = (st >> 24) & 0x1f;
            wreg(dcnDP_AUX0_AUX_SW_DATA, kAuxSwAutoIncDis | kAuxSwDataRw);
            reply = (uint8_t)(rreg(dcnDP_AUX0_AUX_SW_DATA) >> 8);
            for (uint32_t k = 1; k < count && got < len; k++) {
                uint8_t b = (uint8_t)(rreg(dcnDP_AUX0_AUX_SW_DATA) >> 8);
                if (read) buf[got] = b;
                got++;
            }
        }
        wreg(dcnDP_AUX0_AUX_ARB_CONTROL, rreg(dcnDP_AUX0_AUX_ARB_CONTROL) | kAuxDoneUsingReg);

        if (!ok) {
            LOG("aux: %s 0x%05x failed, status 0x%08x\n", read ? "read" : "write", addr, st);
            IODelay(400);
            continue;
        }
        // the reply's command nibble: 0 ack, 1 nack, 2 defer
        uint8_t rc = (reply >> 4) & 3;
        if (rc == 2) {
            IODelay(400);
            continue;
        }
        if (rc != 0) {
            LOG("aux: %s 0x%05x nacked, reply 0x%02x\n", read ? "read" : "write", addr, reply);
            return false;
        }
        if (read && got < len) {
            LOG("aux: read 0x%05x short, %u of %u\n", addr, got, len);
            return false;
        }
        return true;
    }
    return false;
}

bool PDAmdGPU::dpcdRead(uint32_t addr, uint8_t *buf, uint32_t len)
{
    for (uint32_t off = 0; off < len; off += 16) {
        uint8_t n = (uint8_t)((len - off) > 16 ? 16 : (len - off));
        if (!auxXfer(true, addr + off, buf + off, n)) return false;
    }
    return true;
}

bool PDAmdGPU::dpcdWrite(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    for (uint32_t off = 0; off < len; off += 16) {
        uint8_t n = (uint8_t)((len - off) > 16 ? 16 : (len - off));
        if (!auxXfer(false, addr + off, (uint8_t *)buf + off, n)) return false;
    }
    return true;
}

// the panel power sequencer brings vdd up and walks its own t1-t3 delays, then the panel raises hpd
bool PDAmdGPU::edpPowerOn(void)
{
    LOG("edp: before power on PWRSEQ_STATE 0x%08x\n", rreg(dcnPWRSEQ0_PANEL_PWRSEQ_STATE));
    bool ok = dmubLvtma(12);
    // eDP allows up to 200 ms from vdd to a usable aux channel
    IOSleep(250);
    LOG("edp: after power on PWRSEQ_STATE 0x%08x CNTL 0x%08x\n", rreg(dcnPWRSEQ0_PANEL_PWRSEQ_STATE),
        rreg(dcnPWRSEQ0_PANEL_PWRSEQ_CNTL));
    return ok;
}

void PDAmdGPU::edpBacklight(bool on)
{
    dmubLvtma(on ? 3 : 2);
    LOG("edp: backlight %s, BL_PWM_CNTL 0x%08x PWRSEQ_STATE 0x%08x\n", on ? "on" : "off", rreg(dcnPWRSEQ0_BL_PWM_CNTL),
        rreg(dcnPWRSEQ0_PANEL_PWRSEQ_STATE));
}

// source side of a training pattern: index 0 tp1, 1 tp2, with the link marked untrained and the phy out of bypass
void PDAmdGPU::dpSourcePattern(uint32_t index)
{
    wreg(dcnDP0_DP_DPHY_TRAINING_PATTERN_SEL, index);
    wreg(dcnDP0_DP_LINK_CNTL, rreg(dcnDP0_DP_LINK_CNTL) & ~(1u << 4));
    wreg(dcnDP0_DP_DPHY_CNTL, rreg(dcnDP0_DP_DPHY_CNTL) & ~(1u << 16));
    wreg(dcnDP0_DP_DPHY_PRBS_CNTL, rreg(dcnDP0_DP_DPHY_PRBS_CNTL) & ~1u);
}

// after training: edp panel mode with the alternate scrambler reset, framing restored, link marked trained
void PDAmdGPU::dpSourceVideo(void)
{
    wreg(dcnDP0_DP_DPHY_INTERNAL_CTRL, 0x1);
    wreg(dcnDP0_DP_LINK_FRAMING_CNTL, 0x10102000);
    wreg(dcnDP0_DP_DPHY_SCRAM_CNTL, (rreg(dcnDP0_DP_DPHY_SCRAM_CNTL) & ~(0x3ffu << 8)) | (0x1ffu << 8));
    wreg(dcnDP0_DP_LINK_CNTL, rreg(dcnDP0_DP_LINK_CNTL) | (1u << 4));
    wreg(dcnDP0_DP_DPHY_CNTL, rreg(dcnDP0_DP_DPHY_CNTL) & ~(1u << 16));
    wreg(dcnDP0_DP_DPHY_PRBS_CNTL, rreg(dcnDP0_DP_DPHY_PRBS_CNTL) & ~1u);
}

// voltage swing and pre-emphasis for both lanes, on the phy through the vbios table and on the sink
void PDAmdGPU::dpSetLanes(uint8_t laneSet, uint8_t lanes, uint8_t tp)
{
    const uint8_t phy = 0, hpd = 1, conn = 0x14;
    dmubTransmitter(phy, 11, laneSet, lanes, kEdpLinkRate10khz, hpd, 0, conn);
    uint8_t set[4] = { laneSet, laneSet, laneSet, laneSet };
    // max reached flags, so the sink does not ask for more than the source can give
    for (uint8_t l = 0; l < lanes; l++) {
        if ((set[l] & 3) == 3) set[l] |= 0x04;
        if (((set[l] >> 3) & 3) == 3) set[l] |= 0x20;
    }
    uint8_t buf[5] = { tp, set[0], set[1], set[2], set[3] };
    dpcdWrite(0x102, buf, 1 + lanes);
}

// the strongest adjustment either lane asks for, as one lane set
static uint8_t adjustRequest(const uint8_t *st, uint8_t lanes)
{
    uint8_t swing = 0, pre = 0;
    for (uint8_t l = 0; l < lanes; l++) {
        uint8_t a = (st[4 + l / 2] >> ((l & 1) * 4)) & 0xf;
        if ((a & 3) > swing) swing = a & 3;
        if (((a >> 2) & 3) > pre) pre = (a >> 2) & 3;
    }
    return (uint8_t)(swing | (pre << 3));
}

// clock recovery with tp1, then channel equalisation with tp2, at the link the panel and amdgpu agree on
bool PDAmdGPU::edpLinkTrain(uint8_t rate, uint8_t lanes)
{
    uint8_t cfg[2] = { rate, (uint8_t)(lanes | 0x80) };       // enhanced framing
    dpcdWrite(0x100, cfg, 2);
    uint8_t spread = 0x10, coding = 0x01, assr = 0x01;
    dpcdWrite(0x107, &spread, 1);
    dpcdWrite(0x108, &coding, 1);
    dpcdWrite(0x10a, &assr, 1);                                 // alternate scrambler seed reset, as amdgpu sets

    uint8_t laneSet = 0, st[6];
    bool cr = false;
    dpSourcePattern(0);
    for (int tries = 0; tries < 10 && !cr; tries++) {
        dpSetLanes(laneSet, lanes, 0x21);                       // tp1, scrambling off
        IODelay(500);
        if (!dpcdRead(0x202, st, 6)) return false;
        cr = true;
        for (uint8_t l = 0; l < lanes; l++) {
            if (!((st[l / 2] >> ((l & 1) * 4)) & 1)) cr = false;
        }
        LOG("dp: tp1 try %d lane set 0x%02x status %02x %02x align %02x adjust %02x %02x\n", tries, laneSet, st[0], st[1],
            st[2], st[4], st[5]);
        if (!cr) laneSet = adjustRequest(st, lanes);
    }
    if (!cr) {
        LOG("dp: clock recovery failed\n");
        return false;
    }

    bool eq = false;
    dpSourcePattern(1);
    for (int tries = 0; tries < 6 && !eq; tries++) {
        dpSetLanes(laneSet, lanes, 0x22);                       // tp2, scrambling off
        IODelay(1000);
        if (!dpcdRead(0x202, st, 6)) return false;
        eq = (st[2] & 1) != 0;                                  // interlane align done
        for (uint8_t l = 0; l < lanes; l++) {
            if (((st[l / 2] >> ((l & 1) * 4)) & 7) != 7) eq = false;
        }
        LOG("dp: tp2 try %d lane set 0x%02x status %02x %02x align %02x adjust %02x %02x\n", tries, laneSet, st[0], st[1],
            st[2], st[4], st[5]);
        if (!eq) laneSet = adjustRequest(st, lanes);
    }
    uint8_t off = 0;
    dpcdWrite(0x102, &off, 1);
    dpSourceVideo();
    LOG("dp: link %s at rate 0x%02x x%u lanes, lane set 0x%02x\n", eq ? "trained" : "not trained", rate, lanes, laneSet);
    return eq;
}

// the sst stream: dig in dp mode on front end 0, 2 lanes, m/n for 148.51 MHz over 270 MHz, msa from the panel timing
void PDAmdGPU::dpStreamSetup(void)
{
    wreg(dcnDIG0_DIG_BE_CNTL, 0x00000100);                      // back end in dp sst mode, fed from front end 0
    wreg(dcnDIG0_DIG_BE_EN_CNTL, 0x00000101);
    wreg(dcnDP0_DP_CONFIG, kEdpLanes - 1);
    wreg(dcnDP0_DP_PIXEL_FORMAT, 0x01000000);                   // rgb, 8 bpc
    wreg(dcnDP0_DP_MSA_COLORIMETRY, 0x20000000);
    wreg(dcnDP0_DP_VID_TIMING, 0x00000100);
    wreg(dcnDP0_DP_VID_N, 0x00008000);
    wreg(dcnDP0_DP_VID_M, 0x00004686);
    wreg(dcnDP0_DP_VID_MSA_VBID, 0x01000000);
    wreg(dcnDP0_DP_MSA_TIMING_PARAM1, 0x08b60456);              // h total 2230, v total 1110
    wreg(dcnDP0_DP_MSA_TIMING_PARAM2, 0x0106001b);              // h and v start
    wreg(dcnDP0_DP_MSA_TIMING_PARAM3, 0x00208006);              // sync widths
    wreg(dcnDP0_DP_MSA_TIMING_PARAM4, 0x07800438);              // 1920x1080
    wreg(dcnDP0_DP_DPHY_BS_SR_SWAP_CNTL, 0x00000005);
    wreg(dcnDP0_DP_SEC_CNTL6, 0x0fff0000);
    wreg(dcnDIG0_DIG_FE_CNTL, 0x01000100);                      // front end on, source otg0
}

// the eDP equivalent of hdmiLinkUp: panel power, the phy at the link rate, training
bool PDAmdGPU::edpLinkUp(void)
{
    const uint8_t phy = 0, dpMode = 0, conn = 0x14, hpd = 1, encoderId = 0x1e, pll = 20;

    edpPowerOn();
    uint8_t caps[16];
    if (!dpcdRead(0x000, caps, sizeof(caps))) {
        LOG("edp: no dpcd over aux\n");
        return false;
    }
    LOG("edp: dpcd rev %02x max rate %02x max lanes %02x, edp caps %02x\n", caps[0], caps[1], caps[2] & 0x1f, caps[0xd]);

    // dccg: otg0's pixel clock from dp dto 0 off the 600 MHz dprefclk, symclk a from the phy
    wreg(dcnDP_DTO0_PHASE, kEdpPixKhz * 1000);
    wreg(dcnDP_DTO0_MODULO, 600000000);
    wreg(dcnOTG0_PIXEL_RATE_CNTL, 0x80090090);
    wreg(dcnSYMCLKA_CLOCK_ENABLE, 0x00000001);

    dmubSetPixelClock(kEdpLinkRate10khz * 100, pll, encoderId, dpMode, 0);        // the phy pll at the link rate
    dpStreamSetup();
    dmubTransmitter(phy, 1, dpMode, kEdpLanes, kEdpLinkRate10khz, hpd, 0, conn);  // enable dp on the phy
    bool ok = edpLinkTrain(0x0a, kEdpLanes);
    return ok;
}
