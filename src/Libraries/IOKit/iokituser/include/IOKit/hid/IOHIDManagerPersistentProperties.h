/*
 * PureDarwin reconstruction: this header ships empty in the IOKitUser drop.
 * Every signature below is taken from the definitions in IOHIDManager.c and
 * IOHIDElement.c, which are present; only the declarations were stripped.
 */

#ifndef _IOKIT_HID_IOHIDMANAGERPERSISTENTPROPERTIES_H
#define _IOKIT_HID_IOHIDMANAGERPERSISTENTPROPERTIES_H

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>   /* IOOptionBits */
#include <IOKit/hid/IOHIDBase.h>
#include <IOKit/hid/IOHIDManager.h>

__BEGIN_DECLS

/*
 * Initialised positionally at IOHIDManager.c:1391 from
 * IOHIDManagerSaveToPropertyDomain's arguments, and the three CFStringRefs are
 * passed straight to CFPreferencesSetValue at :1457.
 */
typedef struct {
	CFStringRef	applicationID;
	CFStringRef	userName;
	CFStringRef	hostName;
	IOOptionBits	options;
} __IOHIDPropertyContext;

/*
 * CFPreferences root key for the manager's own properties (IOHIDManager.c:1406)
 * and the device property holding its persistent UUID string
 * (IOHIDDevice.c:2060). Both are CFSTR()'d, so they are C string literals; the
 * spellings are ours, and only matter for reading prefs this library wrote.
 */
#ifndef kIOHIDManagerKey
#define kIOHIDManagerKey	"HIDManager"
#endif
#ifndef kIOHIDManagerUUIDKey
#define kIOHIDManagerUUIDKey	"HIDManagerUUID"
#endif

/* CFDictionaryApplierFunction; context is the IOHIDDeviceRef.
 * Defined in IOHIDDevice.c. */
void __IOHIDApplyPropertiesToDeviceFromDictionary(const void *key,
    const void *value, void *context);

/*
 * Initialised { key, value } at IOHIDManager.c:938 and consumed as
 * data->key / data->property by __IOHIDApplyPropertyToDeviceSet.
 */
typedef struct {
	CFStringRef	key;
	CFTypeRef	property;
} __IOHIDApplyPropertyToSetContext;

void __IOHIDApplyPropertyToDeviceSet(const void *value, void *context);

void __IOHIDPropertySaveWithContext(CFStringRef key, CFPropertyListRef value,
    __IOHIDPropertyContext *context);
void __IOHIDPropertySaveToKeyWithSpecialKeys(CFDictionaryRef dictionary,
    CFStringRef key, CFStringRef *specialKeys, __IOHIDPropertyContext *context);
CFMutableDictionaryRef __IOHIDPropertyLoadDictionaryFromKey(CFStringRef key);
CFMutableDictionaryRef __IOHIDPropertyLoadFromKeyWithSpecialKeys(CFStringRef key,
    CFStringRef *specialKeys);

CFStringRef __IOHIDManagerGetRootKey(void);
void __IOHIDManagerSaveProperties(IOHIDManagerRef manager,
    __IOHIDPropertyContext *context);
void __IOHIDManagerLoadProperties(IOHIDManagerRef manager);

CFStringRef __IOHIDDeviceGetUUIDKey(IOHIDDeviceRef device);
void __IOHIDDeviceSaveProperties(IOHIDDeviceRef device,
    __IOHIDPropertyContext *context);
void __IOHIDDeviceLoadProperties(IOHIDDeviceRef device);
/* CFSetApplyFunction callbacks, so the CFSetApplierFunction shape. */
void __IOHIDSaveDeviceSet(const void *value, void *context);
void __IOHIDLoadDeviceSet(const void *value, void *context);

CFStringRef __IOHIDElementGetRootKey(IOHIDElementRef element);
void __IOHIDElementSaveProperties(IOHIDElementRef element,
    __IOHIDPropertyContext *context);
void __IOHIDElementLoadProperties(IOHIDElementRef element);
void __IOHIDSaveElementSet(const void *value, void *context);
void __IOHIDLoadElementSet(const void *value, void *context);

__END_DECLS

#endif /* _IOKIT_HID_IOHIDMANAGERPERSISTENTPROPERTIES_H */
