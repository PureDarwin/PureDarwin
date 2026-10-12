#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

// dcn315_smu.c and dcn316_smu.c VBIOSSMC messages, the same numbers on both, frequencies in mhz
#define kDispMsgGetPmfwVersion      0x02
#define kDispMsgSetDispclk          0x04
#define kDispMsgSetDppclk           0x06
#define kDispMsgSetHardMinDcfclk    0x07
#define kDispMsgIdleOptimizations   0x12
#define kDispMsgGetDprefclk         0x13
#define kDispResultOk               0x01

int32_t PDAmdGPU::dispSmuSend(uint32_t msg, uint32_t arg)
{
    // the display side of the smu, separate from the driver mailbox, the registers come from the chip table
    uint32_t resp = soc15(kHwMP1, 0, chip->dispResp), param = soc15(kHwMP1, 0, chip->dispArg);
    uint32_t msgReg = chip->dispMsgAbsolute ? chip->dispMsg : soc15(kHwMP1, 0, chip->dispMsg);
    uint32_t r = 0;

    for (int i = 0; i < 20000 && (r = rreg(resp)) == 0; i++) IODelay(10);
    if (r == 0) {
        LOG("clk: display mailbox busy before msg 0x%x\n", msg);
        return -1;
    }
    wreg(resp, 0);
    wreg(param, arg);
    // granite ridge's message register sits outside the bar, written through the nbio window and read back,
    // as amdgpu retries it
    for (int i = 0; i < 5; i++) {
        wregAny(msgReg, msg);
        if (rregAny(msgReg) == msg) break;
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

    // mission mode, as dcn315_update_clocks asks before driving a display: the phy refclk stays on, any phy access hangs without it
    int32_t idle = dispSmuSend(kDispMsgIdleOptimizations, 0);
    LOG("clk: display idle optimizations off, answered %d\n", idle);

    int32_t dcf = dispSmuSend(kDispMsgSetHardMinDcfclk, 400);
    int32_t disp = dispSmuSend(kDispMsgSetDispclk, 600);
    int32_t dpp = dispSmuSend(kDispMsgSetDppclk, 600);
    LOG("clk: dcfclk min %d MHz, dispclk %d MHz, dppclk %d MHz\n", dcf, disp, dpp);
    return disp > 0 && dpp > 0;
}
