// CFNetwork's proxy API over SystemConfiguration's proxy settings, which is where macOS keeps them too.
// Proxy auto-configuration scripts need a JavaScript engine this system has none of: they are reported, not run
#include <CFNetwork/CFProxySupport.h>
#include <dlfcn.h>

const CFStringRef kCFProxyTypeKey = CFSTR("kCFProxyTypeKey");
const CFStringRef kCFProxyHostNameKey = CFSTR("kCFProxyHostNameKey");
const CFStringRef kCFProxyPortNumberKey = CFSTR("kCFProxyPortNumberKey");
const CFStringRef kCFProxyAutoConfigurationURLKey = CFSTR("kCFProxyAutoConfigurationURLKey");
const CFStringRef kCFProxyAutoConfigurationJavaScriptKey = CFSTR("kCFProxyAutoConfigurationJavaScriptKey");
const CFStringRef kCFProxyUsernameKey = CFSTR("kCFProxyUsernameKey");
const CFStringRef kCFProxyPasswordKey = CFSTR("kCFProxyPasswordKey");
const CFStringRef kCFProxyAutoConfigurationHTTPResponseKey = CFSTR("kCFProxyAutoConfigurationHTTPResponseKey");

const CFStringRef kCFProxyTypeNone = CFSTR("kCFProxyTypeNone");
const CFStringRef kCFProxyTypeHTTP = CFSTR("kCFProxyTypeHTTP");
const CFStringRef kCFProxyTypeHTTPS = CFSTR("kCFProxyTypeHTTPS");
const CFStringRef kCFProxyTypeSOCKS = CFSTR("kCFProxyTypeSOCKS");
const CFStringRef kCFProxyTypeFTP = CFSTR("kCFProxyTypeFTP");
const CFStringRef kCFProxyTypeAutoConfigurationURL = CFSTR("kCFProxyTypeAutoConfigurationURL");
const CFStringRef kCFProxyTypeAutoConfigurationJavaScript = CFSTR("kCFProxyTypeAutoConfigurationJavaScript");

const CFStringRef kCFNetworkProxiesExceptionsList = CFSTR("ExceptionsList");
const CFStringRef kCFNetworkProxiesExcludeSimpleHostnames = CFSTR("ExcludeSimpleHostnames");
const CFStringRef kCFNetworkProxiesFTPEnable = CFSTR("FTPEnable");
const CFStringRef kCFNetworkProxiesFTPPassive = CFSTR("FTPPassive");
const CFStringRef kCFNetworkProxiesFTPPort = CFSTR("FTPPort");
const CFStringRef kCFNetworkProxiesFTPProxy = CFSTR("FTPProxy");
const CFStringRef kCFNetworkProxiesHTTPEnable = CFSTR("HTTPEnable");
const CFStringRef kCFNetworkProxiesHTTPPort = CFSTR("HTTPPort");
const CFStringRef kCFNetworkProxiesHTTPProxy = CFSTR("HTTPProxy");
const CFStringRef kCFNetworkProxiesHTTPSEnable = CFSTR("HTTPSEnable");
const CFStringRef kCFNetworkProxiesHTTPSPort = CFSTR("HTTPSPort");
const CFStringRef kCFNetworkProxiesHTTPSProxy = CFSTR("HTTPSProxy");
const CFStringRef kCFNetworkProxiesSOCKSEnable = CFSTR("SOCKSEnable");
const CFStringRef kCFNetworkProxiesSOCKSPort = CFSTR("SOCKSPort");
const CFStringRef kCFNetworkProxiesSOCKSProxy = CFSTR("SOCKSProxy");
const CFStringRef kCFNetworkProxiesProxyAutoConfigEnable = CFSTR("ProxyAutoConfigEnable");
const CFStringRef kCFNetworkProxiesProxyAutoConfigURLString = CFSTR("ProxyAutoConfigURLString");
const CFStringRef kCFNetworkProxiesProxyAutoConfigJavaScript = CFSTR("ProxyAutoConfigJavaScript");
const CFStringRef kCFNetworkProxiesProxyAutoDiscoveryEnable = CFSTR("ProxyAutoDiscoveryEnable");

// SCDynamicStoreCopyProxies, looked up so CFNetwork does not link SystemConfiguration
static CFDictionaryRef copyStoreProxies(void)
{
    static CFDictionaryRef (*copy)(void *);
    static int looked;

    if (!looked) {
        void *sc = dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", RTLD_LAZY);

        copy = sc ? (CFDictionaryRef (*)(void *))dlsym(sc, "SCDynamicStoreCopyProxies") : NULL;
        looked = 1;
    }
    return copy ? copy(NULL) : NULL;
}

CFDictionaryRef CFNetworkCopySystemProxySettings(void)
{
    CFDictionaryRef proxies = copyStoreProxies();

    if (proxies)
        return proxies;
    return CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
}

static Boolean enabled(CFDictionaryRef settings, CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(settings, key);
    int on = 0;

    if (value && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberIntType, &on);
    return on != 0;
}

static void addProxy(CFMutableArrayRef list, CFStringRef type, CFDictionaryRef settings, CFStringRef hostKey,
    CFStringRef portKey)
{
    CFMutableDictionaryRef proxy = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFTypeRef host = hostKey ? CFDictionaryGetValue(settings, hostKey) : NULL;
    CFTypeRef port = portKey ? CFDictionaryGetValue(settings, portKey) : NULL;

    CFDictionarySetValue(proxy, kCFProxyTypeKey, type);
    if (host)
        CFDictionarySetValue(proxy, kCFProxyHostNameKey, host);
    if (port)
        CFDictionarySetValue(proxy, kCFProxyPortNumberKey, port);
    CFArrayAppendValue(list, proxy);
    CFRelease(proxy);
}

// the proxies to try for a URL, most specific first, always ending with a direct connection
CFArrayRef CFNetworkCopyProxiesForURL(CFURLRef url, CFDictionaryRef settings)
{
    CFMutableArrayRef list = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFStringRef scheme = url ? CFURLCopyScheme(url) : NULL;

    if (settings) {
        if (enabled(settings, kCFNetworkProxiesProxyAutoConfigEnable)) {
            CFTypeRef pac = CFDictionaryGetValue(settings, kCFNetworkProxiesProxyAutoConfigURLString);
            CFURLRef pacURL = pac ? CFURLCreateWithString(kCFAllocatorDefault, pac, NULL) : NULL;

            if (pacURL) {
                CFMutableDictionaryRef proxy = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

                CFDictionarySetValue(proxy, kCFProxyTypeKey, kCFProxyTypeAutoConfigurationURL);
                CFDictionarySetValue(proxy, kCFProxyAutoConfigurationURLKey, pacURL);
                CFArrayAppendValue(list, proxy);
                CFRelease(proxy);
                CFRelease(pacURL);
            }
        }
        if (scheme && CFStringCompare(scheme, CFSTR("https"), kCFCompareCaseInsensitive) == kCFCompareEqualTo &&
            enabled(settings, kCFNetworkProxiesHTTPSEnable))
            addProxy(list, kCFProxyTypeHTTPS, settings, kCFNetworkProxiesHTTPSProxy, kCFNetworkProxiesHTTPSPort);
        else if (scheme && CFStringCompare(scheme, CFSTR("http"), kCFCompareCaseInsensitive) == kCFCompareEqualTo &&
            enabled(settings, kCFNetworkProxiesHTTPEnable))
            addProxy(list, kCFProxyTypeHTTP, settings, kCFNetworkProxiesHTTPProxy, kCFNetworkProxiesHTTPPort);
        else if (scheme && CFStringCompare(scheme, CFSTR("ftp"), kCFCompareCaseInsensitive) == kCFCompareEqualTo &&
            enabled(settings, kCFNetworkProxiesFTPEnable))
            addProxy(list, kCFProxyTypeFTP, settings, kCFNetworkProxiesFTPProxy, kCFNetworkProxiesFTPPort);
        if (enabled(settings, kCFNetworkProxiesSOCKSEnable))
            addProxy(list, kCFProxyTypeSOCKS, settings, kCFNetworkProxiesSOCKSProxy, kCFNetworkProxiesSOCKSPort);
    }
    addProxy(list, kCFProxyTypeNone, settings, NULL, NULL);
    if (scheme)
        CFRelease(scheme);
    return list;
}

CFArrayRef CFNetworkCopyProxiesForAutoConfigurationScript(CFStringRef script, CFURLRef targetURL, CFErrorRef *error)
{
    if (error)
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, 78 /* ENOSYS */, NULL);
    return NULL;
}

// callers treat a missing source as a script that could not be run and connect directly
CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationURL(CFURLRef proxyAutoConfigURL, CFURLRef targetURL,
    CFProxyAutoConfigurationResultCallback cb, CFStreamClientContext *clientContext)
{
    return NULL;
}

CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationScript(CFStringRef script, CFURLRef targetURL,
    CFProxyAutoConfigurationResultCallback cb, CFStreamClientContext *clientContext)
{
    return NULL;
}
