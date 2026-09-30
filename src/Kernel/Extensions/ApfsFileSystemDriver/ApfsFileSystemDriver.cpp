/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
#include <IOKit/IOBSD.h>
#include "ApfsFileSystemDriver.h"

extern "C" {
#include <apfsrw/apfsrw.h>
}

// libapfsrw's transfers on the container media itself, which this driver opens as writer.
// The BSD node would take IOMediaBSDClient's open lock from inside a user client call
struct AFDMediaIO {
    IOService *client;
    IOMedia *media;
};

static int
afdMediaIO(void *ref, void *buf, size_t n, uint64_t off, int is_write)
{
    AFDMediaIO *io = (AFDMediaIO *)ref;
    IOMemoryDescriptor *md;
    IOReturn ret;

    md = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)buf, n,
        is_write ? kIODirectionOut : kIODirectionIn, kernel_task);
    if (md == NULL)
        return -1;
    if (md->prepare() != kIOReturnSuccess) {
        md->release();
        return -1;
    }
    ret = is_write ? io->media->write(io->client, off, md) : io->media->read(io->client, off, md);
    md->complete();
    md->release();
    return ret == kIOReturnSuccess ? 0 : -1;
}

static int
afdMediaSync(void *ref)
{
    AFDMediaIO *io = (AFDMediaIO *)ref;

    return io->media->synchronize(io->client, 0, 0) == kIOReturnSuccess ? 0 : -1;
}

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/OSByteOrder.h>

#define AFD_LOG(fmt, args...)  kprintf("ApfsFileSystemDriver: " fmt "\n", ## args)
// DEBUG-STEP: volume add progress on the console
#define AFD_STEP(fmt, args...) IOLog("ApfsFileSystemDriver: " fmt "\n", ## args)

// On-disk layout, all little-endian (APFS reference, "Container", "Object Maps", "B-Trees", "Volumes").
// Offsets are from the block start
#define APFS_NX_MAGIC          0x4253584EU        /* 'NXSB' */
#define APFS_APSB_MAGIC        0x42535041U        /* 'APSB' */
#define NX_MAGIC_OFF           32
#define NX_BLOCK_SIZE_OFF      36
#define NX_UUID_OFF            72
#define NX_OMAP_OID_OFF        160
#define NX_BLOCK_COUNT_OFF     40
#define NX_MAX_FS_OFF          180
#define NX_FS_OID_OFF          184
#define NX_MAX_FILE_SYSTEMS    100
#define NX_XID_OFF             16
#define NX_XP_DESC_BLOCKS_OFF  104        /* high bit: the area is not contiguous */
#define NX_XP_DESC_BASE_OFF    112
#define OM_TREE_OID_OFF        48                 /* omap_phys_t.om_tree_oid */
#define BTN_FLAGS_OFF          32                 /* btree_node_phys_t */
#define BTN_LEVEL_OFF          34
#define BTN_NKEYS_OFF          36
#define BTN_TABLE_OFF          40                 /* nloc_t {off, len} */
#define BTN_DATA_OFF           56
#define BTNODE_ROOT            0x0001
#define BTNODE_LEAF            0x0002
#define BTNODE_FIXED_KV_SIZE   0x0004
#define BTREE_INFO_SIZE        40
#define APSB_MAGIC_OFF         32
#define APSB_INCOMPAT_OFF      56
#define APSB_VOL_UUID_OFF      240
#define APSB_VOLNAME_OFF       704                /* apfs_volname[256] */
#define APSB_ROLE_OFF          964                /* apfs_role (uint16) */
#define APSB_RESERVE_OFF       72                 /* apfs_fs_reserve_block_count */
#define APSB_QUOTA_OFF         80                 /* apfs_fs_quota_block_count */
#define APSB_ALLOC_OFF         88                 /* apfs_fs_alloc_count */
#define APFS_INCOMPAT_CASE_INSENSITIVE 0x1
#define APFS_VOL_ROLE_SYSTEM   0x0001
#define APFS_VOL_ROLE_DATA     0x0040
#define APFS_VOL_ROLE_PREBOOT  0x0010

#define kAPFSContainerGUID     "EF57347C-0000-11AA-AA11-00306543ECAC"
#define kAPFSVolumeGUID        "41504653-0000-11AA-AA11-00306543ECAC"

static inline uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return OSSwapLittleToHostInt16(v); }
static inline uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return OSSwapLittleToHostInt32(v); }
static inline uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return OSSwapLittleToHostInt64(v); }

static const char *
roleName(uint16_t role)
{
    switch (role) {
    case 0x0001: return "System";
    case 0x0002: return "User";
    case 0x0004: return "Recovery";
    case 0x0008: return "VM";
    case 0x0010: return "Preboot";
    case 0x0020: return "Installer";
    case 0x0040: return "Data";
    case 0x0080: return "Baseband";
    case 0x00C0: return "Update";
    case 0x0100: return "xART";
    case 0x0140: return "Hardware";
    case 0x0180: return "Backup";
    case 0x0240: return "Enterprise";
    case 0x02C0: return "Prelogin";
    default:     return NULL;
    }
}

OSDefineMetaClassAndStructors(AppleAPFSMedia, IOMedia)
OSDefineMetaClassAndStructors(AppleAPFSVolume, IOMedia)

// Container scheme

#undef  super
#define super IOPartitionScheme
OSDefineMetaClassAndStructors(AppleAPFSContainerScheme, IOPartitionScheme)

bool
AppleAPFSContainerScheme::start(IOService *provider)
{
    IOMedia *media = OSDynamicCast(IOMedia, provider);
    IOBufferMemoryDescriptor *buf = NULL;
    const uint8_t *nx;
    uuid_string_t uuidStr;
    bool ok = false;

    // whole media is matched too (a disk newfs_apfs just formatted has no content hint),
    // but never our own container media
    if (media == NULL || OSDynamicCast(AppleAPFSMedia, provider) != NULL || !super::start(provider))
        return false;
    _media = NULL;

    do {
        UInt64 bs = media->getPreferredBlockSize();
        UInt64 n = bs ? ((4096 + bs - 1) / bs) * bs : 4096;

        buf = IOBufferMemoryDescriptor::withCapacity(n, kIODirectionIn);
        if (buf == NULL)
            break;
        if (!media->open(this, 0, kIOStorageAccessReader))
            break;
        if (media->read(this, 0, buf) != kIOReturnSuccess) {
            media->close(this);
            break;
        }
        media->close(this);
        nx = (const uint8_t *)buf->getBytesNoCopy();
        if (rd32(nx + NX_MAGIC_OFF) != APFS_NX_MAGIC) {
            AFD_LOG("%s: no NXSB magic", media->getName());
            break;
        }
        uuid_unparse_upper(nx + NX_UUID_OFF, uuidStr);

        _media = new AppleAPFSMedia;
        if (_media == NULL)
            break;
        if (!_media->init(0, media->getSize(), media->getPreferredBlockSize(),
                          media->getAttributes(), true, media->isWritable(),
                          kAPFSContainerGUID)) {
            _media->release();
            _media = NULL;
            break;
        }
        _media->setName("AppleAPFSMedia");
        _media->setProperty(kIOMediaUUIDKey, uuidStr);
        _media->setProperty(kIOMediaContentHintKey, kAPFSContainerGUID);
        if (!_media->attach(this)) {
            _media->release();
            _media = NULL;
            break;
        }
        _media->registerService();
        AFD_LOG("container %s on %s", uuidStr, media->getName());
        ok = true;
    } while (false);

    if (buf != NULL)
        buf->release();
    if (!ok)
        super::stop(provider);
    return ok;
}

void
AppleAPFSContainerScheme::stop(IOService *provider)
{
    if (_media != NULL) {
        _media->terminate();
        _media->release();
        _media = NULL;
    }
    super::stop(provider);
}

void
AppleAPFSContainerScheme::free()
{
    if (_media != NULL)
        _media->release();
    super::free();
}

// CONTAINER (volumes)

#undef  super
#define super IOPartitionScheme
OSDefineMetaClassAndStructors(AppleAPFSContainer, IOPartitionScheme)

IOReturn
AppleAPFSContainer::readBlock(IOMedia *media, UInt64 block, UInt32 blockSize,
                              IOBufferMemoryDescriptor **out)
{
    IOBufferMemoryDescriptor *buf =
        IOBufferMemoryDescriptor::withCapacity(blockSize, kIODirectionIn);
    IOReturn ret;

    if (buf == NULL)
        return kIOReturnNoMemory;
    ret = media->read(this, block * blockSize, buf);
    if (ret != kIOReturnSuccess) {
        buf->release();
        return ret;
    }
    *out = buf;
    return kIOReturnSuccess;
}

// Fletcher-64 over the object after its checksum field, as every apfs object carries
static uint64_t
apfsChecksum(const uint8_t *obj, uint32_t size)
{
    uint64_t s1 = 0, s2 = 0, c1, c2;

    for (uint32_t i = 8; i + 4 <= size; i += 4) {
        s1 = (s1 + rd32(obj + i)) % 0xffffffffULL;
        s2 = (s2 + s1) % 0xffffffffULL;
    }
    c1 = 0xffffffffULL - ((s1 + s2) % 0xffffffffULL);
    c2 = 0xffffffffULL - ((s1 + c1) % 0xffffffffULL);
    return (c2 << 32) | c1;
}

// Block 0 holds the superblock as of the last clean unmount. The newest valid one is in the
// checkpoint descriptor area, where a container copied while mounted has moved past block 0
IOReturn
AppleAPFSContainer::readLatestSuperblock(IOMedia *media, UInt32 blockSize,
                                         IOBufferMemoryDescriptor **out)
{
    IOBufferMemoryDescriptor *best = NULL, *cand = NULL;
    const uint8_t *nx;
    uint64_t base, bestXid;
    uint32_t count;
    IOReturn ret = readBlock(media, 0, blockSize, &best);

    if (ret != kIOReturnSuccess)
        return ret;
    nx = (const uint8_t *)best->getBytesNoCopy();
    bestXid = rd64(nx + NX_XID_OFF);
    count = rd32(nx + NX_XP_DESC_BLOCKS_OFF);
    base = rd64(nx + NX_XP_DESC_BASE_OFF);
    // a non-contiguous area is described by a b-tree; keep block 0 then
    for (uint32_t i = 0; !(count & 0x80000000U) && i < count && i < 1024; i++) {
        if (readBlock(media, base + i, blockSize, &cand) != kIOReturnSuccess)
            break;
        nx = (const uint8_t *)cand->getBytesNoCopy();
        if (rd32(nx + NX_MAGIC_OFF) == APFS_NX_MAGIC && rd64(nx) == apfsChecksum(nx, blockSize) &&
            rd64(nx + NX_XID_OFF) > bestXid) {
            bestXid = rd64(nx + NX_XID_OFF);
            best->release();
            best = cand;
        } else {
            cand->release();
        }
        cand = NULL;
    }
    *out = best;
    return kIOReturnSuccess;
}

// Container object map lookup in a physical B-tree keyed by oid and xid, newest xid wins.
// Nodes are a 56 byte header, the table of contents, keys after it, values from the end
IOReturn
AppleAPFSContainer::omapLookup(IOMedia *media, UInt32 blockSize,
                               UInt64 treePaddr, UInt64 oid, UInt64 *paddrOut)
{
    UInt64 cur = treePaddr;
    unsigned depth;

    for (depth = 0; depth < 16; depth++) {
        IOBufferMemoryDescriptor *buf = NULL;
        const uint8_t *node;
        uint16_t flags;
        uint32_t nkeys, toc, keys, vend, i;
        int fixed;
        UInt64 next = 0, bestPaddr = 0, bestXid = 0;
        int found = 0;
        IOReturn ret = readBlock(media, cur, blockSize, &buf);

        if (ret != kIOReturnSuccess)
            return ret;
        node = (const uint8_t *)buf->getBytesNoCopy();
        flags = rd16(node + BTN_FLAGS_OFF);
        nkeys = rd32(node + BTN_NKEYS_OFF);
        toc = BTN_DATA_OFF + rd16(node + BTN_TABLE_OFF);
        keys = toc + rd16(node + BTN_TABLE_OFF + 2);
        vend = blockSize - ((flags & BTNODE_ROOT) ? BTREE_INFO_SIZE : 0);
        fixed = (flags & BTNODE_FIXED_KV_SIZE) != 0;

        for (i = 0; i < nkeys; i++) {
            const uint8_t *key, *val;
            uint16_t koff, voff;
            uint64_t koid, kxid;

            // a stale or foreign node can claim more entries than the block holds
            if (toc + (i + 1) * (fixed ? 4 : 8) > blockSize)
                break;
            if (fixed) {
                koff = rd16(node + toc + i * 4);
                voff = rd16(node + toc + i * 4 + 2);
            } else {
                koff = rd16(node + toc + i * 8);
                voff = rd16(node + toc + i * 8 + 4);
            }
            if (keys + koff + 16 > blockSize || voff > vend)
                break;
            key = node + keys + koff;
            val = node + vend - voff;
            koid = rd64(key);
            kxid = rd64(key + 8);
            if (flags & BTNODE_LEAF) {
                if (koid == oid && kxid >= bestXid) {
                    bestXid = kxid;
                    bestPaddr = rd64(val + 8);
                    found = 1;
                }
            } else if (i == 0 || koid < oid || (koid == oid)) {
                // Last child whose first key does not exceed (oid, max)
                next = rd64(val);
            }
        }
        buf->release();
        if (flags & BTNODE_LEAF) {
            if (!found)
                return kIOReturnNotFound;
            *paddrOut = bestPaddr;
            return kIOReturnSuccess;
        }
        if (next == 0)
            return kIOReturnNotFound;
        cur = next;
    }
    return kIOReturnNotFound;
}

AppleAPFSVolume *
AppleAPFSContainer::publishVolume(IOMedia *media, UInt32 blockSize,
                                  const uint8_t *nx, const uint8_t *vsb,
                                  UInt32 index)
{
    AppleAPFSVolume *vol = new AppleAPFSVolume;
    uuid_string_t volUuid, groupUuid;
    char name[256], location[12];
    uint16_t role = rd16(vsb + APSB_ROLE_OFF);
    uint64_t incompat = rd64(vsb + APSB_INCOMPAT_OFF);
    const char *rname = roleName(role);
    OSArray *roles;
    OSString *s;

    if (vol == NULL)
        return NULL;
    if (!vol->init(0, media->getSize(), blockSize, media->getAttributes(),
                   false, media->isWritable(), kAPFSVolumeGUID)) {
        vol->release();
        return NULL;
    }
    strlcpy(name, (const char *)vsb + APSB_VOLNAME_OFF, sizeof(name));
    if (name[0] == '\0')
        snprintf(name, sizeof(name), "Untitled %u", index);
    snprintf(location, sizeof(location), "%u", index);
    vol->setName(name);
    vol->setLocation(location);
    uuid_unparse_upper(vsb + APSB_VOL_UUID_OFF, volUuid);
    // System and Data volumes form the volume group. Its id is what the loader hands over as
    // apfs-preboot-uuid, which is the container uuid here (nothing else is derivable from block zero)
    if (role == APFS_VOL_ROLE_SYSTEM || role == APFS_VOL_ROLE_DATA)
        uuid_unparse_upper(nx + NX_UUID_OFF, groupUuid);
    else
        strlcpy(groupUuid, "00000000-0000-0000-0000-000000000000", sizeof(groupUuid));

    vol->setProperty(kIOMediaUUIDKey, volUuid);
    vol->setProperty(kIOMediaContentHintKey, kAPFSVolumeGUID);
    // block counts for the space report, as of mount
    vol->setProperty("PDAllocBlocks", rd64(vsb + APSB_ALLOC_OFF), 64);
    vol->setProperty("PDReserveBlocks", rd64(vsb + APSB_RESERVE_OFF), 64);
    vol->setProperty("PDQuotaBlocks", rd64(vsb + APSB_QUOTA_OFF), 64);
    vol->setProperty("PDRole", rd16(vsb + APSB_ROLE_OFF), 16);
    vol->setProperty("Partition ID", index, 32);
    vol->setProperty("FullName", name);
    vol->setProperty("VolGroupUUID", groupUuid);
    vol->setProperty("RoleValue", role, 16);
    roles = OSArray::withCapacity(1);
    if (roles != NULL) {
        s = OSString::withCString(rname ? rname : "None");
        if (s != NULL) {
            roles->setObject(s);
            s->release();
        }
        vol->setProperty("Role", roles);
        roles->release();
    }
    vol->setProperty("Status", "Online");
    vol->setProperty("Sealed", "No");
    vol->setProperty("CaseSensitive",
                     (incompat & APFS_INCOMPAT_CASE_INSENSITIVE) == 0);
    vol->setProperty("IncompatibleFeatures", incompat, 64);
    vol->setProperty("Encrypted", false);
    if (role == APFS_VOL_ROLE_SYSTEM)
        vol->setProperty("VolBootable", true);

    if (!vol->attach(this)) {
        vol->release();
        return NULL;
    }
    // apfs_boot_util finds each container's preboot volume, the iSCPreboot one included, by this
    if (role == APFS_VOL_ROLE_PREBOOT) {
        char path[sizeof(name) + 16];
        snprintf(path, sizeof(path), "%s@%s", name, location);
        s = OSString::withCString(path);
        roles = s != NULL ? OSArray::withObjects((const OSObject **)&s, 1) : NULL;
        if (roles != NULL)
            setProperty("IOAPFSPreBootDevice", roles);
        OSSafeReleaseNULL(roles);
        OSSafeReleaseNULL(s);
    }
    vol->registerService();
    AFD_LOG("volume %u '%s' uuid %s role %s(0x%x)", index, name, volUuid,
            rname ? rname : "None", role);
    return vol;
}

bool
AppleAPFSContainer::start(IOService *provider)
{
    IOMedia *media = OSDynamicCast(IOMedia, provider);
    IOBufferMemoryDescriptor *nxbuf = NULL, *ombuf = NULL;
    const uint8_t *nx;
    UInt32 blockSize, maxFs, i, published = 0;
    UInt64 omapPaddr, treePaddr;
    bool opened = false, nxValid = false;
    uuid_string_t uuidStr;

    if (media == NULL || !super::start(provider))
        return false;
    _volumes = OSSet::withCapacity(4);
    if (_volumes == NULL)
        return false;
    setProperty("IOUserClientClass", "AppleAPFSUserClient");

    do {
        if (!media->open(this, 0, kIOStorageAccessReader))
            break;
        opened = true;
        if (readBlock(media, 0, 4096, &nxbuf) != kIOReturnSuccess)
            break;
        nx = (const uint8_t *)nxbuf->getBytesNoCopy();
        if (rd32(nx + NX_MAGIC_OFF) != APFS_NX_MAGIC)
            break;
        blockSize = rd32(nx + NX_BLOCK_SIZE_OFF);
        if (blockSize < 4096 || blockSize > 65536)
            break;
        nxbuf->release();
        nxbuf = NULL;
        if (readLatestSuperblock(media, blockSize, &nxbuf) != kIOReturnSuccess)
            break;
        nx = (const uint8_t *)nxbuf->getBytesNoCopy();
        omapPaddr = rd64(nx + NX_OMAP_OID_OFF);
        if (readBlock(media, omapPaddr, blockSize, &ombuf) != kIOReturnSuccess)
            break;
        treePaddr = rd64((const uint8_t *)ombuf->getBytesNoCopy() + OM_TREE_OID_OFF);
        nxValid = true;
        uuid_unparse_upper(nx + NX_UUID_OFF, uuidStr);
        setProperty("UUID", uuidStr);
        setProperty("ContainerBlockSize", blockSize, 32);
        maxFs = rd32(nx + NX_MAX_FS_OFF);
        _blockSize = blockSize;
        _blockCount = rd64(nx + NX_BLOCK_COUNT_OFF);
        _maxFs = maxFs;
        if (maxFs > NX_MAX_FILE_SYSTEMS)
            maxFs = NX_MAX_FILE_SYSTEMS;

        published = scanVolumes(media, nx, blockSize, treePaddr, maxFs);
    } while (false);

    if (opened)
        media->close(this);
    if (ombuf != NULL)
        ombuf->release();
    if (nxbuf != NULL)
        nxbuf->release();
    // a container newfs_apfs just made has no volumes yet, and stays online for them to be added
    if (!nxValid) {
        AFD_LOG("%s: unreadable container", media->getName());
        super::stop(provider);
        return false;
    }
    if (published == 0)
        AFD_LOG("%s: empty container", media->getName());
    setProperty("Status", "Online");
    registerService();
    return true;
}

// Publishes every volume in nx_fs_oid[] not already published, so a rescan only adds the new ones
UInt32
AppleAPFSContainer::scanVolumes(IOMedia *media, const uint8_t *nx, UInt32 blockSize,
                                UInt64 treePaddr, UInt32 maxFs)
{
    UInt32 i, published = 0;

    for (i = 0; i < maxFs; i++) {
        UInt64 oid = rd64(nx + NX_FS_OID_OFF + i * 8), vpaddr = 0;
        IOBufferMemoryDescriptor *vbuf = NULL;
        const uint8_t *vsb;
        AppleAPFSVolume *vol;

        if (oid == 0 || (_slotMask[i / 64] & (1ULL << (i % 64))) != 0)
            continue;
        if (omapLookup(media, blockSize, treePaddr, oid, &vpaddr) !=
            kIOReturnSuccess) {
            AFD_LOG("volume oid 0x%llx: omap lookup failed", oid);
            continue;
        }
        if (readBlock(media, vpaddr, blockSize, &vbuf) != kIOReturnSuccess)
            continue;
        vsb = (const uint8_t *)vbuf->getBytesNoCopy();
        if (rd32(vsb + APSB_MAGIC_OFF) == APFS_APSB_MAGIC) {
            vol = publishVolume(media, blockSize, nx, vsb, i + 1);
            if (vol != NULL) {
                _volumes->setObject(vol);
                vol->release();
                _slotMask[i / 64] |= 1ULL << (i % 64);
                published++;
            }
        }
        vbuf->release();
    }
    return published;
}

// After a volume is added: block zero mirrors the newest superblock, so read the object map from it again
void
AppleAPFSContainer::rescanVolumes()
{
    IOMedia *media = OSDynamicCast(IOMedia, getProvider());
    IOBufferMemoryDescriptor *nxbuf = NULL, *ombuf = NULL;
    const uint8_t *nx;
    UInt32 maxFs;

    if (media == NULL || !media->open(this, 0, kIOStorageAccessReader))
        return;
    if (readLatestSuperblock(media, _blockSize, &nxbuf) == kIOReturnSuccess) {
        nx = (const uint8_t *)nxbuf->getBytesNoCopy();
        if (rd32(nx + NX_MAGIC_OFF) == APFS_NX_MAGIC &&
            readBlock(media, rd64(nx + NX_OMAP_OID_OFF), _blockSize, &ombuf) == kIOReturnSuccess) {
            maxFs = rd32(nx + NX_MAX_FS_OFF);
            if (maxFs > NX_MAX_FILE_SYSTEMS)
                maxFs = NX_MAX_FILE_SYSTEMS;
            scanVolumes(media, nx, _blockSize,
                        rd64((const uint8_t *)ombuf->getBytesNoCopy() + OM_TREE_OID_OFF), maxFs);
        }
    }
    media->close(this);
    OSSafeReleaseNULL(ombuf);
    OSSafeReleaseNULL(nxbuf);
}

// Adds a volume through libapfsrw on the container's own device node, as newfs/diskutil do on macOS
IOReturn
AppleAPFSContainer::createVolume(const char *name, uint16_t role, const uint8_t uuid[16],
                                 uint32_t *slot)
{
    IOMedia *media = OSDynamicCast(IOMedia, getProvider());
    struct apfsrw_kern_dev dev;
    struct apfsrw *fs = NULL;
    AFDMediaIO io;
    int err;

    if (media == NULL)
        return kIOReturnNotReady;
    // a second libapfsrw instance would commit behind the mounted volumes' back and
    // leave them on a stale container: refuse while any volume is open
    if (_volumes != NULL) {
        OSIterator *it = OSCollectionIterator::withCollection(_volumes);
        IOMedia *vol;
        bool busy = false;

        while (it != NULL && (vol = OSDynamicCast(IOMedia, it->getNextObject())) != NULL) {
            if (vol->isOpen())
                busy = true;
        }
        OSSafeReleaseNULL(it);
        if (busy) {
            AFD_LOG("add volume '%s': container has open volumes", name);
            return kIOReturnExclusiveAccess;
        }
    }
    AFD_STEP("add '%s': open media", name);
    if (!media->open(this, 0, kIOStorageAccessReaderWriter)) {
        AFD_LOG("add volume '%s': media busy", name);
        return kIOReturnExclusiveAccess;
    }
    io.client = this;
    io.media = media;
    memset(&dev, 0, sizeof(dev));
    dev.block_size = _blockSize;
    dev.dev_bsize = _blockSize;
    dev.io = afdMediaIO;
    dev.sync = afdMediaSync;
    dev.io_ref = &io;
    err = apfsrw_open_kernel(&dev, _blockCount, 1, 0, APFSRW_SLOT_CONTAINER, &fs);
    AFD_STEP("open container %d", err);
    if (err == APFSRW_OK) {
        err = apfsrw_create_volume(fs, name, role, uuid, slot);
        AFD_STEP("create %d", err);
        apfsrw_close(fs);
    }
    media->close(this);
    if (err != APFSRW_OK) {
        AFD_LOG("add volume '%s': %s", name, apfsrw_strerror(err));
        return kIOReturnError;
    }
    AFD_LOG("added volume '%s' role 0x%x in slot %u", name, role, *slot);
    rescanVolumes();
    return kIOReturnSuccess;
}

void
AppleAPFSContainer::stop(IOService *provider)
{
    if (_volumes != NULL) {
        OSIterator *it = OSCollectionIterator::withCollection(_volumes);

        if (it != NULL) {
            IOMedia *vol;

            while ((vol = (IOMedia *)it->getNextObject()) != NULL)
                vol->terminate();
            it->release();
        }
        _volumes->flushCollection();
    }
    super::stop(provider);
}

void
AppleAPFSContainer::free()
{
    if (_volumes != NULL)
        _volumes->release();
    super::free();
}

IOReturn
AppleAPFSContainer::volumeRole(UInt32 index, UInt16 *role)
{
    OSIterator *it = _volumes ? OSCollectionIterator::withCollection(_volumes) : NULL;
    IOReturn ret = kIOReturnNotFound;
    IOMedia *vol;

    if (it == NULL)
        return kIOReturnNoMemory;
    while ((vol = (IOMedia *)it->getNextObject()) != NULL) {
        OSNumber *id = OSDynamicCast(OSNumber, vol->getProperty("Partition ID"));
        OSNumber *rv = OSDynamicCast(OSNumber, vol->getProperty("RoleValue"));

        if (id != NULL && rv != NULL && id->unsigned32BitValue() == index) {
            *role = rv->unsigned16BitValue();
            ret = kIOReturnSuccess;
            break;
        }
    }
    it->release();
    return ret;
}

// User client

#undef  super
#define super IOUserClient
OSDefineMetaClassAndStructors(AppleAPFSUserClient, IOUserClient)

bool
AppleAPFSUserClient::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    _container = OSDynamicCast(AppleAPFSContainer, provider);
    return _container != NULL;
}

IOReturn
AppleAPFSUserClient::clientClose()
{
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

static void
setNumber(OSDictionary *dict, const char *key, UInt64 value, UInt32 bits)
{
    OSNumber *n = OSNumber::withNumber(value, bits);

    if (n != NULL) {
        dict->setObject(key, n);
        n->release();
    }
}

static UInt64
numberProperty(IOService *s, const char *key)
{
    OSNumber *n = OSDynamicCast(OSNumber, s->getProperty(key));

    return n != NULL ? n->unsigned64BitValue() : 0;
}

OSDictionary *
AppleAPFSContainerCopySpaceInfo(AppleAPFSContainer *container)
{
    OSDictionary *out = OSDictionary::withCapacity(8);
    OSDictionary *cd = OSDictionary::withCapacity(8);
    OSString *cname = OSDynamicCast(OSString, container->getProvider() ?
        container->getProvider()->getProperty(kIOBSDNameKey) : NULL);
    UInt64 bs = container->spaceBlockSize(), size = container->spaceBlockCount() * bs, used = 0;
    OSIterator *it = container->copyVolumeIterator();
    UInt32 count = 0;
    AppleAPFSVolume *vol;

    if (out == NULL || cd == NULL || cname == NULL) {
        OSSafeReleaseNULL(out);
        OSSafeReleaseNULL(cd);
        OSSafeReleaseNULL(it);
        return NULL;
    }
    if (it != NULL) {
        while ((vol = OSDynamicCast(AppleAPFSVolume, it->getNextObject())) != NULL)
            used += numberProperty(vol, "PDAllocBlocks") * bs;
        it->reset();
        while ((vol = OSDynamicCast(AppleAPFSVolume, it->getNextObject())) != NULL) {
            OSString *vname = OSDynamicCast(OSString, vol->getProperty(kIOBSDNameKey));
            OSDictionary *vd;

            count++;
            if (vname == NULL || (vd = OSDictionary::withCapacity(6)) == NULL)
                continue;
            setNumber(vd, "fs_size", size, 64);
            setNumber(vd, "fs_reserve", numberProperty(vol, "PDReserveBlocks") * bs, 64);
            setNumber(vd, "fs_quota", numberProperty(vol, "PDQuotaBlocks") * bs, 64);
            setNumber(vd, "fs_free", size - used, 64);
            setNumber(vd, "fs_used", numberProperty(vol, "PDAllocBlocks") * bs, 64);
            setNumber(vd, "Role", numberProperty(vol, "PDRole"), 16);
            out->setObject(vname, vd);
            vd->release();
        }
        it->release();
    }
    setNumber(cd, "blksize", bs, 64);
    setNumber(cd, "size", size, 64);
    setNumber(cd, "reserve", 0, 64);
    setNumber(cd, "free", size - used, 64);
    setNumber(cd, "used", used, 64);
    setNumber(cd, "Volume count", count, 32);
    setNumber(cd, "Max volume count", container->spaceMaxVolumes(), 32);
    out->setObject(cname, cd);
    cd->release();
    return out;
}

// Selector 0, volume add: in 504 bytes (uuid at 0, role at 0x36, name at 0x38), out the new slot
#define kAPFSUCVolumeAdd 0
#define kAPFSUCVolumeAddRoleOff 0x36
#define kAPFSUCVolumeAddNameOff 0x38

// Selector 8, APFSExtendedSpaceInfo: no input, a serialized dictionary out (variable size)
#define kAPFSUCSpaceInfo 8

// Selector 9, as libAPFS's APFSVolumeRoleFind calls it: in {uint32 index0, uint32}, out uint16 role.
// index0 is the volume's registry location minus one, i.e. its nx_fs_oid[] slot
#define kAPFSUCVolumeRole 9
// shapes read back from a real mac: an unencrypted volume reports its crypto operation 100% done,
// volume stats are version 1 counters, container stats are version 5 with block counts first
#define kAPFSUCVolumeCrypto 18
#define kAPFSUCContainerStats 29
#define kAPFSUCVolumeStats 30
#define kAPFSUCVolumeFlag55 55

IOReturn
AppleAPFSUserClient::externalMethod(uint32_t selector,
                                    IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch,
                                    OSObject *target, void *reference)
{
    if (_container == NULL || args == NULL)
        return kIOReturnNotAttached;

    if (selector != kAPFSUCVolumeRole)
        AFD_STEP("selector %u in %u out %u", selector, args->structureInputSize, args->structureOutputSize);
    if (selector == kAPFSUCVolumeAdd) {
        const uint8_t *in = (const uint8_t *)args->structureInput;
        char name[256];
        uint16_t role;
        uint32_t slot = 0, n;
        IOReturn ret;

        if (in == NULL || args->structureInputSize <= kAPFSUCVolumeAddNameOff ||
            args->structureOutput == NULL || args->structureOutputSize < sizeof(slot))
            return kIOReturnBadArgument;
        n = args->structureInputSize - kAPFSUCVolumeAddNameOff;
        if (n > sizeof(name) - 1)
            n = sizeof(name) - 1;
        memcpy(name, in + kAPFSUCVolumeAddNameOff, n);
        name[n] = 0;
        memcpy(&role, in + kAPFSUCVolumeAddRoleOff, sizeof(role));
        ret = _container->createVolume(name, role, in, &slot);
        if (ret != kIOReturnSuccess)
            return ret;
        memcpy(args->structureOutput, &slot, sizeof(slot));
        args->structureOutputSize = sizeof(slot);
        return kIOReturnSuccess;
    }
    if (selector == kAPFSUCSpaceInfo) {
        OSDictionary *info = AppleAPFSContainerCopySpaceInfo(_container);
        OSSerialize *s;

        if (info == NULL)
            return kIOReturnNoMemory;
        s = OSSerialize::withCapacity(4096);
        if (s == NULL || !info->serialize(s)) {
            OSSafeReleaseNULL(s);
            info->release();
            return kIOReturnNoMemory;
        }
        info->release();
        // IOKit copies the serialization out and releases it
        if (args->structureVariableOutputData == NULL) {
            s->release();
            return kIOReturnBadArgument;
        }
        *args->structureVariableOutputData = s;
        return kIOReturnSuccess;
    }
    if (selector == kAPFSUCVolumeRole) {
        uint32_t index0;
        uint16_t role = 0;
        IOReturn ret;

        if (args->structureInput == NULL || args->structureInputSize < 4 ||
            args->structureOutput == NULL ||
            args->structureOutputSize < sizeof(role))
            return kIOReturnBadArgument;
        memcpy(&index0, args->structureInput, sizeof(index0));
        ret = _container->volumeRole(index0 + 1, &role);
        if (ret != kIOReturnSuccess)
            return ret;
        memcpy(args->structureOutput, &role, sizeof(role));
        args->structureOutputSize = sizeof(role);
        return kIOReturnSuccess;
    }
    if (selector == kAPFSUCVolumeCrypto || selector == kAPFSUCVolumeStats ||
        selector == kAPFSUCVolumeFlag55) {
        static const uint64_t crypto[4] = { 0, 100, 0, 1 };
        static const uint64_t stats[8] = { 1, 0, 0, 0, 1, 1, 1, 0 };
        static const uint64_t flag = 0;
        const void *src = selector == kAPFSUCVolumeCrypto ? (const void *)crypto :
            selector == kAPFSUCVolumeStats ? (const void *)stats : (const void *)&flag;
        uint32_t n = selector == kAPFSUCVolumeCrypto ? sizeof(crypto) :
            selector == kAPFSUCVolumeStats ? sizeof(stats) : sizeof(flag);
        uint32_t index0;
        uint16_t role;

        if (args->structureInput == NULL || args->structureInputSize < 4 ||
            args->structureOutput == NULL || args->structureOutputSize < n)
            return kIOReturnBadArgument;
        memcpy(&index0, args->structureInput, sizeof(index0));
        if (_container->volumeRole(index0 + 1, &role) != kIOReturnSuccess)
            return kIOReturnNotFound;
        memcpy(args->structureOutput, src, n);
        args->structureOutputSize = n;
        return kIOReturnSuccess;
    }
    if (selector == kAPFSUCContainerStats) {
        uint64_t stats[35] = { 5 };
        UInt64 used = 0;
        OSIterator *it = _container->copyVolumeIterator();
        AppleAPFSVolume *vol;

        if (args->structureOutput == NULL || args->structureOutputSize < sizeof(stats))
            return kIOReturnBadArgument;
        while (it != NULL && (vol = OSDynamicCast(AppleAPFSVolume, it->getNextObject())) != NULL)
            used += numberProperty(vol, "PDAllocBlocks");
        OSSafeReleaseNULL(it);
        stats[1] = _container->spaceBlockCount();
        stats[2] = stats[1] > used ? stats[1] - used : 0;
        stats[4] = _container->spaceBlockSize();
        memcpy(args->structureOutput, stats, sizeof(stats));
        args->structureOutputSize = sizeof(stats);
        return kIOReturnSuccess;
    }
    AFD_LOG("user client selector %u unsupported", selector);
    return kIOReturnUnsupported;
}
