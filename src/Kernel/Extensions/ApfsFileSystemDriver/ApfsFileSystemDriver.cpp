/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
#include "ApfsFileSystemDriver.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/OSByteOrder.h>

#define AFD_LOG(fmt, args...)  kprintf("ApfsFileSystemDriver: " fmt "\n", ## args)

// On-disk layout, all little-endian (APFS reference, "Container", "Object Maps", "B-Trees", "Volumes").
// Offsets are from the block start
#define APFS_NX_MAGIC          0x4253584EU        /* 'NXSB' */
#define APFS_APSB_MAGIC        0x42535041U        /* 'APSB' */
#define NX_MAGIC_OFF           32
#define NX_BLOCK_SIZE_OFF      36
#define NX_UUID_OFF            72
#define NX_OMAP_OID_OFF        160
#define NX_MAX_FS_OFF          180
#define NX_FS_OID_OFF          184
#define NX_MAX_FILE_SYSTEMS    100
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
#define APFS_INCOMPAT_CASE_INSENSITIVE 0x1
#define APFS_VOL_ROLE_SYSTEM   0x0001
#define APFS_VOL_ROLE_DATA     0x0040

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

    if (media == NULL || !super::start(provider))
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
    bool opened = false;

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
        if (blockSize != 4096) {
            nxbuf->release();
            nxbuf = NULL;
            if (readBlock(media, 0, blockSize, &nxbuf) != kIOReturnSuccess)
                break;
            nx = (const uint8_t *)nxbuf->getBytesNoCopy();
        }
        omapPaddr = rd64(nx + NX_OMAP_OID_OFF);
        if (readBlock(media, omapPaddr, blockSize, &ombuf) != kIOReturnSuccess)
            break;
        treePaddr = rd64((const uint8_t *)ombuf->getBytesNoCopy() + OM_TREE_OID_OFF);
        maxFs = rd32(nx + NX_MAX_FS_OFF);
        if (maxFs > NX_MAX_FILE_SYSTEMS)
            maxFs = NX_MAX_FILE_SYSTEMS;

        for (i = 0; i < maxFs; i++) {
            UInt64 oid = rd64(nx + NX_FS_OID_OFF + i * 8), vpaddr = 0;
            IOBufferMemoryDescriptor *vbuf = NULL;
            const uint8_t *vsb;
            AppleAPFSVolume *vol;

            if (oid == 0)
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
                    published++;
                }
            }
            vbuf->release();
        }
    } while (false);

    if (opened)
        media->close(this);
    if (ombuf != NULL)
        ombuf->release();
    if (nxbuf != NULL)
        nxbuf->release();
    if (published == 0) {
        AFD_LOG("%s: no volumes published", media->getName());
        super::stop(provider);
        return false;
    }
    return true;
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

// Selector 9, as libAPFS's APFSVolumeRoleFind calls it: in {uint32 index0, uint32}, out uint16 role.
// index0 is the volume's registry location minus one, i.e. its nx_fs_oid[] slot
#define kAPFSUCVolumeRole 9

IOReturn
AppleAPFSUserClient::externalMethod(uint32_t selector,
                                    IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch,
                                    OSObject *target, void *reference)
{
    if (_container == NULL || args == NULL)
        return kIOReturnNotAttached;

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
    AFD_LOG("user client selector %u unsupported", selector);
    return kIOReturnUnsupported;
}
