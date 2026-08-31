//
//  IOHIDPreferences.h
//  IOKitUser
//
//  Created by AB on 10/25/19.
//

#ifndef IOHIDPreferences_h
#define IOHIDPreferences_h

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

/*! These APIs are XPC wrapper around corresponding CFPreferences API. https://developer.apple.com/documentation/corefoundation/preferences_utilities
 * XPC HID Preferences APIs are currently available for macOS only .
 */
CF_EXPORT
void IOHIDPreferencesSet(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFDictionaryRef __nullable IOHIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain);

CF_EXPORT
CFTypeRef __nullable IOHIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain);

CF_EXPORT
void IOHIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);


CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* IOHIDPreferences_h */
