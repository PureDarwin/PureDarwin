#include "PDLifs.h"

#include <IOKit/IOLib.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

OSDefineMetaClassAndStructors(com_apple_filesystems_lifs, IOService);
OSDefineMetaClassAndStructors(AppleLIFSUserClient, IOUserClient);

// selectors as macOS 26.6.2's lifs dispatches them, see /hdd/pd/lifs-probe
enum {
	kLifsSetClientDomain = 1,
	kLifsCreateVolumePort = 29,
	kLifsConfigureUserClient = 43,
	kLifsSelectorCount = 45,
};

static const char *const gLifsSelectorNames[kLifsSelectorCount] = {
	"CreateMapping", "SetClientDomain", "GenericReply", "StatfsReply", "CreateReply", "MkdirReply",
	"LookupReply", "RenameReply", "RmdirReply", "ReaddirReply", "SymlinkReply", "LinkReply",
	"ReadlinkReply", "RemoveReply", "SetattrReply", "GetattrReply", "GetattrlistbulkReply", "WriteReply",
	"WriteWrappedReply", "ReadReply", "ReadWrappedReply", "PathconfReply", "SetFsAttrReply",
	"GetFsAttrReply", "XattrReply", "MountReply", "sel26", "OpenKernelFD", "CloseKernelFD",
	"CreateVolumePort", "GetVolumePortReply", "BlockmapFileReply", "WriteMeta", "WriteMetaAsync",
	"WriteMetaDelayed", "WriteMetaSubBlock", "ReadMeta", "ReadMetaWithRA", "FlushMeta", "FlushMetaBlocks",
	"ClearMetaBlocks", "PurgeMetaBlocks", "ReclaimReply", "ConfigureUserClient", "EndIOReply",
};

// vfs.generic.lifs.*: the real kext's tunables, with macOS 26's defaults
SYSCTL_DECL(_vfs_generic);
SYSCTL_NODE(_vfs_generic, OID_AUTO, lifs, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "lifs");

static int gLifsMaxIoThreads = 1, gLifsMaxInlineIo = 256 * 1024;
static int gLifsMaxRead = 8 << 20, gLifsMaxWrite = 2 << 20, gLifsMaxSsdRead = 8 << 20, gLifsMaxSsdWrite = 8 << 20;
static int gLifsMaxDevRead = 4 << 20, gLifsMaxDevWrite = 4 << 20;
static int gLifsMaxReadMap = 1 << 20, gLifsMaxWriteMap = 1 << 20, gLifsMaxSsdReadMap = 256 * 1024;
static int gLifsMaxSsdWriteMap = 256 * 1024, gLifsReadMetaHit, gLifsWriteMetaHit;

SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_io_threads, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxIoThreads, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_inline_io_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxInlineIo, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_read_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxRead, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_write_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxWrite, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_ssd_read_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxSsdRead, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_ssd_write_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxSsdWrite, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_dev_read_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxDevRead, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_dev_write_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxDevWrite, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_read_blockmap_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxReadMap, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_write_blockmap_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxWriteMap, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_ssd_read_blockmap_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxSsdReadMap, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, max_ssd_write_blockmap_size, CTLFLAG_RW | CTLFLAG_LOCKED, &gLifsMaxSsdWriteMap, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, read_meta_cache_hit, CTLFLAG_RD | CTLFLAG_LOCKED, &gLifsReadMetaHit, 0, "");
SYSCTL_INT(_vfs_generic_lifs, OID_AUTO, write_meta_cache_hit, CTLFLAG_RD | CTLFLAG_LOCKED, &gLifsWriteMetaHit, 0, "");

static struct sysctl_oid *const gLifsOids[] = {
	&sysctl__vfs_generic_lifs, &sysctl__vfs_generic_lifs_max_io_threads,
	&sysctl__vfs_generic_lifs_max_inline_io_size, &sysctl__vfs_generic_lifs_max_read_size,
	&sysctl__vfs_generic_lifs_max_write_size, &sysctl__vfs_generic_lifs_max_ssd_read_size,
	&sysctl__vfs_generic_lifs_max_ssd_write_size, &sysctl__vfs_generic_lifs_max_dev_read_size,
	&sysctl__vfs_generic_lifs_max_dev_write_size, &sysctl__vfs_generic_lifs_max_read_blockmap_size,
	&sysctl__vfs_generic_lifs_max_write_blockmap_size, &sysctl__vfs_generic_lifs_max_ssd_read_blockmap_size,
	&sysctl__vfs_generic_lifs_max_ssd_write_blockmap_size, &sysctl__vfs_generic_lifs_read_meta_cache_hit,
	&sysctl__vfs_generic_lifs_write_meta_cache_hit,
};

bool
com_apple_filesystems_lifs::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;

	for (size_t i = 0; i < sizeof(gLifsOids) / sizeof(gLifsOids[0]); i++)
		sysctl_register_oid(gLifsOids[i]);
	registerService();
	IOLog("PDLifs: started, no LiveFS VFS, FSKit mounts are refused\n");
	return true;
}

bool
AppleLIFSUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	if (!IOUserClient::initWithTask(owningTask, securityID, type))
		return false;

	fLock = IOLockAlloc();
	if (fLock == NULL)
		return false;

	fTask = owningTask;
	fPid = proc_selfpid();
	proc_selfname(fName, sizeof(fName));
	IOLog("PDLifs: open type %u by %s[%d]\n", (unsigned)type, fName, fPid);
	return true;
}

void
AppleLIFSUserClient::dropMainPort(void)
{
	mach_port_t port;

	IOLockLock(fLock);
	port = fMainPort;
	fMainPort = MACH_PORT_NULL;
	IOLockUnlock(fLock);

	if (port != MACH_PORT_NULL)
		releaseNotificationPort(port);
}

void
AppleLIFSUserClient::free(void)
{
	if (fLock != NULL) {
		dropMainPort();
		IOLockFree(fLock);
		fLock = NULL;
	}
	IOUserClient::free();
}

IOReturn
AppleLIFSUserClient::clientClose(void)
{
	IOLog("PDLifs: close by %s[%d]\n", fName, fPid);
	dropMainPort();
	terminate();
	return kIOReturnSuccess;
}

IOReturn
AppleLIFSUserClient::registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon)
{
	mach_port_t old;

	(void)refCon;
	IOLog("PDLifs: %s[%d] notification port type %u %s\n", fName, fPid, (unsigned)type,
	    port != MACH_PORT_NULL ? "set" : "cleared");
	// type 0 is fskitd's main port, only held since we never send LiveFS requests
	if (type != 0) {
		if (port != MACH_PORT_NULL)
			releaseNotificationPort(port);
		return kIOReturnBadArgument;
	}

	IOLockLock(fLock);
	old = fMainPort;
	fMainPort = port;
	IOLockUnlock(fLock);

	if (old != MACH_PORT_NULL)
		releaseNotificationPort(old);
	return kIOReturnSuccess;
}

IOReturn
AppleLIFSUserClient::createVolumePort(IOExternalMethodArguments *args)
{
	mach_port_name_t name = MACH_PORT_NULL;
	IOReturn ret;

	// the volume port is an IOKit identity send right on this client, in the caller's space
	if (args->structureOutput == NULL || args->structureOutputSize < sizeof(name))
		return kIOReturnBadArgument;

	ret = copyPortNameForObjectInTask(fTask, this, &name);
	if (ret != kIOReturnSuccess)
		return ret;

	memcpy(args->structureOutput, &name, sizeof(name));
	args->structureOutputSize = sizeof(name);
	IOLog("PDLifs: %s[%d] volume port 0x%x\n", fName, fPid, name);
	return kIOReturnSuccess;
}

IOReturn
AppleLIFSUserClient::configureUserClient(IOExternalMethodArguments *args)
{
	uint32_t in[4];
	OSObject *obj = NULL;

	if (args->structureInput == NULL || args->structureInputSize < sizeof(in))
		return kIOReturnBadArgument;

	// {port name of an extension's volume port, extension pid, ?, 1}
	memcpy(in, args->structureInput, sizeof(in));
	copyObjectForPortNameInTask(fTask, in[0], &obj);
	IOLog("PDLifs: %s[%d] ConfigureUserClient port 0x%x pid %u 0x%x %u -> %s, refused (no LiveFS)\n",
	    fName, fPid, in[0], in[1], in[2], in[3], OSDynamicCast(AppleLIFSUserClient, obj) ? "extension client" : "?");
	OSSafeReleaseNULL(obj);
	return kIOReturnUnsupported;
}

IOReturn
AppleLIFSUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	const char *what = selector < kLifsSelectorCount ? gLifsSelectorNames[selector] : "?";
	IOReturn ret;

	(void)dispatch; (void)target; (void)reference;
	switch (selector) {
	case kLifsSetClientDomain:
		ret = kIOReturnSuccess;
		break;
	case kLifsCreateVolumePort:
		ret = createVolumePort(args);
		break;
	case kLifsConfigureUserClient:
		ret = configureUserClient(args);
		break;
	default:
		// replies and metadata IO only follow a LiveFS mount, which we never start
		ret = kIOReturnUnsupported;
		break;
	}

	IOLog("PDLifs: %s[%d] sel %u %s scIn %u stIn %u scOut %u stOut %u -> 0x%x\n", fName, fPid, selector, what,
	    args->scalarInputCount, args->structureInputSize, args->scalarOutputCount, args->structureOutputSize, ret);
	return ret;
}
