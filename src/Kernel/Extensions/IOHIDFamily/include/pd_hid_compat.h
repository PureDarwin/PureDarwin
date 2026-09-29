// Keys and constants IOHIDFamily-1633.140.4 takes from SDK headers outside the open-source release,
// Apple's values where a later release publishes them, PureDarwin property names otherwise
#ifndef PD_HID_COMPAT_H
#define PD_HID_COMPAT_H

// Published in IOHIDFamily-2238.100.59, IOHIDEventServiceKeys_Private.h
#ifndef kIOHIDKeyboardEnabledKey
#define kIOHIDKeyboardEnabledKey "KeyboardEnabled"
#endif
#ifndef kIOHIDKeyboardEnabledEventKey
#define kIOHIDKeyboardEnabledEventKey "KeyboardEnabledEvent"
#endif
#ifndef kIOHIDKeyboardEnabledEventUsagePageKey
#define kIOHIDKeyboardEnabledEventUsagePageKey "UsagePage"
#endif
#ifndef kIOHIDKeyboardEnabledEventUsageKey
#define kIOHIDKeyboardEnabledEventUsageKey "Usage"
#endif
#ifndef kIOHIDKeyboardEnabledEventEventTypeKey
#define kIOHIDKeyboardEnabledEventEventTypeKey "EventType"
#endif
#ifndef kIOHIDKeyboardEnabledByEventKey
#define kIOHIDKeyboardEnabledByEventKey "KeyboardEnabledByEvent"
#endif
#ifndef kIOHIDEventTypeKey
#define kIOHIDEventTypeKey "EventType"
#endif
#ifndef kIOHIDUsagePageKey
#define kIOHIDUsagePageKey "UsagePage"
#endif
#ifndef kIOHIDUsageKey
#define kIOHIDUsageKey "Usage"
#endif
#ifndef kIOHIDSensorPropertySniffControlKey
#define kIOHIDSensorPropertySniffControlKey "SniffControl"
#endif
#ifndef kIOHIDEventServiceSensorPropertySupportedKey
#define kIOHIDEventServiceSensorPropertySupportedKey "SensorPropertySupported"
#endif
#ifndef PD_HID_SENSOR_PROPERTY_BITS
#define PD_HID_SENSOR_PROPERTY_BITS
enum {
	kSensorPropertyReportInterval = (1 << 0),
	kSensorPropertyReportLatency = (1 << 1),
	kSensorPropertySniffControl = (1 << 2),
	kSensorPropertySampleInterval = (1 << 3),
	kSensorPropertyMaxFIFOEvents = (1 << 4),
};
#endif

// Not published, PureDarwin property names
#ifndef kIOHIDMultipleInterfaceEnabledKey
#define kIOHIDMultipleInterfaceEnabledKey "MultipleInterfaceEnabled"
#endif
#ifndef kIOHIDRequiresTCCAuthorizationKey
#define kIOHIDRequiresTCCAuthorizationKey "RequiresTCCAuthorization"
#endif
#ifndef kIOHIDRegisterServiceKey
#define kIOHIDRegisterServiceKey "RegisterService"
#endif
#ifndef kIOHIDRelaySupportKey
#define kIOHIDRelaySupportKey "RelaySupport"
#endif
#ifndef kIOHIDProtectedAccessKey
#define kIOHIDProtectedAccessKey "ProtectedAccess"
#endif
#ifndef kIOHIDRelayServiceInterfaceActiveKey
#define kIOHIDRelayServiceInterfaceActiveKey "RelayServiceInterfaceActive"
#endif
#ifndef kIOHIDEventDriverBlessedUsagePairsKey
#define kIOHIDEventDriverBlessedUsagePairsKey "BlessedUsagePairs"
#endif
#ifndef kIOHIDDigitizerCollectionDispatchKey
#define kIOHIDDigitizerCollectionDispatchKey "DigitizerCollectionDispatch"
#endif
#ifndef kIOHIDExtendedDataKey
#define kIOHIDExtendedDataKey "ExtendedData"
#endif
#ifndef kIOHIDDeviceTypeHintKey
#define kIOHIDDeviceTypeHintKey "DeviceTypeHint"
#endif
#ifndef kIOHIDDeviceTypeHeadsetKey
#define kIOHIDDeviceTypeHeadsetKey "Headset"
#endif
#ifndef kIOHIDEventDriverBlessedUsagePageKey
#define kIOHIDEventDriverBlessedUsagePageKey "UsagePage"
#endif
#ifndef kIOHIDEventDriverBlessedUsageKey
#define kIOHIDEventDriverBlessedUsageKey "Usage"
#endif
// Vendor usages missing from this release's tables, valued past every defined one
// since they are switch cases
#ifndef PD_HID_VENDOR_USAGES
#define PD_HID_VENDOR_USAGES
enum {
	kHIDUsage_AppleVendorMotion_Type = 0x0009,
	kHIDUsage_AppleVendorMotion_Path = 0x000A,
	kHIDUsage_AppleVendorMotion_Generation = 0x000B,
	kHIDPage_AppleVendorSensor = 0xFF1F,
	kHIDUsage_AppleVendorSensor_BTSniffOff = 0x0001,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous = 0x0020,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortrait = 0x0021,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortraitUpsideDown = 0x0022,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypeLandscapeLeft = 0x0023,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypeLandscapeRight = 0x0024,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypeFaceUp = 0x0025,
	kHIDUsage_AppleVendorMotion_DeviceOrientationTypeFaceDown = 0x0026,
};
#endif
// Usage no real device reports, so the multiple-interface path stays off
#ifndef kHIDUsage_AppleVendor_MultipleInterfaces
#define kHIDUsage_AppleVendor_MultipleInterfaces 0xffff
#endif
// Property lists forwarded between device, interface and service, none so the
// forwarding loops compare against nothing
#ifndef kIOHIDPropagatePropertyKeys
#define kIOHIDPropagatePropertyKeys nullptr
#endif
#ifndef kIOHIDDeviceMessagePropertyUpdateKeys
#define kIOHIDDeviceMessagePropertyUpdateKeys nullptr
#endif

#endif /* PD_HID_COMPAT_H */
