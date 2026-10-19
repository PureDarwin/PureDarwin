// Carbon.h - the HIToolbox parts of Carbon that Cocoa applications still call
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_CARBON_H
#define PD_CARBON_H

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CoreGraphics/CGContext.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARBON_EXPORT extern __attribute__((visibility("default")))

// text input sources
typedef struct __TISInputSource *TISInputSourceRef;

CARBON_EXPORT const CFStringRef kTISPropertyInputSourceCategory;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceType;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceIsASCIICapable;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceIsEnableCapable;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceIsSelectCapable;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceIsEnabled;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceIsSelected;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceID;
CARBON_EXPORT const CFStringRef kTISPropertyBundleID;
CARBON_EXPORT const CFStringRef kTISPropertyInputModeID;
CARBON_EXPORT const CFStringRef kTISPropertyLocalizedName;
CARBON_EXPORT const CFStringRef kTISPropertyInputSourceLanguages;
CARBON_EXPORT const CFStringRef kTISPropertyUnicodeKeyLayoutData;
CARBON_EXPORT const CFStringRef kTISPropertyIconRef;
CARBON_EXPORT const CFStringRef kTISPropertyIconImageURL;

CARBON_EXPORT const CFStringRef kTISCategoryKeyboardInputSource;
CARBON_EXPORT const CFStringRef kTISCategoryPaletteInputSource;
CARBON_EXPORT const CFStringRef kTISCategoryInkInputSource;

CARBON_EXPORT const CFStringRef kTISTypeKeyboardLayout;
CARBON_EXPORT const CFStringRef kTISTypeKeyboardInputMethodWithoutModes;
CARBON_EXPORT const CFStringRef kTISTypeKeyboardInputMethodModeEnabled;
CARBON_EXPORT const CFStringRef kTISTypeKeyboardInputMode;
CARBON_EXPORT const CFStringRef kTISTypeCharacterPalette;
CARBON_EXPORT const CFStringRef kTISTypeKeyboardViewer;
CARBON_EXPORT const CFStringRef kTISTypeInk;

CARBON_EXPORT const CFStringRef kTISNotifySelectedKeyboardInputSourceChanged;
CARBON_EXPORT const CFStringRef kTISNotifyEnabledKeyboardInputSourcesChanged;

CARBON_EXPORT CFTypeID TISInputSourceGetTypeID(void);
CARBON_EXPORT void *TISGetInputSourceProperty(TISInputSourceRef inputSource, CFStringRef propertyKey);
CARBON_EXPORT CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled);
CARBON_EXPORT TISInputSourceRef TISCopyCurrentKeyboardInputSource(void);
CARBON_EXPORT TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void);
CARBON_EXPORT TISInputSourceRef TISCopyCurrentASCIICapableKeyboardInputSource(void);
CARBON_EXPORT TISInputSourceRef TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void);
CARBON_EXPORT TISInputSourceRef TISCopyInputSourceForLanguage(CFStringRef language);
CARBON_EXPORT CFArrayRef TISCreateASCIICapableInputSourceList(void);
CARBON_EXPORT TISInputSourceRef TISCopyInputMethodKeyboardLayoutOverride(void);
CARBON_EXPORT OSStatus TISSetInputMethodKeyboardLayoutOverride(TISInputSourceRef keyboardLayout);
CARBON_EXPORT OSStatus TISSelectInputSource(TISInputSourceRef inputSource);
CARBON_EXPORT OSStatus TISDeselectInputSource(TISInputSourceRef inputSource);
CARBON_EXPORT OSStatus TISEnableInputSource(TISInputSourceRef inputSource);
CARBON_EXPORT OSStatus TISDisableInputSource(TISInputSourceRef inputSource);

// keyboard hardware
enum {
    kKeyboardJIS = 'JIS ',
    kKeyboardANSI = 'ANSI',
    kKeyboardISO = 'ISO ',
    kKeyboardUnknown = kUnknownType,
};
typedef UInt32 PhysicalKeyboardLayoutType;

CARBON_EXPORT UInt8 LMGetKbdType(void);
CARBON_EXPORT PhysicalKeyboardLayoutType KBGetLayoutType(SInt16 keyboardType);

// event modifiers
enum {
    cmdKey = 1 << 8,
    shiftKey = 1 << 9,
    alphaLock = 1 << 10,
    optionKey = 1 << 11,
    controlKey = 1 << 12,
    rightShiftKey = 1 << 13,
    rightOptionKey = 1 << 14,
    rightControlKey = 1 << 15,
    kEventKeyModifierNumLockMask = 1 << 16,
    kEventKeyModifierFnMask = 1 << 17,
};

// virtual key codes of the ANSI keyboard and the keys every layout shares
enum {
    kVK_ANSI_A = 0x00, kVK_ANSI_S = 0x01, kVK_ANSI_D = 0x02, kVK_ANSI_F = 0x03,
    kVK_ANSI_H = 0x04, kVK_ANSI_G = 0x05, kVK_ANSI_Z = 0x06, kVK_ANSI_X = 0x07,
    kVK_ANSI_C = 0x08, kVK_ANSI_V = 0x09, kVK_ANSI_B = 0x0B, kVK_ANSI_Q = 0x0C,
    kVK_ANSI_W = 0x0D, kVK_ANSI_E = 0x0E, kVK_ANSI_R = 0x0F, kVK_ANSI_Y = 0x10,
    kVK_ANSI_T = 0x11, kVK_ANSI_1 = 0x12, kVK_ANSI_2 = 0x13, kVK_ANSI_3 = 0x14,
    kVK_ANSI_4 = 0x15, kVK_ANSI_6 = 0x16, kVK_ANSI_5 = 0x17, kVK_ANSI_Equal = 0x18,
    kVK_ANSI_9 = 0x19, kVK_ANSI_7 = 0x1A, kVK_ANSI_Minus = 0x1B, kVK_ANSI_8 = 0x1C,
    kVK_ANSI_0 = 0x1D, kVK_ANSI_RightBracket = 0x1E, kVK_ANSI_O = 0x1F, kVK_ANSI_U = 0x20,
    kVK_ANSI_LeftBracket = 0x21, kVK_ANSI_I = 0x22, kVK_ANSI_P = 0x23, kVK_ANSI_L = 0x25,
    kVK_ANSI_J = 0x26, kVK_ANSI_Quote = 0x27, kVK_ANSI_K = 0x28, kVK_ANSI_Semicolon = 0x29,
    kVK_ANSI_Backslash = 0x2A, kVK_ANSI_Comma = 0x2B, kVK_ANSI_Slash = 0x2C, kVK_ANSI_N = 0x2D,
    kVK_ANSI_M = 0x2E, kVK_ANSI_Period = 0x2F, kVK_ANSI_Grave = 0x32,
    kVK_ANSI_KeypadDecimal = 0x41, kVK_ANSI_KeypadMultiply = 0x43, kVK_ANSI_KeypadPlus = 0x45,
    kVK_ANSI_KeypadClear = 0x47, kVK_ANSI_KeypadDivide = 0x4B, kVK_ANSI_KeypadEnter = 0x4C,
    kVK_ANSI_KeypadMinus = 0x4E, kVK_ANSI_KeypadEquals = 0x51, kVK_ANSI_Keypad0 = 0x52,
    kVK_ANSI_Keypad1 = 0x53, kVK_ANSI_Keypad2 = 0x54, kVK_ANSI_Keypad3 = 0x55,
    kVK_ANSI_Keypad4 = 0x56, kVK_ANSI_Keypad5 = 0x57, kVK_ANSI_Keypad6 = 0x58,
    kVK_ANSI_Keypad7 = 0x59, kVK_ANSI_Keypad8 = 0x5B, kVK_ANSI_Keypad9 = 0x5C,
};
enum {
    kVK_Return = 0x24, kVK_Tab = 0x30, kVK_Space = 0x31, kVK_Delete = 0x33, kVK_Escape = 0x35,
    kVK_RightCommand = 0x36, kVK_Command = 0x37, kVK_Shift = 0x38, kVK_CapsLock = 0x39,
    kVK_Option = 0x3A, kVK_Control = 0x3B, kVK_RightShift = 0x3C, kVK_RightOption = 0x3D,
    kVK_RightControl = 0x3E, kVK_Function = 0x3F, kVK_F17 = 0x40, kVK_VolumeUp = 0x48,
    kVK_VolumeDown = 0x49, kVK_Mute = 0x4A, kVK_F18 = 0x4F, kVK_F19 = 0x50, kVK_F20 = 0x5A,
    kVK_F5 = 0x60, kVK_F6 = 0x61, kVK_F7 = 0x62, kVK_F3 = 0x63, kVK_F8 = 0x64, kVK_F9 = 0x65,
    kVK_F11 = 0x67, kVK_F13 = 0x69, kVK_F16 = 0x6A, kVK_F14 = 0x6B, kVK_F10 = 0x6D,
    kVK_ContextualMenu = 0x6E, kVK_F12 = 0x6F, kVK_F15 = 0x71, kVK_Help = 0x72, kVK_Home = 0x73,
    kVK_PageUp = 0x74, kVK_ForwardDelete = 0x75, kVK_F4 = 0x76, kVK_End = 0x77, kVK_F2 = 0x78,
    kVK_PageDown = 0x79, kVK_F1 = 0x7A, kVK_LeftArrow = 0x7B, kVK_RightArrow = 0x7C,
    kVK_DownArrow = 0x7D, kVK_UpArrow = 0x7E,
};
enum {
    kVK_ISO_Section = 0x0A,
    kVK_JIS_Yen = 0x5D, kVK_JIS_Underscore = 0x5E, kVK_JIS_KeypadComma = 0x5F,
    kVK_JIS_Eisu = 0x66, kVK_JIS_Kana = 0x68,
};

CARBON_EXPORT UInt32 GetCurrentEventKeyModifiers(void);
CARBON_EXPORT UInt32 GetCurrentKeyModifiers(void);

CARBON_EXPORT OSStatus EnableSecureEventInput(void);
CARBON_EXPORT OSStatus DisableSecureEventInput(void);
CARBON_EXPORT Boolean IsSecureEventInputEnabled(void);

// text services manager documents
typedef struct OpaqueTSMDocumentID *TSMDocumentID;
CARBON_EXPORT TSMDocumentID TSMGetActiveDocument(void);
CARBON_EXPORT OSStatus TSMSetDocumentProperty(TSMDocumentID docID, OSType propertyTag, UInt32 propertySize, void *propertyData);
CARBON_EXPORT OSStatus TSMGetDocumentProperty(TSMDocumentID docID, OSType propertyTag, UInt32 bufferSize, UInt32 *actualSize, void *propertyBuffer);
CARBON_EXPORT OSStatus TSMRemoveDocumentProperty(TSMDocumentID docID, OSType propertyTag);

enum {
    tsmDocPropertyNotFoundErr = -12039,
};


// the Carbon Event Manager, as far as application handlers and hot keys go
typedef struct OpaqueEventRef *EventRef;
typedef struct OpaqueEventTargetRef *EventTargetRef;
typedef struct OpaqueEventHandlerRef *EventHandlerRef;
typedef struct OpaqueEventHandlerCallRef *EventHandlerCallRef;
typedef struct OpaqueEventHotKeyRef *EventHotKeyRef;
typedef OSStatus (*EventHandlerProcPtr)(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
typedef EventHandlerProcPtr EventHandlerUPP;
typedef double EventTime;
typedef OSType EventParamName;
typedef OSType EventParamType;

typedef struct EventTypeSpec {
    OSType eventClass;
    UInt32 eventKind;
} EventTypeSpec;

typedef struct EventHotKeyID {
    OSType signature;
    UInt32 id;
} EventHotKeyID;

enum {
    kEventClassKeyboard = 'keyb',
    kEventHotKeyPressed = 5,
    kEventHotKeyReleased = 6,
};
enum {
    kEventParamDirectObject = '----',
    typeEventHotKeyID = 'hkid',
};
enum {
    kEventHotKeyNoOptions = 0,
    kEventHotKeyExclusive = 1 << 0,
};
enum {
    eventNotHandledErr = -9874,
    eventParameterNotFoundErr = -9870,
    eventHotKeyExistsErr = -9878,
    eventHotKeyInvalidErr = -9879,
};

CARBON_EXPORT EventTargetRef GetApplicationEventTarget(void);
CARBON_EXPORT OSStatus InstallEventHandler(EventTargetRef inTarget, EventHandlerUPP inHandler, ItemCount inNumTypes,
                                           const EventTypeSpec *inList, void *inUserData, EventHandlerRef *outRef);
CARBON_EXPORT OSStatus RemoveEventHandler(EventHandlerRef inHandlerRef);
CARBON_EXPORT OSStatus RegisterEventHotKey(UInt32 inHotKeyCode, UInt32 inHotKeyModifiers, EventHotKeyID inHotKeyID,
                                           EventTargetRef inTarget, OptionBits inOptions, EventHotKeyRef *outRef);
CARBON_EXPORT OSStatus UnregisterEventHotKey(EventHotKeyRef inHotKey);
CARBON_EXPORT OSStatus GetEventParameter(EventRef inEvent, EventParamName inName, EventParamType inDesiredType,
                                         EventParamType *outActualType, ByteCount inBufferSize, ByteCount *outActualSize,
                                         void *outData);
CARBON_EXPORT OSType GetEventClass(EventRef inEvent);
CARBON_EXPORT UInt32 GetEventKind(EventRef inEvent);
CARBON_EXPORT EventTime GetEventTime(EventRef inEvent);
CARBON_EXPORT EventHandlerUPP NewEventHandlerUPP(EventHandlerProcPtr userRoutine);
CARBON_EXPORT void DisposeEventHandlerUPP(EventHandlerUPP userUPP);
#define InstallApplicationEventHandler(handler, numTypes, list, userData, outHandlerRef) \
    InstallEventHandler(GetApplicationEventTarget(), (handler), (numTypes), (list), (userData), (outHandlerRef))

// appearance manager and HITheme
typedef UInt32 ThemeMetric;
typedef UInt16 ThemeButtonKind;
typedef UInt16 ThemeButtonValue;
typedef UInt16 ThemeButtonAdornment;
typedef UInt32 ThemeDrawState;
typedef UInt32 HIThemeOrientation;
typedef CGRect HIRect;
typedef CGPoint HIPoint;
typedef CGSize HISize;

enum {
    kThemeMetricScrollBarWidth = 0,
    kThemeMetricSmallScrollBarWidth = 1,
    kThemeMetricCheckBoxHeight = 2,
    kThemeMetricRadioButtonHeight = 3,
    kThemeMetricEditTextWhitespace = 4,
    kThemeMetricEditTextFrameOutset = 5,
    kThemeMetricListBoxFrameOutset = 6,
    kThemeMetricFocusRectOutset = 7,
    kThemeMetricImageWellThickness = 8,
    kThemeMetricScrollBarOverlap = 9,
    kThemeMetricLargeTabHeight = 10,
    kThemeMetricLargeTabCapsWidth = 11,
    kThemeMetricTabFrameOverlap = 12,
    kThemeMetricTabIndentOrStyle = 13,
    kThemeMetricTabOverlap = 14,
    kThemeMetricSmallTabHeight = 15,
    kThemeMetricSmallTabCapsWidth = 16,
    kThemeMetricPushButtonHeight = 19,
    kThemeMetricListHeaderHeight = 20,
    kThemeMetricDisclosureTriangleHeight = 25,
    kThemeMetricDisclosureTriangleWidth = 26,
    kThemeMetricLittleArrowsHeight = 27,
    kThemeMetricLittleArrowsWidth = 28,
    kThemeMetricPopupButtonHeight = 30,
    kThemeMetricSmallPopupButtonHeight = 31,
    kThemeMetricLargeProgressBarThickness = 32,
    kThemeMetricPullDownHeight = 33,
    kThemeMetricSmallPullDownHeight = 34,
    kThemeMetricSmallPushButtonHeight = 35,
    kThemeMetricSmallRadioButtonHeight = 36,
    kThemeMetricRelevanceIndicatorHeight = 37,
    kThemeMetricResizeControlHeight = 38,
    kThemeMetricSmallResizeControlHeight = 39,
    kThemeMetricLargeRoundButtonSize = 40,
    kThemeMetricHSliderHeight = 41,
    kThemeMetricHSliderTickHeight = 42,
    kThemeMetricVSliderWidth = 45,
    kThemeMetricVSliderTickWidth = 46,
    kThemeMetricTitleBarControlsHeight = 49,
    kThemeMetricCheckBoxWidth = 50,
    kThemeMetricSmallCheckBoxWidth = 51,
    kThemeMetricRadioButtonWidth = 52,
    kThemeMetricSmallRadioButtonWidth = 53,
    kThemeMetricSmallCheckBoxHeight = 54,
    kThemeMetricNormalProgressBarThickness = 58,
    kThemeMetricProgressBarShadowOutset = 59,
    kThemeMetricSmallProgressBarShadowOutset = 60,
    kThemeMetricPrimaryGroupBoxContentInset = 61,
    kThemeMetricSecondaryGroupBoxContentInset = 62,
    kThemeMetricMenuMarkColumnWidth = 63,
    kThemeMetricMenuExcludedMarkColumnWidth = 64,
    kThemeMetricMenuMarkIndent = 65,
    kThemeMetricMenuTextLeadingEdgeMargin = 66,
    kThemeMetricMenuTextTrailingEdgeMargin = 67,
    kThemeMetricMenuIndentWidth = 68,
    kThemeMetricMenuIconTrailingEdgeMargin = 69,
};

enum {
    kThemePushButton = 0,
    kThemeCheckBox = 1,
    kThemeRadioButton = 2,
    kThemeBevelButton = 3,
    kThemeArrowButton = 4,
    kThemePopupButton = 5,
    kThemeDisclosureButton = 6,
    kThemeIncDecButton = 7,
    kThemeSmallBevelButton = 8,
    kThemeMediumBevelButton = 3,
    kThemeLargeBevelButton = 9,
    kThemeListHeaderButton = 10,
    kThemeRoundButton = 11,
    kThemeLargeRoundButton = 12,
    kThemeSmallCheckBox = 13,
    kThemeSmallRadioButton = 14,
    kThemeRoundedBevelButton = 15,
    kThemeComboBox = 16,
    kThemeComboBoxSmall = 17,
    kThemeComboBoxMini = 18,
    kThemeMiniCheckBox = 19,
    kThemeMiniRadioButton = 20,
    kThemePushButtonNormal = 21,
    kThemePushButtonSmall = 22,
    kThemePushButtonMini = 23,
};

enum {
    kThemeButtonOff = 0,
    kThemeButtonOn = 1,
    kThemeButtonMixed = 2,
};

enum {
    kThemeStateInactive = 0,
    kThemeStateActive = 1,
    kThemeStatePressed = 2,
    kThemeStateUnavailable = 7,
    kThemeStateUnavailableInactive = 8,
};

enum {
    kThemeAdornmentNone = 0,
    kThemeAdornmentDefault = 1 << 0,
    kThemeAdornmentFocus = 1 << 2,
};

enum {
    kHIThemeOrientationNormal = 0,
    kHIThemeOrientationInverted = 1,
};

typedef struct HIThemeAnimationTimeInfo {
    CFAbsoluteTime start;
    CFAbsoluteTime current;
} HIThemeAnimationTimeInfo;

typedef struct HIThemeAnimationFrameInfo {
    UInt32 index;
} HIThemeAnimationFrameInfo;

typedef union HIThemeAnimationInfo {
    HIThemeAnimationTimeInfo time;
    HIThemeAnimationFrameInfo frame;
} HIThemeAnimationInfo;

typedef struct HIThemeButtonDrawInfo {
    UInt32 version;
    ThemeDrawState state;
    ThemeButtonKind kind;
    ThemeButtonValue value;
    ThemeButtonAdornment adornment;
    HIThemeAnimationInfo animation;
} HIThemeButtonDrawInfo;

CARBON_EXPORT OSStatus GetThemeMetric(ThemeMetric metric, SInt32 *outMetric);

// the resize grow box in a window's corner
typedef UInt16 ThemeGrowDirection;
enum {
    kThemeGrowLeft = 1 << 0,
    kThemeGrowRight = 1 << 1,
    kThemeGrowUp = 1 << 2,
    kThemeGrowDown = 1 << 3,
};
typedef UInt32 HIThemeGrowBoxKind;
enum {
    kHIThemeGrowBoxKindNormal = 0,
    kHIThemeGrowBoxKindNone = 1,
};
typedef UInt32 HIThemeGrowBoxSize;
enum {
    kHIThemeGrowBoxSizeNormal = 0,
    kHIThemeGrowBoxSizeSmall = 1,
};
typedef struct HIThemeGrowBoxDrawInfo {
    UInt32 version;
    ThemeDrawState state;
    HIThemeGrowBoxKind kind;
    ThemeGrowDirection direction;
    HIThemeGrowBoxSize size;
} HIThemeGrowBoxDrawInfo;
CARBON_EXPORT OSStatus HIThemeGetGrowBoxBounds(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo,
                                               HIRect *outBounds);
CARBON_EXPORT OSStatus HIThemeDrawButton(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo, CGContextRef inContext,
                                         HIThemeOrientation inOrientation, HIRect *outLabelRect);

#ifdef __cplusplus
}
#endif

#endif
