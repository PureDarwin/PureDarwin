#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

// the display side of the smu, the mailbox dcn315_smu.c uses, separate from the driver mailbox
#define regMP1_SMN_C2PMSG_37        0x0265      // mp1 segment 0, argument and returned value
#define regMP1_SMN_C2PMSG_38        0x0266      // mp1 segment 0, response, 0 while busy
#define kDispMsgIndexByte           0x3b1050c   // mp1 c2pmsg 3, past the bar, reached through nbio

// dcn315_smu.c VBIOSSMC messages, frequencies in mhz
#define kDispMsgGetPmfwVersion      0x02
#define kDispMsgSetDispclk          0x04
#define kDispMsgSetDppclk           0x06
#define kDispMsgSetHardMinDcfclk    0x07
#define kDispMsgGetDprefclk         0x13
#define kDispResultOk               0x01

int32_t PDAmdGPU::dispSmuSend(uint32_t msg, uint32_t arg)
{
    uint32_t resp = soc15(kHwMP1, 0, regMP1_SMN_C2PMSG_38), param = soc15(kHwMP1, 0, regMP1_SMN_C2PMSG_37);
    uint32_t r = 0;

    for (int i = 0; i < 20000 && (r = rreg(resp)) == 0; i++) IODelay(10);
    if (r == 0) {
        LOG("clk: display mailbox busy before msg 0x%x\n", msg);
        return -1;
    }
    wreg(resp, 0);
    wreg(param, arg);
    // the message register sits outside the bar, written through the nbio window and read back, as amdgpu retries it
    for (int i = 0; i < 5; i++) {
        wregAny(kDispMsgIndexByte / 4, msg);
        if (rregAny(kDispMsgIndexByte / 4) == msg) break;
        IODelay(2);
    }
    for (int i = 0; i < 200000 && (r = rreg(resp)) == 0; i++) IODelay(10);
    if (r != kDispResultOk) {
        LOG("clk: display msg 0x%x arg %u answered 0x%x\n", msg, arg, r);
        return -1;
    }
    return (int32_t)rreg(param);
}

// dcn315_update_clocks for one 1080p60 hdmi stream, fixed clocks instead of the dml calculation
bool PDAmdGPU::clkInit(void)
{
    int32_t ver = dispSmuSend(kDispMsgGetPmfwVersion, 0);
    int32_t dpref = dispSmuSend(kDispMsgGetDprefclk, 0);
    LOG("clk: display pmfw 0x%x, dprefclk %d MHz\n", ver, dpref);

    int32_t dcf = dispSmuSend(kDispMsgSetHardMinDcfclk, 400);
    int32_t disp = dispSmuSend(kDispMsgSetDispclk, 600);
    int32_t dpp = dispSmuSend(kDispMsgSetDppclk, 600);
    LOG("clk: dcfclk min %d MHz, dispclk %d MHz, dppclk %d MHz\n", dcf, disp, dpp);
    return disp > 0 && dpp > 0;
}
