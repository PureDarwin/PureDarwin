// CTFontDescriptor.m - font descriptors over the registry of installed fonts
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTInternal.h"

@interface CTFontDescriptor : NSObject {
@public
    CFDictionaryRef _attributes;
}
@end

static CFTypeID descriptorTypeID;

@implementation CTFontDescriptor

+ (void)initialize {
    if (self == [CTFontDescriptor class])
        descriptorTypeID = CTRegisterTypeID("CTFontDescriptor");
}

- (CFTypeID)_cfTypeID {
    return descriptorTypeID;
}

- (void)dealloc {
    if (_attributes) CFRelease(_attributes);
    [super dealloc];
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[CTFontDescriptor class]] && CFEqual(_attributes, ((CTFontDescriptor *)other)->_attributes);
}

- (NSUInteger)hash {
    return CFHash(_attributes);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<CTFontDescriptor %p> %@", self, (id)_attributes];
}

@end

CFTypeID CTFontDescriptorGetTypeID(void) {
    [CTFontDescriptor class];
    return descriptorTypeID;
}

static CTFontDescriptorRef createDescriptor(CFDictionaryRef attributes) {
    CTFontDescriptor *d = [[CTFontDescriptor alloc] init];

    d->_attributes = attributes ? CFDictionaryCreateCopy(NULL, attributes)
                                : CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return (CTFontDescriptorRef)d;
}

static CFDictionaryRef attributesOf(CTFontDescriptorRef descriptor) {
    return descriptor ? ((CTFontDescriptor *)descriptor)->_attributes : NULL;
}

CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes) {
    return createDescriptor(attributes);
}

CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef name, CGFloat size) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef d;

    if (name)
        CFDictionarySetValue(dict, kCTFontNameAttribute, name);
    if (size > 0) {
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberCGFloatType, &size);

        CFDictionarySetValue(dict, kCTFontSizeAttribute, n);
        CFRelease(n);
    }
    d = createDescriptor(dict);
    CFRelease(dict);
    return d;
}

static CFMutableDictionaryRef mutableAttributes(CTFontDescriptorRef original) {
    CFDictionaryRef attrs = attributesOf(original);

    return attrs ? CFDictionaryCreateMutableCopy(NULL, 0, attrs)
                 : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithAttributes(CTFontDescriptorRef original, CFDictionaryRef attributes) {
    CFMutableDictionaryRef dict = mutableAttributes(original);
    CTFontDescriptorRef d;
    CFIndex count = attributes ? CFDictionaryGetCount(attributes) : 0;
    const void **keys = malloc(sizeof(void *) * (count ? count : 1));
    const void **values = malloc(sizeof(void *) * (count ? count : 1));

    if (count)
        CFDictionaryGetKeysAndValues(attributes, keys, values);
    for (CFIndex i = 0; i < count; i++)
        CFDictionarySetValue(dict, keys[i], values[i]);
    free(keys);
    free(values);
    d = createDescriptor(dict);
    CFRelease(dict);
    return d;
}

// the family a descriptor resolves to, from its own attributes or its best match
static CFStringRef copyResolvedFamily(CTFontDescriptorRef descriptor) {
    CFDictionaryRef attrs = attributesOf(descriptor);
    CFStringRef family = attrs ? CFDictionaryGetValue(attrs, kCTFontFamilyNameAttribute) : NULL;
    const CTFontRecord **matches = NULL;
    CFIndex count;

    if (family)
        return CFRetain(family);
    count = CTRegistryCopyMatches(attrs, &matches);
    family = count > 0 ? CFRetain(matches[0]->familyName) : NULL;
    free(matches);
    return family;
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithFamily(CTFontDescriptorRef original, CFStringRef family) {
    CFMutableDictionaryRef dict = mutableAttributes(original);
    CTFontDescriptorRef d;

    // a new family replaces the face, the traits pick the member
    CFDictionaryRemoveValue(dict, kCTFontNameAttribute);
    CFDictionaryRemoveValue(dict, kCTFontDisplayNameAttribute);
    CFDictionaryRemoveValue(dict, kCTFontURLAttribute);
    CFDictionarySetValue(dict, kCTFontFamilyNameAttribute, family);
    d = createDescriptor(dict);
    CFRelease(dict);
    return d;
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorRef original, CTFontSymbolicTraits value, CTFontSymbolicTraits mask) {
    CFMutableDictionaryRef dict = mutableAttributes(original);
    CFDictionaryRef oldTraits = CFDictionaryGetValue(dict, kCTFontTraitsAttribute);
    CFMutableDictionaryRef traits;
    CFStringRef family = copyResolvedFamily(original);
    uint32_t symbolic = 0;
    CFNumberRef n;
    CTFontDescriptorRef d;

    if (oldTraits && CFGetTypeID(oldTraits) == CFDictionaryGetTypeID()) {
        CFNumberRef old = CFDictionaryGetValue(oldTraits, kCTFontSymbolicTrait);

        if (old) CFNumberGetValue(old, kCFNumberSInt32Type, &symbolic);
        traits = CFDictionaryCreateMutableCopy(NULL, 0, oldTraits);
    } else {
        traits = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    symbolic = (symbolic & ~mask) | (value & mask);
    n = CFNumberCreate(NULL, kCFNumberSInt32Type, &symbolic);
    CFDictionarySetValue(traits, kCTFontSymbolicTrait, n);
    // the weight follows the bold bit when it is asked for
    if (mask & kCTFontTraitBold)
        CFDictionaryRemoveValue(traits, kCTFontWeightTrait);
    if (mask & kCTFontTraitItalic)
        CFDictionaryRemoveValue(traits, kCTFontSlantTrait);
    CFDictionarySetValue(dict, kCTFontTraitsAttribute, traits);
    CFDictionaryRemoveValue(dict, kCTFontNameAttribute);
    CFDictionaryRemoveValue(dict, kCTFontDisplayNameAttribute);
    CFDictionaryRemoveValue(dict, kCTFontStyleNameAttribute);
    CFDictionaryRemoveValue(dict, kCTFontURLAttribute);
    if (family) {
        CFDictionarySetValue(dict, kCTFontFamilyNameAttribute, family);
        CFRelease(family);
    }
    d = createDescriptor(dict);
    CFRelease(n);
    CFRelease(traits);
    CFRelease(dict);
    return d;
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithVariation(CTFontDescriptorRef original, CFNumberRef identifier, CGFloat value) {
    CFMutableDictionaryRef dict = mutableAttributes(original);
    CFDictionaryRef old = CFDictionaryGetValue(dict, kCTFontVariationAttribute);
    CFMutableDictionaryRef variation = old ? CFDictionaryCreateMutableCopy(NULL, 0, old)
                                           : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberCGFloatType, &value);
    CTFontDescriptorRef d;

    CFDictionarySetValue(variation, identifier, n);
    CFDictionarySetValue(dict, kCTFontVariationAttribute, variation);
    d = createDescriptor(dict);
    CFRelease(n);
    CFRelease(variation);
    CFRelease(dict);
    return d;
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithFeature(CTFontDescriptorRef original, CFNumberRef type, CFNumberRef selector) {
    CFMutableDictionaryRef dict = mutableAttributes(original);
    CFArrayRef old = CFDictionaryGetValue(dict, kCTFontFeatureSettingsAttribute);
    CFMutableArrayRef settings = old ? CFArrayCreateMutableCopy(NULL, 0, old) : CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    const void *keys[2] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    const void *values[2] = { type, selector };
    CFDictionaryRef setting = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef d;

    CFArrayAppendValue(settings, setting);
    CFDictionarySetValue(dict, kCTFontFeatureSettingsAttribute, settings);
    d = createDescriptor(dict);
    CFRelease(setting);
    CFRelease(settings);
    CFRelease(dict);
    return d;
}

// a matched face's attributes plus the caller's size, matrix, features and cascade list
static CTFontDescriptorRef createMatchedDescriptor(const CTFontRecord *record, CFDictionaryRef original) {
    CFDictionaryRef base = CTRecordCopyAttributes(record);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(NULL, 0, base);
    const CFStringRef carried[] = { kCTFontSizeAttribute, kCTFontMatrixAttribute, kCTFontFeatureSettingsAttribute,
                                    kCTFontCascadeListAttribute, kCTFontVariationAttribute, kCTFontFixedAdvanceAttribute,
                                    kCTFontOrientationAttribute };
    CTFontDescriptorRef d;

    for (size_t i = 0; original && i < sizeof(carried) / sizeof(carried[0]); i++) {
        CFTypeRef v = CFDictionaryGetValue(original, carried[i]);

        if (v) CFDictionarySetValue(dict, carried[i], v);
    }
    d = createDescriptor(dict);
    CFRelease(dict);
    CFRelease(base);
    return d;
}

CFArrayRef CTFontDescriptorCreateMatchingFontDescriptors(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes) {
    CFDictionaryRef attrs = attributesOf(descriptor);
    const CTFontRecord **matches = NULL;
    CFIndex count = CTRegistryCopyMatches(attrs, &matches);
    CFMutableArrayRef result;

    if (count == 0) {
        free(matches);
        return NULL;
    }
    result = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < count; i++) {
        CTFontDescriptorRef d = createMatchedDescriptor(matches[i], attrs);

        CFArrayAppendValue(result, d);
        CFRelease(d);
    }
    free(matches);
    return result;
}

CTFontDescriptorRef CTFontDescriptorCreateMatchingFontDescriptor(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes) {
    CFDictionaryRef attrs = attributesOf(descriptor);
    const CTFontRecord **matches = NULL;
    CFIndex count = CTRegistryCopyMatches(attrs, &matches);
    CTFontDescriptorRef d = count > 0 ? createMatchedDescriptor(matches[0], attrs) : NULL;

    free(matches);
    return d;
}

CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor) {
    CFDictionaryRef attrs = attributesOf(descriptor);

    return attrs ? CFRetain(attrs) : NULL;
}

// font attributes a descriptor leaves out come from the face it names
static bool resolvable(CFStringRef attribute) {
    return CFEqual(attribute, kCTFontNameAttribute) || CFEqual(attribute, kCTFontFamilyNameAttribute) ||
           CFEqual(attribute, kCTFontStyleNameAttribute) || CFEqual(attribute, kCTFontDisplayNameAttribute) ||
           CFEqual(attribute, kCTFontTraitsAttribute) || CFEqual(attribute, kCTFontURLAttribute);
}

CFTypeRef CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
    CFDictionaryRef attrs = attributesOf(descriptor);
    CFTypeRef value = attrs ? CFDictionaryGetValue(attrs, attribute) : NULL;
    const CTFontRecord **matches = NULL;

    if (value)
        return CFRetain(value);
    if (attrs == NULL || CFDictionaryGetCount(attrs) == 0 || !resolvable(attribute))
        return NULL;
    if (CTRegistryCopyMatches(attrs, &matches) > 0) {
        CFDictionaryRef matched = CTRecordCopyAttributes(matches[0]);

        value = CFDictionaryGetValue(matched, attribute);
        if (value) CFRetain(value);
        CFRelease(matched);
    }
    free(matches);
    return value;
}

CFTypeRef CTFontDescriptorCopyLocalizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute, CFStringRef *language) {
    // the names are the English ones, that is what the registry reads
    if (language)
        *language = CFSTR("en");
    return CTFontDescriptorCopyAttribute(descriptor, attribute);
}
