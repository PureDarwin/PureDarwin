/*
 * Report.m - gathers the hardware and software report from the I/O Registry, sysctl and the network stack.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "Report.h"
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

static NSString *sysctlString(const char *name)
{
    char buf[1024];
    size_t len = sizeof(buf);

    if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
        return nil;
    buf[MIN(len, sizeof(buf) - 1)] = 0;
    return [NSString stringWithUTF8String:buf];
}

static uint64_t sysctlNumber(const char *name)
{
    uint64_t v = 0;
    size_t len = sizeof(v);

    if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
        return 0;
    // a 32-bit value fills only the low half
    return len == 4 ? (uint32_t)v : v;
}

static NSString *bytes(uint64_t n)
{
    const char *units[] = { "bytes", "KB", "MB", "GB", "TB" };
    double v = n;
    int u = 0;

    while (v >= 1024 && u < 4) {
        v /= 1024;
        u++;
    }
    if (u == 0)
        return [NSString stringWithFormat:@"%llu bytes", (unsigned long long)n];
    return [NSString stringWithFormat:v >= 100 || v == floor(v) ? @"%.0f %s" : @"%.1f %s", v, units[u]];
}

static NSString *hex(uint32_t v, int digits)
{
    return [NSString stringWithFormat:@"0x%0*x", digits, v];
}

// registry properties come back as CFData, CFString or CFNumber depending on the driver
static CFTypeRef property(io_registry_entry_t entry, const char *key)
{
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, name, kCFAllocatorDefault, 0);

    CFRelease(name);
    return value != NULL ? (CFTypeRef)[(id)value autorelease] : NULL;
}

static uint32_t wordProperty(io_registry_entry_t entry, const char *key)
{
    CFTypeRef v = property(entry, key);
    uint32_t w = 0;

    if (v == NULL)
        return 0;
    if (CFGetTypeID(v) == CFDataGetTypeID() && CFDataGetLength(v) >= 4)
        CFDataGetBytes(v, CFRangeMake(0, 4), (UInt8 *)&w);
    else if (CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt32Type, &w);
    return w;
}

static uint64_t numberProperty(io_registry_entry_t entry, const char *key)
{
    CFTypeRef v = property(entry, key);
    uint64_t n = 0;

    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt64Type, &n);
    return n;
}

static BOOL boolProperty(io_registry_entry_t entry, const char *key)
{
    CFTypeRef v = property(entry, key);

    return v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v);
}

// text from a CFString, a NUL-terminated CFData, or UTF-16 data as EFI stores firmware strings
static NSString *stringProperty(io_registry_entry_t entry, const char *key)
{
    CFTypeRef v = property(entry, key);

    if (v == NULL)
        return nil;
    if (CFGetTypeID(v) == CFStringGetTypeID())
        return [[(NSString *)v copy] autorelease];
    if (CFGetTypeID(v) == CFDataGetTypeID()) {
        const UInt8 *p = CFDataGetBytePtr(v);
        CFIndex n = CFDataGetLength(v);

        if (n >= 4 && p[1] == 0 && p[3] == 0) {
            NSString *s = [[[NSString alloc] initWithBytes:p length:n encoding:NSUTF16LittleEndianStringEncoding] autorelease];

            return [s stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\0"]];
        }
        return [[[NSString alloc] initWithBytes:p length:strnlen((const char *)p, n) encoding:NSUTF8StringEncoding] autorelease];
    }
    if (CFGetTypeID(v) == CFNumberGetTypeID())
        return [(NSNumber *)v stringValue];
    return nil;
}

static NSString *entryName(io_registry_entry_t entry)
{
    io_name_t name;

    if (IORegistryEntryGetName(entry, name) != KERN_SUCCESS)
        return @"?";
    return [NSString stringWithUTF8String:name];
}

static NSString *entryClass(io_registry_entry_t entry)
{
    io_name_t name;

    if (IOObjectGetClass(entry, name) != KERN_SUCCESS)
        return @"?";
    return [NSString stringWithUTF8String:name];
}

// every service of a class, handed to a block that turns it into a group
static NSArray *eachService(const char *cls, NSDictionary *(^describe)(io_service_t))
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_service_t s;
    NSMutableArray *groups = [NSMutableArray array];

    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching(cls), &iter) != KERN_SUCCESS)
        return groups;
    while ((s = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        NSDictionary *group = describe(s);

        if (group != nil)
            [groups addObject:group];
        IOObjectRelease(s);
    }
    IOObjectRelease(iter);
    return groups;
}

// the driver attached under a device, the class name of its first child in the service plane
static NSString *driverFor(io_registry_entry_t entry)
{
    io_registry_entry_t child = IO_OBJECT_NULL;
    NSString *cls;

    if (IORegistryEntryGetChildEntry(entry, kIOServicePlane, &child) != KERN_SUCCESS)
        return @"None";
    cls = entryClass(child);
    IOObjectRelease(child);
    return cls;
}

static NSString *pciVendor(uint32_t vendor)
{
    switch (vendor) {
        case 0x1002: return @"AMD";
        case 0x10de: return @"NVIDIA";
        case 0x8086: return @"Intel";
        case 0x1af4: return @"Red Hat (virtio)";
        case 0x1b36: return @"Red Hat (QEMU)";
        case 0x1234: return @"QEMU";
        case 0x15ad: return @"VMware";
        case 0x10ec: return @"Realtek";
        case 0x14e4: return @"Broadcom";
        case 0x168c: return @"Qualcomm Atheros";
        case 0x1022: return @"AMD";
        case 0x106b: return @"Apple";
        case 0x144d: return @"Samsung";
        case 0x1b4b: return @"Marvell";
        case 0x1912: return @"Renesas";
        default: return nil;
    }
}

static NSString *pciClass(uint32_t code)
{
    switch (code >> 16) {
        case 0x00: return @"Unclassified";
        case 0x01: return (code >> 8 & 0xff) == 0x06 ? @"SATA controller" : (code >> 8 & 0xff) == 0x08 ? @"NVMe controller" : @"Storage controller";
        case 0x02: return @"Network controller";
        case 0x03: return @"Display controller";
        case 0x04: return @"Multimedia controller";
        case 0x05: return @"Memory controller";
        case 0x06: return @"Bridge";
        case 0x07: return @"Communication controller";
        case 0x08: return @"System peripheral";
        case 0x0c: return (code >> 8 & 0xff) == 0x03 ? @"USB controller" : @"Serial bus controller";
        case 0x0d: return @"Wireless controller";
        default: return [NSString stringWithFormat:@"Class %@", hex(code >> 16, 2)];
    }
}

static NSString *vendorText(uint32_t vendor)
{
    NSString *name = pciVendor(vendor);

    return name != nil ? [NSString stringWithFormat:@"%@ (%@)", name, hex(vendor, 4)] : hex(vendor, 4);
}

static NSDictionary *group(NSString *title, NSArray *rows)
{
    return title != nil ? @{ @"title": title, @"rows": rows } : @{ @"rows": rows };
}

// each section gathers only when first shown, the registry walks are what make the window slow
static NSDictionary *section(NSString *name, NSString *heading, PDReportBuilder build)
{
    return @{ @"name": name, @"heading": heading, @"build": [[build copy] autorelease] };
}

static NSArray *overview(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    io_registry_entry_t platform = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IOPlatformExpertDevice"));
    io_registry_entry_t efi = IORegistryEntryFromPath(MACH_PORT_NULL, "IODeviceTree:/efi");
    NSString *model = sysctlString("hw.model");
    NSString *cpu = sysctlString("machdep.cpu.brand_string");
    struct utsname u;

    uname(&u);
    [rows addObject:@[ @"Model Identifier", model ?: @"Unknown" ]];
    [rows addObject:@[ @"Processor Name", cpu ?: [NSString stringWithUTF8String:u.machine] ]];
    [rows addObject:@[ @"Architecture", [NSString stringWithUTF8String:u.machine] ]];
    [rows addObject:@[ @"Total Number of Cores", [NSString stringWithFormat:@"%llu", sysctlNumber("hw.physicalcpu")] ]];
    [rows addObject:@[ @"Threads", [NSString stringWithFormat:@"%llu", sysctlNumber("hw.logicalcpu")] ]];
    if (sysctlNumber("hw.cpufrequency") > 0)
        [rows addObject:@[ @"Processor Speed", [NSString stringWithFormat:@"%.2f GHz", sysctlNumber("hw.cpufrequency") / 1e9] ]];
    if (sysctlNumber("hw.l2cachesize") > 0)
        [rows addObject:@[ @"L2 Cache", bytes(sysctlNumber("hw.l2cachesize")) ]];
    if (sysctlNumber("hw.l3cachesize") > 0)
        [rows addObject:@[ @"L3 Cache", bytes(sysctlNumber("hw.l3cachesize")) ]];
    [rows addObject:@[ @"Memory", bytes(sysctlNumber("hw.memsize")) ]];
    if (efi != IO_OBJECT_NULL) {
        NSString *vendor = stringProperty(efi, "firmware-vendor");
        uint32_t rev = wordProperty(efi, "firmware-revision");
        NSString *abi = stringProperty(efi, "firmware-abi");

        if (vendor != nil)
            [rows addObject:@[ @"Firmware Vendor", vendor ]];
        if (rev != 0)
            [rows addObject:@[ @"Firmware Revision", hex(rev, 8) ]];
        if ([abi length] > 0)
            [rows addObject:@[ @"Firmware ABI", abi ]];
        IOObjectRelease(efi);
    }
    if (platform != IO_OBJECT_NULL) {
        NSString *serial = stringProperty(platform, "IOPlatformSerialNumber");
        NSString *uuid = stringProperty(platform, "IOPlatformUUID");

        if ([serial length] > 0)
            [rows addObject:@[ @"Serial Number", serial ]];
        if ([uuid length] > 0)
            [rows addObject:@[ @"Hardware UUID", uuid ]];
        IOObjectRelease(platform);
    }
    return @[ group(nil, rows) ];
}

static NSArray *graphics(void)
{
    return eachService("IOPCIDevice", ^NSDictionary *(io_service_t d) {
        uint32_t cls = wordProperty(d, "class-code");
        NSString *model = stringProperty(d, "model");

        if (cls >> 16 != 0x03)
            return nil;
        return group([model length] > 0 ? model : entryName(d), @[
            @[ @"Chipset Model", [model length] > 0 ? model : pciClass(cls) ],
            @[ @"Type", @"GPU" ],
            @[ @"Bus", @"PCI" ],
            @[ @"Vendor", vendorText(wordProperty(d, "vendor-id") & 0xffff) ],
            @[ @"Device ID", hex(wordProperty(d, "device-id") & 0xffff, 4) ],
            @[ @"Revision ID", hex(wordProperty(d, "revision-id") & 0xff, 4) ],
            @[ @"Driver", driverFor(d) ],
        ]);
    });
}

static NSArray *memoryInfo(void)
{
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    uint64_t page = sysctlNumber("hw.pagesize");
    NSMutableArray *rows = [NSMutableArray arrayWithObject:@[ @"Memory", bytes(sysctlNumber("hw.memsize")) ]];

    [rows addObject:@[ @"Page Size", bytes(page) ]];
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS) {
        [rows addObject:@[ @"Free", bytes(vm.free_count * page) ]];
        [rows addObject:@[ @"Active", bytes(vm.active_count * page) ]];
        [rows addObject:@[ @"Inactive", bytes(vm.inactive_count * page) ]];
        [rows addObject:@[ @"Wired", bytes(vm.wire_count * page) ]];
        [rows addObject:@[ @"Compressed", bytes(vm.compressor_page_count * page) ]];
    }
    return @[ group(nil, rows) ];
}

static NSArray *pci(void)
{
    return eachService("IOPCIDevice", ^NSDictionary *(io_service_t d) {
        uint32_t cls = wordProperty(d, "class-code");
        uint32_t vendor = wordProperty(d, "vendor-id") & 0xffff;
        NSMutableArray *rows = [NSMutableArray arrayWithArray:@[
            @[ @"Type", pciClass(cls) ],
            @[ @"Vendor", vendorText(vendor) ],
            @[ @"Device ID", hex(wordProperty(d, "device-id") & 0xffff, 4) ],
            @[ @"Subsystem", [NSString stringWithFormat:@"%@ %@", hex(wordProperty(d, "subsystem-vendor-id") & 0xffff, 4),
                                                         hex(wordProperty(d, "subsystem-id") & 0xffff, 4)] ],
            @[ @"Class Code", hex(cls, 6) ],
            @[ @"Driver", driverFor(d) ],
        ]];
        NSString *loc = stringProperty(d, "pcidebug");

        if ([loc length] > 0)
            [rows addObject:@[ @"Location", loc ]];
        return group(entryName(d), rows);
    });
}

static NSArray *usb(void)
{
    NSDictionary *(^describe)(io_service_t) = ^NSDictionary *(io_service_t d) {
        NSString *product = stringProperty(d, "USB Product Name") ?: stringProperty(d, "kUSBProductString");
        NSString *vendorName = stringProperty(d, "USB Vendor Name") ?: stringProperty(d, "kUSBVendorString");
        NSString *serial = stringProperty(d, "USB Serial Number") ?: stringProperty(d, "kUSBSerialNumberString");
        NSMutableArray *rows = [NSMutableArray arrayWithArray:@[
            @[ @"Product ID", hex(wordProperty(d, "idProduct") & 0xffff, 4) ],
            @[ @"Vendor ID", hex(wordProperty(d, "idVendor") & 0xffff, 4) ],
            @[ @"Location ID", hex(wordProperty(d, "locationID"), 8) ],
        ]];

        if (vendorName != nil)
            [rows insertObject:@[ @"Manufacturer", vendorName ] atIndex:0];
        if ([serial length] > 0)
            [rows addObject:@[ @"Serial Number", serial ]];
        [rows addObject:@[ @"Driver", driverFor(d) ]];
        return group([product length] > 0 ? product : entryName(d), rows);
    };
    NSArray *devices = eachService("IOUSBHostDevice", describe);

    return [devices count] > 0 ? devices : eachService("IOUSBDevice", describe);
}

static NSArray *storage(void)
{
    return eachService("IOMedia", ^NSDictionary *(io_service_t m) {
        NSString *bsd = stringProperty(m, "BSD Name");
        NSString *content = stringProperty(m, "Content");
        BOOL whole = boolProperty(m, "Whole");
        NSMutableArray *rows = [NSMutableArray arrayWithArray:@[
            @[ @"Capacity", bytes(numberProperty(m, "Size")) ],
            @[ @"BSD Name", bsd ?: @"?" ],
            @[ @"Kind", whole ? @"Disk" : @"Partition" ],
        ]];

        if ([content length] > 0)
            [rows addObject:@[ @"Content", content ]];
        [rows addObject:@[ @"Removable", boolProperty(m, "Removable") ? @"Yes" : @"No" ]];
        [rows addObject:@[ @"Writable", boolProperty(m, "Writable") ? @"Yes" : @"No" ]];
        return group(whole ? entryName(m) : [NSString stringWithFormat:@"%@ (%@)", entryName(m), bsd ?: @"?"], rows);
    });
}

static NSArray *network(void)
{
    struct ifaddrs *list = NULL, *a;
    NSMutableDictionary *byName = [NSMutableDictionary dictionary];
    NSMutableArray *order = [NSMutableArray array];
    NSMutableArray *groups = [NSMutableArray array];

    if (getifaddrs(&list) != 0)
        return groups;
    for (a = list; a != NULL; a = a->ifa_next) {
        NSString *name = [NSString stringWithUTF8String:a->ifa_name];
        NSMutableArray *rows = byName[name];
        char text[INET6_ADDRSTRLEN];

        if (rows == nil) {
            rows = [NSMutableArray arrayWithObject:@[ @"Status", (a->ifa_flags & IFF_UP) ? ((a->ifa_flags & IFF_RUNNING) ? @"Up" : @"Up, no link") : @"Down" ]];
            byName[name] = rows;
            [order addObject:name];
        }
        if (a->ifa_addr == NULL)
            continue;
        if (a->ifa_addr->sa_family == AF_LINK) {
            struct sockaddr_dl *dl = (struct sockaddr_dl *)a->ifa_addr;
            const unsigned char *mac = (const unsigned char *)LLADDR(dl);

            if (dl->sdl_alen == 6)
                [rows addObject:@[ @"MAC Address", [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]] ]];
        } else if (a->ifa_addr->sa_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, text, sizeof(text));
            [rows addObject:@[ @"IPv4 Address", [NSString stringWithUTF8String:text] ]];
        } else if (a->ifa_addr->sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)a->ifa_addr)->sin6_addr, text, sizeof(text));
            [rows addObject:@[ @"IPv6 Address", [NSString stringWithUTF8String:text] ]];
        }
    }
    freeifaddrs(list);
    for (NSString *name in order)
        [groups addObject:group(name, byName[name])];
    return groups;
}

static NSArray *software(void)
{
    NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    struct timeval boot = { 0, 0 };
    size_t len = sizeof(boot);
    char host[256] = "";
    struct utsname u;
    NSMutableArray *rows = [NSMutableArray array];
    long up;

    uname(&u);
    gethostname(host, sizeof(host));
    sysctlbyname("kern.boottime", &boot, &len, NULL, 0);
    up = boot.tv_sec > 0 ? (long)(time(NULL) - boot.tv_sec) : 0;
    [rows addObject:@[ @"System Version", [NSString stringWithFormat:@"%@ %@ (%@)", sv[@"ProductName"] ?: @"PureDarwin",
                                                   sv[@"ProductVersion"] ?: @"?", sv[@"ProductBuildVersion"] ?: @"?"] ]];
    [rows addObject:@[ @"Kernel Version", [NSString stringWithFormat:@"%s %s", u.sysname, u.release] ]];
    [rows addObject:@[ @"Kernel Build", sysctlString("kern.version") ?: @"?" ]];
    [rows addObject:@[ @"Boot Arguments", [sysctlString("kern.bootargs") length] > 0 ? sysctlString("kern.bootargs") : @"None" ]];
    [rows addObject:@[ @"Computer Name", [NSString stringWithUTF8String:host] ]];
    [rows addObject:@[ @"User Name", NSUserName() ?: @"?" ]];
    [rows addObject:@[ @"Time since boot", [NSString stringWithFormat:@"%ld days, %ld:%02ld", up / 86400, up / 3600 % 24, up / 60 % 60] ]];
    return @[ group(nil, rows) ];
}

// every bundle that put a driver into the registry, with the classes it provides
static NSArray *extensions(void)
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_registry_entry_t e;
    NSMutableDictionary *bundles = [NSMutableDictionary dictionary];
    NSMutableArray *groups = [NSMutableArray array];

    if (IORegistryCreateIterator(MACH_PORT_NULL, kIOServicePlane, kIORegistryIterateRecursively, &iter) != KERN_SUCCESS)
        return groups;
    while ((e = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        NSString *bundle = stringProperty(e, "CFBundleIdentifier");

        if ([bundle length] > 0) {
            NSMutableSet *classes = bundles[bundle];

            if (classes == nil)
                bundles[bundle] = classes = [NSMutableSet set];
            [classes addObject:entryClass(e)];
        }
        IOObjectRelease(e);
    }
    IOObjectRelease(iter);
    for (NSString *bundle in [[bundles allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
        NSArray *classes = [[bundles[bundle] allObjects] sortedArrayUsingSelector:@selector(compare:)];

        [groups addObject:group(bundle, @[ @[ @"Classes", [classes componentsJoinedByString:@", "] ] ])];
    }
    return groups;
}

NSArray *PDReportSections(void)
{
    return @[
        section(@"Hardware", @"Hardware Overview", ^{ return overview(); }),
        section(@"Graphics", @"Graphics/Displays", ^{ return graphics(); }),
        section(@"Memory", @"Memory", ^{ return memoryInfo(); }),
        section(@"PCI", @"PCI Devices", ^{ return pci(); }),
        section(@"USB", @"USB Devices", ^{ return usb(); }),
        section(@"Storage", @"Storage", ^{ return storage(); }),
        section(@"Network", @"Network Interfaces", ^{ return network(); }),
        section(@"Software", @"System Software Overview", ^{ return software(); }),
        section(@"Extensions", @"Drivers in Use", ^{ return extensions(); }),
    ];
}
