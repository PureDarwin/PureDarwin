/*
 * CFURLCopyResourcePropertyForKey() / kCFURLFileSizeKey.
 *
 * PureDarwin's CoreFoundation comes from swift-corelibs-foundation, where the
 * URL resource-property API is a trampoline into the Swift NSURL bridge
 * (CFURL.c: "The base implementation of these functions exclusively exists in
 * Swift only") - so it resolves to nothing in a DEPLOYMENT_RUNTIME_OBJC build
 * like this one.
 *
 * SystemConfiguration uses exactly one property, the file size in
 * _SCCreatePropertyListFromResource(), to size a plist before reading it.
 * That answer comes from stat(2), which is what the real implementation
 * ultimately reports too.
 */

#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>

const CFStringRef kCFURLFileSizeKey = CFSTR("NSURLFileSizeKey");

Boolean
CFURLCopyResourcePropertyForKey(CFURLRef url, CFStringRef key,
    void *propertyValueTypeRefPtr, CFErrorRef *error)
{
	char path[PATH_MAX];
	struct stat sb;
	SInt64 size;

	if (error != NULL) {
		*error = NULL;
	}
	if (url == NULL || key == NULL || propertyValueTypeRefPtr == NULL) {
		return FALSE;
	}
	if (!CFEqual(key, kCFURLFileSizeKey)) {
		/* No other property is used here; report "not available" rather
		 * than inventing an answer. */
		*(CFTypeRef *)propertyValueTypeRefPtr = NULL;
		return FALSE;
	}
	if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)path,
	    sizeof(path))) {
		return FALSE;
	}
	if (stat(path, &sb) != 0) {
		return FALSE;
	}

	size = (SInt64)sb.st_size;
	*(CFTypeRef *)propertyValueTypeRefPtr =
	    CFNumberCreate(NULL, kCFNumberSInt64Type, &size);
	return TRUE;
}
