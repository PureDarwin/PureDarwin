#include <CoreServices/CoreServices.h>
#include <string.h>
#include "common.h"

int main(void)
{
    FSRef ref, ref2;
    UInt8 path[1024];
    Boolean dir = false;

    CHECK(FSPathMakeRef((const UInt8 *)"/usr/lib", &ref, &dir) == noErr && dir, "FSPathMakeRef /usr/lib is a directory");
    CHECK(FSRefMakePath(&ref, path, sizeof(path)) == noErr && strcmp((char *)path, "/usr/lib") == 0, "FSRefMakePath gives %s", path);
    CHECK(FSPathMakeRef((const UInt8 *)"/usr/lib/", &ref2, NULL) == noErr && FSCompareFSRefs(&ref, &ref2) == noErr, "FSCompareFSRefs same dir");
    CHECK(FSPathMakeRef((const UInt8 *)"/no/such/file", &ref, NULL) == fnfErr, "FSPathMakeRef missing file is fnfErr");
    CHECK(FSFindFolder(kUserDomain, kApplicationSupportFolderType, true, &ref) == noErr &&
          FSRefMakePath(&ref, path, sizeof(path)) == noErr, "FSFindFolder app support: %s", path);
    CHECK(FSFindFolder(kUserDomain, kTemporaryFolderType, false, &ref) == noErr, "FSFindFolder temporary");

    CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, CFSTR("html"), NULL);
    CHECK(uti && CFEqual(uti, kUTTypeHTML), "html extension is public.html");
    CHECK(uti && UTTypeConformsTo(uti, kUTTypeText), "public.html conforms to public.text");
    CFStringRef mime = uti ? UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType) : NULL;
    CHECK(mime && CFEqual(mime, CFSTR("text/html")), "public.html MIME type is text/html");
    CFStringRef dyn = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, CFSTR("zzqq"), NULL);
    CHECK(dyn && UTTypeIsDynamic(dyn), "unknown extension gives a dynamic type");

    CFURLRef url = CFURLCreateWithString(NULL, CFSTR("file:///tmp/pdtest-sfl"), NULL);
    LSSharedFileListRef list = LSSharedFileListCreate(NULL, kLSSharedFileListRecentDocumentItems, NULL);
    LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, CFSTR("t"), NULL, url, NULL, NULL);
    CFArrayRef snap = LSSharedFileListCopySnapshot(list, NULL);
    CHECK(item && snap && CFArrayGetCount(snap) >= 1, "LSSharedFileList insert and snapshot (%ld items)", snap ? (long)CFArrayGetCount(snap) : -1L);
    CHECK(LSSharedFileListItemRemove(list, item) == noErr, "LSSharedFileListItemRemove");

    MDItemRef md = MDItemCreate(NULL, CFSTR("/usr/lib"));
    CFStringRef name = md ? MDItemCopyAttribute(md, kMDItemDisplayName) : NULL;
    CHECK(name && CFEqual(name, CFSTR("lib")), "MDItem display name");
    CFTypeRef where = md ? MDItemCopyAttribute(md, kMDItemWhereFroms) : NULL;
    CHECK(where == NULL, "kMDItemWhereFroms absent on a file with no download origin");

    CFURLRef http = CFURLCreateWithString(NULL, CFSTR("http://example.org"), NULL);
    CFURLRef app = LSCopyDefaultApplicationURLForURL(http, kLSRolesAll, NULL);
    printf("info LSCopyDefaultApplicationURLForURL(http) = %s\n", app ? "found" : "none");
    CFStringRef handler = LSCopyDefaultHandlerForURLScheme(CFSTR("pdtestscheme"));
    CHECK(handler == NULL, "no handler for an unregistered scheme");
    CHECK(LSSetDefaultHandlerForURLScheme(CFSTR("pdtestscheme"), CFSTR("org.puredarwin.test")) == noErr, "LSSetDefaultHandlerForURLScheme");
    // an override naming an app that is not installed is stored but not returned
    CFPreferencesAppSynchronize(CFSTR("com.apple.LaunchServices"));
    CFArrayRef stored = CFPreferencesCopyAppValue(CFSTR("LSHandlers"), CFSTR("com.apple.LaunchServices"));
    CHECK(stored && CFArrayGetCount(stored) >= 1, "handler override stored in LSHandlers");
    CHECK(LSCopyDefaultHandlerForURLScheme(CFSTR("pdtestscheme")) == NULL, "override to a missing app is not returned");
    DONE("CoreServices");
}
