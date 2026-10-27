// SPDX-License-Identifier: MIT
// the seam between the FreeBSD side and the xnu side of fbsdkpi
// fixed-width types and opaque pointers only, both sides include it
#ifndef PD_FBSD_OS_H
#define PD_FBSD_OS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// console and failure
void pdos_log(const char *msg);
// the fbsdkpi_debug boot-arg turns on the layer's and drivers' own messages
int pdos_debug_enabled(void);
// an integer boot-arg, def when it is absent
int pdos_bootarg_int(const char *name, int def);
int pdos_bootarg_str(const char *name, char *buf, int len);
void pdos_panic(const char *msg) __attribute__((noreturn));

// time, microseconds since boot
int64_t pdos_uptime_us(void);
void pdos_delay_us(uint64_t us);
void pdos_sleep_us(uint64_t us);

// memory, sizes are remembered by the backend
void *pdos_malloc(size_t size, int zero, int canwait);
void pdos_free(void *ptr);
size_t pdos_malloc_size(void *ptr);
void *pdos_malloc_contig(size_t size, size_t align, uint64_t maxaddr, uint64_t *phys);
void *pdos_malloc_contig_wc(size_t size, size_t align, uint64_t maxaddr, uint64_t *phys);
void pdos_free_contig(void *ptr, size_t size);
uint64_t pdos_vtophys(const void *va);
// uncached: 0 cached, 1 uncached, 2 write-combining
void *pdos_map_phys(uint64_t phys, size_t size, int uncached);
void pdos_unmap_phys(void *va, size_t size);
// maps a physically contiguous range for a device's dma through its iommu,
// bouncing it below 2^addrbits when needed, NULL on failure
void *pdos_dma_map_phys(void *pcidev, uint64_t phys, uint64_t len, int addrbits,
	uint64_t *busaddr);
// copies a bounced mapping toward the device (out) or back from it (in)
void pdos_dma_sync(void *cookie, int to_device);
void pdos_dma_unmap(void *cookie);
// reads firmware/<name> from the kext bundle's Resources once the root
// filesystem is mounted, the buffer goes back through pdos_free_firmware
int pdos_load_firmware(const char *name, void **data, size_t *size);
void pdos_free_firmware(void *data, size_t size);
int pdos_dma_has_mapper(void *pcidev);

// sleepable mutexes, kernel thread context only
void *pdos_mtx_alloc(const char *name);
void pdos_mtx_free(void *m);
void pdos_mtx_lock(void *m);
int pdos_mtx_trylock(void *m);
void pdos_mtx_unlock(void *m);

// spin locks, usable from interrupt context
void *pdos_spin_alloc(void);
void pdos_spin_free(void *s);
void pdos_spin_lock(void *s);
void pdos_spin_unlock(void *s);

// interrupt state on this cpu, returns the previous state
int pdos_intr_disable(void);
void pdos_intr_restore(int state);

// reader writer locks
void *pdos_rw_alloc(void);
void pdos_rw_free(void *rw);
void pdos_rw_rlock(void *rw);
void pdos_rw_runlock(void *rw);
void pdos_rw_wlock(void *rw);
void pdos_rw_wunlock(void *rw);
int pdos_rw_try_rlock(void *rw);
int pdos_rw_try_wlock(void *rw);
void pdos_rw_downgrade(void *rw);

// one global wait lock with event sleeps, interrupt safe
// sleep releases the lock while blocked and returns with it held
int pdos_wait_lock(void);
void pdos_wait_unlock(int state);
#define PDOS_WAIT_OK		0
#define PDOS_WAIT_TIMEOUT	1
#define PDOS_WAIT_INTR		2
int pdos_wait_sleep(const void *event, int64_t timeout_us, int interruptible, int *state);
void pdos_wakeup(const void *event, int all);

// kernel threads
void *pdos_thread_start(void (*fn)(void *), void *arg);
void *pdos_thread_current(void);
void pdos_thread_exit(void) __attribute__((noreturn));
void pdos_thread_set_priority(void *thread, int prio);
int pdos_cpu_count(void);
int pdos_cpu_number(void);

// timer callbacks run on a thread
void *pdos_tcall_alloc(void (*fn)(void *, void *), void *arg0);
void pdos_tcall_free(void *tc);
void pdos_tcall_enter_us(void *tc, void *arg1, uint64_t us);
int pdos_tcall_cancel(void *tc);

// pci, a device is the opaque IOPCIDevice registered by the driver class
uint32_t pdos_pci_read(void *dev, uint32_t off, int size);
void pdos_pci_write(void *dev, uint32_t off, int size, uint32_t val);
int pdos_pci_location(void *dev, uint8_t *bus, uint8_t *slot, uint8_t *func);
int pdos_pci_enumerate(void **devs, int max);
uint64_t pdos_pci_bar_map(void *dev, int bar, uint64_t *size, uint64_t *phys);
void pdos_pci_bar_unmap(void *dev, int bar);
uint32_t pdos_pci_io_read(void *dev, uint32_t port, int size);
void pdos_pci_io_write(void *dev, uint32_t port, int size, uint32_t val);
int pdos_pci_msi_count(void *dev);
int pdos_pci_msix_count(void *dev);

// interrupts, the handler runs at primary interrupt level
// pci index 0 is the legacy line and 1 up are msi vectors, a device tree index n is the n-th interrupt
typedef int (*pdos_intr_fn)(void *arg);
void *pdos_intr_install(void *dev, int index, pdos_intr_fn fn, void *arg);
void pdos_intr_remove(void *cookie);

// device tree, a node is an opaque IORegistryEntry in the device tree plane
// property data stays valid while the node does, returns 0 when found
int pdos_dt_prop(void *node, const char *name, const void **data, uint32_t *len);
void *pdos_dt_parent(void *node);
// children in order, NULL past the last one
void *pdos_dt_child(void *node, int index);
void *pdos_dt_find_phandle(uint32_t phandle);
void *pdos_dt_root(void);
void *pdos_dt_find_compatible(const char *compat);
// the node's name with its unit address, e.g. "gpu@1c00000"
void pdos_dt_name(void *node, char *buf, uint32_t len);
// the network boundary, called by the FreeBSD side
// the ifnet pointer identifies the controller
void pdos_pci_note(void *dev);
int pdos_copyin(const void *uaddr, void *kaddr, size_t len);
int pdos_copyout(const void *kaddr, void *uaddr, size_t len);
void *pdos_map_pages(const uint64_t *phys, size_t count, int uncached);
void pdos_unmap_pages(void *va);
int pdos_acpi_get_table(const char *signature, uint32_t instance, void **data,
	size_t *size);
// gives back a table copy from pdos_acpi_get_table
void pdos_acpi_put_table(void *data, size_t size);
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len);
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf);
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate);

#ifdef __cplusplus
}
#endif

#endif
