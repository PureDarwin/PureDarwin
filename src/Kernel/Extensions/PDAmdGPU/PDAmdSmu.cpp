#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

// nbio 7.2 indirect window for registers past the end of the bar
#define regBIF_BX0_PCIE_INDEX2  0x000e
#define regBIF_BX0_PCIE_DATA2   0x000f

// smu 13.0.5 mailbox, from smu_v13_0_5_ppt.c (MIT)
#define mmMP1_C2PMSG_2          (0xbee142 + 0xb00000 / 4)
#define mmMP1_C2PMSG_33         (0xbee261 + 0xb00000 / 4)
#define mmMP1_C2PMSG_34         (0xbee262 + 0xb00000 / 4)

// smu_v13_0_5_ppsmc.h
#define kSmuResultOk            0x1
#define kSmuMsgGetSmuVersion    2
#define kSmuMsgGfxDriverReset   10
#define kSmuMsgDisallowGfxOff   23
#define kSmuResetMode2          2

uint32_t PDAmdGPU::rregAny(uint32_t idx)
{
    if (idx < regCount) return rreg(idx);

    uint32_t index = soc15(kHwNBIF, 0, regBIF_BX0_PCIE_INDEX2);
    uint32_t data = soc15(kHwNBIF, 0, regBIF_BX0_PCIE_DATA2);
    wreg(index, idx * 4);
    (void)rreg(index);
    return rreg(data);
}

void PDAmdGPU::wregAny(uint32_t idx, uint32_t v)
{
    if (idx < regCount) {
        wreg(idx, v);
        return;
    }

    uint32_t index = soc15(kHwNBIF, 0, regBIF_BX0_PCIE_INDEX2);
    uint32_t data = soc15(kHwNBIF, 0, regBIF_BX0_PCIE_DATA2);
    wreg(index, idx * 4);
    (void)rreg(index);
    wreg(data, v);
    (void)rreg(data);
}

// smu_cmn_send_smc_msg_with_param: wait for the last reply, clear it, post the argument and the message, wait again
bool PDAmdGPU::smuSend(uint32_t msg, uint32_t arg, uint32_t *out)
{
    uint32_t msgReg = soc15(kHwMP1, 0, mmMP1_C2PMSG_2);
    uint32_t respReg = soc15(kHwMP1, 0, mmMP1_C2PMSG_33);
    uint32_t argReg = soc15(kHwMP1, 0, mmMP1_C2PMSG_34);
    uint32_t resp = 0;

    for (int i = 0; i < 1000 && (resp = rregAny(respReg)) == 0; i++)
        IODelay(1000);
    if (resp == 0) LOG("smu: mailbox busy before msg %u\n", msg);

    wregAny(respReg, 0);
    wregAny(argReg, arg);
    wregAny(msgReg, msg);

    resp = 0;
    for (int i = 0; i < 1000 && (resp = rregAny(respReg)) == 0; i++)
        IODelay(1000);
    if (out) *out = rregAny(argReg);
    if (resp != kSmuResultOk) LOG("smu: msg %u arg 0x%x answered 0x%x\n", msg, arg, resp);
    return resp == kSmuResultOk;
}

// gfxoff powers the gfx block down when idle, so keep it up while the rings come up
// before the psp has brought the firmware up the smu only answers queries, gfxoff is disallowed after autoload,
// the order amdgpu follows with its smu block after the psp block, the mode 2 reset now lives in the teardown
void PDAmdGPU::smuInit(void)
{
    uint32_t ver = 0;

    if (smuSend(kSmuMsgGetSmuVersion, 0, &ver))
        LOG("smu: firmware %u.%u.%u\n", (ver >> 16) & 0xff, (ver >> 8) & 0xff, ver & 0xff);
}

void PDAmdGPU::smuDisallowGfxOff(const char *when)
{
    bool ok = smuSend(kSmuMsgDisallowGfxOff, 0, nullptr);
    LOG("smu: gfxoff %s %s, response reg 0x%x\n", when, ok ? "disallowed" : "still allowed",
        rregAny(soc15(kHwMP1, 0, mmMP1_C2PMSG_33)));
}

void PDAmdGPU::smuMode2Reset(const char *when)
{
    bool ok = smuSend(kSmuMsgGfxDriverReset, kSmuResetMode2, nullptr);
    LOG("smu: %s mode 2 reset %s\n", when, ok ? "done" : "failed");
}
