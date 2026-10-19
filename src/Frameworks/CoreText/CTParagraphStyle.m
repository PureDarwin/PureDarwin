// CTParagraphStyle.m - paragraph styles, text tabs and run delegates
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTInternal.h"
#import "CTLayout.h"
#import <objc/message.h>

@implementation CTParagraphStyle

static CFTypeID paragraphTypeID;

+ (void)initialize {
    if (self == [CTParagraphStyle class])
        paragraphTypeID = CTRegisterTypeID("CTParagraphStyle");
}

- (CFTypeID)_cfTypeID {
    return paragraphTypeID;
}

- (void)dealloc {
    if (_tabStops) CFRelease(_tabStops);
    [super dealloc];
}

@end

@interface CTTextTab : NSObject {
@public
    CTTextAlignment _alignment;
    double _location;
    CFDictionaryRef _options;
}
@end

static CFTypeID tabTypeID;

@implementation CTTextTab

+ (void)initialize {
    if (self == [CTTextTab class])
        tabTypeID = CTRegisterTypeID("CTTextTab");
}

- (CFTypeID)_cfTypeID {
    return tabTypeID;
}

- (void)dealloc {
    if (_options) CFRelease(_options);
    [super dealloc];
}

@end

CFTypeID CTParagraphStyleGetTypeID(void) {
    [CTParagraphStyle class];
    return paragraphTypeID;
}

CFTypeID CTTextTabGetTypeID(void) {
    [CTTextTab class];
    return tabTypeID;
}

void CTParagraphStyleGetDefaults(CTParagraphStyleValues *v) {
    memset(v, 0, sizeof(*v));
    v->alignment = kCTTextAlignmentNatural;
    v->defaultTabInterval = 0;
    v->lineBreakMode = kCTLineBreakByWordWrapping;
    v->baseWritingDirection = kCTWritingDirectionNatural;
}

// each specifier's value size and where it lives
static void *fieldFor(CTParagraphStyleValues *v, CTParagraphStyleSpecifier spec, size_t *size) {
    switch (spec) {
    case kCTParagraphStyleSpecifierAlignment: *size = sizeof(CTTextAlignment); return &v->alignment;
    case kCTParagraphStyleSpecifierFirstLineHeadIndent: *size = sizeof(CGFloat); return &v->firstLineHeadIndent;
    case kCTParagraphStyleSpecifierHeadIndent: *size = sizeof(CGFloat); return &v->headIndent;
    case kCTParagraphStyleSpecifierTailIndent: *size = sizeof(CGFloat); return &v->tailIndent;
    case kCTParagraphStyleSpecifierTabStops: *size = sizeof(CFArrayRef); return &v->tabStops;
    case kCTParagraphStyleSpecifierDefaultTabInterval: *size = sizeof(CGFloat); return &v->defaultTabInterval;
    case kCTParagraphStyleSpecifierLineBreakMode: *size = sizeof(CTLineBreakMode); return &v->lineBreakMode;
    case kCTParagraphStyleSpecifierLineHeightMultiple: *size = sizeof(CGFloat); return &v->lineHeightMultiple;
    case kCTParagraphStyleSpecifierMaximumLineHeight: *size = sizeof(CGFloat); return &v->maximumLineHeight;
    case kCTParagraphStyleSpecifierMinimumLineHeight: *size = sizeof(CGFloat); return &v->minimumLineHeight;
    case kCTParagraphStyleSpecifierLineSpacing: *size = sizeof(CGFloat); return &v->lineSpacing;
    case kCTParagraphStyleSpecifierParagraphSpacing: *size = sizeof(CGFloat); return &v->paragraphSpacing;
    case kCTParagraphStyleSpecifierParagraphSpacingBefore: *size = sizeof(CGFloat); return &v->paragraphSpacingBefore;
    case kCTParagraphStyleSpecifierBaseWritingDirection: *size = sizeof(CTWritingDirection); return &v->baseWritingDirection;
    case kCTParagraphStyleSpecifierMaximumLineSpacing: *size = sizeof(CGFloat); return &v->maximumLineSpacing;
    case kCTParagraphStyleSpecifierMinimumLineSpacing: *size = sizeof(CGFloat); return &v->minimumLineSpacing;
    case kCTParagraphStyleSpecifierLineSpacingAdjustment: *size = sizeof(CGFloat); return &v->lineSpacingAdjustment;
    case kCTParagraphStyleSpecifierLineBoundsOptions: *size = sizeof(CTLineBoundsOptions); return &v->lineBoundsOptions;
    default: return NULL;
    }
}

CTParagraphStyleRef CTParagraphStyleCreate(const CTParagraphStyleSetting *settings, size_t settingCount) {
    CTParagraphStyle *style = [[CTParagraphStyle alloc] init];

    CTParagraphStyleGetDefaults(&style->_values);
    for (size_t i = 0; settings && i < settingCount; i++) {
        size_t size;
        void *field = fieldFor(&style->_values, settings[i].spec, &size);

        if (field == NULL || settings[i].value == NULL || settings[i].valueSize != size) continue;
        if (settings[i].spec == kCTParagraphStyleSpecifierTabStops) {
            CFArrayRef tabs = *(CFArrayRef *)settings[i].value;

            if (style->_tabStops) CFRelease(style->_tabStops);
            style->_tabStops = tabs ? CFArrayCreateCopy(NULL, tabs) : NULL;
            style->_values.tabStops = style->_tabStops;
            continue;
        }
        memcpy(field, settings[i].value, size);
    }
    return (CTParagraphStyleRef)style;
}

CTParagraphStyleRef CTParagraphStyleCreateCopy(CTParagraphStyleRef paragraphStyle) {
    CTParagraphStyle *style = [[CTParagraphStyle alloc] init];

    if (paragraphStyle) {
        style->_values = ((CTParagraphStyle *)paragraphStyle)->_values;
        style->_tabStops = style->_values.tabStops ? CFRetain(style->_values.tabStops) : NULL;
    } else
        CTParagraphStyleGetDefaults(&style->_values);
    return (CTParagraphStyleRef)style;
}

bool CTParagraphStyleGetValueForSpecifier(CTParagraphStyleRef paragraphStyle, CTParagraphStyleSpecifier spec, size_t valueBufferSize, void *valueBuffer) {
    CTParagraphStyleValues defaults, *v;
    size_t size;
    void *field;

    CTParagraphStyleGetDefaults(&defaults);
    v = paragraphStyle ? &((CTParagraphStyle *)paragraphStyle)->_values : &defaults;
    field = fieldFor(v, spec, &size);
    if (field == NULL || valueBufferSize != size || valueBuffer == NULL) return false;
    memcpy(valueBuffer, field, size);
    return true;
}

// AppKit's paragraph style is only known by its selectors here
static CGFloat floatValue(id object, SEL selector) {
    return ((CGFloat (*)(id, SEL))objc_msgSend)(object, selector);
}

static NSInteger integerValue(id object, SEL selector) {
    return ((NSInteger (*)(id, SEL))objc_msgSend)(object, selector);
}

// the paragraph style in a run's attributes, CTParagraphStyle or AppKit's NSParagraphStyle
void CTParagraphStyleValuesFromAttributes(CFDictionaryRef attributes, CTParagraphStyleValues *v) {
    id style = attributes ? (id)CFDictionaryGetValue(attributes, kCTParagraphStyleAttributeName) : nil;

    CTParagraphStyleGetDefaults(v);
    if (style == nil) return;
    if ([style isKindOfClass:[CTParagraphStyle class]]) {
        *v = ((CTParagraphStyle *)style)->_values;
        return;
    }
    if ([style respondsToSelector:@selector(alignment)]) {
        // NSTextAlignment's center and right swapped places between AppKit generations, names avoid that
        NSInteger a = integerValue(style, @selector(alignment));

        v->alignment = a == 1 ? kCTTextAlignmentRight : a == 2 ? kCTTextAlignmentCenter : a == 3 ? kCTTextAlignmentJustified : a == 4 ? kCTTextAlignmentNatural : kCTTextAlignmentLeft;
    }
    if ([style respondsToSelector:@selector(lineBreakMode)]) v->lineBreakMode = (CTLineBreakMode)integerValue(style, @selector(lineBreakMode));
    if ([style respondsToSelector:@selector(firstLineHeadIndent)]) v->firstLineHeadIndent = floatValue(style, @selector(firstLineHeadIndent));
    if ([style respondsToSelector:@selector(headIndent)]) v->headIndent = floatValue(style, @selector(headIndent));
    if ([style respondsToSelector:@selector(tailIndent)]) v->tailIndent = floatValue(style, @selector(tailIndent));
    if ([style respondsToSelector:@selector(lineSpacing)]) v->lineSpacing = floatValue(style, @selector(lineSpacing));
    if ([style respondsToSelector:@selector(paragraphSpacing)]) v->paragraphSpacing = floatValue(style, @selector(paragraphSpacing));
    if ([style respondsToSelector:@selector(paragraphSpacingBefore)]) v->paragraphSpacingBefore = floatValue(style, @selector(paragraphSpacingBefore));
    if ([style respondsToSelector:@selector(minimumLineHeight)]) v->minimumLineHeight = floatValue(style, @selector(minimumLineHeight));
    if ([style respondsToSelector:@selector(maximumLineHeight)]) v->maximumLineHeight = floatValue(style, @selector(maximumLineHeight));
    if ([style respondsToSelector:@selector(lineHeightMultiple)]) v->lineHeightMultiple = floatValue(style, @selector(lineHeightMultiple));
    if ([style respondsToSelector:@selector(baseWritingDirection)]) v->baseWritingDirection = (CTWritingDirection)integerValue(style, @selector(baseWritingDirection));
    if ([style respondsToSelector:@selector(defaultTabInterval)]) v->defaultTabInterval = floatValue(style, @selector(defaultTabInterval));
}

CTTextTabRef CTTextTabCreate(CTTextAlignment alignment, double location, CFDictionaryRef options) {
    CTTextTab *tab = [[CTTextTab alloc] init];

    tab->_alignment = alignment;
    tab->_location = location;
    tab->_options = options ? CFDictionaryCreateCopy(NULL, options) : NULL;
    return (CTTextTabRef)tab;
}

CTTextAlignment CTTextTabGetAlignment(CTTextTabRef tab) {
    return ((CTTextTab *)tab)->_alignment;
}

double CTTextTabGetLocation(CTTextTabRef tab) {
    return ((CTTextTab *)tab)->_location;
}

CFDictionaryRef CTTextTabGetOptions(CTTextTabRef tab) {
    return ((CTTextTab *)tab)->_options;
}

@implementation CTRunDelegate

static CFTypeID delegateTypeID;

+ (void)initialize {
    if (self == [CTRunDelegate class])
        delegateTypeID = CTRegisterTypeID("CTRunDelegate");
}

- (CFTypeID)_cfTypeID {
    return delegateTypeID;
}

- (void)dealloc {
    if (_callbacks.dealloc) _callbacks.dealloc(_refCon);
    [super dealloc];
}

@end

CFTypeID CTRunDelegateGetTypeID(void) {
    [CTRunDelegate class];
    return delegateTypeID;
}

CTRunDelegateRef CTRunDelegateCreate(const CTRunDelegateCallbacks *callbacks, void *refCon) {
    CTRunDelegate *d;

    if (callbacks == NULL || callbacks->version != kCTRunDelegateVersion1) return NULL;
    d = [[CTRunDelegate alloc] init];
    d->_callbacks = *callbacks;
    d->_refCon = refCon;
    return (CTRunDelegateRef)d;
}

void *CTRunDelegateGetRefCon(CTRunDelegateRef runDelegate) {
    return ((CTRunDelegate *)runDelegate)->_refCon;
}
