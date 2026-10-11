#include "PDAmdGPU.h"

#include <string.h>

// layout from the amdgpu discovery.h (MIT)
#define kBinarySignature 0x28211407
#define kIpDiscSignature 0x53445049

struct __attribute__((packed)) table_info { uint16_t offset, checksum, size, padding; };

struct __attribute__((packed)) binary_header {
    uint32_t signature;
    uint16_t major, minor, checksum, size;
    table_info tables[6];
};

struct __attribute__((packed)) die_info { uint16_t die_id, die_offset; };

struct __attribute__((packed)) ip_disc_header {
    uint32_t signature;
    uint16_t version, size;
    uint32_t id;
    uint16_t num_dies;
    die_info dies[16];
    uint8_t  flags;     // bit 0, v4 base addresses are 64-bit
};

struct __attribute__((packed)) die_header { uint16_t die_id, num_ips; };

struct __attribute__((packed)) ip_entry {
    uint16_t hw_id;
    uint8_t  instance, num_base, major, minor, revision, harvest;
};

static const struct { uint16_t id; const char *name; } kHwIds[] = {
    { 1, "MP1 (SMU)" }, { 3, "THM" }, { 4, "SMUIO" }, { 5, "FUSE" }, { 6, "CLKA" },
    { 10, "PWR" }, { 11, "GC" }, { 12, "VCN" }, { 13, "AUDIO" }, { 14, "ACP" },
    { 15, "DCI" }, { 16, "DCO" }, { 34, "MMHUB" }, { 35, "ATHUB" }, { 40, "OSSSYS" },
    { 41, "HDP" }, { 42, "SDMA0" }, { 43, "SDMA1" }, { 46, "DF" }, { 108, "NBIF" },
    { 150, "UMC" }, { 255, "MP0 (PSP)" }, { 271, "DMU (DCN)" }, { 272, "DIO" },
};

static const char *hwName(uint16_t id)
{
    for (auto &h : kHwIds)
        if (h.id == id) return h.name;
    return "?";
}

const AmdIp *AmdIpTable::find(uint16_t hw, uint8_t inst) const
{
    for (uint32_t i = 0; i < n; i++)
        if (ip[i].hw == hw && ip[i].inst == inst) return &ip[i];
    return nullptr;
}

bool amdParseDiscovery(const uint8_t *bin, uint32_t len, AmdIpTable *out)
{
    const binary_header *bh = (const binary_header *)bin;

    if (bh->signature != kBinarySignature) {
        LOG("discovery signature 0x%08x, expected 0x%08x\n", bh->signature, kBinarySignature);
        return false;
    }
    LOG("discovery binary v%u.%u size %u\n", bh->major, bh->minor, bh->size);

    uint16_t off = bh->tables[0].offset;
    if (off + sizeof(ip_disc_header) > len) return false;

    const ip_disc_header *ih = (const ip_disc_header *)(bin + off);
    if (ih->signature != kIpDiscSignature) {
        LOG("ip table signature 0x%08x\n", ih->signature);
        return false;
    }
    bool wide = ih->version >= 4 && (ih->flags & 1);
    LOG("ip table v%u, %u die(s)%s\n", ih->version, ih->num_dies, wide ? ", 64-bit bases" : "");

    for (uint16_t d = 0; d < ih->num_dies && d < 16; d++) {
        uint32_t p = ih->dies[d].die_offset;
        if (p + sizeof(die_header) > len) break;

        const die_header *dh = (const die_header *)(bin + p);
        p += sizeof(die_header);
        LOG("die %u: %u ips\n", dh->die_id, dh->num_ips);

        for (uint16_t i = 0; i < dh->num_ips && p + sizeof(ip_entry) <= len; i++) {
            const ip_entry *ip = (const ip_entry *)(bin + p);
            const uint8_t *base = bin + p + sizeof(ip_entry);
            uint32_t step = wide ? 8 : 4;

            if (out->n < 64) {
                AmdIp *e = &out->ip[out->n++];
                e->hw = ip->hw_id;
                e->inst = ip->instance;
                e->count = ip->num_base < 8 ? ip->num_base : 8;
                e->major = ip->major;
                e->minor = ip->minor;
                e->rev = ip->revision;
                for (uint8_t k = 0; k < e->count; k++)
                    memcpy(&e->base[k], base + k * step, 4);
            }
            LOG("ip %s hw%u.%u v%u.%u.%u\n", hwName(ip->hw_id), ip->hw_id, ip->instance, ip->major, ip->minor, ip->revision);
            p += sizeof(ip_entry) + ip->num_base * step;
        }
    }
    return out->n > 0;
}
