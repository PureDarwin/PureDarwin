#include "PDPowerVRUserClient.h"
#include "PDPowerVR.h"

#include <IOKit/IOLib.h>

#include "pd_pvr_uapi.h"
#include "pd_display.h"
#include "pvr_gem.h"
extern "C" {
#include "pvr_sync.h"
#include "pvr_queue.h"
}

#define super IOUserClient
OSDefineMetaClassAndStructors(PDPowerVRUserClient, IOUserClient);

// the largest ioctl argument struct, for the on-stack copy
#define kMaxArgs	256

static IOReturn
pd_errno_return(int err)
{
	return err == 0 ? kIOReturnSuccess : (IOReturn)PD_PVR_ERR_FROM_ERRNO(err < 0 ? -err : err);
}

bool
PDPowerVRUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type)
{
	if (!super::initWithTask(owningTask, securityToken, type))
		return false;
	fTask = owningTask;
	return true;
}

bool
PDPowerVRUserClient::start(IOService *provider)
{
	fGPU = OSDynamicCast(PDPowerVR, provider);
	if (fGPU == NULL || fGPU->device() == NULL || !super::start(provider))
		return false;
	fFile = pvr_file_open(fGPU->device());
	return fFile != NULL;
}

IOReturn
PDPowerVRUserClient::clientClose(void)
{
	if (fScanout != NULL)
		flip(0, 0, 0, NULL, NULL);
	if (fFile != NULL) {
		pvr_gem_exports_release_file(fFile);
		pvr_file_close(fFile);
		fFile = NULL;
	}
	terminate();
	return kIOReturnSuccess;
}

void
PDPowerVRUserClient::free(void)
{
	if (fScanout != NULL)
		flip(0, 0, 0, NULL, NULL);
	if (fFile != NULL) {
		pvr_gem_exports_release_file(fFile);
		pvr_file_close(fFile);
		fFile = NULL;
	}
	super::free();
}

IOReturn
PDPowerVRUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *, OSObject *, void *)
{
	struct pvr_device *dev = fGPU->device();
	uint8_t buf[kMaxArgs];
	int err;

	if (fFile == NULL)
		return kIOReturnNotOpen;

	if (selector >= PD_PVR_SEL_IOCTL_FIRST && selector <= PD_PVR_SEL_IOCTL_LAST) {
		u32 size = pvr_ioctl_args_size(selector);

		if (size == 0 || size > sizeof(buf) || args->structureInput == NULL ||
		    args->structureInputSize != size || args->structureOutputSize < size)
			return kIOReturnBadArgument;
		memcpy(buf, args->structureInput, size);
		// what the task wrote through its maps reaches memory before the gpu can read it
		if (selector == DRM_IOCTL_PVR_SUBMIT_JOBS)
			pvr_gem_file_cpu_flush(fFile);
		err = pvr_ioctl(dev, fFile, selector, buf, size);
		if (err != 0 && selector == DRM_IOCTL_PVR_SUBMIT_JOBS)
			IOLog("PDPowerVR: submit ioctl failed %d\n", err);
		if (err == 0) {
			memcpy(args->structureOutput, buf, size);
			args->structureOutputSize = size;
		}
		return pd_errno_return(err);
	}

	if (selector >= PD_PVR_SEL_DUMB_CREATE && selector <= PD_PVR_SEL_FLIP)
		return scanoutMethod(selector, args);

	switch (selector) {
	case PD_PVR_SEL_GEM_CLOSE:
		if (args->scalarInputCount != 1)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_gem_object_close_handle(fFile, (u32)args->scalarInput[0]));

	case PD_PVR_SEL_SYNCOBJ_CREATE: {
		u32 handle = 0;

		if (args->scalarInputCount != 1 || args->scalarOutputCount != 1)
			return kIOReturnBadArgument;
		err = pvr_syncobj_create(fFile, (u32)args->scalarInput[0] & PVR_SYNCOBJ_CREATE_SIGNALED,
		    &handle);
		args->scalarOutput[0] = handle;
		return pd_errno_return(err);
	}

	case PD_PVR_SEL_SYNCOBJ_DESTROY:
		if (args->scalarInputCount != 1)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_syncobj_destroy(fFile, (u32)args->scalarInput[0]));

	case PD_PVR_SEL_SYNCOBJ_RESET:
		if (args->scalarInputCount != 1)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_syncobj_reset(fFile, (u32)args->scalarInput[0]));

	case PD_PVR_SEL_SYNCOBJ_SIGNAL:
		if (args->scalarInputCount != 1)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_syncobj_signal(fFile, (u32)args->scalarInput[0]));

	case PD_PVR_SEL_SYNCOBJ_TRANSFER:
		if (args->scalarInputCount != 3)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_syncobj_transfer(fFile, (u32)args->scalarInput[0],
		    (u32)args->scalarInput[1], args->scalarInput[2] != 0));

	case PD_PVR_SEL_SYNCOBJ_WAIT: {
		struct pd_pvr_syncobj_wait w;
		u32 *handles;

		if (args->structureInputSize != sizeof(w) || args->structureOutputSize < sizeof(w))
			return kIOReturnBadArgument;
		memcpy(&w, args->structureInput, sizeof(w));
		if (w.count == 0 || w.count > 1024)
			return kIOReturnBadArgument;
		handles = (u32 *)IOMalloc(w.count * sizeof(u32));
		if (handles == NULL)
			return kIOReturnNoMemory;
		if (copyin((user_addr_t)w.handles, handles, w.count * sizeof(u32)) != 0) {
			IOFree(handles, w.count * sizeof(u32));
			return pd_errno_return(-EFAULT);
		}
		err = pvr_syncobj_wait(dev, fFile, handles, w.count,
		    (w.flags & PD_PVR_SYNCOBJ_WAIT_ALL) ? PVR_SYNCOBJ_WAIT_ALL : 0, w.timeout_us,
		    &w.first_signaled);
		IOFree(handles, w.count * sizeof(u32));
		// and what the gpu wrote is not hidden behind stale lines once the wait returns
		if (err == 0)
			pvr_gem_file_cpu_flush(fFile);
		memcpy(args->structureOutput, &w, sizeof(w));
		args->structureOutputSize = sizeof(w);
		return pd_errno_return(err);
	}
	}
	return kIOReturnUnsupported;
}

// IOConnectMapMemory64 with a buffer handle as the type maps that buffer write-combined
IOReturn
PDPowerVRUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory)
{
	struct pvr_gem_object *obj;
	IOMemoryDescriptor *md;

	if (fFile == NULL)
		return kIOReturnNotOpen;
	obj = pvr_gem_object_from_handle(fFile, type);
	if (obj == NULL)
		return kIOReturnBadArgument;
	if (!(obj->flags & DRM_PVR_BO_ALLOW_CPU_USERSPACE_ACCESS)) {
		pvr_gem_object_put(obj);
		return kIOReturnNotPermitted;
	}
	md = (IOMemoryDescriptor *)obj->phys_md;
	md->retain();
	*memory = md;
	*options = (*options & ~kIOMapCacheMask) |
	    ((obj->flags & PVR_BO_PD_CPU_CACHED) ? kIOMapCopybackCache : kIOMapWriteCombineCache);
	// the mapping retains the descriptor, pvr_gem keeps the pages until that retain is gone
	obj->user_mapped = true;
	pvr_gem_object_put(obj);
	return kIOReturnSuccess;
}

static IOService *
display_service(void)
{
	static IOService *display;

	if (display == NULL) {
		OSDictionary *match = IOService::serviceMatching(PD_DISPLAY_CLASS);
		IOService *d = match != NULL ? IOService::copyMatchingService(match) : NULL;

		OSSafeReleaseNULL(match);
		// one display for the system's lifetime, the reference is kept
		if (d != NULL && !OSCompareAndSwapPtr(NULL, d, &display))
			d->release();
	}
	return display;
}

static IOReturn
display_call(const char *fn, void *p1, void *p2, void *p3)
{
	IOService *d = display_service();
	const OSSymbol *sym;
	IOReturn ret;

	if (d == NULL)
		return kIOReturnNotAttached;
	sym = OSSymbol::withCString(fn);
	if (sym == NULL)
		return kIOReturnNoMemory;
	ret = d->callPlatformFunction(sym, true, p1, p2, p3, NULL);
	sym->release();
	return ret;
}

// handle 0 gives the display back to the console, the previous buffer is released once replaced
IOReturn
PDPowerVRUserClient::flip(uint32_t handle, uint64_t offset, uint32_t pitch, uint64_t *seq, uint64_t *ns)
{
	struct pd_display_info info;
	struct pd_display_flip done = { 0, 0 };
	struct pvr_gem_object *obj = NULL, *old;
	IOReturn ret;
	u64 pa = 0;

	if (handle != 0) {
		if (display_call(PD_DISPLAY_FN_INFO, &info, NULL, NULL) != kIOReturnSuccess)
			return pd_errno_return(-ENODEV);
		obj = pvr_gem_object_from_handle(fFile, handle);
		if (obj == NULL)
			return pd_errno_return(-ENOENT);
		pa = pvr_gem_contiguous_addr(obj);
		if (pa == 0 || pa + offset + (u64)pitch * info.height > (1ULL << 32) ||
		    offset + (u64)pitch * info.height > obj->size || pitch < info.width * 4) {
			pvr_gem_object_put(obj);
			return pd_errno_return(-EINVAL);
		}
		// the gpu work already queued is what this frame holds
		pvr_queue_wait_submitted(fGPU->device(), 1000000);
		ret = display_call(PD_DISPLAY_FN_FLIP, (void *)(uintptr_t)(pa + offset), (void *)(uintptr_t)pitch, &done);
	} else {
		ret = display_call(PD_DISPLAY_FN_RESTORE, NULL, NULL, NULL);
	}
	if (ret != kIOReturnSuccess && ret != kIOReturnTimeout) {
		pvr_gem_object_put(obj);
		return ret;
	}
	old = fScanout;
	fScanout = obj;
	pvr_gem_object_put(old);
	if (seq != NULL)
		*seq = done.sequence;
	if (ns != NULL)
		*ns = done.time_ns;
	return kIOReturnSuccess;
}

IOReturn
PDPowerVRUserClient::scanoutMethod(uint32_t selector, IOExternalMethodArguments *args)
{
	struct pvr_gem_object *obj;
	u32 handle = 0;
	u64 token = 0;
	int err;

	switch (selector) {
	case PD_PVR_SEL_DUMB_CREATE: {
		u64 size;

		if (args->scalarInputCount < 1 || args->scalarInputCount > 2 || args->scalarOutputCount != 2)
			return kIOReturnBadArgument;
		size = args->scalarInput[0];
		if (size == 0 || size > (256ULL << 20))
			return pd_errno_return(-EINVAL);
		obj = pvr_gem_object_create(fGPU->device(), size,
		    DRM_PVR_BO_ALLOW_CPU_USERSPACE_ACCESS | PVR_BO_PD_CONTIGUOUS |
		    (args->scalarInputCount > 1 && (args->scalarInput[1] & PD_PVR_DUMB_CPU_CACHED) ?
		    PVR_BO_PD_CPU_CACHED : 0));
		if (IS_ERR(obj))
			return pd_errno_return(PTR_ERR(obj));
		err = pvr_gem_object_into_handle(obj, fFile, &handle);
		if (err) {
			pvr_gem_object_put(obj);
			return pd_errno_return(err);
		}
		args->scalarOutput[0] = handle;
		args->scalarOutput[1] = pvr_gem_contiguous_addr(obj) != 0;
		return kIOReturnSuccess;
	}

	case PD_PVR_SEL_PRIME_EXPORT:
		if (args->scalarInputCount != 1 || args->scalarOutputCount != 2)
			return kIOReturnBadArgument;
		obj = pvr_gem_object_from_handle(fFile, (u32)args->scalarInput[0]);
		if (obj == NULL)
			return pd_errno_return(-ENOENT);
		args->scalarOutput[1] = obj->size;
		pvr_gem_object_put(obj);
		err = pvr_gem_export(fFile, (u32)args->scalarInput[0], &token);
		args->scalarOutput[0] = token;
		return pd_errno_return(err);

	case PD_PVR_SEL_PRIME_IMPORT:
		if (args->scalarInputCount != 1 || args->scalarOutputCount != 2)
			return kIOReturnBadArgument;
		err = pvr_gem_import(fFile, args->scalarInput[0], &handle);
		if (err)
			return pd_errno_return(err);
		obj = pvr_gem_object_from_handle(fFile, handle);
		args->scalarOutput[0] = handle;
		args->scalarOutput[1] = obj != NULL ? obj->size : 0;
		pvr_gem_object_put(obj);
		return kIOReturnSuccess;

	case PD_PVR_SEL_PRIME_RELEASE:
		if (args->scalarInputCount != 1)
			return kIOReturnBadArgument;
		return pd_errno_return(pvr_gem_export_release(fFile, args->scalarInput[0]));

	case PD_PVR_SEL_DISPLAY_INFO: {
		struct pd_display_info info;
		struct pd_pvr_display_info out = { 0 };

		if (args->structureOutputSize < sizeof(out))
			return kIOReturnBadArgument;
		if (display_call(PD_DISPLAY_FN_INFO, &info, NULL, NULL) != kIOReturnSuccess)
			return pd_errno_return(-ENODEV);
		out.width = info.width;
		out.height = info.height;
		out.pitch = info.pitch;
		out.format = info.format;
		out.refresh_mhz = info.refresh_mhz;
		memcpy(args->structureOutput, &out, sizeof(out));
		args->structureOutputSize = sizeof(out);
		return kIOReturnSuccess;
	}

	case PD_PVR_SEL_FLIP:
		if (args->scalarInputCount != 3 || args->scalarOutputCount != 2)
			return kIOReturnBadArgument;
		return flip((u32)args->scalarInput[0], args->scalarInput[1], (u32)args->scalarInput[2],
		    &args->scalarOutput[0], &args->scalarOutput[1]);
	}
	return kIOReturnUnsupported;
}
