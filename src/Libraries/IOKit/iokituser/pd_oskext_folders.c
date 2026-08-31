/*
 * OSKextGetSystemExtensionsFolderURLs, split out of kext.subproj: it is the
 * only OSKext call IOCFPlugIn.c makes, and the rest of that subproject needs a
 * kext-loading story PureDarwin does not have yet. Folder list and order from
 * kext.subproj/OSKextPrivate.h.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>

static CFArrayRef __sSystemExtensionsFolderURLs;
static pthread_once_t __sOnce = PTHREAD_ONCE_INIT;

static void
__initSystemExtensionsFolderURLs(void)
{
	static const char *const paths[] = {
		"/System/Library/Extensions",
		"/Library/Extensions",
		"/AppleInternal/Library/Extensions",
		"/System/Library/DriverExtensions",
		"/Library/DriverExtensions",
		"/Library/Apple/System/Library/Extensions",
	};
	const CFIndex count = (CFIndex)(sizeof(paths) / sizeof(paths[0]));
	CFMutableArrayRef urls;
	CFIndex i;

	urls = CFArrayCreateMutable(kCFAllocatorDefault, count,
	    &kCFTypeArrayCallBacks);
	if (urls == NULL) {
		return;
	}

	for (i = 0; i < count; i++) {
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(
		    kCFAllocatorDefault, (const UInt8 *)paths[i],
		    (CFIndex)strlen(paths[i]), true);
		if (url == NULL) {
			continue;
		}
		CFArrayAppendValue(urls, url);
		CFRelease(url);
	}

	__sSystemExtensionsFolderURLs = urls;
}

CFArrayRef
OSKextGetSystemExtensionsFolderURLs(void)
{
	pthread_once(&__sOnce, __initSystemExtensionsFolderURLs);
	return __sSystemExtensionsFolderURLs;
}
