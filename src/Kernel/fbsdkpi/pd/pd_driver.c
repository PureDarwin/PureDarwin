// SPDX-License-Identifier: MIT
// stands in for haiku's driver.c and device_hooks.c
// devices come from an iokit driver class, and packets cross to xnu here

#include "device.h"
#include "sysinit.h"

#include <stdlib.h>
#include <sys/sockio.h>
#include <PCI.h>

#include <compat/sys/haiku-module.h>
#include <compat/sys/bus.h>
#include <compat/sys/mbuf.h>
#include <compat/net/ethernet.h>
#include <compat/net/if_media.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_attach.h"
#include "pd_haiku_kernel.h"
#include <compat/sys/rman.h>
#include <compat/dev/pci/pcivar.h>
#include "fbsd-extra/dev/pci/pci_private.h"

const char *gDeviceNameList[MAX_DEVICES + 1];
struct ifnet *gDevices[MAX_DEVICES];
int32 gDeviceCount;

// set by the linuxkpi glue, which keeps pci state in each device's ivars and
// makes the wlan vap once the driver attached
void (*pd_fbsd_prepare_device)(device_t dev);
int (*pd_fbsd_post_attach)(device_t dev);
// set by the wlan glue, ethernet frames then go to if_transmit
int pd_fbsd_frames_on_transmit;

static void *sCookies[MAX_DEVICES];
static void *sPciDevs[MAX_DEVICES];
static void *sPendingPci;
static struct ifnet *sAttached;
// drivers without an ifnet (drm) only report the device they attached
static device_t sAttachedDevice;
static int sWantIfnet = 1;
static int sInitialised;

status_t
init_root_device(device_t *_root, int bus_type)
{
	static driver_t sRootDriverPCI = {
		"pci",
		NULL,
		sizeof(struct root_device_softc),
	};

	device_t root = device_add_child(NULL, NULL, 0);
	if (root == NULL)
		return B_NO_MEMORY;

	root->softc = malloc(sizeof(struct root_device_softc));
	if (root->softc == NULL) {
		device_delete_child(NULL, root);
		return B_NO_MEMORY;
	}
	bzero(root->softc, sizeof(struct root_device_softc));
	if (bus_type != BUS_pci)
		panic("only pci devices attach on puredarwin");
	root->driver = &sRootDriverPCI;
	((struct root_device_softc *)root->softc)->bus = bus_type;
	root->root = root;
	if (_root != NULL)
		*_root = root;
	return B_OK;
}

// devices a driver looks up without attaching to them (i915 reads the host bridge),
// one device_t each, kept for the life of the kext
#define PD_PEER_MAX		16
static struct {
	void *pci;
	device_t dev;
} sPeers[PD_PEER_MAX];

static device_t
peer_device(void *pci)
{
	int i;

	for (i = 0; i < PD_PEER_MAX && sPeers[i].pci != NULL; i++) {
		if (sPeers[i].pci == pci)
			return sPeers[i].dev;
	}
	if (i == PD_PEER_MAX || init_pci() != B_OK)
		return NULL;

	device_t root;
	if (init_root_device(&root, BUS_pci) != B_OK)
		return NULL;
	if (pd_pci_add_peer(pci, get_device_pci_info(root)) != B_OK) {
		device_delete_child(NULL, root);
		return NULL;
	}
	device_t dev = device_add_child(root, NULL, 0);
	if (dev == NULL) {
		device_delete_child(NULL, root);
		return NULL;
	}
	pci_info *info = get_device_pci_info(root);
	snprintf(dev->nameunit, sizeof(dev->nameunit), "pci%d:%d:%d", info->bus,
		info->device, info->function);
	if (pd_fbsd_prepare_device != NULL)
		pd_fbsd_prepare_device(dev);
	// before the linuxkpi glue is up (amdgpu looks for the vga device at module
	// load) the device still needs the pci_devinfo linuxkpi reads
	if (device_get_ivars(dev) == NULL)
		device_set_ivars(dev, calloc(1, sizeof(struct pci_devinfo)));
	sPeers[i].pci = pci;
	sPeers[i].dev = dev;
	return dev;
}

// the next device after from, in bus order, that match accepts
device_t
pd_pci_peer_next(device_t from, int (*match)(const pci_info *info, void *arg), void *arg)
{
	void *devs[64];
	int n = pdos_pci_enumerate(devs, 64);
	bool past = from == NULL;
	pci_info *start = from != NULL ? get_device_pci_info(from->root) : NULL;

	for (int i = 0; i < n; i++) {
		uint8 bus, slot, func;
		pdos_pci_location(devs[i], &bus, &slot, &func);
		if (!past) {
			if (bus == start->bus && slot == start->device && func == start->function)
				past = true;
			continue;
		}
		pci_info info = {};
		info.bus = bus;
		info.device = slot;
		info.function = func;
		info.vendor_id = pdos_pci_read(devs[i], PCI_vendor_id, 2);
		info.device_id = pdos_pci_read(devs[i], PCI_device_id, 2);
		info.class_base = pdos_pci_read(devs[i], PCI_class_base, 1);
		info.class_sub = pdos_pci_read(devs[i], PCI_class_sub, 1);
		if (match(&info, arg))
			return peer_device(devs[i]);
	}
	return NULL;
}

static int
match_location(const pci_info *info, void *arg)
{
	const uint8 *loc = arg;
	return info->bus == loc[0] && info->device == loc[1] && info->function == loc[2];
}

device_t
pd_pci_peer_at(uint8 bus, uint8 slot, uint8 func)
{
	uint8 loc[3] = { bus, slot, func };
	return pd_pci_peer_next(NULL, match_location, loc);
}

void
report_probed_device(int bus, void *compat_device, driver_t *driver,
	void (*prepare_attach)(void *, device_t), void (*free_compat_device)(void *))
{
	free_compat_device(compat_device);
}

// called from the driver's __haiku_init_hardware with its driver list, while
// pd_fbsd_attach_pci has a device pending
status_t
_fbsd_init_hardware_pci(driver_t *drivers[])
{
	if (sPendingPci == NULL)
		return B_NOT_SUPPORTED;

	status_t status = init_pci();
	if (status != B_OK)
		return status;

	device_t root;
	status = init_root_device(&root, BUS_pci);
	if (status != B_OK)
		return status;

	pci_info *info = get_device_pci_info(root);
	status = pd_pci_register(sPendingPci, info);
	if (status != B_OK) {
		device_delete_child(NULL, root);
		return status;
	}

	struct device probe = {};
	probe.parent = root;
	probe.root = root;
	if (pd_fbsd_prepare_device != NULL)
		pd_fbsd_prepare_device(&probe);
	driver_t *driver = __haiku_probe_drivers(&probe, drivers);
	if (driver == NULL) {
		dprintf("%s: no driver claimed the device\n", gDriverName);
		pd_pci_unregister(sPendingPci);
		device_delete_child(NULL, root);
		return B_NOT_SUPPORTED;
	}

	device_t device = device_add_child(root, NULL, 0);
	if (device == NULL) {
		pd_pci_unregister(sPendingPci);
		device_delete_child(NULL, root);
		return B_NO_MEMORY;
	}
	strlcpy(device->device_name, driver->name, sizeof(device->device_name));
	if (pd_fbsd_prepare_device != NULL)
		pd_fbsd_prepare_device(device);
	if (device_set_driver(device, driver) != 0) {
		pd_pci_unregister(sPendingPci);
		device_delete_child(NULL, root);
		return B_ERROR;
	}

	int32 before = gDeviceCount;
	mtx_lock(&Giant);
	if (device->methods.device_probe(device) >= 0 && device_attach(device) == 0
		&& (pd_fbsd_post_attach == NULL || pd_fbsd_post_attach(device) == 0)) {
		dprintf("%s: attached %s\n", gDriverName, device_get_nameunit(device));
	} else {
		dprintf("%s: probe, attach or wlan setup failed\n", gDriverName);
		mtx_unlock(&Giant);
		pd_pci_unregister(sPendingPci);
		device_delete_child(NULL, root);
		return B_ERROR;
	}
	mtx_unlock(&Giant);
	sAttachedDevice = device;
	if (!sWantIfnet)
		return B_OK;

	// the newest ifnet belongs to this device
	sAttached = NULL;
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (gDevices[i] != NULL && gDevices[i]->root_device == root)
			sAttached = gDevices[i];
	}
	if (sAttached == NULL && gDeviceCount > before)
		sAttached = gDevices[gDeviceCount - 1];
	if (sAttached == NULL)
		return B_ERROR;
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (gDevices[i] == sAttached)
			sPciDevs[i] = sPendingPci;
	}
	return B_OK;
}

int
pd_fbsd_init(void)
{
	if (sInitialised)
		return 0;

	status_t status = init_mutexes();
	if (status < B_OK)
		return status;
	status = init_mbufs();
	if (status < B_OK)
		return status;
	status = init_callout();
	if (status < B_OK)
		return status;
	if (HAIKU_DRIVER_REQUIRES(FBSD_TASKQUEUES)) {
		status = init_taskqueues();
		if (status < B_OK)
			return status;
	}
	init_sysinit();
	status = init_wlan_stack();
	if (status < B_OK)
		return status;
	sInitialised = 1;
	return 0;
}

void *
pd_fbsd_attach_pci(void *pcidev)
{
	if (pd_fbsd_init() != 0)
		return NULL;
	sPendingPci = pcidev;
	sAttached = NULL;
	__haiku_init_hardware();
	sPendingPci = NULL;
	return sAttached;
}

void *
pd_fbsd_attach_pci_device(void *pcidev)
{
	if (pd_fbsd_init() != 0)
		return NULL;
	sPendingPci = pcidev;
	sAttachedDevice = NULL;
	sWantIfnet = 0;
	__haiku_init_hardware();
	sWantIfnet = 1;
	sPendingPci = NULL;
	return sAttachedDevice;
}

// a newbus device to stand behind a linux platform device's struct device
device_t
pd_platform_bsddev(const char *name, const char *desc)
{
	static device_t root;
	device_t dev;

	if (root == NULL && init_root_device(&root, BUS_pci) != B_OK)
		return NULL;
	dev = device_add_child(root, name, -1);
	if (dev != NULL)
		device_set_desc(dev, desc);
	return dev;
}

// the linux platform bus of a linuxkpi kext, absent from kexts without linuxkpi
extern void *pd_lkpi_platform_attach(const struct pd_fbsd_platform *pd) __attribute__((weak));

void *
pd_fbsd_attach_platform_device(const struct pd_fbsd_platform *pd)
{
	void *dev;

	if (pd_fbsd_init() != 0 || pd_lkpi_platform_attach == NULL)
		return NULL;
	// the driver's sysinits registered its platform drivers in pd_fbsd_init
	__haiku_init_hardware();
	mtx_lock(&Giant);
	dev = pd_lkpi_platform_attach(pd);
	mtx_unlock(&Giant);
	return dev;
}

// runs the driver's shutdown method, a gpu driver puts its firmware to rest there
void
pd_fbsd_shutdown_device(void *cookie)
{
	device_t device = cookie;

	if (device == NULL || device->methods.device_shutdown == NULL)
		return;
	mtx_lock(&Giant);
	device->methods.device_shutdown(device);
	mtx_unlock(&Giant);
}

void
pd_fbsd_detach(void *cookie)
{
	struct ifnet *ifp = cookie;
	device_t root = ifp->root_device;
	void *pci = NULL;
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (gDevices[i] == ifp) {
			pci = sPciDevs[i];
			sPciDevs[i] = NULL;
			sCookies[i] = NULL;
		}
	}
	pd_fbsd_down(ifp);
	mtx_lock(&Giant);
	device_delete_child(NULL, root);
	mtx_unlock(&Giant);
	if (pci != NULL)
		pd_pci_unregister(pci);
}

static int
cookie_slot(struct ifnet *ifp)
{
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (gDevices[i] == ifp)
			return i;
	}
	return -1;
}

void
pd_fbsd_set_cookie(void *cookie_ifp, void *cookie)
{
	int slot = cookie_slot(cookie_ifp);
	if (slot >= 0)
		sCookies[slot] = cookie;
}

int
pd_fbsd_get_lladdr(void *cookie, uint8_t mac[6])
{
	struct ifnet *ifp = cookie;
	memcpy(mac, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	return 0;
}

uint32_t
pd_fbsd_get_mtu(void *cookie)
{
	struct ifnet *ifp = cookie;
	return ifp->if_mtu;
}

// bringing the interface up follows haiku's compat_open
int
pd_fbsd_up(void *cookie)
{
	struct ifnet *ifp = cookie;
	struct ifreq ifr;

	IFF_LOCKGIANT(ifp);
	if (ifp->if_init != NULL)
		ifp->if_init(ifp->if_softc);

	ifp->if_flags &= ~IFF_UP;
	ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL);

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_media = IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0);
	if (ifp->if_ioctl(ifp, SIOCSIFMEDIA, (caddr_t)&ifr) != 0) {
		ifr.ifr_media = IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0);
		ifp->if_ioctl(ifp, SIOCSIFMEDIA, (caddr_t)&ifr);
	}

	ifp->if_flags |= IFF_UP;
	ifp->flags &= ~DEVICE_CLOSED;
	int status = ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

int
pd_fbsd_down(void *cookie)
{
	struct ifnet *ifp = cookie;
	IFF_LOCKGIANT(ifp);
	ifp->flags |= DEVICE_CLOSED;
	ifp->if_flags &= ~IFF_UP;
	int status = ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

int
pd_fbsd_transmit(void *cookie, const void *frame, size_t len)
{
	struct ifnet *ifp = cookie;
	struct mbuf *mb;

	if ((ifp->flags & DEVICE_CLOSED) != 0)
		return ENETDOWN;
	if (len <= MHLEN)
		mb = m_gethdr(M_NOWAIT, MT_DATA);
	else
		mb = m_get2(len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (mb == NULL)
		return ENOBUFS;
	if (len > (size_t)M_SIZE(mb)) {
		m_freem(mb);
		return E2BIG;
	}
	memcpy(mtod(mb, void *), frame, len);
	mb->m_pkthdr.len = mb->m_len = len;

	// a net80211 vap takes ethernet frames on if_transmit, its if_output
	// wants raw 802.11 with a sockaddr
	IFF_LOCKGIANT(ifp);
	int result = pd_fbsd_frames_on_transmit && ifp->if_transmit != NULL
		? ifp->if_transmit(ifp, mb) : ifp->if_output(ifp, mb, NULL, NULL);
	IFF_UNLOCKGIANT(ifp);
	return result;
}

int
pd_fbsd_set_promisc(void *cookie, int on)
{
	struct ifnet *ifp = cookie;
	if (on)
		ifp->if_flags |= IFF_PROMISC;
	else
		ifp->if_flags &= ~IFF_PROMISC;
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

int
pd_fbsd_set_allmulti(void *cookie, int on)
{
	struct ifnet *ifp = cookie;
	if (on)
		ifp->if_flags |= IFF_ALLMULTI;
	else
		ifp->if_flags &= ~IFF_ALLMULTI;
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

int
pd_fbsd_add_multi(void *cookie, const uint8_t mac[6])
{
	struct ifnet *ifp = cookie;
	struct sockaddr_dl address;
	if ((ifp->if_flags & IFF_MULTICAST) == 0)
		return B_NOT_SUPPORTED;
	memset(&address, 0, sizeof(address));
	address.sdl_family = AF_LINK;
	memcpy(LLADDR(&address), mac, ETHER_ADDR_LEN);
	IFF_LOCKGIANT(ifp);
	int status = if_addmulti(ifp, (struct sockaddr *)&address, NULL);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

int
pd_fbsd_get_link(void *cookie, int *up, uint64_t *baudrate)
{
	struct ifnet *ifp = cookie;
	struct ifmediareq mediareq;
	memset(&mediareq, 0, sizeof(mediareq));
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCGIFMEDIA, (caddr_t)&mediareq);
	IFF_UNLOCKGIANT(ifp);
	if (status != 0)
		return status;
	*up = (mediareq.ifm_status & IFM_ACTIVE) != 0;
	*baudrate = ifmedia_baudrate(mediareq.ifm_active);
	return 0;
}

static void
describe_medium(int word, struct pd_fbsd_medium *medium)
{
	medium->word = word;
	medium->full_duplex = (word & IFM_FDX) != 0;
	switch (IFM_SUBTYPE(word)) {
		case IFM_10_T: medium->speed = 10; break;
		case IFM_100_TX: medium->speed = 100; break;
		case IFM_1000_T: medium->speed = 1000; break;
		case IFM_AUTO: medium->speed = 0; break;
		default: medium->speed = ifmedia_baudrate(word) / 1000000; break;
	}
}

int
pd_fbsd_media_list(void *cookie, struct pd_fbsd_medium *media, int max)
{
	struct ifnet *ifp = cookie;
	struct ifmediareq mediareq;
	int words[32];

	memset(&mediareq, 0, sizeof(mediareq));
	mediareq.ifm_ulist = words;
	mediareq.ifm_count = 32;
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCGIFMEDIA, (caddr_t)&mediareq);
	IFF_UNLOCKGIANT(ifp);
	if (status != 0)
		return 0;
	int count = mediareq.ifm_count < 32 ? mediareq.ifm_count : 32;
	if (count > max)
		count = max;
	for (int i = 0; i < count; i++)
		describe_medium(words[i], &media[i]);
	return count;
}

int
pd_fbsd_active_media(void *cookie, struct pd_fbsd_medium *active)
{
	struct ifnet *ifp = cookie;
	struct ifmediareq mediareq;
	memset(&mediareq, 0, sizeof(mediareq));
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCGIFMEDIA, (caddr_t)&mediareq);
	IFF_UNLOCKGIANT(ifp);
	if (status != 0)
		return status;
	describe_medium(mediareq.ifm_active, active);
	return 0;
}

int
pd_fbsd_set_media(void *cookie, int32_t word)
{
	struct ifnet *ifp = cookie;
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_media = word;
	IFF_LOCKGIANT(ifp);
	int status = ifp->if_ioctl(ifp, SIOCSIFMEDIA, (caddr_t)&ifr);
	IFF_UNLOCKGIANT(ifp);
	return status;
}

// the receive side, called from ether_input in if.c

// set by the wlan supplicant, it takes 802.1X frames and returns 1 when it consumed one
int (*pd_fbsd_eapol_input)(struct ifnet *ifp, struct mbuf *m);

static bool
is_eapol(struct mbuf *m)
{
	if (m->m_len < 14)
		return false;
	const uint8_t *frame = mtod(m, const uint8_t *);
	return frame[12] == 0x88 && frame[13] == 0x8e;
}

void
pd_netif_input(struct ifnet *ifp, struct mbuf *m)
{
	int slot = cookie_slot(ifp);
	void *cookie = slot >= 0 ? sCookies[slot] : NULL;

	while (m != NULL) {
		struct mbuf *next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (pd_fbsd_eapol_input != NULL && is_eapol(m) && pd_fbsd_eapol_input(ifp, m)) {
			m = next;
			continue;
		}
		if (cookie != NULL && (ifp->flags & DEVICE_CLOSED) == 0) {
			if (m->m_next == NULL)
				pdos_net_input(cookie, mtod(m, void *), m->m_len);
			else
				pdos_net_input_chain(cookie, m);
		}
		m_freem(m);
		m = next;
	}
}

void
pd_netif_link(struct ifnet *ifp, int linkState)
{
	int slot = cookie_slot(ifp);
	if (slot < 0 || sCookies[slot] == NULL)
		return;
	pdos_net_link(sCookies[slot], linkState == LINK_STATE_UP,
		ifp->if_baudrate);
}

// walks a chain for the xnu side, which cannot see struct mbuf
int
pd_fbsd_mbuf_copydata(void *chain, void *buffer, size_t max)
{
	struct mbuf *m = chain;
	size_t len = m->m_pkthdr.len;
	if (len > max)
		len = max;
	m_copydata(m, 0, len, buffer);
	return len;
}
