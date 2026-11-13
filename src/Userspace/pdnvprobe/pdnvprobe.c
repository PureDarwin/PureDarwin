// pdnvprobe.c - talks to the NVIDIA resource manager through /dev/nvidiactl: version, cards, a client,
// its device and subdevice, the GPU's name and UUID, system memory mapped into this process, and a
// software event raised on the subdevice and read back after poll
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

// NVIDIA's MIT headers, unmodified: the escapes and RM API from open-gpu-kernel-modules (nvidia/), the
// interface layer's own ioctls from the driver package
#include <nvos.h>
#include <nv_escape.h>
#include <nv-unix-nvos-params-wrappers.h>
#include <nvmisc.h>
#include <class/cl003e.h>
#include <class/cl0005.h>
#include <ctrl/ctrl2080/ctrl2080event.h>
#include <class/cl0080.h>
#include <class/cl2080.h>
#include <ctrl/ctrl0000/ctrl0000gpu.h>
#include <ctrl/ctrl2080/ctrl2080gpu.h>
#include <nv-ioctl.h>
#include <nv-ioctl-numbers.h>

#define NV_VERSION          "580.196.07"
#define NV01_ROOT_CLIENT    0x00000041      // class/cl0041.h

#define NV_IOCTL(nr, type)  _IOWR(NV_IOCTL_MAGIC, (nr), type)

static int sCtl = -1;

static NvU32
rm_alloc(NvU32 hRoot, NvU32 hParent, NvU32 hNew, NvU32 hClass, void *params, NvU32 size, NvU32 *out)
{
	NVOS21_PARAMETERS p;

	memset(&p, 0, sizeof(p));
	p.hRoot = hRoot;
	p.hObjectParent = hParent;
	p.hObjectNew = hNew;
	p.hClass = hClass;
	p.pAllocParms = (NvP64)(uintptr_t)params;
	p.paramsSize = size;
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_RM_ALLOC, NVOS21_PARAMETERS), &p) != 0)
		return NV_ERR_GENERIC;
	if (out != NULL)
		*out = p.hObjectNew;
	return p.status;
}

static NvU32
rm_control(NvU32 hClient, NvU32 hObject, NvU32 cmd, void *params, NvU32 size)
{
	NVOS54_PARAMETERS p;

	memset(&p, 0, sizeof(p));
	p.hClient = hClient;
	p.hObject = hObject;
	p.cmd = cmd;
	p.params = (NvP64)(uintptr_t)params;
	p.paramsSize = size;
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_RM_CONTROL, NVOS54_PARAMETERS), &p) != 0)
		return NV_ERR_GENERIC;
	return p.status;
}

static void rm_free(NvU32 hRoot, NvU32 hParent, NvU32 hObject);
static NvU32 rm_alloc(NvU32 hRoot, NvU32 hParent, NvU32 hNew, NvU32 hClass, void *params, NvU32 size, NvU32 *out);
static NvU32 rm_control(NvU32 hClient, NvU32 hObject, NvU32 cmd, void *params, NvU32 size);

#define MAP_TEST_SIZE   (64 * 1024)

// system memory under the device, allocated on the GPU node and mapped through a control descriptor of
// its own, as the RM's escapes want, written and read back
static int
map_test(int gpu_fd, NvU32 hClient, NvU32 hDevice)
{
	nv_ioctl_nvos02_parameters_with_fd a;
	nv_ioctl_nvos33_parameters_with_fd m;
	NVOS34_PARAMETERS u;
	NvU32 hMemory = 0xcaf00010, bad = 0;
	volatile NvU32 *p;
	int fd, rc = 1;

	memset(&a, 0, sizeof(a));
	a.params.hRoot = hClient;
	a.params.hObjectParent = hDevice;
	a.params.hObjectNew = hMemory;
	a.params.hClass = NV01_MEMORY_SYSTEM;
	a.params.flags = DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) | DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
	    DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED) | DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP);
	a.params.limit = MAP_TEST_SIZE - 1;
	a.fd = -1;
	if (ioctl(gpu_fd, NV_IOCTL(NV_ESC_RM_ALLOC_MEMORY, nv_ioctl_nvos02_parameters_with_fd), &a) != 0 ||
	    a.params.status != NV_OK) {
		printf("pdnvprobe: system memory: %s, status 0x%x\n", strerror(errno), a.params.status);
		return 1;
	}

	fd = open("/dev/nvidiactl", O_RDWR);
	if (fd < 0) {
		printf("pdnvprobe: second /dev/nvidiactl: %s\n", strerror(errno));
		goto out_free;
	}
	memset(&m, 0, sizeof(m));
	m.params.hClient = hClient;
	m.params.hDevice = hDevice;
	m.params.hMemory = hMemory;
	m.params.length = MAP_TEST_SIZE;
	m.fd = fd;
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_RM_MAP_MEMORY, nv_ioctl_nvos33_parameters_with_fd), &m) != 0 ||
	    m.params.status != NV_OK) {
		printf("pdnvprobe: map memory: %s, status 0x%x\n", strerror(errno), m.params.status);
		goto out_close;
	}

	p = mmap(NULL, MAP_TEST_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		printf("pdnvprobe: mmap: %s\n", strerror(errno));
	} else {
		for (NvU32 i = 0; i < MAP_TEST_SIZE / 4; i++)
			p[i] = 0x9e000000u ^ i;
		for (NvU32 i = 0; i < MAP_TEST_SIZE / 4; i++)
			bad += p[i] != (0x9e000000u ^ i);
		printf("pdnvprobe: 0x%x bytes of system memory mapped, %u words wrong\n", MAP_TEST_SIZE, bad);
		rc = bad != 0;
		munmap((void *)p, MAP_TEST_SIZE);
	}

	memset(&u, 0, sizeof(u));
	u.hClient = hClient;
	u.hDevice = hDevice;
	u.hMemory = hMemory;
	u.pLinearAddress = m.params.pLinearAddress;
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_RM_UNMAP_MEMORY, NVOS34_PARAMETERS), &u) != 0 || u.status != NV_OK)
		printf("pdnvprobe: unmap memory: status 0x%x\n", u.status);
out_close:
	close(fd);
out_free:
	rm_free(hClient, hDevice, hMemory);
	return rc;
}

// a software event on the subdevice: an OS event on a control descriptor of its own, armed, triggered,
// then poll and the event read back
static int
event_test(NvU32 hClient, NvU32 hDevice, NvU32 hSubdevice)
{
	nv_ioctl_alloc_os_event_t ae;
	nv_ioctl_free_os_event_t fe;
	NV0005_ALLOC_PARAMETERS ep;
	NV2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS sn;
	NVOS41_PARAMETERS ge;
	NvUnixEvent ev;
	struct pollfd pfd;
	NvU32 hEvent = 0xcaf00020, st;
	int fd, n, rc = 1;

	fd = open("/dev/nvidiactl", O_RDWR);
	if (fd < 0) {
		printf("pdnvprobe: event descriptor: %s\n", strerror(errno));
		return 1;
	}
	memset(&ae, 0, sizeof(ae));
	ae.hClient = hClient;
	ae.hDevice = hDevice;
	ae.fd = (NvU32)fd;
	if (ioctl(fd, NV_IOCTL(NV_ESC_ALLOC_OS_EVENT, nv_ioctl_alloc_os_event_t), &ae) != 0 || ae.Status != NV_OK) {
		printf("pdnvprobe: os event: %s, status 0x%x\n", strerror(errno), ae.Status);
		goto out_close;
	}

	memset(&ep, 0, sizeof(ep));
	ep.hParentClient = hClient;
	ep.hSrcResource = hSubdevice;
	ep.hClass = NV01_EVENT_OS_EVENT;
	ep.notifyIndex = NV2080_NOTIFIERS_SW;
	ep.data = (NvP64)(uintptr_t)fd;
	st = rm_alloc(hClient, hSubdevice, hEvent, NV01_EVENT_OS_EVENT, &ep, sizeof(ep), NULL);
	if (st != NV_OK) {
		printf("pdnvprobe: event object: status 0x%x\n", st);
		goto out_os_event;
	}

	memset(&sn, 0, sizeof(sn));
	sn.event = NV2080_NOTIFIERS_SW;
	sn.action = NV2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;
	st = rm_control(hClient, hSubdevice, NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION, &sn, sizeof(sn));
	if (st != NV_OK) {
		printf("pdnvprobe: arm event: status 0x%x\n", st);
		goto out_event;
	}

	pfd.fd = fd;
	pfd.events = POLLIN | POLLPRI;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	printf("pdnvprobe: poll before the trigger: %d, revents 0x%x\n", n, pfd.revents);
	if (n != 0)
		goto out_event;

	st = rm_control(hClient, hSubdevice, NV2080_CTRL_CMD_EVENT_SET_TRIGGER, NULL, 0);
	if (st != NV_OK) {
		printf("pdnvprobe: trigger: status 0x%x\n", st);
		goto out_event;
	}
	pfd.revents = 0;
	n = poll(&pfd, 1, 1000);
	printf("pdnvprobe: poll after the trigger: %d, revents 0x%x\n", n, pfd.revents);

	memset(&ev, 0, sizeof(ev));
	memset(&ge, 0, sizeof(ge));
	ge.pEvent = (NvP64)(uintptr_t)&ev;
	if (ioctl(fd, NV_IOCTL(NV_ESC_RM_GET_EVENT_DATA, NVOS41_PARAMETERS), &ge) != 0 || ge.status != NV_OK) {
		printf("pdnvprobe: event data: %s, status 0x%x\n", strerror(errno), ge.status);
	} else {
		printf("pdnvprobe: event on 0x%x, notify index %u, more %u\n", ev.hObject, ev.NotifyIndex, ge.MoreEvents);
		rc = (n == 1 && (pfd.revents & (POLLIN | POLLPRI)) != 0 && (pfd.revents & POLLNVAL) == 0) ? 0 : 1;
	}

out_event:
	rm_free(hClient, hSubdevice, hEvent);
out_os_event:
	memset(&fe, 0, sizeof(fe));
	fe.hClient = hClient;
	fe.hDevice = hDevice;
	fe.fd = (NvU32)fd;
	ioctl(fd, NV_IOCTL(NV_ESC_FREE_OS_EVENT, nv_ioctl_free_os_event_t), &fe);
out_close:
	close(fd);
	return rc;
}

static void
rm_free(NvU32 hRoot, NvU32 hParent, NvU32 hObject)
{
	NVOS00_PARAMETERS p;

	memset(&p, 0, sizeof(p));
	p.hRoot = hRoot;
	p.hObjectParent = hParent;
	p.hObjectOld = hObject;
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_RM_FREE, NVOS00_PARAMETERS), &p) != 0 || p.status != NV_OK)
		printf("pdnvprobe: free 0x%x: status 0x%x\n", hObject, p.status);
}

int
main(void)
{
	nv_ioctl_rm_api_version_t ver;
	nv_ioctl_card_info_t ci[8];
	NV0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS ids;
	NV0080_ALLOC_PARAMETERS dev;
	NV2080_ALLOC_PARAMETERS sub;
	NV2080_CTRL_GPU_GET_NAME_STRING_PARAMS name;
	NV2080_CTRL_GPU_GET_GID_INFO_PARAMS gid;
	NvU32 hClient = 0, hDevice = 0xcaf00001, hSubdevice = 0xcaf00002, st, gpu_id = 0;
	int gpu_fd, i, rc = 0;

	sCtl = open("/dev/nvidiactl", O_RDWR);
	if (sCtl < 0) {
		printf("pdnvprobe: /dev/nvidiactl: %s\n", strerror(errno));
		return 1;
	}

	memset(&ver, 0, sizeof(ver));
	ver.cmd = NV_RM_API_VERSION_CMD_STRICT;
	strlcpy(ver.versionString, NV_VERSION, sizeof(ver.versionString));
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_CHECK_VERSION_STR, nv_ioctl_rm_api_version_t), &ver) != 0) {
		printf("pdnvprobe: version check: %s, kernel has \"%s\"\n", strerror(errno), ver.versionString);
		return 1;
	}
	printf("pdnvprobe: version %s accepted\n", NV_VERSION);

	memset(ci, 0, sizeof(ci));
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_CARD_INFO, ci), ci) != 0) {
		printf("pdnvprobe: card info: %s\n", strerror(errno));
		return 1;
	}
	for (i = 0; i < 8; i++) {
		if (!ci[i].valid)
			continue;
		printf("pdnvprobe: card %d: %02x:%02x %04x:%04x gpu id 0x%x, minor %u\n", i, ci[i].pci_info.bus,
		    ci[i].pci_info.slot, ci[i].pci_info.vendor_id, ci[i].pci_info.device_id, ci[i].gpu_id,
		    ci[i].minor_number);
		if (gpu_id == 0)
			gpu_id = ci[i].gpu_id;
	}
	if (gpu_id == 0) {
		printf("pdnvprobe: no card\n");
		return 1;
	}

	// the GPU's own node, and the GPU attached to this descriptor, as the user libraries do
	gpu_fd = open("/dev/nvidia0", O_RDWR);
	if (gpu_fd < 0) {
		printf("pdnvprobe: /dev/nvidia0: %s\n", strerror(errno));
	} else {
		// the GPU descriptor shares the control descriptor's clients once registered with it
		nv_ioctl_register_fd_t reg = { .ctl_fd = sCtl };

		if (ioctl(gpu_fd, NV_IOCTL(NV_ESC_REGISTER_FD, nv_ioctl_register_fd_t), &reg) != 0)
			printf("pdnvprobe: register /dev/nvidia0 with /dev/nvidiactl: %s\n", strerror(errno));
	}
	if (ioctl(sCtl, NV_IOCTL(NV_ESC_ATTACH_GPUS_TO_FD, NvU32), &gpu_id) != 0)
		printf("pdnvprobe: attach gpu 0x%x: %s\n", gpu_id, strerror(errno));

	st = rm_alloc(0, 0, 0, NV01_ROOT_CLIENT, NULL, 0, &hClient);
	if (st != NV_OK) {
		printf("pdnvprobe: root client: status 0x%x\n", st);
		return 1;
	}
	printf("pdnvprobe: root client 0x%x\n", hClient);

	memset(&ids, 0, sizeof(ids));
	st = rm_control(hClient, hClient, NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS, &ids, sizeof(ids));
	printf("pdnvprobe: attached gpu ids: status 0x%x, first 0x%x\n", st, ids.gpuIds[0]);

	memset(&dev, 0, sizeof(dev));
	dev.deviceId = 0;
	dev.hClientShare = hClient;
	st = rm_alloc(hClient, hClient, hDevice, NV01_DEVICE_0, &dev, sizeof(dev), NULL);
	if (st != NV_OK) {
		printf("pdnvprobe: device: status 0x%x\n", st);
		rc = 1;
		goto out;
	}
	memset(&sub, 0, sizeof(sub));
	st = rm_alloc(hClient, hDevice, hSubdevice, NV20_SUBDEVICE_0, &sub, sizeof(sub), NULL);
	if (st != NV_OK) {
		printf("pdnvprobe: subdevice: status 0x%x\n", st);
		rc = 1;
		goto out_device;
	}
	printf("pdnvprobe: device 0x%x, subdevice 0x%x\n", hDevice, hSubdevice);

	memset(&name, 0, sizeof(name));
	name.gpuNameStringFlags = NV2080_CTRL_GPU_GET_NAME_STRING_FLAGS_TYPE_ASCII;
	st = rm_control(hClient, hSubdevice, NV2080_CTRL_CMD_GPU_GET_NAME_STRING, &name, sizeof(name));
	if (st == NV_OK)
		printf("pdnvprobe: name \"%s\"\n", (const char *)name.gpuNameString.ascii);
	else {
		printf("pdnvprobe: name: status 0x%x\n", st);
		rc = 1;
	}

	memset(&gid, 0, sizeof(gid));
	gid.flags = NV2080_GPU_CMD_GPU_GET_GID_FLAGS_FORMAT_ASCII;
	st = rm_control(hClient, hSubdevice, NV2080_CTRL_CMD_GPU_GET_GID_INFO, &gid, sizeof(gid));
	if (st == NV_OK)
		printf("pdnvprobe: uuid %.*s\n", (int)gid.length, (const char *)gid.data);
	else {
		printf("pdnvprobe: uuid: status 0x%x\n", st);
		rc = 1;
	}

	if (gpu_fd < 0 || map_test(gpu_fd, hClient, hDevice) != 0)
		rc = 1;
	if (event_test(hClient, hDevice, hSubdevice) != 0)
		rc = 1;

	rm_free(hClient, hDevice, hSubdevice);
out_device:
	rm_free(hClient, hClient, hDevice);
out:
	rm_free(hClient, 0, hClient);
	printf("pdnvprobe: freed\n");
	if (gpu_fd >= 0)
		close(gpu_fd);
	close(sCtl);
	return rc;
}
