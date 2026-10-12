// uACPI's full-mode kernel interface over IOKit and xnu: allocation, locks, events, port and pci
// config access, timing, deferred work and the SCI. Table mapping and logging stay in PDACPIGlue.cpp.

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterrupts.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <architecture/i386/pio.h>

extern "C" {
#include <uacpi/kernel_api.h>
boolean_t ml_set_interrupts_enabled(boolean_t enable);
}

#include "PDACPIPlatformExpert.h"

extern "C" {

// uacpi_kernel_free gets no size, so each block carries its own in front
#define kAllocHeader 16

void *uacpi_kernel_alloc(uacpi_size size)
{
    uint8_t *p = (uint8_t *)IOMalloc(size + kAllocHeader);
    if (p == NULL)
        return UACPI_NULL;
    *(uacpi_size *)p = size + kAllocHeader;
    return p + kAllocHeader;
}

void uacpi_kernel_free(void *mem)
{
    if (mem == UACPI_NULL)
        return;
    uint8_t *p = (uint8_t *)mem - kAllocHeader;
    IOFree(p, *(uacpi_size *)p);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    uint64_t ns;
    absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
    return ns;
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    IODelay(usec);
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    IOSleep((unsigned)msec);
}

// legacy cf8/cfc configuration access covers the 256 bytes aml touches on x86 firmware
struct PDACPIPciDevice {
    uacpi_pci_address address;
};

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    if (address.segment != 0)
        return UACPI_STATUS_UNIMPLEMENTED;
    PDACPIPciDevice *dev = (PDACPIPciDevice *)IOMalloc(sizeof(*dev));
    if (dev == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;
    dev->address = address;
    *out_handle = dev;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    IOFree(handle, sizeof(PDACPIPciDevice));
}

static IOSimpleLock *gPciLock;

static uint32_t pciRead(uacpi_handle handle, uacpi_size offset)
{
    PDACPIPciDevice *dev = (PDACPIPciDevice *)handle;
    uint32_t addr = 0x80000000u | ((uint32_t)dev->address.bus << 16) | ((uint32_t)dev->address.device << 11) |
                    ((uint32_t)dev->address.function << 8) | ((uint32_t)offset & 0xfc);
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(gPciLock);
    outl(0xcf8, addr);
    uint32_t v = inl(0xcfc);
    IOSimpleLockUnlockEnableInterrupt(gPciLock, is);
    return v;
}

static void pciWrite(uacpi_handle handle, uacpi_size offset, uint32_t value, uint32_t mask)
{
    PDACPIPciDevice *dev = (PDACPIPciDevice *)handle;
    uint32_t addr = 0x80000000u | ((uint32_t)dev->address.bus << 16) | ((uint32_t)dev->address.device << 11) |
                    ((uint32_t)dev->address.function << 8) | ((uint32_t)offset & 0xfc);
    uint32_t shift = (offset & 3) * 8;
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(gPciLock);
    outl(0xcf8, addr);
    uint32_t v = mask == 0xffffffffu ? value : (inl(0xcfc) & ~(mask << shift)) | ((value & mask) << shift);
    outl(0xcf8, addr);
    outl(0xcfc, v);
    IOSimpleLockUnlockEnableInterrupt(gPciLock, is);
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle d, uacpi_size off, uacpi_u8 *v)
{
    if (off > 0xff) return UACPI_STATUS_UNIMPLEMENTED;
    *v = (uacpi_u8)(pciRead(d, off) >> ((off & 3) * 8));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle d, uacpi_size off, uacpi_u16 *v)
{
    if (off > 0xfe) return UACPI_STATUS_UNIMPLEMENTED;
    *v = (uacpi_u16)(pciRead(d, off) >> ((off & 2) * 8));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle d, uacpi_size off, uacpi_u32 *v)
{
    if (off > 0xfc) return UACPI_STATUS_UNIMPLEMENTED;
    *v = pciRead(d, off);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle d, uacpi_size off, uacpi_u8 v)
{
    if (off > 0xff) return UACPI_STATUS_UNIMPLEMENTED;
    pciWrite(d, off, v, 0xff);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle d, uacpi_size off, uacpi_u16 v)
{
    if (off > 0xfe) return UACPI_STATUS_UNIMPLEMENTED;
    pciWrite(d, off, v, 0xffff);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle d, uacpi_size off, uacpi_u32 v)
{
    if (off > 0xfc) return UACPI_STATUS_UNIMPLEMENTED;
    pciWrite(d, off, v, 0xffffffffu);
    return UACPI_STATUS_OK;
}

// an io "mapping" is just its base port
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    if (base > 0xffff)
        return UACPI_STATUS_INVALID_ARGUMENT;
    *out_handle = (uacpi_handle)(uintptr_t)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle)
{
}

#define PORT(h, off) ((uint16_t)((uintptr_t)(h) + (off)))

uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size off, uacpi_u8 *v) { *v = inb(PORT(h, off)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off, uacpi_u16 *v) { *v = inw(PORT(h, off)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off, uacpi_u32 *v) { *v = inl(PORT(h, off)); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off, uacpi_u8 v) { outb(PORT(h, off), v); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off, uacpi_u16 v) { outw(PORT(h, off), v); return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off, uacpi_u32 v) { outl(PORT(h, off), v); return UACPI_STATUS_OK; }

// aml mutexes take a timeout in ms, 0xffff waits forever
uacpi_handle uacpi_kernel_create_mutex(void)
{
    return IOLockAlloc();
}

void uacpi_kernel_free_mutex(uacpi_handle h)
{
    IOLockFree((IOLock *)h);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 timeout)
{
    IOLock *lock = (IOLock *)h;
    if (timeout == 0xffff) {
        IOLockLock(lock);
        return UACPI_STATUS_OK;
    }
    for (uint32_t waited = 0;; waited++) {
        if (IOLockTryLock(lock))
            return UACPI_STATUS_OK;
        if (waited >= timeout)
            return UACPI_STATUS_TIMEOUT;
        IOSleep(1);
    }
}

void uacpi_kernel_release_mutex(uacpi_handle h)
{
    IOLockUnlock((IOLock *)h);
}

// a counting event: signal adds one, a wait takes one
struct PDACPIEvent {
    IOLock  *lock;
    uint32_t count;
};

uacpi_handle uacpi_kernel_create_event(void)
{
    PDACPIEvent *e = (PDACPIEvent *)IOMallocZero(sizeof(*e));
    if (e == NULL)
        return UACPI_NULL;
    e->lock = IOLockAlloc();
    return e;
}

void uacpi_kernel_free_event(uacpi_handle h)
{
    PDACPIEvent *e = (PDACPIEvent *)h;
    IOLockFree(e->lock);
    IOFree(e, sizeof(*e));
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle h, uacpi_u16 timeout)
{
    PDACPIEvent *e = (PDACPIEvent *)h;
    uint64_t deadline = 0;
    if (timeout != 0xffff)
        clock_interval_to_deadline(timeout, kMillisecondScale, &deadline);

    IOLockLock(e->lock);
    while (e->count == 0) {
        if (timeout == 0)
            break;
        int r = timeout == 0xffff ? IOLockSleep(e->lock, e, THREAD_UNINT)
                                  : IOLockSleepDeadline(e->lock, e, deadline, THREAD_UNINT);
        if (r == THREAD_TIMED_OUT)
            break;
    }
    bool got = e->count > 0;
    if (got)
        e->count--;
    IOLockUnlock(e->lock);
    return got;
}

void uacpi_kernel_signal_event(uacpi_handle h)
{
    PDACPIEvent *e = (PDACPIEvent *)h;
    IOLockLock(e->lock);
    e->count++;
    IOLockWakeup(e->lock, e, true);
    IOLockUnlock(e->lock);
}

void uacpi_kernel_reset_event(uacpi_handle h)
{
    PDACPIEvent *e = (PDACPIEvent *)h;
    IOLockLock(e->lock);
    e->count = 0;
    IOLockUnlock(e->lock);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return (uacpi_thread_id)current_thread();
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void)
{
    return ml_set_interrupts_enabled(FALSE);
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state)
{
    ml_set_interrupts_enabled((boolean_t)state);
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    return IOSimpleLockAlloc();
}

void uacpi_kernel_free_spinlock(uacpi_handle h)
{
    IOSimpleLockFree((IOSimpleLock *)h);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h)
{
    return (uacpi_cpu_flags)IOSimpleLockLockDisableInterrupt((IOSimpleLock *)h);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags flags)
{
    IOSimpleLockUnlockEnableInterrupt((IOSimpleLock *)h, (IOInterruptState)flags);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req)
{
    switch (req->type) {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        IOLog("PDACPIPlatform: aml breakpoint\n");
        break;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        IOLog("PDACPIPlatform: aml fatal type 0x%x code 0x%x arg 0x%llx\n", req->fatal.type, req->fatal.code,
              (uint64_t)req->fatal.arg);
        break;
    }
    return UACPI_STATUS_OK;
}

// gpe methods and notifications run on thread calls, counted so a caller can wait for all of them
struct PDACPIWork {
    uacpi_work_handler handler;
    uacpi_handle       ctx;
    thread_call_t      call;
};

static IOLock  *gWorkLock;
static uint32_t gWorkPending;

static void pd_acpi_run_work(thread_call_param_t p0, thread_call_param_t)
{
    PDACPIWork *w = (PDACPIWork *)p0;
    w->handler(w->ctx);
    thread_call_free(w->call);
    IOFree(w, sizeof(*w));

    IOLockLock(gWorkLock);
    if (--gWorkPending == 0)
        IOLockWakeup(gWorkLock, &gWorkPending, false);
    IOLockUnlock(gWorkLock);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx)
{
    PDACPIWork *w = (PDACPIWork *)IOMalloc(sizeof(*w));
    if (w == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;
    w->handler = handler;
    w->ctx = ctx;
    w->call = thread_call_allocate(pd_acpi_run_work, w);
    if (w->call == NULL) {
        IOFree(w, sizeof(*w));
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    IOLockLock(gWorkLock);
    gWorkPending++;
    IOLockUnlock(gWorkLock);
    thread_call_enter(w->call);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    IOLockLock(gWorkLock);
    while (gWorkPending != 0)
        IOLockSleep(gWorkLock, &gWorkPending, THREAD_UNINT);
    IOLockUnlock(gWorkLock);
    return UACPI_STATUS_OK;
}

// the sci: uacpi asks once, the platform expert attaches it to a nub once the ioapic is up
struct PDACPIIrq {
    uacpi_interrupt_handler handler;
    uacpi_handle            ctx;
    uacpi_u32               irq;
};

static PDACPIIrq gSci;

static void pd_acpi_sci(OSObject *, void *refCon, IOService *, int)
{
    PDACPIIrq *irq = (PDACPIIrq *)refCon;
    irq->handler(irq->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
                                                    uacpi_handle *out_irq_handle)
{
    if (gSci.handler != UACPI_NULL)
        return UACPI_STATUS_ALREADY_EXISTS;
    gSci.handler = handler;
    gSci.ctx = ctx;
    gSci.irq = irq;
    *out_irq_handle = &gSci;
    PDACPIPlatformExpert::attachSCI(irq, (void *)pd_acpi_sci, &gSci);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    PDACPIPlatformExpert::detachSCI();
    gSci.handler = UACPI_NULL;
    return UACPI_STATUS_OK;
}

} // extern "C"

void pd_acpi_kernel_init(void)
{
    if (gPciLock == NULL)
        gPciLock = IOSimpleLockAlloc();
    if (gWorkLock == NULL)
        gWorkLock = IOLockAlloc();
}
