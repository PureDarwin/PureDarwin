// IOKit.c - the IOKit registry calls PureDarwin's apps make, answered from sysfs, udev and /proc
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// entries carry the class and property names IOKit gives the same hardware, so callers read them unchanged
// the service plane lists the loaded kernel modules as the extensions

#define _GNU_SOURCE
#include <IOKit/IOKitLib.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

struct entry {
    char cls[64];
    char name[128];
    char child[64];
    CFMutableDictionaryRef props;
    io_object_t *items;      // an iterator's entries
    size_t count, next;
    int refs;
};

#define MAX_OBJECTS 8192
static struct entry *objects[MAX_OBJECTS];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static io_object_t new_object(struct entry *e)
{
    pthread_mutex_lock(&lock);
    for (io_object_t h = 1; h < MAX_OBJECTS; h++) {
        if (objects[h] == NULL) {
            e->refs = 1;
            objects[h] = e;
            pthread_mutex_unlock(&lock);
            return h;
        }
    }
    pthread_mutex_unlock(&lock);
    return IO_OBJECT_NULL;
}

static struct entry *get(io_object_t h)
{
    return h > 0 && h < MAX_OBJECTS ? objects[h] : NULL;
}

static struct entry *new_entry(const char *cls, const char *name)
{
    struct entry *e = calloc(1, sizeof(*e));

    snprintf(e->cls, sizeof(e->cls), "%s", cls);
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return e;
}

static void set_string(struct entry *e, const char *key, const char *value)
{
    if (value == NULL || *value == '\0')
        return;
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFStringRef v = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
    if (k && v)
        CFDictionarySetValue(e->props, k, v);
    if (k) CFRelease(k);
    if (v) CFRelease(v);
}

static void set_number(struct entry *e, const char *key, long long value)
{
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFNumberRef v = CFNumberCreate(NULL, kCFNumberLongLongType, &value);
    CFDictionarySetValue(e->props, k, v);
    CFRelease(k);
    CFRelease(v);
}

static void set_bool(struct entry *e, const char *key, int value)
{
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFDictionarySetValue(e->props, k, value ? kCFBooleanTrue : kCFBooleanFalse);
    CFRelease(k);
}

// one line of a sysfs file, without its newline
static int read_line(const char *dir, const char *file, char *out, size_t size)
{
    char path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    if ((f = fopen(path, "r")) == NULL)
        return 0;
    out[0] = '\0';
    if (fgets(out, (int)size, f) == NULL)
        out[0] = '\0';
    fclose(f);
    out[strcspn(out, "\n")] = '\0';
    return out[0] != '\0';
}

static long long read_number(const char *dir, const char *file, int base, long long fallback)
{
    char line[64];

    return read_line(dir, file, line, sizeof(line)) ? strtoll(line, NULL, base) : fallback;
}

// a property from udev's database for a device, such as ID_MODEL_FROM_DATABASE
static int udev_property(const char *id, const char *key, char *out, size_t size)
{
    char path[256], line[512];
    size_t n = strlen(key);
    FILE *f;
    int found = 0;

    snprintf(path, sizeof(path), "/run/udev/data/%s", id);
    if ((f = fopen(path, "r")) == NULL)
        return 0;
    while (!found && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "E:", 2) == 0 && strncmp(line + 2, key, n) == 0 && line[2 + n] == '=') {
            snprintf(out, size, "%s", line + 3 + n);
            out[strcspn(out, "\n")] = '\0';
            found = 1;
        }
    }
    fclose(f);
    return found;
}

// the driver bound to a sysfs device, as IOKit's child entry
static void bound_driver(struct entry *e, const char *dir)
{
    char path[512], target[512];
    ssize_t n;

    snprintf(path, sizeof(path), "%s/driver", dir);
    if ((n = readlink(path, target, sizeof(target) - 1)) > 0) {
        target[n] = '\0';
        snprintf(e->child, sizeof(e->child), "%s", strrchr(target, '/') ? strrchr(target, '/') + 1 : target);
    }
}

typedef void (*add_fn)(struct entry *, void *);

static void pci_devices(add_fn add, void *list)
{
    DIR *d = opendir("/sys/bus/pci/devices");
    struct dirent *de;

    while (d && (de = readdir(d))) {
        char dir[512], model[256], id[300];
        if (de->d_name[0] == '.')
            continue;
        snprintf(dir, sizeof(dir), "/sys/bus/pci/devices/%s", de->d_name);
        struct entry *e = new_entry("IOPCIDevice", de->d_name);
        set_number(e, "vendor-id", read_number(dir, "vendor", 16, 0));
        set_number(e, "device-id", read_number(dir, "device", 16, 0));
        set_number(e, "class-code", read_number(dir, "class", 16, 0));
        set_number(e, "revision-id", read_number(dir, "revision", 16, 0));
        set_number(e, "subsystem-vendor-id", read_number(dir, "subsystem_vendor", 16, 0));
        set_number(e, "subsystem-id", read_number(dir, "subsystem_device", 16, 0));
        snprintf(id, sizeof(id), "+pci:%s", de->d_name);
        if (udev_property(id, "ID_MODEL_FROM_DATABASE", model, sizeof(model)))
            set_string(e, "model", model);
        set_string(e, "pcidebug", de->d_name);
        bound_driver(e, dir);
        add(e, list);
    }
    if (d)
        closedir(d);
}

static void usb_devices(add_fn add, void *list)
{
    DIR *d = opendir("/sys/bus/usb/devices");
    struct dirent *de;

    while (d && (de = readdir(d))) {
        char dir[512], text[256];
        snprintf(dir, sizeof(dir), "/sys/bus/usb/devices/%s", de->d_name);
        if (de->d_name[0] == '.' || !read_line(dir, "idVendor", text, sizeof(text)))
            continue;
        struct entry *e = new_entry("IOUSBHostDevice", de->d_name);
        set_number(e, "idVendor", strtol(text, NULL, 16));
        set_number(e, "idProduct", read_number(dir, "idProduct", 16, 0));
        set_number(e, "locationID", (read_number(dir, "busnum", 10, 0) << 24) | read_number(dir, "devnum", 10, 0));
        if (read_line(dir, "product", text, sizeof(text))) {
            set_string(e, "USB Product Name", text);
            set_string(e, "kUSBProductString", text);
            snprintf(e->name, sizeof(e->name), "%s", text);
        }
        if (read_line(dir, "manufacturer", text, sizeof(text))) {
            set_string(e, "USB Vendor Name", text);
            set_string(e, "kUSBVendorString", text);
        }
        if (read_line(dir, "serial", text, sizeof(text))) {
            set_string(e, "USB Serial Number", text);
            set_string(e, "kUSBSerialNumberString", text);
        }
        bound_driver(e, dir);
        add(e, list);
    }
    if (d)
        closedir(d);
}

static void add_media(const char *name, const char *dir, int whole, add_fn add, void *list)
{
    char text[128], id[64], type[128];
    unsigned maj = 0, min = 0;
    struct entry *e = new_entry("IOMedia", name);

    set_string(e, "BSD Name", name);
    set_number(e, "Size", read_number(dir, "size", 10, 0) * 512);
    set_bool(e, "Whole", whole);
    set_bool(e, "Writable", read_number(dir, "ro", 10, 0) == 0);
    set_bool(e, "Removable", read_number(dir, whole ? "removable" : "../removable", 10, 0) != 0);
    if (read_line(dir, "dev", text, sizeof(text)) && sscanf(text, "%u:%u", &maj, &min) == 2) {
        snprintf(id, sizeof(id), "b%u:%u", maj, min);
        if (udev_property(id, whole ? "ID_PART_TABLE_TYPE" : "ID_PART_ENTRY_TYPE", type, sizeof(type))) {
            // IOKit names a GPT disk's content by its scheme and a partition by its type GUID
            for (char *c = type; *c; c++)
                if (*c >= 'a' && *c <= 'f') *c -= 32;
            set_string(e, "Content", strcmp(type, "GPT") == 0 ? "GUID_partition_scheme" : strcmp(type, "DOS") == 0 ? "FDisk_partition_scheme" : type);
        }
    }
    add(e, list);
}

static void media(add_fn add, void *list)
{
    DIR *d = opendir("/sys/block");
    struct dirent *de;

    while (d && (de = readdir(d))) {
        char dir[512];
        if (de->d_name[0] == '.' || strncmp(de->d_name, "loop", 4) == 0 || strncmp(de->d_name, "ram", 3) == 0 || strncmp(de->d_name, "zram", 4) == 0)
            continue;
        snprintf(dir, sizeof(dir), "/sys/block/%s", de->d_name);
        add_media(de->d_name, dir, 1, add, list);
        DIR *p = opendir(dir);
        struct dirent *pe;
        while (p && (pe = readdir(p))) {
            char part[600], tmp[16];
            snprintf(part, sizeof(part), "%s/%s", dir, pe->d_name);
            if (strncmp(pe->d_name, de->d_name, strlen(de->d_name)) == 0 && read_line(part, "partition", tmp, sizeof(tmp)))
                add_media(pe->d_name, part, 0, add, list);
        }
        if (p)
            closedir(p);
    }
    if (d)
        closedir(d);
}

static void power_sources(add_fn add, void *list)
{
    DIR *d = opendir("/sys/class/power_supply");
    struct dirent *de;

    while (d && (de = readdir(d))) {
        char dir[512], text[64];
        snprintf(dir, sizeof(dir), "/sys/class/power_supply/%s", de->d_name);
        if (de->d_name[0] == '.' || !read_line(dir, "type", text, sizeof(text)) || strcmp(text, "Battery") != 0)
            continue;
        struct entry *e = new_entry("IOPMPowerSource", de->d_name);
        set_number(e, "CurrentCapacity", read_number(dir, "capacity", 10, 0));
        set_number(e, "MaxCapacity", 100);
        set_number(e, "IsCharging", read_line(dir, "status", text, sizeof(text)) && strcmp(text, "Charging") == 0);
        set_bool(e, "ExternalConnected", strcmp(text, "Discharging") != 0);
        add(e, list);
    }
    if (d)
        closedir(d);
}

// /proc/asound/cards: " 0 [PCH            ]: HDA-Intel - HDA Intel PCH" then the long name
static void audio_devices(add_fn add, void *list)
{
    FILE *f = fopen("/proc/asound/cards", "r");
    char line[512], longname[512];

    while (f && fgets(line, sizeof(line), f)) {
        char *bracket = strchr(line, '['), *close = strchr(line, ']'), *dash = strstr(line, " - ");
        if (bracket == NULL || close == NULL || dash == NULL)
            continue;
        *close = '\0';
        char shortname[64];
        snprintf(shortname, sizeof(shortname), "%s", bracket + 1);
        shortname[strcspn(shortname, " ")] = '\0';
        dash[strcspn(dash, "\n")] = '\0';
        struct entry *e = new_entry("IOAudioDevice", shortname);
        set_string(e, "IOAudioDeviceName", dash + 3);
        set_string(e, "IOAudioDeviceShortName", shortname);
        if (fgets(longname, sizeof(longname), f)) {
            char *t = longname + strspn(longname, " ");
            t[strcspn(t, "\n")] = '\0';
            set_string(e, "IOAudioDeviceManufacturerName", t);
        }
        add(e, list);
    }
    if (f)
        fclose(f);
}

// /proc/bus/input/devices blocks: "I: Bus=0003 ...", "N: Name=\"...\"", "H: Handlers=... kbd/mouse"
static void hid_devices(add_fn add, void *list)
{
    FILE *f = fopen("/proc/bus/input/devices", "r");
    char line[512], name[256] = "", handlers[256] = "";
    unsigned bus = 0;

    while (f && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "I:", 2) == 0) {
            char *b = strstr(line, "Bus=");
            bus = b ? (unsigned)strtoul(b + 4, NULL, 16) : 0;
        } else if (strncmp(line, "N: Name=\"", 9) == 0) {
            snprintf(name, sizeof(name), "%s", line + 9);
            name[strcspn(name, "\"")] = '\0';
        } else if (strncmp(line, "H: Handlers=", 12) == 0) {
            snprintf(handlers, sizeof(handlers), "%s", line + 12);
        } else if (line[0] == '\n' && name[0]) {
            // only what a person types or points with, power buttons and lid switches are left out
            if (strstr(handlers, "kbd") || strstr(handlers, "mouse")) {
                struct entry *e = new_entry("IOHIDDevice", name);
                set_string(e, "Product", name);
                set_string(e, "Transport", bus == 0x03 ? "USB" : bus == 0x05 ? "Bluetooth" : bus == 0x18 ? "I2C" : bus == 0x11 ? "PS/2" : "Virtual");
                add(e, list);
            }
            name[0] = handlers[0] = '\0';
        }
    }
    if (f)
        fclose(f);
}

static void platform(add_fn add, void *list)
{
    const char *dmi = "/sys/class/dmi/id";
    char text[256];
    struct entry *e = new_entry("IOPlatformExpertDevice", "platform");

    if (read_line(dmi, "product_name", text, sizeof(text)))
        set_string(e, "model", text);
    if (read_line(dmi, "sys_vendor", text, sizeof(text)))
        set_string(e, "manufacturer", text);
    if (read_line(dmi, "product_serial", text, sizeof(text)))
        set_string(e, "IOPlatformSerialNumber", text);
    if (read_line(dmi, "product_uuid", text, sizeof(text)))
        set_string(e, "IOPlatformUUID", text);
    add(e, list);
}

// the loaded kernel modules, IOKit's extensions on Linux
static void modules(add_fn add, void *list)
{
    FILE *f = fopen("/proc/modules", "r");
    char line[512];

    while (f && fgets(line, sizeof(line), f)) {
        line[strcspn(line, " ")] = '\0';
        struct entry *e = new_entry("IOService", line);
        set_string(e, "CFBundleIdentifier", line);
        add(e, list);
    }
    if (f)
        fclose(f);
}

struct list { io_object_t *items; size_t count, capacity; };

static void append(struct entry *e, void *opaque)
{
    struct list *l = opaque;
    io_object_t h = new_object(e);

    if (h == IO_OBJECT_NULL)
        return;
    if (l->count == l->capacity) {
        l->capacity = l->capacity ? l->capacity * 2 : 32;
        l->items = realloc(l->items, l->capacity * sizeof(*l->items));
    }
    l->items[l->count++] = h;
}

static io_iterator_t make_iterator(struct list *l)
{
    struct entry *it = new_entry("IOIterator", "");

    it->items = l->items;
    it->count = l->count;
    return new_object(it);
}

CFMutableDictionaryRef IOServiceMatching(const char *name)
{
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef v = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    CFDictionarySetValue(d, CFSTR("IOProviderClass"), v);
    CFRelease(v);
    return d;
}

kern_return_t IOServiceGetMatchingServices(mach_port_t main, CFDictionaryRef matching, io_iterator_t *existing)
{
    struct list l = { 0 };
    char cls[64] = "";
    CFStringRef name = matching ? CFDictionaryGetValue(matching, CFSTR("IOProviderClass")) : NULL;

    (void)main;
    if (name)
        CFStringGetCString(name, cls, sizeof(cls), kCFStringEncodingUTF8);
    if (strcmp(cls, "IOPCIDevice") == 0) pci_devices(append, &l);
    else if (strcmp(cls, "IOUSBHostDevice") == 0 || strcmp(cls, "IOUSBDevice") == 0) usb_devices(append, &l);
    else if (strcmp(cls, "IOMedia") == 0) media(append, &l);
    else if (strcmp(cls, "IOPMPowerSource") == 0) power_sources(append, &l);
    else if (strcmp(cls, "IOAudioDevice") == 0) audio_devices(append, &l);
    else if (strcmp(cls, "IOHIDDevice") == 0) hid_devices(append, &l);
    else if (strcmp(cls, "IOPlatformExpertDevice") == 0) platform(append, &l);
    // the dictionary is consumed, as IOKit does
    if (matching)
        CFRelease(matching);
    *existing = make_iterator(&l);
    return KERN_SUCCESS;
}

io_service_t IOServiceGetMatchingService(mach_port_t main, CFDictionaryRef matching)
{
    io_iterator_t it;
    io_service_t first;

    if (IOServiceGetMatchingServices(main, matching, &it) != KERN_SUCCESS)
        return IO_OBJECT_NULL;
    first = IOIteratorNext(it);
    IOObjectRelease(it);
    return first;
}

io_object_t IOIteratorNext(io_iterator_t iterator)
{
    struct entry *it = get(iterator);

    if (it == NULL || it->next >= it->count)
        return IO_OBJECT_NULL;
    // the caller releases what it is handed, the iterator keeps its own reference
    io_object_t h = it->items[it->next++];
    IOObjectRetain(h);
    return h;
}

kern_return_t IOObjectRetain(io_object_t object)
{
    struct entry *e = get(object);

    if (e == NULL)
        return KERN_INVALID_ARGUMENT;
    pthread_mutex_lock(&lock);
    e->refs++;
    pthread_mutex_unlock(&lock);
    return KERN_SUCCESS;
}

kern_return_t IOObjectRelease(io_object_t object)
{
    struct entry *e = get(object);
    int gone;

    if (e == NULL)
        return KERN_INVALID_ARGUMENT;
    pthread_mutex_lock(&lock);
    gone = --e->refs == 0;
    if (gone)
        objects[object] = NULL;
    pthread_mutex_unlock(&lock);
    if (gone) {
        for (size_t i = 0; i < e->count; i++)
            IOObjectRelease(e->items[i]);
        free(e->items);
        CFRelease(e->props);
        free(e);
    }
    return KERN_SUCCESS;
}

kern_return_t IOObjectGetClass(io_object_t object, io_name_t className)
{
    struct entry *e = get(object);

    if (e == NULL)
        return KERN_INVALID_ARGUMENT;
    snprintf(className, sizeof(io_name_t), "%s", e->cls);
    return KERN_SUCCESS;
}

kern_return_t IORegistryEntryGetName(io_registry_entry_t entry, io_name_t name)
{
    struct entry *e = get(entry);

    if (e == NULL)
        return KERN_INVALID_ARGUMENT;
    snprintf(name, sizeof(io_name_t), "%s", e->name);
    return KERN_SUCCESS;
}

CFTypeRef IORegistryEntryCreateCFProperty(io_registry_entry_t entry, CFStringRef key, CFAllocatorRef allocator, IOOptionBits options)
{
    struct entry *e = get(entry);
    CFTypeRef v;

    (void)allocator;
    (void)options;
    if (e == NULL || key == NULL)
        return NULL;
    v = CFDictionaryGetValue(e->props, key);
    return v ? CFRetain(v) : NULL;
}

kern_return_t IORegistryEntryCreateCFProperties(io_registry_entry_t entry, CFMutableDictionaryRef *properties, CFAllocatorRef allocator, IOOptionBits options)
{
    struct entry *e = get(entry);

    (void)options;
    if (e == NULL)
        return KERN_INVALID_ARGUMENT;
    *properties = CFDictionaryCreateMutableCopy(allocator, 0, e->props);
    return KERN_SUCCESS;
}

// a device's driver, as the class of its first child
kern_return_t IORegistryEntryGetChildEntry(io_registry_entry_t entry, const io_name_t plane, io_registry_entry_t *child)
{
    struct entry *e = get(entry);

    (void)plane;
    if (e == NULL || e->child[0] == '\0')
        return KERN_INVALID_ARGUMENT;
    *child = new_object(new_entry(e->child, e->child));
    return *child ? KERN_SUCCESS : KERN_FAILURE;
}

// IODeviceTree:/efi when the machine booted through UEFI, nothing else in the device tree
io_registry_entry_t IORegistryEntryFromPath(mach_port_t main, const io_string_t path)
{
    struct stat st;

    (void)main;
    if (strcmp(path, "IODeviceTree:/efi") != 0 || stat("/sys/firmware/efi", &st) != 0)
        return IO_OBJECT_NULL;
    struct entry *e = new_entry("IOService", "efi");
    char text[64];
    set_string(e, "firmware-abi", read_line("/sys/firmware/efi", "fw_platform_size", text, sizeof(text)) && strcmp(text, "32") == 0 ? "EFI32" : "EFI64");
    return new_object(e);
}

io_registry_entry_t IORegistryGetRootEntry(mach_port_t main)
{
    (void)main;
    return new_object(new_entry("IORegistryEntry", "Root"));
}

kern_return_t IORegistryCreateIterator(mach_port_t main, const io_name_t plane, IOOptionBits options, io_iterator_t *iterator)
{
    struct list l = { 0 };

    (void)main;
    (void)plane;
    if (options & kIORegistryIterateRecursively)
        modules(append, &l);
    *iterator = make_iterator(&l);
    return KERN_SUCCESS;
}
