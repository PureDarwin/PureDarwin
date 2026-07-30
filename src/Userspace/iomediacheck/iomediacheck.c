/*
 * iomediacheck - replay diskarbitrationd's DADiskCreateFromIOMedia() checks
 * against one IOMedia and report which one fails.
 *
 * DA logs only "unable to create disk, id = /dev/diskNsM" - it does not say
 * which of its ~30 mandatory checks bailed out. This performs the same checks in
 * the same order and prints each result, so the failing one is named.
 *
 * The two checks worth watching both walk the registry towards the parents,
 * which nothing in PureDarwin exercised before diskarbitrationd:
 *
 *   - IORegistryEntrySearchCFProperty(..., kIORegistryIterateParents)
 *     for IOMediaIcon, which partitions inherit from the whole media
 *   - IORegistryEntryCreateIterator(..., kIORegistryIterateParents) looking for
 *     an ancestor conforming to IOBlockStorageDevice
 *
 * ioreg exercises only the child direction, so a parent-direction bug would have
 * gone unnoticed until now.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOBlockStorageDevice.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void
report(const char *what, int ok, const char *detail)
{
	printf("  %-42s %s%s%s\n", what, ok ? "ok" : "FAIL",
	       detail ? "  " : "", detail ? detail : "");
	if (!ok) {
		failures++;
	}
}

static void
check_property(CFDictionaryRef properties, const char *key)
{
	CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key,
						  kCFStringEncodingUTF8);
	int ok;

	if (k == NULL) {
		report(key, 0, "(key alloc failed)");
		return;
	}
	ok = CFDictionaryGetValue(properties, k) != NULL;
	CFRelease(k);
	report(key, ok, NULL);
}

int
main(int argc, char *argv[])
{
	const char        *bsdname = (argc > 1) ? argv[1] : "disk0";
	CFMutableDictionaryRef matching;
	io_service_t       media;
	CFMutableDictionaryRef properties = NULL;
	kern_return_t      kr;
	CFTypeRef          icon;
	io_iterator_t      services = IO_OBJECT_NULL;
	io_service_t       device = IO_OBJECT_NULL;
	io_name_t          name;

	printf("iomediacheck: %s\n", bsdname);

	/*
	 * IOBSDNameMatching is itself one of the CoreFoundation-based IOKitLib
	 * entry points that only became available with the real IOKitUser port,
	 * so a failure here is already informative.
	 */
	matching = IOBSDNameMatching(kIOMasterPortDefault, 0, bsdname);
	if (matching == NULL) {
		report("IOBSDNameMatching", 0, "(returned NULL)");
		return 1;
	}
	report("IOBSDNameMatching", 1, NULL);

	media = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
	if (media == IO_OBJECT_NULL) {
		report("IOServiceGetMatchingService", 0, "(no such media)");
		return 1;
	}
	report("IOServiceGetMatchingService", 1, NULL);

	kr = IORegistryEntryCreateCFProperties(media, &properties,
					       kCFAllocatorDefault, 0);
	if (kr != KERN_SUCCESS || properties == NULL) {
		report("IORegistryEntryCreateCFProperties", 0, "(kr != success)");
		return 1;
	}
	report("IORegistryEntryCreateCFProperties", 1, NULL);

	/* The properties DA requires, in its order. */
	check_property(properties, kIOBSDNameKey);
	check_property(properties, kIOMediaPreferredBlockSizeKey);
	check_property(properties, kIOBSDMajorKey);
	check_property(properties, kIOBSDMinorKey);
	check_property(properties, kIOBSDUnitKey);
	check_property(properties, kIOMediaContentKey);
	check_property(properties, kIOMediaEjectableKey);
	check_property(properties, kIOMediaLeafKey);
	check_property(properties, kIOMediaRemovableKey);
	check_property(properties, kIOMediaSizeKey);
	check_property(properties, kIOMediaWholeKey);
	check_property(properties, kIOMediaWritableKey);

	/* Suspect 1: parent-direction property search. */
	icon = IORegistryEntrySearchCFProperty(media, kIOServicePlane,
					       CFSTR(kIOMediaIconKey),
					       kCFAllocatorDefault,
					       kIORegistryIterateParents |
					       kIORegistryIterateRecursively);
	report("SearchCFProperty(IOMediaIcon, parents)", icon != NULL,
	       icon != NULL ? NULL : "(DA bails here)");
	if (icon) {
		CFRelease(icon);
	}

	/* For contrast: the same key without walking parents. */
	icon = IORegistryEntrySearchCFProperty(media, kIOServicePlane,
					       CFSTR(kIOMediaIconKey),
					       kCFAllocatorDefault, 0);
	report("SearchCFProperty(IOMediaIcon, self only)", icon != NULL,
	       icon != NULL ? "(present on this entry)" : "(inherited, if at all)");
	if (icon) {
		CFRelease(icon);
	}

	/* Suspect 2: parent-direction iteration to the IOBlockStorageDevice. */
	kr = IORegistryEntryCreateIterator(media, kIOServicePlane,
					   kIORegistryIterateParents |
					   kIORegistryIterateRecursively,
					   &services);
	report("CreateIterator(parents)", kr == KERN_SUCCESS, NULL);

	if (kr == KERN_SUCCESS) {
		int seen = 0;

		while ((device = IOIteratorNext(services)) != IO_OBJECT_NULL) {
			if (IORegistryEntryGetName(device, name) == KERN_SUCCESS) {
				printf("      ancestor: %s\n", name);
			}
			seen++;
			if (IOObjectConformsTo(device, kIOBlockStorageDeviceClass)) {
				break;
			}
			IOObjectRelease(device);
			device = IO_OBJECT_NULL;
		}
		IOObjectRelease(services);

		printf("      ancestors walked: %d\n", seen);
		report("found IOBlockStorageDevice ancestor",
		       device != IO_OBJECT_NULL,
		       device != IO_OBJECT_NULL ? NULL : "(DA bails here)");
	}

	/*
	 * The media UUID. DA turns the "UUID" string property into a CFUUID and
	 * bails if that conversion fails - so this only matters for partitions,
	 * which carry a UUID; a whole disk has none and DA skips it.
	 */
	{
		CFStringRef uuidString = CFDictionaryGetValue(properties,
							      CFSTR(kIOMediaUUIDKey));

		if (uuidString == NULL) {
			report("media UUID", 1, "(absent - DA skips)");
		} else {
			CFUUIDRef uuid = CFUUIDCreateFromString(kCFAllocatorDefault,
								uuidString);

			report("CFUUIDCreateFromString(UUID)", uuid != NULL,
			       uuid != NULL ? NULL : "(DA bails here)");
			if (uuid) {
				CFRelease(uuid);
			}
		}
	}

	if (device == IO_OBJECT_NULL) {
		printf("%s: %d check(s) failed (no device, stopping)\n",
		       bsdname, failures);
		return failures ? 1 : 0;
	}

	/* Device properties and its path in the service plane. */
	{
		CFMutableDictionaryRef devProps = NULL;
		io_string_t            path;

		kr = IORegistryEntryCreateCFProperties(device, &devProps,
						       kCFAllocatorDefault, 0);
		report("device CreateCFProperties", kr == KERN_SUCCESS, NULL);
		if (devProps) {
			CFRelease(devProps);
		}

		kr = IORegistryEntryGetPath(device, kIOServicePlane, path);
		report("device GetPath(IOService)", kr == KERN_SUCCESS,
		       kr == KERN_SUCCESS ? path : "(DA bails here)");
	}

	/*
	 * The bus: DA walks the device's parents for the first entry that is also
	 * in the IODeviceTree plane, then asks for its name and path *in that
	 * plane*. Those two calls are the last untested things in DA's path, and
	 * nothing else in PureDarwin asks for a name or path in IODeviceTree.
	 */
	{
		io_service_t bus = IO_OBJECT_NULL;

		kr = IORegistryEntryCreateIterator(device, kIOServicePlane,
						   kIORegistryIterateParents |
						   kIORegistryIterateRecursively,
						   &services);
		report("device CreateIterator(parents)", kr == KERN_SUCCESS, NULL);

		if (kr == KERN_SUCCESS) {
			while ((bus = IOIteratorNext(services)) != IO_OBJECT_NULL) {
				if (IORegistryEntryGetName(bus, name) == KERN_SUCCESS) {
					printf("      device ancestor: %-24s inDeviceTree=%s\n",
					       name,
					       IORegistryEntryInPlane(bus, kIODeviceTreePlane)
					           ? "yes" : "no");
				}
				if (IORegistryEntryInPlane(bus, kIODeviceTreePlane)) {
					break;
				}
				IOObjectRelease(bus);
				bus = IO_OBJECT_NULL;
			}
			IOObjectRelease(services);
		}

		if (bus == IO_OBJECT_NULL) {
			/* DA guards the bus block with if (bus), so this is not fatal. */
			report("bus in IODeviceTree", 1, "(none - DA skips bus block)");
		} else {
			io_name_t   busname;
			io_string_t buspath;

			kr = IORegistryEntryGetNameInPlane(bus, kIODeviceTreePlane, busname);
			report("bus GetNameInPlane(IODeviceTree)", kr == KERN_SUCCESS,
			       kr == KERN_SUCCESS ? busname : "(DA bails here)");

			kr = IORegistryEntryGetPath(bus, kIODeviceTreePlane, buspath);
			report("bus GetPath(IODeviceTree)", kr == KERN_SUCCESS,
			       kr == KERN_SUCCESS ? buspath : "(DA bails here)");

			IOObjectRelease(bus);
		}
	}

	IOObjectRelease(device);

	printf("%s: %d check(s) failed\n", bsdname, failures);
	return failures ? 1 : 0;
}
