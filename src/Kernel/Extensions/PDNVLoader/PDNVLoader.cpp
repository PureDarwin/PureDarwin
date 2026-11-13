// PDNVLoader.cpp - links NVIDIA's closed resource manager object into the kernel as shipped, in the top 2 GB
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <mach/mach_time.h>
#include <mach/mach_types.h>
#include <mach/vm_prot.h>
#include <pexpert/pexpert.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>

extern "C" {
#include "pd_elf_rel.h"
#include "pdnv_port.h"
#include "pdnv_pci.h"

struct nvidia_stack_s;
int pdnv_stack_alloc(struct nvidia_stack_s **sp);
void pdnv_stack_free(struct nvidia_stack_s *sp);

// the image is ordinary wired kernel memory, aliased page by page into the top 2 GB through the pmap
kern_return_t pmap_enter(pmap_t pmap, vm_map_offset_t va, ppnum_t pn, vm_prot_t prot, vm_prot_t fault_type,
    unsigned int flags, boolean_t wired, unsigned int mapping_type);
void pmap_remove(pmap_t pmap, vm_map_offset_t s, vm_map_offset_t e);
ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
}

#define PDNV_DEFAULT_BLOB   "/usr/share/nvidia/580.196.07/nv-kernel.o_binary"
#define PDNV_WAIT_SECONDS   120
#define PDNV_READ_CHUNK     (1024 * 1024)

// R_X86_64_32S reaches only the top 2 GB. PureDarwin's xnu keeps its kmem ranges below it, so no vm_map
// entry ever covers it, and the image's pages are entered there directly: a window is one with no page yet
uintptr_t pdnv_image_base;

static const uint64_t sBottom = 0xffffffff80000000ull;
static const uint64_t sTop = 0xfffffffff0000000ull;
static const uint64_t sStep = 0x1000000ull;

class PDNVLoader : public IOService {
	OSDeclareDefaultStructors(PDNVLoader);

public:
	bool start(IOService *provider) override;

private:
	static void loadThread(void *arg, wait_result_t wr);
	void load();
	void initRM(struct pdelf_obj *obj);

	thread_t fThread;
};

OSDefineMetaClassAndStructors(PDNVLoader, IOService);

struct pdnv_file {
	vnode_t vp;
	vfs_context_t ctx;
};

static int
file_read(void *c, uint64_t off, void *buf, size_t len)
{
	struct pdnv_file *f = (struct pdnv_file *)c;

	// in pieces, vn_rdwr takes an int length
	while (len > 0) {
		int take = len > PDNV_READ_CHUNK ? PDNV_READ_CHUNK : (int)len;
		int resid = 0;
		int error = vn_rdwr(UIO_READ, f->vp, (caddr_t)buf, take, (off_t)off, UIO_SYSSPACE, 0,
		    vfs_context_ucred(f->ctx), &resid, vfs_context_proc(f->ctx));

		if (error != 0 || resid != 0)
			return -1;

		buf = (uint8_t *)buf + take, off += (uint64_t)take, len -= (size_t)take;
	}
	return 0;
}

static void *
kalloc_env(void *c, size_t len)
{
	return IOMalloc(len);
}

static void
kfree_env(void *c, void *p, size_t len)
{
	IOFree(p, len);
}

static void
log_env(void *c, const char *msg, const char *name, uint64_t value)
{
	IOLog("PDNVLoader: %s %s 0x%llx\n", msg, name, (unsigned long long)value);
}

// what an import the loader could not place at all calls: the trap table ran out
extern "C" void
pdnv_unported_import(void)
{
	IOLog("PDNVLoader: an import with no trap left was called\n");
	panic("PDNVLoader: the NVIDIA resource manager called an import with no trap left for it");
}

struct pdnv_alias {
	uint8_t *backing;       // the image as allocated, written through here
	uint64_t va;            // where it runs, in the top 2 GB
	int failed;
};

// every page of a range at its alias, with the access that range needs, wired
static void
alias_range(void *c, uint64_t off, uint64_t size, int writable, int executable)
{
	struct pdnv_alias *a = (struct pdnv_alias *)c;
	vm_prot_t prot = VM_PROT_READ | (writable ? VM_PROT_WRITE : 0) | (executable ? VM_PROT_EXECUTE : 0);

	for (uint64_t p = off; p < off + size && !a->failed; p += PAGE_SIZE) {
		ppnum_t pn = pmap_find_phys(kernel_pmap, (addr64_t)(uintptr_t)(a->backing + p));

		if (pn == 0 || pmap_enter(kernel_pmap, a->va + p, pn, prot, VM_PROT_NONE, 0, TRUE, 0) != KERN_SUCCESS) {
			IOLog("PDNVLoader: cannot alias the page at offset 0x%llx (page %u)\n", (unsigned long long)p, pn);
			a->failed = 1;
		}
	}
}

// a window of the top 2 GB with nothing mapped in it
static bool
window_free(uint64_t va, uint64_t size)
{
	for (uint64_t p = 0; p < size; p += PAGE_SIZE) {
		if (pmap_find_phys(kernel_pmap, va + p) != 0)
			return false;
	}
	return true;
}

bool
PDNVLoader::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;

	// the object is on the root filesystem, the boot does not wait for it
	retain();
	if (kernel_thread_start(loadThread, this, &fThread) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(fThread);
	registerService();
	return true;
}

void
PDNVLoader::loadThread(void *arg, wait_result_t wr)
{
	PDNVLoader *self = (PDNVLoader *)arg;

	self->load();
	self->release();
	thread_terminate(current_thread());
}

void
PDNVLoader::load()
{
	static const struct pdelf_env env = { NULL, file_read, kalloc_env, kfree_env, log_env };
	char path[256] = PDNV_DEFAULT_BLOB;
	struct pdnv_file file = { NULL, NULL };
	struct pdelf_env fenv = env;
	struct pdelf_obj obj;
	struct pdnv_alias alias = { NULL, 0, 0 };
	uint64_t t0 = 0, base = 0;
	int error = -1, waited;

	// each step with the milliseconds since the file was opened, so a stall shows where it is
	auto step = [&](const char *what, const char *detail, uint64_t value) {
		uint64_t ns;

		absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
		IOLog("PDNVLoader: %s %s 0x%llx at %llu ms\n", what, detail, (unsigned long long)value,
		    (unsigned long long)(ns / 1000000));
	};

	PE_parse_boot_argn("pdnvblob", path, sizeof(path));
	file.ctx = vfs_context_create(NULL);

	// path lookup is only safe once there is a root filesystem, then the file may still be on its way
	for (waited = 0; waited < PDNV_WAIT_SECONDS; waited++) {
		vnode_t root = vfs_rootvnode();

		if (root != NULL) {
			vnode_put(root);
			if (vnode_open(path, FREAD, 0, 0, &file.vp, file.ctx) == 0)
				break;
		}

		file.vp = NULL;
		IOSleep(1000);
	}
	if (file.vp == NULL) {
		IOLog("PDNVLoader: no %s after %d s\n", path, PDNV_WAIT_SECONDS);
		vfs_context_rele(file.ctx);
		return;
	}

	fenv.ctx = &file;
	t0 = mach_absolute_time();
	step("opened", path, 0);
	pdnv_trap_set_trace(PE_parse_boot_argn("pdnvtrace", &waited, sizeof(waited)) && waited != 0);
	if (pdelf_open(&obj, &fenv, PAGE_SIZE, pdnv_resolve, NULL, (uint64_t)&pdnv_unported_import) != 0) {
		IOLog("PDNVLoader: %s is not a loadable object\n", path);
		goto close;
	}
	step("tables read, image bytes", "", obj.image_size);

	// the image in wired memory, then a free window of the top 2 GB for it to run at
	alias.backing = (uint8_t *)IOMallocAligned(obj.image_size, PAGE_SIZE);
	if (alias.backing == NULL) {
		IOLog("PDNVLoader: no 0x%llx bytes for the image\n", (unsigned long long)obj.image_size);
		goto close;
	}
	for (uint64_t try_base = sBottom; obj.image_size <= sTop - sBottom && try_base <= sTop - obj.image_size;
	    try_base += sStep) {
		if (window_free(try_base, obj.image_size)) {
			base = try_base;
			break;
		}
	}
	if (base == 0) {
		IOLog("PDNVLoader: no free 0x%llx bytes in the top 2 GB\n", (unsigned long long)obj.image_size);
		goto release;
	}
	step("window in the top 2 GB at offset", "", base - sBottom);

	if (pdelf_load(&obj, alias.backing, base) != 0) {
		IOLog("PDNVLoader: linking %s failed\n", path);
		goto release;
	}
	step("linked", "imports", obj.nimports);

	alias.va = base;
	pdelf_ranges(&obj, alias_range, &alias);
	if (alias.failed)
		goto unmap;
	step("aliased", "", 0);

	IOLog("PDNVLoader: %s linked at 0x%llx, 0x%llx bytes, %u imports (%u unresolved), rm_init_rm 0x%llx, "
	    "nvidia_caps_root cell 0x%llx\n", path, (unsigned long long)base, (unsigned long long)obj.image_size,
	    obj.nimports, obj.unresolved, (unsigned long long)pdelf_export(&obj, "rm_init_rm"),
	    (unsigned long long)pdelf_import_slot(&obj, "nvidia_caps_root", 1));
	error = 0;
	pdnv_image_base = (uintptr_t)base;
	initRM(&obj);
	goto close;

unmap:
	pmap_remove(kernel_pmap, base, base + obj.image_size);
release:
	IOFreeAligned(alias.backing, obj.image_size);
close:
	pdelf_close(&obj);
out:
	vnode_close(file.vp, FREAD, file.ctx);
	vfs_context_rele(file.ctx);
	if (error != 0)
		IOLog("PDNVLoader: %s not loaded\n", path);
}

typedef unsigned char (*rm_init_fn)(struct nvidia_stack_s *);

// the resource manager's own start, as nv_module_init makes it once the interface layer is up
void
PDNVLoader::initRM(struct pdelf_obj *obj)
{
	struct nvidia_stack_s *sp = NULL;
	rm_init_fn init = (rm_init_fn)pdelf_export(obj, "rm_init_rm");

	pdnv_rm.write_registry_dword = (void *)pdelf_export(obj, "rm_write_registry_dword");
	pdnv_rm.write_registry_string = (void *)pdelf_export(obj, "rm_write_registry_string");
	pdnv_rm.parse_option_string = (void *)pdelf_export(obj, "rm_parse_option_string");
	pdnv_rm.is_altstack_in_use = (void *)pdelf_export(obj, "rm_is_altstack_in_use");
	pdnv_rm.is_supported_device = (void *)pdelf_export(obj, "rm_is_supported_device");
	pdnv_rm.init_private_state = (void *)pdelf_export(obj, "rm_init_private_state");
	pdnv_rm.init_event_locks = (void *)pdelf_export(obj, "rm_init_event_locks");
	pdnv_rm.init_adapter = (void *)pdelf_export(obj, "rm_init_adapter");
	pdnv_rm.isr = (void *)pdelf_export(obj, "rm_isr");
	pdnv_rm.isr_bh = (void *)pdelf_export(obj, "rm_isr_bh");
	pdnv_rm.execute_work_item = (void *)pdelf_export(obj, "rm_execute_work_item");
	pdnv_rm.run_rc_callback = (void *)pdelf_export(obj, "rm_run_rc_callback");
	pdnv_rm.ioctl = (void *)pdelf_export(obj, "rm_ioctl");
	pdnv_rm.cleanup_file_private = (void *)pdelf_export(obj, "rm_cleanup_file_private");
	pdnv_rm.perform_version_check = (void *)pdelf_export(obj, "rm_perform_version_check");
	pdnv_rm.get_gpu_numa_info = (void *)pdelf_export(obj, "rm_get_gpu_numa_info");
	pdnv_pci_init();
	pdnv_work_init();
	if (init == NULL || pdnv_rm.is_altstack_in_use == NULL) {
		IOLog("PDNVLoader: the object has no rm_init_rm or rm_is_altstack_in_use\n");
		return;
	}

	if (pdnv_stack_alloc(&sp) != 0) {
		IOLog("PDNVLoader: no memory for the resource manager's stack\n");
		return;
	}
	IOLog("PDNVLoader: rm_init_rm, %s stack\n", sp != NULL ? "its own" : "this thread's");
	bool ok = init(sp);
	IOLog("PDNVLoader: rm_init_rm returned %s\n", ok ? "true" : "false");
	pdnv_stack_free(sp);
	// the GPU drivers wait on this
	if (ok && pdnv_dev_init() != 0)
		IOLog("PDNVLoader: no /dev/nvidiactl\n");
	if (ok)
		publishResource(PDNV_RM_RESOURCE, this);
}
