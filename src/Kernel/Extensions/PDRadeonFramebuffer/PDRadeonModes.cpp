#include "PDRadeonCard.h"

// vesa dmt timings for the fallback list
static const RadeonMode stdModes[] = {
    { 148500, 1920, 2008, 2052, 2200, 1080, 1084, 1089, 1125, false, false },
    { 108000, 1280, 1328, 1440, 1688, 1024, 1025, 1028, 1066, false, false },
    { 74250, 1280, 1390, 1430, 1650, 720, 725, 730, 750, false, false },
    { 65000, 1024, 1048, 1184, 1344, 768, 771, 777, 806, true, true },
    { 40000, 800, 840, 968, 1056, 600, 601, 605, 628, false, false },
    { 25175, 640, 656, 752, 800, 480, 490, 492, 525, true, true },
};

static bool fits(const RadeonMode &m)
{
    uint32_t pitch = (m.hdisp * 4 + 255) & ~255u;

    if (m.clock < 20000 || m.clock > 165000)
        return false;
    if (m.hdisp < 640 || m.hdisp > 1920 || m.vdisp < 480 || m.vdisp > 1200)
        return false;
    if (m.htotal <= m.hsyncEnd || m.vtotal <= m.vsyncEnd)
        return false;
    return (uint64_t)pitch * m.vdisp <= kRadeonHeadVram - 0x100000;
}

static bool duplicate(const RadeonMode *list, uint32_t n, const RadeonMode &m)
{
    for (uint32_t i = 0; i < n; i++)
        if (list[i].hdisp == m.hdisp && list[i].vdisp == m.vdisp) {
            uint32_t a = list[i].clock, b = m.clock;
            if ((a > b ? a - b : b - a) < 3000)
                return true;
        }
    return false;
}

// detailed descriptors from the edid first, then the standard timings that fit, preferred index returned
uint32_t radeonEdidModes(const uint8_t *edid, RadeonMode *modes, uint32_t max, int *preferred)
{
    uint32_t n = 0;

    *preferred = -1;

    if (edid) {
        for (int k = 0; k < 4; k++) {
            const uint8_t *d = edid + 54 + k * 18;
            uint32_t clock = (d[0] | (d[1] << 8)) * 10;

            if (!clock || (d[17] & 0x80))
                continue;

            RadeonMode m;
            uint16_t hact = d[2] | ((d[4] & 0xf0) << 4), hbl = d[3] | ((d[4] & 0x0f) << 8);
            uint16_t vact = d[5] | ((d[7] & 0xf0) << 4), vbl = d[6] | ((d[7] & 0x0f) << 8);
            uint16_t hso = d[8] | ((d[11] & 0xc0) << 2), hsw = d[9] | ((d[11] & 0x30) << 4);
            uint16_t vso = (d[10] >> 4) | ((d[11] & 0x0c) << 2), vsw = (d[10] & 0xf) | ((d[11] & 0x03) << 4);

            m.clock = clock;
            m.hdisp = hact;
            m.hsyncStart = hact + hso;
            m.hsyncEnd = m.hsyncStart + hsw;
            m.htotal = hact + hbl;
            m.vdisp = vact;
            m.vsyncStart = vact + vso;
            m.vsyncEnd = m.vsyncStart + vsw;
            m.vtotal = vact + vbl;
            m.nhsync = !(d[17] & 0x02);
            m.nvsync = !(d[17] & 0x04);

            if (!fits(m) || duplicate(modes, n, m) || n >= max)
                continue;
            if (*preferred < 0 && k == 0)
                *preferred = (int)n;
            modes[n++] = m;
        }
    }

    for (unsigned i = 0; i < sizeof(stdModes) / sizeof(stdModes[0]) && n < max; i++) {
        if (fits(stdModes[i]) && !duplicate(modes, n, stdModes[i]))
            modes[n++] = stdModes[i];
    }

    if (*preferred < 0) {
        *preferred = 0;
        for (uint32_t i = 0; i < n; i++)
            if (modes[i].hdisp == 1024 && modes[i].vdisp == 768)
                *preferred = (int)i;
    }

    return n;
}
