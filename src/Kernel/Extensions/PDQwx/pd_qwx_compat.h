// SPDX-License-Identifier: MIT
// the openbsd pci, bus_space and bus_dma interfaces qwx(4) uses, carried out by PDQwx.cpp
#ifndef PD_QWX_COMPAT_H
#define PD_QWX_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

// autoconf

struct device {
	char		dv_xname[16];
};
struct cfattach {
	size_t		ca_devsize;
	int		(*ca_match)(struct device *, void *, void *);
	void		(*ca_attach)(struct device *, struct device *, void *);
	int		(*ca_detach)(struct device *, int);
	int		(*ca_activate)(struct device *, int);
};
struct cfdriver {
	void		**cd_devs;
	const char	*cd_name;
	int		cd_class;
};
#define DV_IFNET	1
#define DVACT_QUIESCE	2
#define DVACT_SUSPEND	3
#define DVACT_RESUME	4
#define DVACT_WAKEUP	5
#define DVACT_POWERDOWN	6
// PDQwx runs the hook itself once the root file system is mounted
void	pd_qwx_mountroot(struct device *, void (*)(struct device *));
#define config_mountroot(dev, fn)	pd_qwx_mountroot((dev), (fn))

// registers

typedef int		bus_space_tag_t;
typedef uintptr_t	bus_space_handle_t;
typedef uint64_t	bus_size_t;
typedef uint64_t	bus_addr_t;

static inline uint32_t
bus_space_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	(void)t;
	return OSSwapLittleToHostInt32(*(volatile uint32_t *)(h + o));
}

static inline void
bus_space_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint32_t v)
{
	(void)t;
	*(volatile uint32_t *)(h + o) = OSSwapHostToLittleInt32(v);
}

#define BUS_SPACE_BARRIER_READ	1
#define BUS_SPACE_BARRIER_WRITE	2
#define bus_space_barrier(t, h, o, l, f)	__sync_synchronize()

// dma, every map and segment remembers the iokit objects behind it

typedef struct pd_qwx_dma *bus_dma_tag_t;

typedef struct {
	bus_addr_t	ds_addr;
	bus_size_t	ds_len;
	void		*_ds_mem;
} bus_dma_segment_t;

struct bus_dmamap {
	bus_size_t	dm_mapsize;
	int		dm_nsegs;
	int		_dm_maxsegs;
	bus_size_t	_dm_size;
	void		*_dm_cmd;
	void		*_dm_md;
	bus_dma_segment_t dm_segs[1];
};
typedef struct bus_dmamap *bus_dmamap_t;

#define BUS_DMA_WAITOK		0x0000
#define BUS_DMA_NOWAIT		0x0001
#define BUS_DMA_ALLOCNOW	0x0002
#define BUS_DMA_COHERENT	0x0004
#define BUS_DMA_ZERO		0x0008
#define BUS_DMA_64BIT		0x0010
#define BUS_DMA_READ		0x0200
#define BUS_DMA_WRITE		0x0400
#define BUS_DMASYNC_PREREAD	0x01
#define BUS_DMASYNC_POSTREAD	0x02
#define BUS_DMASYNC_PREWRITE	0x04
#define BUS_DMASYNC_POSTWRITE	0x08

int	bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t, bus_size_t, int, bus_dmamap_t *);
void	bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t, void *, int);
int	bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *, int);
int	bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *, int, bus_size_t, int);
void	bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t, bus_size_t, int);
int	bus_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t, bus_dma_segment_t *, int, int *, int);
int	bus_dmamem_alloc_range(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t, bus_dma_segment_t *, int, int *, int, bus_addr_t, bus_addr_t);
void	bus_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int	bus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t, caddr_t *, int);
void	bus_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);

// pci, the tag is the IOPCIDevice

typedef uint32_t	pcireg_t;
typedef void		*pci_chipset_tag_t;
typedef void		*pcitag_t;
typedef struct {
	int		ih_index;
} pci_intr_handle_t;

struct pci_attach_args {
	pci_chipset_tag_t pa_pc;
	pcitag_t	pa_tag;
	bus_dma_tag_t	pa_dmat;
	pcireg_t	pa_id;
};
struct pci_matchid {
	uint16_t	pm_vid;
	uint16_t	pm_pid;
};

#define PCI_VENDOR(id)		((id) & 0xffff)
#define PCI_PRODUCT(id)		(((id) >> 16) & 0xffff)
#define PCI_VENDOR_QUALCOMM	0x17cb
#define PCI_PRODUCT_QUALCOMM_QCNFA765	0x1103
#define PCI_ID_REG		0x00
#define PCI_COMMAND_STATUS_REG	0x04
#define PCI_COMMAND_MASTER_ENABLE	0x00000004
#define PCI_MAPREG_START	0x10
#define PCI_SUBSYS_ID_REG	0x2c
#define PCI_MAPREG_TYPE_MEM	0x00000000
#define PCI_MAPREG_MEM_TYPE_64BIT	0x00000004
#define PCI_CAP_MSI		0x05
#define PCI_CAP_PCIEXPRESS	0x10
#define PCI_MSI_MC		0x00
#define PCI_MSI_MC_C64		0x00800000
#define PCI_MSI_MC_MSIE		0x00010000
#define PCI_MSI_MA		0x04
#define PCI_MSI_MAU32		0x08
#define PCI_MSI_MD32		0x08
#define PCI_MSI_MD64		0x0c
#define PCI_PCIE_LCSR		0x10
#define PCI_PCIE_LCSR_ASPM_L0S	0x00000001
#define PCI_PCIE_LCSR_ASPM_L1	0x00000002
#define PCI_PMCSR_STATE_D0	0x0000

pcireg_t pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void	pci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);
int	pci_get_capability(pci_chipset_tag_t, pcitag_t, int, int *, pcireg_t *);
pcireg_t pci_mapreg_type(pci_chipset_tag_t, pcitag_t, int);
int	pci_mapreg_map(struct pci_attach_args *, int, pcireg_t, int, bus_space_tag_t *,
	    bus_space_handle_t *, bus_addr_t *, bus_size_t *, bus_size_t);
int	pci_matchbyid(struct pci_attach_args *, const struct pci_matchid *, int);
int	pci_set_powerstate(pci_chipset_tag_t, pcitag_t, int);
int	pci_intr_enable_msivec(struct pci_attach_args *, int);
int	pci_intr_map_msi(struct pci_attach_args *, pci_intr_handle_t *);
int	pci_intr_map_msivec(struct pci_attach_args *, int, pci_intr_handle_t *);
int	pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
void	*pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t, int, int (*)(void *), void *, const char *);
void	pci_intr_disestablish(pci_chipset_tag_t, void *);

// the rest of the kernel qwx touches

#define le16toh(x)	OSSwapLittleToHostInt16(x)
#define le32toh(x)	OSSwapLittleToHostInt32(x)
#define le64toh(x)	OSSwapLittleToHostInt64(x)

#define IPL_MPSAFE	0
#define PZERO		22
#define RW_WRITE	0x01
#define RW_NOSLEEP	0x02
#define RW_INTR		0x04
#define rw_enter(l, f)	(pd80211_lock(), 0)
#define rw_exit(l)	pd80211_unlock()
#define rw_assert_wrlock(l)	((void)0)
#define taskq_create(n, t, ipl, f)	((struct taskq *)systq)
#define taskq_destroy(tq)	((void)0)

struct refcnt {
	volatile int	r_refs;
};
void	refcnt_init(struct refcnt *);
void	refcnt_take(struct refcnt *);
int	refcnt_rele(struct refcnt *);
void	refcnt_rele_wake(struct refcnt *);
void	refcnt_finalize(struct refcnt *, const char *);

int	loadfirmware(const char *, u_char **, size_t *);
int	pd_qwx_m_defrag(struct mbuf **, int);
#define m_defrag(m, how)	pd_qwx_m_defrag(&(m), (how))
struct ifnet;
int	if_setlladdr(struct ifnet *, const uint8_t *);

#ifdef __cplusplus
}
#endif

#endif
