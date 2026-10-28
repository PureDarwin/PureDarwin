// PureDarwin CFNetwork: the proxy API (CFProxySupport.h)
#ifndef CFNETWORK_CFPROXYSUPPORT_H
#define CFNETWORK_CFPROXYSUPPORT_H

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

typedef void (*CFProxyAutoConfigurationResultCallback)(void *client, CFArrayRef proxyList, CFErrorRef error);

CF_EXPORT CFDictionaryRef CFNetworkCopySystemProxySettings(void);
CF_EXPORT CFArrayRef CFNetworkCopyProxiesForURL(CFURLRef url, CFDictionaryRef proxySettings);
CF_EXPORT CFArrayRef CFNetworkCopyProxiesForAutoConfigurationScript(CFStringRef proxyAutoConfigurationScript,
    CFURLRef targetURL, CFErrorRef *error);
CF_EXPORT CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationURL(CFURLRef proxyAutoConfigURL, CFURLRef targetURL,
    CFProxyAutoConfigurationResultCallback cb, CFStreamClientContext *clientContext);
CF_EXPORT CFRunLoopSourceRef CFNetworkExecuteProxyAutoConfigurationScript(CFStringRef proxyAutoConfigurationScript,
    CFURLRef targetURL, CFProxyAutoConfigurationResultCallback cb, CFStreamClientContext *clientContext);

// the keys of a proxy dictionary
CF_EXPORT const CFStringRef kCFProxyTypeKey;
CF_EXPORT const CFStringRef kCFProxyHostNameKey;
CF_EXPORT const CFStringRef kCFProxyPortNumberKey;
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationURLKey;
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationJavaScriptKey;
CF_EXPORT const CFStringRef kCFProxyUsernameKey;
CF_EXPORT const CFStringRef kCFProxyPasswordKey;
CF_EXPORT const CFStringRef kCFProxyAutoConfigurationHTTPResponseKey;

// the values of kCFProxyTypeKey
CF_EXPORT const CFStringRef kCFProxyTypeNone;
CF_EXPORT const CFStringRef kCFProxyTypeHTTP;
CF_EXPORT const CFStringRef kCFProxyTypeHTTPS;
CF_EXPORT const CFStringRef kCFProxyTypeSOCKS;
CF_EXPORT const CFStringRef kCFProxyTypeFTP;
CF_EXPORT const CFStringRef kCFProxyTypeAutoConfigurationURL;
CF_EXPORT const CFStringRef kCFProxyTypeAutoConfigurationJavaScript;

// the keys of CFNetworkCopySystemProxySettings, SystemConfiguration's proxy keys
CF_EXPORT const CFStringRef kCFNetworkProxiesExceptionsList;
CF_EXPORT const CFStringRef kCFNetworkProxiesExcludeSimpleHostnames;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPPassive;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesFTPProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesHTTPSProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSPort;
CF_EXPORT const CFStringRef kCFNetworkProxiesSOCKSProxy;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigEnable;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigURLString;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoConfigJavaScript;
CF_EXPORT const CFStringRef kCFNetworkProxiesProxyAutoDiscoveryEnable;

CF_EXTERN_C_END

#endif
