//
//  IOHIDPreferences.c
//  IOKit
//
//  Created by AB on 10/25/19.
//

#include "IOHIDPreferences.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include "IOHIDLibPrivate.h"

#define HIDPreferencesFrameworkPath "/System/Library/PrivateFrameworks/HIDPreferences.framework/HIDPreferences"

#define kHIDPreferencesSet          "HIDPreferencesSet"
#define kHIDPreferencesSetMultiple  "HIDPreferencesSetMultiple"
#define kHIDPreferencesCopy         "HIDPreferencesCopy"
#define kHIDPreferencesCopyMultiple "HIDPreferencesCopyMultiple"
#define kHIDPreferencesSynchronize  "HIDPreferencesSynchronize"
#define kHIDPreferencesCopyDomain   "HIDPreferencesCopyDomain"
#define kHIDPreferencesSetDomain    "HIDPreferencesSetDomain"


typedef void (*HIDPreferencesSetPtr)(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSetMultiplePtr)(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyPtr)(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFDictionaryRef (*HIDPreferencesCopyMultiplePtr)(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSynchronizePtr)(CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyDomainPtr)(CFStringRef key, CFStringRef domain);
typedef void (*HIDPreferencesSetDomainPtr)(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);



static HIDPreferencesSetPtr __setPtr = NULL;
static HIDPreferencesSetMultiplePtr __setMultiplePtr = NULL;
static HIDPreferencesCopyPtr __copyPtr = NULL;
static HIDPreferencesCopyMultiplePtr __copyMultiplePtr = NULL;
static HIDPreferencesSynchronizePtr __synchronizePtr = NULL;
static HIDPreferencesCopyDomainPtr __copyDomainPtr = NULL;
static HIDPreferencesSetDomainPtr __setDomainPtr = NULL;


static void* HIDPreferencesFrameworkGetSymbol( void* handle, const char* symbolName) {
    
    if (!handle) {
        return NULL;
    }
    
    return dlsym(handle, symbolName);
}

static void __loadFramework()
{
 
    static void *haHandle = NULL;
    static dispatch_once_t haOnce = 0;
    
    dispatch_once(&haOnce, ^{
        
        haHandle = dlopen(HIDPreferencesFrameworkPath, RTLD_LAZY);
        // safe place for saving required symbols
        if (!haHandle) {
            IOHIDLog("Failed to load %s",HIDPreferencesFrameworkPath);
            return;
        }
        __setPtr = (HIDPreferencesSetPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSet);
        __setMultiplePtr = (HIDPreferencesSetMultiplePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetMultiple);
        __copyPtr = (HIDPreferencesCopyPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopy);
        __copyMultiplePtr = (HIDPreferencesCopyMultiplePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyMultiple);
        __synchronizePtr = (HIDPreferencesSynchronizePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSynchronize);
        __copyDomainPtr = (HIDPreferencesCopyDomainPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyDomain);
        __setDomainPtr = (HIDPreferencesSetDomainPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetDomain);
            
    });
    
}



void IOHIDPreferencesSet(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();
    //Switch back to original
    if (!__setPtr) {
        IOHIDLogInfo("Failed to find %s for set, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetValue(key, value, domain, user, host);
        return;
    }
    
    __setPtr(key, value, user, host, domain);
}

void IOHIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__setMultiplePtr) {
        IOHIDLogInfo("Failed to find %s for set multiple , switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetMultiple(keysToSet, keysToRemove, domain, user, host);
        return;
    }
    
    __setMultiplePtr(keysToSet, keysToRemove, user, host, domain);
}


CFTypeRef __nullable IOHIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyPtr) {
        IOHIDLogInfo("Failed to find %s for copy, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyValue(key, domain, user, host);
    }
    
    return __copyPtr(key, user, host, domain);
    
}

CFDictionaryRef __nullable IOHIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyMultiplePtr) {
        IOHIDLogInfo("Failed to find %s for copy multiple, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyMultiple(keys, domain, user, host);
    }
    
    return __copyMultiplePtr(keys, user, host, domain);
}

void IOHIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__synchronizePtr) {
        IOHIDLogInfo("Failed to find %s for synchronize, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSynchronize(domain, user, host);
        return;
    }
    
    __synchronizePtr(user, host, domain);
    
}

CFTypeRef __nullable IOHIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyDomainPtr) {
        IOHIDLogInfo("Failed to find %s for copy domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyAppValue(key, domain);;
    }
    
    return __copyDomainPtr(key, domain);
    
}

void IOHIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain) {
    
    __loadFramework();

    if (!__setDomainPtr) {
        IOHIDLogInfo("Failed to find %s for set domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetAppValue(key, value, domain);
        return;
    }
    
    __setDomainPtr(key, value, domain);
}
