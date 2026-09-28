#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <riscv/machine_routines.h>
#include "PDRiscvCPU.h"
#include "PDRiscvDT.h"
#include "PDRiscvPLIC.h"

class PDRiscvPlatformExpert : public IODTPlatformExpert
{
	OSDeclareDefaultStructors(PDRiscvPlatformExpert)

private:
	PDRiscvCPU *bootCPU;
	IOService *fFramebufferNub;

public:
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	bool getMachineName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	bool getModelName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;

protected:
	virtual bool initPlatformInterrupts(void);
	virtual void initPlatformInterruptsLate(void);
	virtual const char *platformModelName(void);
	bool startCPUs(void);
	void publishSocNubs(void);
	void publishFramebuffer(void);
};

// sbi system reset, eid "SRST", from the risc-v sbi specification
#define PD_SBI_EXT_SRST			0x53525354UL
#define PD_SBI_SRST_SHUTDOWN		0UL
#define PD_SBI_SRST_COLD_REBOOT		1UL
#define PD_SBI_SRST_NO_REASON		0UL
// the legacy v0.1 shutdown call, for firmware without srst
#define PD_SBI_LEGACY_SHUTDOWN		0x08UL

static long
pd_sbi_call(unsigned long ext, unsigned long fid, unsigned long arg0, unsigned long arg1)
{
	register unsigned long a0 __asm__ ("a0") = arg0;
	register unsigned long a1 __asm__ ("a1") = arg1;
	register unsigned long a6 __asm__ ("a6") = fid;
	register unsigned long a7 __asm__ ("a7") = ext;

	__asm__ volatile ("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
	return (long)a0;
}

// PE_halt_restart, which IOPlatformExpert::haltRestart calls once iokit has shut down
static int
pd_riscv_halt_restart(unsigned int type)
{
	switch (type) {
	case kPEHaltCPU:
		(void)pd_sbi_call(PD_SBI_EXT_SRST, 0, PD_SBI_SRST_SHUTDOWN, PD_SBI_SRST_NO_REASON);
		(void)pd_sbi_call(PD_SBI_LEGACY_SHUTDOWN, 0, 0, 0);
		return -1;
	case kPERestartCPU:
	case kPEPanicRestartCPU:
	case kPEPanicRestartCPUNoCallouts:
		(void)pd_sbi_call(PD_SBI_EXT_SRST, 0, PD_SBI_SRST_COLD_REBOOT, PD_SBI_SRST_NO_REASON);
		return -1;
	default:
		// panic begin and end and the paging notices need nothing from the firmware
		return 0;
	}
}

#define super IODTPlatformExpert
OSDefineMetaClassAndStructors(PDRiscvPlatformExpert, IODTPlatformExpert);

IOService *
PDRiscvPlatformExpert::probe(IOService *provider, SInt32 *score)
{
	IOService *result = super::probe(provider, score);
	if (result != 0 && score != 0) *score = 10000;
	return result;
}

bool
PDRiscvPlatformExpert::start(IOService *provider)
{
	// configure() inside super::start publishes the /cpus and top level nubs
	if (!super::start(provider)) return false;
	registerService();

	PE_halt_restart = pd_riscv_halt_restart;

	if (!initPlatformInterrupts()) return false;
	if (!startCPUs()) return false;

	initPlatformInterruptsLate();

	publishSocNubs();
	publishFramebuffer();
	return true;
}

// configure() stops at the root's children, the peripherals sit one level down on the soc bus
// their nubs carry the interrupt specifiers IODeviceTreeSupport mapped onto the plic
void
PDRiscvPlatformExpert::publishSocNubs(void)
{
	static const char * const socNames[] = { "riscv-io", "soc" };
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	IORegistryEntry *soc = NULL;

	if (root == NULL) return;
	for (unsigned int i = 0; soc == NULL && i < sizeof(socNames) / sizeof(socNames[0]); i++) {
		soc = root->childFromPath(socNames[i], gIODTPlane);
	}
	root->release();
	if (soc == NULL) {
		IOLog("PDRiscvPlatformExpert: no riscv-io or soc node, no peripheral nubs\n");
		return;
	}

	// the nubs hang off the platform expert itself, IOPlatformDevice takes its provider
	// for the platform expert when it matches names and resolves reg and interrupts
	createNubs(this, IODTFindMatchingEntries(soc, kIODTExclusive, NULL));
	soc->release();
}

// the /cpus nub for a hart, the IOCPU hangs off it the way AppleARMCPU does on apple parts
static IOService *
pd_cpu_nub(uint32_t hart)
{
	IORegistryEntry *cpus = IORegistryEntry::fromPath("/cpus", gIODTPlane);
	IOService *nub = NULL;
	OSIterator *iter;

	if (cpus == NULL) return NULL;
	iter = cpus->getChildIterator(gIODTPlane);
	while (iter != NULL) {
		IORegistryEntry *cpu = OSDynamicCast(IORegistryEntry, iter->getNextObject());
		OSData *type;
		uint64_t reg;

		if (cpu == NULL) break;
		type = OSDynamicCast(OSData, cpu->getProperty("device_type"));
		if (type == NULL || strncmp((const char *)type->getBytesNoCopy(), "cpu", type->getLength()) != 0) {
			continue;
		}
		if (PDRiscvDT_getCell(cpu, "reg", &reg) && (uint32_t)reg == hart) {
			nub = OSDynamicCast(IOService, cpu);
			break;
		}
	}
	OSSafeReleaseNULL(iter);
	cpus->release();
	return nub;
}

// one IOCPU per processor in xnu's topology, the logical number and hart id come from there
// the boot cpu registers first and finishes its setup, the kernel starts the rest through sbi hsm
bool
PDRiscvPlatformExpert::startCPUs(void)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	unsigned int ncpu = (topo != NULL) ? topo->num_cpus : 1;
	unsigned int bootIndex = 0;

	if (topo != NULL && topo->boot_cpu != NULL) {
		bootIndex = (unsigned int)(topo->boot_cpu - topo->cpus);
	}

	PDRiscvCPU::setCPUCount(ncpu);

	for (unsigned int n = 0; n < ncpu; n++) {
		// boot cpu first, then the others in topology order
		unsigned int i = (n == 0) ? bootIndex : ((n <= bootIndex) ? n - 1 : n);
		unsigned int logical = (topo != NULL) ? topo->cpus[i].cpu_id : 0;
		uint32_t hart = (topo != NULL) ? topo->cpus[i].phys_id : 0;
		bool boot = (n == 0);
		IOService *nub = pd_cpu_nub(hart);
		PDRiscvCPU *cpu = new PDRiscvCPU;

		if (cpu == NULL || !cpu->init()) {
			OSSafeReleaseNULL(cpu);
			return false;
		}
		if (nub == NULL) {
			IOLog("PDRiscvPlatformExpert: no /cpus node for hart %u, attaching to the platform\n", hart);
			nub = this;
		}
		if (!cpu->attach(nub) || !cpu->start(nub)) {
			IOLog("PDRiscvPlatformExpert: could not start the IOCPU for hart %u\n", hart);
			cpu->release();
			return false;
		}
		if (!cpu->startForCPU(logical, hart, boot)) {
			cpu->release();
			return false;
		}
		if (boot) bootCPU = cpu;
		// the registry holds it through the attach
		cpu->release();
	}
	return bootCPU != NULL;
}

// firmware framebuffers are described by boot_args, publish a nub whenever the loader passed one
void
PDRiscvPlatformExpert::publishFramebuffer(void)
{
	PE_Video console;
	IOService *nub = NULL;

	if (fFramebufferNub != NULL) return;

	if (getConsoleInfo(&console) != kIOReturnSuccess ||
	    console.v_baseAddr == 0 || console.v_width == 0 || console.v_height == 0)
		return;

	nub = OSTypeAlloc(IOPlatformDevice);
	if (nub == NULL) goto fail;
	if (!nub->init()) goto fail;
	nub->setName("framebuffer");
	if (!nub->attach(this)) goto fail;

	fFramebufferNub = nub;
	nub->registerService();
	return;

fail:
	IOLog("PDRiscvPlatformExpert: could not publish the framebuffer nub\n");
	OSSafeReleaseNULL(nub);
}

const char *
PDRiscvPlatformExpert::deleteList(void)
{
	return "('pd-delete-none')";
}

const char *
PDRiscvPlatformExpert::excludeList(void)
{
	return NULL;
}

bool
PDRiscvPlatformExpert::getMachineName(char *name, int maxLength)
{
	if (name == 0 || maxLength <= 0) return false;
	strlcpy(name, "riscv64", (size_t)maxLength);
	return true;
}

bool
PDRiscvPlatformExpert::getModelName(char *name, int maxLength)
{
	IORegistryEntry *root;
	OSData *model = NULL;

	if (name == 0 || maxLength <= 0) return false;

	// the fdt's root model names the board, the class default covers loaders that drop it
	root = IORegistryEntry::fromPath("/", gIODTPlane);
	if (root != NULL) {
		model = OSDynamicCast(OSData, root->getProperty("model"));
	}
	if (model != NULL && model->getLength() > 1) {
		size_t len = strnlen((const char *)model->getBytesNoCopy(), model->getLength());
		size_t n = (len < (size_t)maxLength - 1) ? len : (size_t)maxLength - 1;

		memcpy(name, model->getBytesNoCopy(), n);
		name[n] = '\0';
	} else {
		strlcpy(name, platformModelName(), (size_t)maxLength);
	}
	OSSafeReleaseNULL(root);
	return true;
}

// the plic has to be quiet before any hart registers, the kernel enables sei on each hart it starts
bool
PDRiscvPlatformExpert::initPlatformInterrupts(void)
{
	if (!PDRiscvPLIC_init()) {
		IOLog("PDRiscvPlatformExpert: no plic, device interrupts will not be delivered\n");
	}
	return true;
}

// every cpu has installed the cpu interrupt handler by now, so the plic can hang off it
void
PDRiscvPlatformExpert::initPlatformInterruptsLate(void)
{
	if (!PDRiscvPLIC_start()) {
		IOLog("PDRiscvPlatformExpert: plic not published, device interrupts will not be delivered\n");
	}
}

const char *
PDRiscvPlatformExpert::platformModelName(void)
{
	return "QEMU Virtual RISC-V";
}
