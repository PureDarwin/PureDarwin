/*
 * SettingsData.m - what the Settings panes read from the running system and write back to it.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "SettingsData.h"
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pwd.h>
#include <spawn.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>

extern char **environ;

NSString * const PDDockDomain = @"org.puredarwin.Dock";
NSString * const PDMenuBarDomain = @"org.puredarwin.MenuBar";

id PDPref(NSString *domain, NSString *key)
{
    CFPreferencesAppSynchronize((CFStringRef)domain);
    return [(id)CFPreferencesCopyAppValue((CFStringRef)key, (CFStringRef)domain) autorelease];
}

void PDSetPref(NSString *domain, NSString *key, id value)
{
    CFPreferencesSetAppValue((CFStringRef)key, (CFPropertyListRef)value, (CFStringRef)domain);
    CFPreferencesAppSynchronize((CFStringRef)domain);
}

id PDGlobalPref(NSString *key)
{
    return PDPref((NSString *)kCFPreferencesAnyApplication, key);
}

void PDSetGlobalPref(NSString *key, id value)
{
    PDSetPref((NSString *)kCFPreferencesAnyApplication, key, value);
}

static NSString *sysctlString(const char *name)
{
    char buf[1024];
    size_t len = sizeof(buf);

    if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
        return nil;
    buf[MIN(len, sizeof(buf) - 1)] = 0;
    return [NSString stringWithUTF8String:buf];
}

static NSDictionary *systemVersion(void)
{
    return [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
}

NSString *PDSystemVersion(void)
{
    return systemVersion()[@"ProductVersion"] ?: @"?";
}

NSString *PDBuildVersion(void)
{
    return systemVersion()[@"ProductBuildVersion"] ?: @"?";
}

NSString *PDKernelVersion(void)
{
    struct utsname u;

    if (uname(&u) != 0)
        return @"Darwin";
    return [NSString stringWithFormat:@"%s %s", u.sysname, u.release];
}

// the first xnu-<digits> in kern.version, a plain scan since that's all it takes
static NSRange xnuTagRange(NSString *version)
{
    const char *s = [version UTF8String], *p = s;

    while (s != NULL && (p = strstr(p, "xnu-")) != NULL) {
        if (p[4] >= '0' && p[4] <= '9')
            return NSMakeRange(p - s, 4);
        p += 4;
    }
    return NSMakeRange(NSNotFound, 0);
}

NSString *PDXNUTag(void)
{
    NSString *version = sysctlString("kern.version");
    NSRange xnu = version != nil ? xnuTagRange(version) : NSMakeRange(NSNotFound, 0);
    NSString *rest;
    NSRange end;

    // a local build says xnu-build, which says nothing, so only a numbered tag is shown
    if (xnu.location == NSNotFound)
        return @"xnu";
    rest = [version substringFromIndex:xnu.location];
    // the whole source version, xnu-12377.121.6, up to the build number after ~
    end = [rest rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"~/: "]];
    return end.location != NSNotFound ? [rest substringToIndex:end.location] : rest;
}

NSString *PDProcessor(void)
{
    struct utsname u;
    int cpus = 0;
    size_t len = sizeof(cpus);

    sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0);
    uname(&u);
    return [NSString stringWithFormat:@"%d-core %s", cpus > 0 ? cpus : 1, u.machine];
}

NSString *PDMemory(void)
{
    uint64_t bytes = 0;
    size_t len = sizeof(bytes);

    if (sysctlbyname("hw.memsize", &bytes, &len, NULL, 0) != 0 || bytes == 0)
        return @"Unknown";
    return [NSString stringWithFormat:@"%llu GB", (unsigned long long)((bytes + (1ULL << 29)) >> 30)];
}

static CFTypeRef property(io_registry_entry_t e, const char *key)
{
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    CFTypeRef v = IORegistryEntryCreateCFProperty(e, name, kCFAllocatorDefault, 0);

    CFRelease(name);
    return v != NULL ? (CFTypeRef)[(id)v autorelease] : NULL;
}

static uint32_t wordProperty(io_registry_entry_t e, const char *key)
{
    CFTypeRef v = property(e, key);
    uint32_t w = 0;

    if (v != NULL && CFGetTypeID(v) == CFDataGetTypeID() && CFDataGetLength(v) >= 4)
        CFDataGetBytes(v, CFRangeMake(0, 4), (UInt8 *)&w);
    else if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt32Type, &w);
    return w;
}

static NSString *stringProperty(io_registry_entry_t e, const char *key)
{
    CFTypeRef v = property(e, key);

    if (v == NULL)
        return nil;
    if (CFGetTypeID(v) == CFStringGetTypeID())
        return [[(NSString *)v copy] autorelease];
    if (CFGetTypeID(v) == CFDataGetTypeID())
        return [[[NSString alloc] initWithBytes:CFDataGetBytePtr(v) length:strnlen((const char *)CFDataGetBytePtr(v), CFDataGetLength(v))
                                       encoding:NSUTF8StringEncoding] autorelease];
    if (CFGetTypeID(v) == CFNumberGetTypeID())
        return [(NSNumber *)v stringValue];
    return nil;
}

static void eachService(const char *cls, void (^visit)(io_service_t))
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_service_t s;

    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching(cls), &iter) != KERN_SUCCESS)
        return;
    while ((s = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        visit(s);
        IOObjectRelease(s);
    }
    IOObjectRelease(iter);
}

NSString *PDGraphics(void)
{
    __block NSString *result = nil;

    eachService("IOPCIDevice", ^(io_service_t d) {
        NSString *model;

        if (result != nil || wordProperty(d, "class-code") >> 16 != 0x03)
            return;
        model = stringProperty(d, "model");
        if ([model length] > 0) {
            result = model;
            return;
        }
        switch (wordProperty(d, "vendor-id") & 0xffff) {
            case 0x1002: result = @"AMD Radeon"; break;
            case 0x10de: result = @"NVIDIA"; break;
            case 0x8086: result = @"Intel Graphics"; break;
            case 0x1af4: result = @"Virtio GPU"; break;
            case 0x1234: result = @"QEMU Standard VGA"; break;
            default: result = @"Display controller"; break;
        }
    });
    return result ?: @"Unknown";
}

NSString *PDHostName(void)
{
    char host[256] = "";
    NSString *name;

    gethostname(host, sizeof(host));
    name = [NSString stringWithUTF8String:host];
    if ([name length] == 0)
        name = @"puredarwin";
    return [name hasSuffix:@".local"] ? name : [name stringByAppendingString:@".local"];
}

NSString *PDBootArgs(void)
{
    return sysctlString("kern.bootargs") ?: @"";
}

NSString *PDStartupDisk(void)
{
    struct statfs fs;

    if (statfs("/", &fs) != 0)
        return @"PureDarwin";
    return [NSString stringWithFormat:@"PureDarwin · %s · %@", fs.f_fstypename,
                                      [[NSString stringWithUTF8String:fs.f_mntfromname] lastPathComponent]];
}

NSString *PDTimeZone(void)
{
    char buf[1024];
    ssize_t n = readlink("/etc/localtime", buf, sizeof(buf) - 1);
    NSString *path;
    NSRange zi;

    if (n <= 0)
        return [[NSTimeZone localTimeZone] name] ?: @"UTC";
    buf[n] = 0;
    path = [NSString stringWithUTF8String:buf];
    zi = [path rangeOfString:@"zoneinfo/"];
    return zi.location != NSNotFound ? [path substringFromIndex:NSMaxRange(zi)] : path;
}

NSSet *PDRunningProcesses(void)
{
    int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
    struct kinfo_proc *procs;
    size_t len = 0;
    NSMutableSet *names = [NSMutableSet set];

    if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0)
        return names;
    len += 16 * sizeof(*procs);
    if ((procs = malloc(len)) == NULL)
        return names;
    if (sysctl(mib, 3, procs, &len, NULL, 0) == 0) {
        for (size_t i = 0; i < len / sizeof(*procs); i++)
            [names addObject:[NSString stringWithUTF8String:procs[i].kp_proc.p_comm]];
    }
    free(procs);
    return names;
}

// the desktop session itself can't be switched off from inside it
static BOOL lockedDaemon(NSString *label)
{
    return [label isEqualToString:@"org.puredarwin.wayland-session"] || [label hasPrefix:@"com.apple.launchd"];
}

NSArray *PDLaunchDaemons(void)
{
    NSMutableArray *list = [NSMutableArray array];
    NSSet *running = PDRunningProcesses();

    for (NSString *dir in @[ @"/System/Library/LaunchDaemons", @"/Library/LaunchDaemons" ]) {
        for (NSString *name in [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:NULL] sortedArrayUsingSelector:@selector(compare:)]) {
            NSString *path = [dir stringByAppendingPathComponent:name];
            NSDictionary *plist;
            NSString *label, *program, *comm;

            if (![name hasSuffix:@".plist"] || (plist = [NSDictionary dictionaryWithContentsOfFile:path]) == nil)
                continue;
            label = plist[@"Label"] ?: [name stringByDeletingPathExtension];
            program = plist[@"Program"] ?: [plist[@"ProgramArguments"] firstObject] ?: @"";
            comm = [program lastPathComponent];
            if ([comm length] > MAXCOMLEN)
                comm = [comm substringToIndex:MAXCOMLEN];
            [list addObject:@{
                @"label": label,
                @"what": plist[@"ServiceDescription"] ?: program,
                @"path": path,
                // Disabled can hold other types in the wild, only a plain boolean counts
                @"disabled": @([plist[@"Disabled"] isKindOfClass:[NSNumber class]] && [plist[@"Disabled"] boolValue]),
                @"running": @([comm length] > 0 && [running containsObject:comm]),
                @"locked": @(lockedDaemon(label)),
            }];
        }
    }
    return list;
}

// launchd reads Disabled from the job's own plist, which is what launchctl -w used to rewrite
BOOL PDSetDaemonDisabled(NSString *path, BOOL disabled)
{
    NSMutableDictionary *plist = [NSMutableDictionary dictionaryWithContentsOfFile:path];
    NSData *data;

    if (plist == nil)
        return NO;
    if (disabled)
        plist[@"Disabled"] = @YES;
    else
        [plist removeObjectForKey:@"Disabled"];
    data = [NSPropertyListSerialization dataWithPropertyList:plist format:NSPropertyListXMLFormat_v1_0 options:0 error:NULL];
    return data != nil && [data writeToFile:path atomically:YES];
}

static NSString *familyFor(NSString *bundle, NSString *name)
{
    NSString *s = [[bundle stringByAppendingString:name] lowercaseString];
    NSArray *map = @[
        @[ @"usb", @"usb" ], @[ @"hid", @"input" ], @[ @"ps2", @"input" ], @[ @"audio", @"audio" ], @[ @"hda", @"audio" ],
        @[ @"ahci", @"storage" ], @[ @"nvme", @"storage" ], @[ @"storage", @"storage" ], @[ @"ext4", @"file system" ],
        @[ @"apfs", @"file system" ], @[ @"hfs", @"file system" ], @[ @"msdos", @"file system" ], @[ @"network", @"network" ],
        @[ @"e1000", @"network" ], @[ @"ethernet", @"network" ], @[ @"virtio", @"virtio" ], @[ @"framebuffer", @"display" ],
        @[ @"graphics", @"display" ], @[ @"radeon", @"display" ], @[ @"gop", @"display" ], @[ @"pci", @"bus" ], @[ @"acpi", @"platform" ],
    ];

    for (NSArray *m in map) {
        if ([s rangeOfString:m[0]].location != NSNotFound)
            return m[1];
    }
    return @"system";
}

NSArray *PDKernelExtensions(void)
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_registry_entry_t e;
    NSMutableSet *inUse = [NSMutableSet set];
    NSMutableArray *list = [NSMutableArray array];

    if (IORegistryCreateIterator(MACH_PORT_NULL, kIOServicePlane, kIORegistryIterateRecursively, &iter) == KERN_SUCCESS) {
        while ((e = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
            NSString *bundle = stringProperty(e, "CFBundleIdentifier");

            if ([bundle length] > 0)
                [inUse addObject:bundle];
            IOObjectRelease(e);
        }
        IOObjectRelease(iter);
    }
    for (NSString *name in [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:@"/System/Library/Extensions" error:NULL]
                               sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
        NSString *plist = [NSString stringWithFormat:@"/System/Library/Extensions/%@/Contents/Info.plist", name];
        NSString *bundle;

        if (![name hasSuffix:@".kext"])
            continue;
        bundle = [NSDictionary dictionaryWithContentsOfFile:plist][@"CFBundleIdentifier"] ?: @"";
        [list addObject:@{
            @"name": [name stringByDeletingPathExtension],
            @"family": familyFor(bundle, name),
            @"state": [inUse containsObject:bundle] ? @"in use" : @"not matched",
        }];
        [inUse removeObject:bundle];
    }
    // drivers linked into the kernel collection without a bundle on disk
    for (NSString *bundle in [[inUse allObjects] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
        if ([bundle hasPrefix:@"__kernel__"] || [bundle isEqualToString:@"com.apple.kernel"])
            continue;
        [list addObject:@{ @"name": [[bundle componentsSeparatedByString:@"."] lastObject], @"family": familyFor(bundle, @""), @"state": @"in use" }];
    }
    return list;
}

NSArray *PDNetworkInterfaces(void)
{
    struct ifaddrs *all = NULL, *a;
    NSMutableArray *rows = [NSMutableArray array];

    if (getifaddrs(&all) != 0)
        return rows;
    for (a = all; a != NULL; a = a->ifa_next) {
        char text[INET_ADDRSTRLEN];

        if (a->ifa_addr == NULL || a->ifa_addr->sa_family != AF_INET || (a->ifa_flags & IFF_LOOPBACK))
            continue;
        inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, text, sizeof(text));
        [rows addObject:@[ [NSString stringWithUTF8String:a->ifa_name],
                           [NSString stringWithFormat:@"%s · %@", text, (a->ifa_flags & IFF_RUNNING) ? @"connected" : @"no link"] ]];
    }
    freeifaddrs(all);
    return rows;
}

NSArray *PDNameServers(void)
{
    NSString *conf = [NSString stringWithContentsOfFile:@"/etc/resolv.conf" encoding:NSUTF8StringEncoding error:NULL];
    NSMutableArray *servers = [NSMutableArray array];

    for (NSString *line in [conf componentsSeparatedByString:@"\n"]) {
        NSArray *words = [line componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

        if ([words count] >= 2 && [words[0] isEqualToString:@"nameserver"])
            [servers addObject:words[1]];
    }
    return servers;
}

NSArray *PDAudioDevices(void)
{
    NSMutableArray *rows = [NSMutableArray array];

    eachService("IOAudioDevice", ^(io_service_t d) {
        NSString *name = stringProperty(d, "IOAudioDeviceName") ?: stringProperty(d, "IOAudioDeviceShortName") ?: @"Audio device";
        NSString *maker = stringProperty(d, "IOAudioDeviceManufacturerName");

        [rows addObject:@[ name, maker ?: @"IOAudioFamily" ]];
    });
    return rows;
}

NSArray *PDInputDevices(void)
{
    NSMutableArray *rows = [NSMutableArray array];

    eachService("IOHIDDevice", ^(io_service_t d) {
        NSString *product = stringProperty(d, "Product");
        NSString *transport = stringProperty(d, "Transport");

        if ([product length] > 0)
            [rows addObject:@[ product, transport ?: @"HID" ]];
    });
    return rows;
}

NSDictionary *PDBattery(void)
{
    __block NSDictionary *battery = nil;

    eachService("IOPMPowerSource", ^(io_service_t s) {
        uint32_t cur = wordProperty(s, "CurrentCapacity"), max = wordProperty(s, "MaxCapacity");

        if (battery == nil && max > 0)
            battery = @{ @"percent": @(cur * 100 / max), @"charging": @(wordProperty(s, "IsCharging") != 0) };
    });
    return battery;
}

NSDictionary *PDAccount(void)
{
    struct passwd *pw = getpwuid(getuid());
    NSString *gecos;

    if (pw == NULL)
        return @{ @"name": NSUserName() ?: @"root", @"short": NSUserName() ?: @"root", @"uid": @(getuid()), @"home": NSHomeDirectory(), @"shell": @"/bin/sh" };
    gecos = [[[NSString stringWithUTF8String:pw->pw_gecos ?: ""] componentsSeparatedByString:@","] firstObject];
    return @{
        @"name": [gecos length] > 0 ? gecos : [NSString stringWithUTF8String:pw->pw_name],
        @"short": [NSString stringWithUTF8String:pw->pw_name],
        @"uid": @(pw->pw_uid),
        @"home": [NSString stringWithUTF8String:pw->pw_dir],
        @"shell": [NSString stringWithUTF8String:pw->pw_shell],
    };
}

// the EFI system partition, by its GPT type
static NSString *espDevice(void)
{
    __block NSString *bsd = nil;

    eachService("IOMedia", ^(io_service_t m) {
        NSString *content = stringProperty(m, "Content");

        if (bsd == nil && [content caseInsensitiveCompare:@"C12A7328-F81F-11D2-BA4B-00A0C93EC93B"] == NSOrderedSame)
            bsd = stringProperty(m, "BSD Name");
    });
    return bsd;
}

static int run(NSArray *argv)
{
    char **args = calloc([argv count] + 1, sizeof(char *));
    pid_t pid;
    int status = -1;
    NSUInteger i;

    for (i = 0; i < [argv count]; i++)
        args[i] = (char *)[argv[i] UTF8String];
    if (posix_spawn(&pid, args[0], NULL, NULL, args, environ) == 0)
        waitpid(pid, &status, 0);
    free(args);
    return status;
}

NSString *PDWriteBootArgs(NSString *args)
{
    NSString *dev = espDevice(), *mnt = @"/Volumes/EFI", *file;
    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL ok;

    if (dev == nil)
        return @"No EFI partition was found.";
    [fm createDirectoryAtPath:mnt withIntermediateDirectories:YES attributes:nil error:NULL];
    // PureDarwin's mount handles msdos itself, there is no separate mount_msdos
    if (run(@[ @"/sbin/mount", @"-t", @"msdos", [@"/dev/" stringByAppendingString:dev], mnt ]) != 0)
        return [NSString stringWithFormat:@"Could not mount the EFI partition (%@).", dev];
    file = [mnt stringByAppendingPathComponent:@"EFI/BOOT/boot-args.txt"];
    ok = [[args stringByAppendingString:@"\n"] writeToFile:file atomically:NO encoding:NSUTF8StringEncoding error:NULL];
    unmount([mnt fileSystemRepresentation], 0);
    return ok ? nil : @"The EFI partition is mounted read-only.";
}
