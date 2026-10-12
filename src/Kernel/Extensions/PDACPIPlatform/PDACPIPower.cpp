// full acpi on top of the early tables: the aml namespace, the sci, the power button, and s5 and reset through uacpi

#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <kern/thread_call.h>
// kern_event.h has no c linkage of its own
extern "C" {
#include <sys/kern_event.h>
}

extern "C" {
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/notify.h>
#include <uacpi/utilities.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
}

#include "PDACPIPlatformExpert.h"

// AppleAPIC's specifier flags (PICShared.h)
enum { kSciLevel = 0x01, kSciActiveLow = 0x02, kSciShareable = 0x04 };

#define super IOACPIPlatformExpert

extern "C" void kprintf(const char *fmt, ...);
void pd_acpi_kernel_init(void);

// a power button press, delivered to whoever watches this service
OSDefineMetaClassAndStructors(PDACPIPowerButton, IOService);

static PDACPIPlatformExpert *sExpert;
static PDACPIPowerButton    *sPowerButton;
static thread_call_t         sInitCall, sSciCall, sButtonCall;
static bool                  sNamespaceReady;

static UInt32       sSciIrq;
static void        *sSciHandler, *sSciRef;
static IOService   *sSciNub;

// the madt's override for the sci, or acpi's default of level, active low
static UInt32 pd_sci_flags(UInt32 irq)
{
    UInt32 flags = kSciLevel | kSciActiveLow | kSciShareable;
    uacpi_table table;
    if (uacpi_unlikely_error(uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table)))
        return flags;

    struct acpi_madt *madt = (struct acpi_madt *)table.ptr;
    for (uint32_t off = sizeof(*madt); off + sizeof(struct acpi_entry_hdr) <= madt->hdr.length;) {
        struct acpi_entry_hdr *e = (struct acpi_entry_hdr *)((uint8_t *)madt + off);
        if (e->length < sizeof(*e)) break;
        if (e->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE &&
            e->length >= sizeof(struct acpi_madt_interrupt_source_override)) {
            struct acpi_madt_interrupt_source_override *o = (struct acpi_madt_interrupt_source_override *)e;
            if (o->source == irq || o->gsi == irq) {
                // mps inti flags: polarity 1 high, 3 low, trigger 1 edge, 3 level, 0 conforms (level, low for the sci)
                UInt32 pol = o->flags & 3, trig = (o->flags >> 2) & 3;
                flags = kSciShareable;
                if (trig != 1) flags |= kSciLevel;
                if (pol != 1) flags |= kSciActiveLow;
                kprintf("PDACPIPlatform: sci gsi %u override flags 0x%x\n", irq, o->flags);
            }
        }
        off += e->length;
    }
    uacpi_table_unref(&table);
    return flags;
}

// registerInterrupt waits for the ioapic's controller, so the nub is made on a thread
static void pd_sci_attach(thread_call_param_t, thread_call_param_t)
{
    IOService *nub = new IOService;
    if (nub == NULL || !nub->init()) {
        OSSafeReleaseNULL(nub);
        return;
    }
    nub->setName("acpi-sci");
    UInt32 flags = pd_sci_flags(sSciIrq);
    sExpert->setNubInterruptVector(nub, sSciIrq, flags);
    nub->attach(sExpert);

    IOReturn r = nub->registerInterrupt(0, sExpert, (IOInterruptAction)sSciHandler, sSciRef);
    if (r == kIOReturnSuccess)
        r = nub->enableInterrupt(0);
    kprintf("PDACPIPlatform: sci on gsi %u, flags 0x%x: 0x%x\n", sSciIrq, flags, r);
    sSciNub = nub;
}

void PDACPIPlatformExpert::attachSCI(UInt32 irq, void *handler, void *ref)
{
    sSciIrq = irq;
    sSciHandler = handler;
    sSciRef = ref;
    if (sSciCall == NULL)
        sSciCall = thread_call_allocate(pd_sci_attach, NULL);
    thread_call_enter(sSciCall);
}

void PDACPIPlatformExpert::detachSCI(void)
{
    if (sSciNub == NULL)
        return;
    sSciNub->disableInterrupt(0);
    sSciNub->unregisterInterrupt(0);
    sSciNub->detach(sExpert);
    OSSafeReleaseNULL(sSciNub);
}

// the root domain hears about it as on apple hardware, the shutdown policy lives in userspace
static void pd_power_button(thread_call_param_t, thread_call_param_t)
{
    kprintf("PDACPIPlatform: power button\n");
    if (IOPMrootDomain *root = IOService::getPMRootDomain())
        root->receivePowerNotification(kIOPMPowerButton);
    if (sPowerButton)
        sPowerButton->messageClients(kPDACPIMessagePowerButton);

    // and a kernel event, which pdpowerd reads with nothing but libSystem
    struct kev_msg ev = {};
    if (kev_vendor_code_find(kPDACPIEventVendor, &ev.vendor_code) == 0) {
        ev.kev_class = kPDACPIEventClass;
        ev.kev_subclass = kPDACPIEventSubclass;
        ev.event_code = kPDACPIEventPowerButton;
        kev_msg_post(&ev);
    }
}

static uacpi_interrupt_ret pd_fixed_power_button(uacpi_handle)
{
    thread_call_enter(sButtonCall);
    return UACPI_INTERRUPT_HANDLED;
}

// a control method power button, PNP0C0C, notifies 0x80 when pressed
static uacpi_status pd_button_notify(uacpi_handle, uacpi_namespace_node *, uacpi_u64 value)
{
    if (value == 0x80)
        thread_call_enter(sButtonCall);
    return UACPI_STATUS_OK;
}

static uacpi_iteration_decision pd_found_button(void *, uacpi_namespace_node *node, uacpi_u32)
{
    uacpi_install_notify_handler(node, pd_button_notify, UACPI_NULL);
    kprintf("PDACPIPlatform: control method power button found\n");
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void pd_acpi_init(thread_call_param_t, thread_call_param_t)
{
    uacpi_status st = uacpi_initialize(0);
    if (st == UACPI_STATUS_OK) st = uacpi_namespace_load();
    if (st == UACPI_STATUS_OK) st = uacpi_namespace_initialize();
    if (st != UACPI_STATUS_OK) {
        kprintf("PDACPIPlatform: acpi namespace failed: %s\n", uacpi_status_to_string(st));
        return;
    }

    st = uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON, pd_fixed_power_button, UACPI_NULL);
    kprintf("PDACPIPlatform: fixed power button %s\n", uacpi_status_to_string(st));
    uacpi_find_devices("PNP0C0C", pd_found_button, UACPI_NULL);
    uacpi_finalize_gpe_initialization();

    sNamespaceReady = true;
    kprintf("PDACPIPlatform: acpi namespace up\n");

    // userspace can only look up a vendor code that already exists, so register it before any press
    uint32_t code = 0;
    kev_vendor_code_find(kPDACPIEventVendor, &code);


    sPowerButton = new PDACPIPowerButton;
    if (sPowerButton && sPowerButton->init()) {
        sPowerButton->attach(sExpert);
        sPowerButton->registerService();
    }
}

void PDACPIPlatformExpert::startACPI(void)
{
    sExpert = this;
    pd_acpi_kernel_init();
    sButtonCall = thread_call_allocate(pd_power_button, NULL);
    sInitCall = thread_call_allocate(pd_acpi_init, NULL);
    thread_call_enter(sInitCall);
}

// the platform expert is not on the power plane, so it never gets systemWillShutdown: _PTS and the
// _S5 sleep types are prepared here, on the halt path, just before the s5 write
bool PDACPIPlatformExpert::acpiPowerOff(void)
{
    if (!sNamespaceReady) return false;
    uacpi_status st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (st == UACPI_STATUS_OK) st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    kprintf("PDACPIPlatform: acpi s5 failed: %s\n", uacpi_status_to_string(st));
    return false;
}

bool PDACPIPlatformExpert::acpiReset(void)
{
    if (!sNamespaceReady) return false;
    return uacpi_reboot() == UACPI_STATUS_OK;
}
