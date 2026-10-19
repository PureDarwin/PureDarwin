// Carbon.c - text input sources, keyboard state, TSM documents and theme drawing
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "Carbon.h"
#include "KeyLayouts.h"
#include <stdint.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const CFStringRef kTISPropertyInputSourceCategory = CFSTR("TISPropertyInputSourceCategory");
const CFStringRef kTISPropertyInputSourceType = CFSTR("TISPropertyInputSourceType");
const CFStringRef kTISPropertyInputSourceIsASCIICapable = CFSTR("TISPropertyInputSourceIsASCIICapable");
const CFStringRef kTISPropertyInputSourceIsEnableCapable = CFSTR("TISPropertyInputSourceIsEnableCapable");
const CFStringRef kTISPropertyInputSourceIsSelectCapable = CFSTR("TISPropertyInputSourceIsSelectCapable");
const CFStringRef kTISPropertyInputSourceIsEnabled = CFSTR("TISPropertyInputSourceIsEnabled");
const CFStringRef kTISPropertyInputSourceIsSelected = CFSTR("TISPropertyInputSourceIsSelected");
const CFStringRef kTISPropertyInputSourceID = CFSTR("TISPropertyInputSourceID");
const CFStringRef kTISPropertyBundleID = CFSTR("TISPropertyBundleID");
const CFStringRef kTISPropertyInputModeID = CFSTR("TISPropertyInputModeID");
const CFStringRef kTISPropertyLocalizedName = CFSTR("TISPropertyLocalizedName");
const CFStringRef kTISPropertyInputSourceLanguages = CFSTR("TISPropertyInputSourceLanguages");
const CFStringRef kTISPropertyUnicodeKeyLayoutData = CFSTR("TISPropertyUnicodeKeyLayoutData");
const CFStringRef kTISPropertyIconRef = CFSTR("TISPropertyIconRef");
const CFStringRef kTISPropertyIconImageURL = CFSTR("TISPropertyIconImageURL");

const CFStringRef kTISCategoryKeyboardInputSource = CFSTR("TISCategoryKeyboardInputSource");
const CFStringRef kTISCategoryPaletteInputSource = CFSTR("TISCategoryPaletteInputSource");
const CFStringRef kTISCategoryInkInputSource = CFSTR("TISCategoryInkInputSource");

const CFStringRef kTISTypeKeyboardLayout = CFSTR("TISTypeKeyboardLayout");
const CFStringRef kTISTypeKeyboardInputMethodWithoutModes = CFSTR("TISTypeKeyboardInputMethodWithoutModes");
const CFStringRef kTISTypeKeyboardInputMethodModeEnabled = CFSTR("TISTypeKeyboardInputMethodModeEnabled");
const CFStringRef kTISTypeKeyboardInputMode = CFSTR("TISTypeKeyboardInputMode");
const CFStringRef kTISTypeCharacterPalette = CFSTR("TISTypeCharacterPalette");
const CFStringRef kTISTypeKeyboardViewer = CFSTR("TISTypeKeyboardViewer");
const CFStringRef kTISTypeInk = CFSTR("TISTypeInk");

const CFStringRef kTISNotifySelectedKeyboardInputSourceChanged = CFSTR("AppleSelectedInputSourcesChangedNotification");
const CFStringRef kTISNotifyEnabledKeyboardInputSourcesChanged = CFSTR("AppleEnabledInputSourcesChangedNotification");

// an input source is its property dictionary, one per keyboard layout in KeyLayouts.c
static CFMutableDictionaryRef sources[kPDKeyLayoutCount];
static enum PDKeyLayout selected = kPDKeyLayoutUS;
static pthread_once_t sourcesOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t sourcesLock = PTHREAD_MUTEX_INITIALIZER;

// the selection is shared through com.apple.HIToolbox, under the keys macOS uses
#define kHIToolboxDomain CFSTR("com.apple.HIToolbox")
#define kCurrentLayoutKey CFSTR("AppleCurrentKeyboardLayoutInputSourceID")
#define kSelectedSourcesKey CFSTR("AppleSelectedInputSources")
// notify(3) name for processes without CoreFoundation, such as the compositor
#define kSelectedSourceNotifyName "com.apple.Carbon.TISNotifySelectedKeyboardInputSourceChanged"

// libnotify ships in libSystem without its header in this SDK
extern uint32_t notify_post(const char *name);
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_set_state(int token, uint64_t state);

static const struct {
    CFStringRef name;
    CFStringRef language;
} sourceInfo[kPDKeyLayoutCount] = {
    [kPDKeyLayoutUS] = { CFSTR("U.S."), CFSTR("en") },
    [kPDKeyLayoutGerman] = { CFSTR("German"), CFSTR("de") },
    [kPDKeyLayoutFrench] = { CFSTR("French"), CFSTR("fr") },
};

static CFStringRef CopySourceID(enum PDKeyLayout layout)
{
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("com.apple.keylayout.%s"), PDKeyLayoutName(layout));
}

static enum PDKeyLayout LayoutForSourceID(CFStringRef sourceID, Boolean *found)
{
    for (int i = 0; i < kPDKeyLayoutCount; i++) {
        CFStringRef candidate = CopySourceID((enum PDKeyLayout)i);
        Boolean same = sourceID != NULL && CFEqual(candidate, sourceID);
        CFRelease(candidate);
        if (same) {
            *found = true;
            return (enum PDKeyLayout)i;
        }
    }
    *found = false;
    return kPDKeyLayoutUS;
}

// reads the selected layout the last TISSelectInputSource, in this or another process, saved
static void LoadSelection(void)
{
    CFPropertyListRef value;
    Boolean found = false;
    enum PDKeyLayout layout;

    CFPreferencesAppSynchronize(kHIToolboxDomain);
    value = CFPreferencesCopyAppValue(kCurrentLayoutKey, kHIToolboxDomain);
    if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID()) {
        layout = LayoutForSourceID(value, &found);
        if (found)
            selected = layout;
    }
    if (value != NULL)
        CFRelease(value);
    for (int i = 0; i < kPDKeyLayoutCount; i++)
        CFDictionarySetValue(sources[i], kTISPropertyInputSourceIsSelected, i == (int)selected ? kCFBooleanTrue : kCFBooleanFalse);
}

// CFNotificationCenter is looked up when used, older CoreFoundation builds do not have it
static void (*addObserver)(CFNotificationCenterRef, const void *, CFNotificationCallback, CFStringRef, const void *,
                           CFNotificationSuspensionBehavior);
static void (*postNotification)(CFNotificationCenterRef, CFNotificationName, const void *, CFDictionaryRef, Boolean);

static CFNotificationCenterRef NotificationCenter(void)
{
    static CFNotificationCenterRef (*distributedCenter)(void);
    static Boolean looked;

    if (!looked) {
        distributedCenter = dlsym(RTLD_DEFAULT, "CFNotificationCenterGetDistributedCenter");
        addObserver = dlsym(RTLD_DEFAULT, "CFNotificationCenterAddObserver");
        postNotification = dlsym(RTLD_DEFAULT, "CFNotificationCenterPostNotification");
        looked = true;
    }
    return distributedCenter ? distributedCenter() : NULL;
}

static void SelectionChanged(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    pthread_mutex_lock(&sourcesLock);
    LoadSelection();
    pthread_mutex_unlock(&sourcesLock);
}

static void CreateSources(void)
{
    for (int i = 0; i < kPDKeyLayoutCount; i++) {
        CFMutableDictionaryRef source = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
        CFStringRef sourceID = CopySourceID((enum PDKeyLayout)i);
        CFArrayRef languages = CFArrayCreate(kCFAllocatorDefault, (const void **)&sourceInfo[i].language, 1, &kCFTypeArrayCallBacks);
        CFDataRef layout = PDCreateKeyLayoutData((enum PDKeyLayout)i);

        CFDictionarySetValue(source, kTISPropertyInputSourceCategory, kTISCategoryKeyboardInputSource);
        CFDictionarySetValue(source, kTISPropertyInputSourceType, kTISTypeKeyboardLayout);
        CFDictionarySetValue(source, kTISPropertyInputSourceID, sourceID);
        CFDictionarySetValue(source, kTISPropertyLocalizedName, sourceInfo[i].name);
        CFDictionarySetValue(source, kTISPropertyInputSourceLanguages, languages);
        CFDictionarySetValue(source, kTISPropertyInputSourceIsASCIICapable, kCFBooleanTrue);
        CFDictionarySetValue(source, kTISPropertyInputSourceIsEnableCapable, kCFBooleanTrue);
        CFDictionarySetValue(source, kTISPropertyInputSourceIsSelectCapable, kCFBooleanTrue);
        CFDictionarySetValue(source, kTISPropertyInputSourceIsEnabled, kCFBooleanTrue);
        if (layout != NULL)
            CFDictionarySetValue(source, kTISPropertyUnicodeKeyLayoutData, layout);
        sources[i] = source;
        CFRelease(sourceID);
        CFRelease(languages);
        if (layout != NULL)
            CFRelease(layout);
    }
    LoadSelection();
    if (NotificationCenter() != NULL && addObserver != NULL)
        addObserver(NotificationCenter(), NULL, SelectionChanged, kTISNotifySelectedKeyboardInputSourceChanged, NULL,
                    CFNotificationSuspensionBehaviorDeliverImmediately);
}

static TISInputSourceRef SourceAt(int index)
{
    pthread_once(&sourcesOnce, CreateSources);
    return (TISInputSourceRef)sources[index];
}

static TISInputSourceRef CopySelectedSource(void)
{
    TISInputSourceRef source;

    pthread_once(&sourcesOnce, CreateSources);
    pthread_mutex_lock(&sourcesLock);
    source = (TISInputSourceRef)CFRetain(sources[selected]);
    pthread_mutex_unlock(&sourcesLock);
    return source;
}

static int IndexOfSource(TISInputSourceRef source)
{
    for (int i = 0; i < kPDKeyLayoutCount; i++) {
        if (SourceAt(i) == source)
            return i;
    }
    return -1;
}

CFTypeID TISInputSourceGetTypeID(void)
{
    return CFDictionaryGetTypeID();
}

void *TISGetInputSourceProperty(TISInputSourceRef inputSource, CFStringRef propertyKey)
{
    if (inputSource == NULL || propertyKey == NULL)
        return NULL;
    return (void *)CFDictionaryGetValue((CFDictionaryRef)inputSource, propertyKey);
}

// a source is listed when every property asked for has the value given
static Boolean SourceMatches(TISInputSourceRef source, CFDictionaryRef properties)
{
    CFIndex count;
    const void **keys, **values;
    Boolean matches = true;

    if (properties == NULL || (count = CFDictionaryGetCount(properties)) == 0)
        return true;
    keys = malloc(count * sizeof(void *));
    values = malloc(count * sizeof(void *));
    CFDictionaryGetKeysAndValues(properties, keys, values);
    for (CFIndex i = 0; i < count && matches; i++) {
        const void *have = CFDictionaryGetValue((CFDictionaryRef)source, keys[i]);
        matches = have != NULL && CFEqual(have, values[i]);
    }
    free(keys);
    free(values);
    return matches;
}

CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled)
{
    CFMutableArrayRef list = CFArrayCreateMutable(kCFAllocatorDefault, kPDKeyLayoutCount, &kCFTypeArrayCallBacks);

    pthread_once(&sourcesOnce, CreateSources);
    pthread_mutex_lock(&sourcesLock);
    for (int i = 0; i < kPDKeyLayoutCount; i++) {
        if (SourceMatches(SourceAt(i), properties))
            CFArrayAppendValue(list, SourceAt(i));
    }
    pthread_mutex_unlock(&sourcesLock);
    return list;
}

CFArrayRef TISCreateASCIICapableInputSourceList(void)
{
    return TISCreateInputSourceList(NULL, false);
}

// every source is a keyboard layout and ASCII capable, so all four ask for the selected one
TISInputSourceRef TISCopyCurrentKeyboardInputSource(void)
{
    return CopySelectedSource();
}

TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void)
{
    return CopySelectedSource();
}

TISInputSourceRef TISCopyCurrentASCIICapableKeyboardInputSource(void)
{
    return CopySelectedSource();
}

TISInputSourceRef TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void)
{
    return CopySelectedSource();
}

// the layout for a language, nothing for languages without one, as on macOS
TISInputSourceRef TISCopyInputSourceForLanguage(CFStringRef language)
{
    if (language == NULL)
        return NULL;
    for (int i = 0; i < kPDKeyLayoutCount; i++) {
        if (CFStringHasPrefix(language, sourceInfo[i].language))
            return (TISInputSourceRef)CFRetain(SourceAt(i));
    }
    return NULL;
}

// no input method is active, so no layout override is in place
TISInputSourceRef TISCopyInputMethodKeyboardLayoutOverride(void)
{
    return NULL;
}

OSStatus TISSetInputMethodKeyboardLayoutOverride(TISInputSourceRef keyboardLayout)
{
    return IndexOfSource(keyboardLayout) >= 0 ? noErr : paramErr;
}

// saves the choice for every process and tells them, the compositor through notify(3)
OSStatus TISSelectInputSource(TISInputSourceRef inputSource)
{
    int index = IndexOfSource(inputSource);
    CFStringRef sourceID;
    CFMutableDictionaryRef entry;
    CFArrayRef selectedList;

    if (index < 0)
        return paramErr;
    sourceID = CopySourceID((enum PDKeyLayout)index);
    entry = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(entry, CFSTR("InputSourceKind"), CFSTR("Keyboard Layout"));
    CFDictionarySetValue(entry, CFSTR("KeyboardLayout Name"), sourceInfo[index].name);
    selectedList = CFArrayCreate(kCFAllocatorDefault, (const void **)&entry, 1, &kCFTypeArrayCallBacks);
    CFPreferencesSetAppValue(kCurrentLayoutKey, sourceID, kHIToolboxDomain);
    CFPreferencesSetAppValue(kSelectedSourcesKey, selectedList, kHIToolboxDomain);
    CFPreferencesAppSynchronize(kHIToolboxDomain);
    CFRelease(selectedList);
    CFRelease(entry);
    CFRelease(sourceID);

    pthread_mutex_lock(&sourcesLock);
    selected = (enum PDKeyLayout)index;
    for (int i = 0; i < kPDKeyLayoutCount; i++)
        CFDictionarySetValue(sources[i], kTISPropertyInputSourceIsSelected, i == index ? kCFBooleanTrue : kCFBooleanFalse);
    pthread_mutex_unlock(&sourcesLock);

    if (NotificationCenter() != NULL && postNotification != NULL)
        postNotification(NotificationCenter(), kTISNotifySelectedKeyboardInputSourceChanged, NULL, NULL, true);
    // the state carries the layout as one plus its index, the token stays open so notifyd keeps it
    static int token = -1;
    if (token >= 0 || notify_register_check(kSelectedSourceNotifyName, &token) == 0)
        notify_set_state(token, (uint64_t)index + 1);
    notify_post(kSelectedSourceNotifyName);
    return noErr;
}

// a keyboard layout stays selected until another is chosen
OSStatus TISDeselectInputSource(TISInputSourceRef inputSource)
{
    return paramErr;
}

OSStatus TISEnableInputSource(TISInputSourceRef inputSource)
{
    return IndexOfSource(inputSource) >= 0 ? noErr : paramErr;
}

OSStatus TISDisableInputSource(TISInputSourceRef inputSource)
{
    return paramErr;
}

// gestalt keyboard type 40 is the generic ANSI USB keyboard
UInt8 LMGetKbdType(void)
{
    return 40;
}

PhysicalKeyboardLayoutType KBGetLayoutType(SInt16 keyboardType)
{
    switch (keyboardType) {
    case 41:
    case 43:
        return kKeyboardISO;
    case 42:
        return kKeyboardJIS;
    }
    return kKeyboardANSI;
}

// the event system's modifier state is AppKit's, so read it there when AppKit is loaded
UInt32 GetCurrentEventKeyModifiers(void)
{
    Class event = objc_getClass("NSEvent");
    unsigned long flags;
    UInt32 modifiers = 0;

    if (event == Nil)
        return 0;
    flags = ((unsigned long (*)(id, SEL))objc_msgSend)((id)event, sel_registerName("modifierFlags"));
    if (flags & (1UL << 16))
        modifiers |= alphaLock;
    if (flags & (1UL << 17))
        modifiers |= shiftKey;
    if (flags & (1UL << 18))
        modifiers |= controlKey;
    if (flags & (1UL << 19))
        modifiers |= optionKey;
    if (flags & (1UL << 20))
        modifiers |= cmdKey;
    if (flags & (1UL << 21))
        modifiers |= kEventKeyModifierNumLockMask;
    if (flags & (1UL << 23))
        modifiers |= kEventKeyModifierFnMask;
    return modifiers;
}

UInt32 GetCurrentKeyModifiers(void)
{
    return GetCurrentEventKeyModifiers();
}

static pthread_mutex_t secureInputLock = PTHREAD_MUTEX_INITIALIZER;
static int secureInputCount;

// a count, as on macOS, every enable needs a matching disable
OSStatus EnableSecureEventInput(void)
{
    pthread_mutex_lock(&secureInputLock);
    secureInputCount++;
    pthread_mutex_unlock(&secureInputLock);
    return noErr;
}

OSStatus DisableSecureEventInput(void)
{
    pthread_mutex_lock(&secureInputLock);
    if (secureInputCount > 0)
        secureInputCount--;
    pthread_mutex_unlock(&secureInputLock);
    return noErr;
}

Boolean IsSecureEventInputEnabled(void)
{
    return secureInputCount > 0;
}

// one document stands for the focused text input, its properties are byte copies keyed by tag
struct OpaqueTSMDocumentID {
    CFMutableDictionaryRef properties;
};

static struct OpaqueTSMDocumentID activeDocument;
static pthread_mutex_t documentLock = PTHREAD_MUTEX_INITIALIZER;

TSMDocumentID TSMGetActiveDocument(void)
{
    return &activeDocument;
}

OSStatus TSMSetDocumentProperty(TSMDocumentID docID, OSType propertyTag, UInt32 propertySize, void *propertyData)
{
    CFNumberRef key;
    CFDataRef value;

    if (docID == NULL || (propertySize != 0 && propertyData == NULL))
        return paramErr;
    key = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &propertyTag);
    value = CFDataCreate(kCFAllocatorDefault, propertyData, propertySize);
    pthread_mutex_lock(&documentLock);
    if (docID->properties == NULL)
        docID->properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(docID->properties, key, value);
    pthread_mutex_unlock(&documentLock);
    CFRelease(key);
    CFRelease(value);
    return noErr;
}

OSStatus TSMGetDocumentProperty(TSMDocumentID docID, OSType propertyTag, UInt32 bufferSize, UInt32 *actualSize, void *propertyBuffer)
{
    CFNumberRef key;
    CFDataRef value = NULL;
    OSStatus status = tsmDocPropertyNotFoundErr;

    if (docID == NULL)
        return paramErr;
    key = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &propertyTag);
    pthread_mutex_lock(&documentLock);
    if (docID->properties != NULL)
        value = CFDictionaryGetValue(docID->properties, key);
    if (value != NULL) {
        UInt32 length = (UInt32)CFDataGetLength(value);
        if (actualSize != NULL)
            *actualSize = length;
        if (propertyBuffer != NULL)
            memcpy(propertyBuffer, CFDataGetBytePtr(value), length < bufferSize ? length : bufferSize);
        status = noErr;
    }
    pthread_mutex_unlock(&documentLock);
    CFRelease(key);
    return status;
}

OSStatus TSMRemoveDocumentProperty(TSMDocumentID docID, OSType propertyTag)
{
    CFNumberRef key;
    OSStatus status = tsmDocPropertyNotFoundErr;

    if (docID == NULL)
        return paramErr;
    key = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &propertyTag);
    pthread_mutex_lock(&documentLock);
    if (docID->properties != NULL && CFDictionaryContainsKey(docID->properties, key)) {
        CFDictionaryRemoveValue(docID->properties, key);
        status = noErr;
    }
    pthread_mutex_unlock(&documentLock);
    CFRelease(key);
    return status;
}

// the Aqua metrics macOS reports for regular-size controls
OSStatus GetThemeMetric(ThemeMetric metric, SInt32 *outMetric)
{
    SInt32 value;

    if (outMetric == NULL)
        return paramErr;
    switch (metric) {
    case kThemeMetricScrollBarWidth: value = 15; break;
    case kThemeMetricSmallScrollBarWidth: value = 11; break;
    case kThemeMetricCheckBoxHeight: value = 14; break;
    case kThemeMetricCheckBoxWidth: value = 14; break;
    case kThemeMetricSmallCheckBoxHeight: value = 12; break;
    case kThemeMetricSmallCheckBoxWidth: value = 12; break;
    case kThemeMetricRadioButtonHeight: value = 16; break;
    case kThemeMetricRadioButtonWidth: value = 16; break;
    case kThemeMetricSmallRadioButtonHeight: value = 13; break;
    case kThemeMetricSmallRadioButtonWidth: value = 13; break;
    case kThemeMetricEditTextWhitespace: value = 2; break;
    case kThemeMetricEditTextFrameOutset: value = 1; break;
    case kThemeMetricListBoxFrameOutset: value = 1; break;
    case kThemeMetricFocusRectOutset: value = 4; break;
    case kThemeMetricImageWellThickness: value = 3; break;
    case kThemeMetricScrollBarOverlap: value = 1; break;
    case kThemeMetricLargeTabHeight: value = 23; break;
    case kThemeMetricLargeTabCapsWidth: value = 8; break;
    case kThemeMetricSmallTabHeight: value = 19; break;
    case kThemeMetricSmallTabCapsWidth: value = 5; break;
    case kThemeMetricPushButtonHeight: value = 20; break;
    case kThemeMetricSmallPushButtonHeight: value = 17; break;
    case kThemeMetricListHeaderHeight: value = 17; break;
    case kThemeMetricDisclosureTriangleHeight: value = 13; break;
    case kThemeMetricDisclosureTriangleWidth: value = 13; break;
    case kThemeMetricLittleArrowsHeight: value = 22; break;
    case kThemeMetricLittleArrowsWidth: value = 13; break;
    case kThemeMetricPopupButtonHeight: value = 20; break;
    case kThemeMetricSmallPopupButtonHeight: value = 17; break;
    case kThemeMetricPullDownHeight: value = 20; break;
    case kThemeMetricSmallPullDownHeight: value = 17; break;
    case kThemeMetricLargeProgressBarThickness: value = 16; break;
    case kThemeMetricNormalProgressBarThickness: value = 14; break;
    case kThemeMetricProgressBarShadowOutset: value = 1; break;
    case kThemeMetricSmallProgressBarShadowOutset: value = 1; break;
    case kThemeMetricResizeControlHeight: value = 15; break;
    case kThemeMetricSmallResizeControlHeight: value = 11; break;
    case kThemeMetricLargeRoundButtonSize: value = 25; break;
    case kThemeMetricHSliderHeight: value = 15; break;
    case kThemeMetricHSliderTickHeight: value = 4; break;
    case kThemeMetricVSliderWidth: value = 15; break;
    case kThemeMetricVSliderTickWidth: value = 4; break;
    case kThemeMetricTitleBarControlsHeight: value = 14; break;
    case kThemeMetricPrimaryGroupBoxContentInset: value = 16; break;
    case kThemeMetricSecondaryGroupBoxContentInset: value = 12; break;
    case kThemeMetricMenuMarkColumnWidth: value = 16; break;
    case kThemeMetricMenuExcludedMarkColumnWidth: value = 16; break;
    case kThemeMetricMenuMarkIndent: value = 5; break;
    case kThemeMetricMenuTextLeadingEdgeMargin: value = 5; break;
    case kThemeMetricMenuTextTrailingEdgeMargin: value = 15; break;
    case kThemeMetricMenuIndentWidth: value = 12; break;
    case kThemeMetricMenuIconTrailingEdgeMargin: value = 5; break;
    default:
        *outMetric = 0;
        return paramErr;
    }
    *outMetric = value;
    return noErr;
}

static void AddRoundedRect(CGContextRef context, CGRect rect, CGFloat radius)
{
    CGFloat minX = CGRectGetMinX(rect), maxX = CGRectGetMaxX(rect);
    CGFloat minY = CGRectGetMinY(rect), maxY = CGRectGetMaxY(rect);

    if (radius > rect.size.width / 2)
        radius = rect.size.width / 2;
    if (radius > rect.size.height / 2)
        radius = rect.size.height / 2;
    CGContextMoveToPoint(context, minX + radius, minY);
    CGContextAddArcToPoint(context, maxX, minY, maxX, maxY, radius);
    CGContextAddArcToPoint(context, maxX, maxY, minX, maxY, radius);
    CGContextAddArcToPoint(context, minX, maxY, minX, minY, radius);
    CGContextAddArcToPoint(context, minX, minY, maxX, minY, radius);
    CGContextClosePath(context);
}

// a flat Aqua-style bezel: white face, grey border, accent fill when on or default, darker when pressed
OSStatus HIThemeDrawButton(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo, CGContextRef inContext,
                           HIThemeOrientation inOrientation, HIRect *outLabelRect)
{
    CGRect rect;
    CGFloat radius = 5;
    Boolean on, pressed, disabled, round = false;
    CGFloat face[4] = { 1, 1, 1, 1 };
    CGFloat border[4] = { 0.70, 0.70, 0.70, 1 };

    if (inBounds == NULL || inDrawInfo == NULL)
        return paramErr;
    rect = CGRectInset(*inBounds, 0.5, 0.5);
    on = inDrawInfo->value == kThemeButtonOn || inDrawInfo->value == kThemeButtonMixed ||
         (inDrawInfo->adornment & kThemeAdornmentDefault);
    pressed = inDrawInfo->state == kThemeStatePressed;
    disabled = inDrawInfo->state == kThemeStateUnavailable || inDrawInfo->state == kThemeStateUnavailableInactive;

    switch (inDrawInfo->kind) {
    case kThemeCheckBox:
    case kThemeSmallCheckBox:
    case kThemeMiniCheckBox:
        radius = 3;
        break;
    case kThemeRadioButton:
    case kThemeSmallRadioButton:
    case kThemeMiniRadioButton:
    case kThemeRoundButton:
    case kThemeLargeRoundButton:
        round = true;
        break;
    case kThemeBevelButton:
    case kThemeSmallBevelButton:
    case kThemeLargeBevelButton:
    case kThemeListHeaderButton:
        radius = 2;
        break;
    }
    if (on && inDrawInfo->kind != kThemeListHeaderButton) {
        face[0] = 0.0, face[1] = 0.48, face[2] = 1.0;
        border[0] = 0.0, border[1] = 0.40, border[2] = 0.85;
    }
    if (pressed) {
        for (int i = 0; i < 3; i++) {
            face[i] *= 0.85;
            border[i] *= 0.85;
        }
    }
    if (disabled)
        face[3] = border[3] = 0.5;

    CGContextSaveGState(inContext);
    if (round)
        CGContextAddEllipseInRect(inContext, rect);
    else
        AddRoundedRect(inContext, rect, radius);
    CGContextSetRGBFillColor(inContext, face[0], face[1], face[2], face[3]);
    CGContextFillPath(inContext);
    if (round)
        CGContextAddEllipseInRect(inContext, rect);
    else
        AddRoundedRect(inContext, rect, radius);
    CGContextSetRGBStrokeColor(inContext, border[0], border[1], border[2], border[3]);
    CGContextSetLineWidth(inContext, 1);
    CGContextStrokePath(inContext);
    if (inDrawInfo->adornment & kThemeAdornmentFocus) {
        AddRoundedRect(inContext, CGRectInset(*inBounds, -2, -2), radius + 2);
        CGContextSetRGBStrokeColor(inContext, 0.0, 0.48, 1.0, 0.5);
        CGContextSetLineWidth(inContext, 3);
        CGContextStrokePath(inContext);
    }
    CGContextRestoreGState(inContext);

    if (outLabelRect != NULL)
        *outLabelRect = CGRectInset(*inBounds, round ? inBounds->size.width / 4 : 8, 2);
    return noErr;
}

// the Carbon Event Manager covers application handlers and hot keys. A hot key fires when the
// application itself receives the key, through a local NSEvent monitor, PD has no global hot keys yet
struct OpaqueEventRef {
    OSType eventClass;
    UInt32 eventKind;
    EventTime time;
    EventHotKeyID hotKeyID;
};

struct OpaqueEventHandlerRef {
    struct OpaqueEventHandlerRef *next;
    EventHandlerUPP handler;
    void *userData;
    ItemCount count;
    EventTypeSpec *types;
};

struct OpaqueEventHotKeyRef {
    struct OpaqueEventHotKeyRef *next;
    UInt32 keyCode;
    UInt32 modifiers;
    EventHotKeyID hotKeyID;
};

static pthread_mutex_t gEventLock = PTHREAD_MUTEX_INITIALIZER;
static struct OpaqueEventHandlerRef *gHandlers;
static struct OpaqueEventHotKeyRef *gHotKeys;
static id gHotKeyMonitor;

EventTargetRef GetApplicationEventTarget(void)
{
    static int target;
    return (EventTargetRef)&target;
}

EventHandlerUPP NewEventHandlerUPP(EventHandlerProcPtr userRoutine)
{
    return userRoutine;
}

void DisposeEventHandlerUPP(EventHandlerUPP userUPP)
{
    (void)userUPP;
}

OSStatus InstallEventHandler(EventTargetRef inTarget, EventHandlerUPP inHandler, ItemCount inNumTypes,
                             const EventTypeSpec *inList, void *inUserData, EventHandlerRef *outRef)
{
    if (inTarget != GetApplicationEventTarget() || !inHandler || (inNumTypes && !inList)) return paramErr;

    struct OpaqueEventHandlerRef *h = calloc(1, sizeof(*h));
    if (!h) return memFullErr;
    h->types = calloc(inNumTypes ? inNumTypes : 1, sizeof(EventTypeSpec));
    if (!h->types) {
        free(h);
        return memFullErr;
    }
    memcpy(h->types, inList, inNumTypes * sizeof(EventTypeSpec));
    h->count = inNumTypes;
    h->handler = inHandler;
    h->userData = inUserData;
    pthread_mutex_lock(&gEventLock);
    h->next = gHandlers;
    gHandlers = h;
    pthread_mutex_unlock(&gEventLock);
    if (outRef) *outRef = h;
    return noErr;
}

OSStatus RemoveEventHandler(EventHandlerRef inHandlerRef)
{
    OSStatus ret = paramErr;

    pthread_mutex_lock(&gEventLock);
    for (struct OpaqueEventHandlerRef **p = &gHandlers; *p; p = &(*p)->next) {
        if (*p == inHandlerRef) {
            *p = inHandlerRef->next;
            free(inHandlerRef->types);
            free(inHandlerRef);
            ret = noErr;
            break;
        }
    }
    pthread_mutex_unlock(&gEventLock);
    return ret;
}

OSType GetEventClass(EventRef inEvent)
{
    return inEvent ? inEvent->eventClass : 0;
}

UInt32 GetEventKind(EventRef inEvent)
{
    return inEvent ? inEvent->eventKind : 0;
}

EventTime GetEventTime(EventRef inEvent)
{
    return inEvent ? inEvent->time : 0;
}

OSStatus GetEventParameter(EventRef inEvent, EventParamName inName, EventParamType inDesiredType,
                           EventParamType *outActualType, ByteCount inBufferSize, ByteCount *outActualSize, void *outData)
{
    if (!inEvent) return paramErr;
    if (inEvent->eventClass != kEventClassKeyboard || inName != kEventParamDirectObject ||
        inDesiredType != typeEventHotKeyID)
        return eventParameterNotFoundErr;
    if (outActualType) *outActualType = typeEventHotKeyID;
    if (outActualSize) *outActualSize = sizeof(EventHotKeyID);
    if (outData) {
        if (inBufferSize < sizeof(EventHotKeyID)) return paramErr;
        memcpy(outData, &inEvent->hotKeyID, sizeof(EventHotKeyID));
    }
    return noErr;
}

// runs every application handler registered for the event, the first one that doesn't pass it on wins
static OSStatus dispatchApplicationEvent(EventRef event)
{
    OSStatus ret = eventNotHandledErr;

    pthread_mutex_lock(&gEventLock);
    for (struct OpaqueEventHandlerRef *h = gHandlers; h && ret == eventNotHandledErr; h = h->next) {
        for (ItemCount i = 0; i < h->count; i++) {
            if (h->types[i].eventClass != event->eventClass || h->types[i].eventKind != event->eventKind) continue;
            EventHandlerUPP handler = h->handler;
            void *userData = h->userData;
            pthread_mutex_unlock(&gEventLock);
            ret = handler(NULL, event, userData);
            pthread_mutex_lock(&gEventLock);
            break;
        }
    }
    pthread_mutex_unlock(&gEventLock);
    return ret;
}

// NSEvent modifier flags to the Carbon modifier bits hot keys are registered with
static UInt32 carbonModifiers(unsigned long flags)
{
    UInt32 mods = 0;

    if (flags & (1UL << 17)) mods |= shiftKey;
    if (flags & (1UL << 18)) mods |= controlKey;
    if (flags & (1UL << 19)) mods |= optionKey;
    if (flags & (1UL << 20)) mods |= cmdKey;
    return mods;
}

static void installHotKeyMonitor(void)
{
    Class eventClass = objc_getClass("NSEvent");

    if (gHotKeyMonitor || !eventClass) return;
    // NSEventMaskKeyDown | NSEventMaskKeyUp
    unsigned long long mask = (1ULL << 10) | (1ULL << 11);
    id (^block)(id) = ^id(id nsevent) {
        unsigned short keyCode = ((unsigned short (*)(id, SEL))objc_msgSend)(nsevent, sel_registerName("keyCode"));
        unsigned long flags = ((unsigned long (*)(id, SEL))objc_msgSend)(nsevent, sel_registerName("modifierFlags"));
        unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(nsevent, sel_registerName("type"));
        double time = ((double (*)(id, SEL))objc_msgSend)(nsevent, sel_registerName("timestamp"));
        UInt32 mods = carbonModifiers(flags);
        struct OpaqueEventRef event = { 0 };
        BOOL found = NO;

        pthread_mutex_lock(&gEventLock);
        for (struct OpaqueEventHotKeyRef *k = gHotKeys; k; k = k->next) {
            if (k->keyCode == keyCode && k->modifiers == mods) {
                event.hotKeyID = k->hotKeyID;
                found = YES;
                break;
            }
        }
        pthread_mutex_unlock(&gEventLock);
        if (!found) return nsevent;

        event.eventClass = kEventClassKeyboard;
        event.eventKind = type == 10 ? kEventHotKeyPressed : kEventHotKeyReleased;
        event.time = time;
        return dispatchApplicationEvent(&event) == noErr ? nil : nsevent;
    };
    gHotKeyMonitor = ((id (*)(id, SEL, unsigned long long, id))objc_msgSend)(
        (id)eventClass, sel_registerName("addLocalMonitorForEventsMatchingMask:handler:"), mask, (id)block);
    if (gHotKeyMonitor) ((id (*)(id, SEL))objc_msgSend)(gHotKeyMonitor, sel_registerName("retain"));
}

OSStatus RegisterEventHotKey(UInt32 inHotKeyCode, UInt32 inHotKeyModifiers, EventHotKeyID inHotKeyID,
                             EventTargetRef inTarget, OptionBits inOptions, EventHotKeyRef *outRef)
{
    (void)inOptions;
    if (inTarget != GetApplicationEventTarget() || !outRef) return paramErr;

    UInt32 mods = inHotKeyModifiers & (cmdKey | shiftKey | optionKey | controlKey);
    pthread_mutex_lock(&gEventLock);
    for (struct OpaqueEventHotKeyRef *k = gHotKeys; k; k = k->next) {
        if (k->keyCode == inHotKeyCode && k->modifiers == mods) {
            pthread_mutex_unlock(&gEventLock);
            return eventHotKeyExistsErr;
        }
    }
    struct OpaqueEventHotKeyRef *k = calloc(1, sizeof(*k));
    if (!k) {
        pthread_mutex_unlock(&gEventLock);
        return memFullErr;
    }
    k->keyCode = inHotKeyCode;
    k->modifiers = mods;
    k->hotKeyID = inHotKeyID;
    k->next = gHotKeys;
    gHotKeys = k;
    pthread_mutex_unlock(&gEventLock);
    installHotKeyMonitor();
    *outRef = k;
    return noErr;
}

OSStatus UnregisterEventHotKey(EventHotKeyRef inHotKey)
{
    OSStatus ret = eventHotKeyInvalidErr;

    pthread_mutex_lock(&gEventLock);
    for (struct OpaqueEventHotKeyRef **p = &gHotKeys; *p; p = &(*p)->next) {
        if (*p == inHotKey) {
            *p = inHotKey->next;
            free(inHotKey);
            ret = noErr;
            break;
        }
    }
    pthread_mutex_unlock(&gEventLock);
    return ret;
}

// the grow box is the 15 point square (11 for small windows) that grows away from the origin in each direction
OSStatus HIThemeGetGrowBoxBounds(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo, HIRect *outBounds)
{
    if (!inOrigin || !inDrawInfo || !outBounds) return paramErr;

    CGFloat side = inDrawInfo->kind == kHIThemeGrowBoxKindNone ? 0 :
                   inDrawInfo->size == kHIThemeGrowBoxSizeSmall ? 11 : 15;
    CGFloat x = (inDrawInfo->direction & kThemeGrowLeft) ? inOrigin->x - side : inOrigin->x;
    CGFloat y = (inDrawInfo->direction & kThemeGrowUp) ? inOrigin->y - side : inOrigin->y;

    *outBounds = CGRectMake(x, y, side, side);
    return noErr;
}
