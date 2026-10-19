// CTFontCollection.m - font collections as query descriptors over the registry
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "CTInternal.h"

@interface CTFontCollection : NSObject {
@public
    CFArrayRef _query;
    CFArrayRef _exclusions;
    bool _all;
}
@end

static CFTypeID collectionTypeID;

@implementation CTFontCollection

+ (void)initialize {
    if (self == [CTFontCollection class])
        collectionTypeID = CTRegisterTypeID("CTFontCollection");
}

- (CFTypeID)_cfTypeID {
    return collectionTypeID;
}

- (void)dealloc {
    if (_query) CFRelease(_query);
    if (_exclusions) CFRelease(_exclusions);
    [super dealloc];
}

@end

CFTypeID CTFontCollectionGetTypeID(void) {
    [CTFontCollection class];
    return collectionTypeID;
}

static CTFontCollection *createCollection(CFArrayRef query, CFArrayRef exclusions, bool all) {
    CTFontCollection *c = [[CTFontCollection alloc] init];

    c->_query = query ? CFArrayCreateCopy(NULL, query) : NULL;
    c->_exclusions = exclusions ? CFArrayCreateCopy(NULL, exclusions) : NULL;
    c->_all = all;
    return c;
}

CTFontCollectionRef CTFontCollectionCreateFromAvailableFonts(CFDictionaryRef options) {
    return (CTFontCollectionRef)createCollection(NULL, NULL, true);
}

CTFontCollectionRef CTFontCollectionCreateWithFontDescriptors(CFArrayRef queryDescriptors, CFDictionaryRef options) {
    return (CTFontCollectionRef)createCollection(queryDescriptors, NULL, false);
}

CTFontCollectionRef CTFontCollectionCreateCopyWithFontDescriptors(CTFontCollectionRef original, CFArrayRef queryDescriptors, CFDictionaryRef options) {
    CTFontCollection *o = (CTFontCollection *)original;
    CFMutableArrayRef query = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CTFontCollection *c;

    if (o->_query) CFArrayAppendArray(query, o->_query, CFRangeMake(0, CFArrayGetCount(o->_query)));
    if (queryDescriptors) CFArrayAppendArray(query, queryDescriptors, CFRangeMake(0, CFArrayGetCount(queryDescriptors)));
    c = createCollection(query, o->_exclusions, o->_all);
    CFRelease(query);
    return (CTFontCollectionRef)c;
}

CTMutableFontCollectionRef CTFontCollectionCreateMutableCopy(CTFontCollectionRef original) {
    CTFontCollection *o = (CTFontCollection *)original;

    return (CTMutableFontCollectionRef)createCollection(o->_query, o->_exclusions, o->_all);
}

CFArrayRef CTFontCollectionCopyQueryDescriptors(CTFontCollectionRef collection) {
    CTFontCollection *c = (CTFontCollection *)collection;

    return c->_query ? CFRetain(c->_query) : NULL;
}

void CTFontCollectionSetQueryDescriptors(CTMutableFontCollectionRef collection, CFArrayRef descriptors) {
    CTFontCollection *c = (CTFontCollection *)collection;

    if (c->_query) CFRelease(c->_query);
    c->_query = descriptors ? CFArrayCreateCopy(NULL, descriptors) : NULL;
    c->_all = false;
}

CFArrayRef CTFontCollectionCopyExclusionDescriptors(CTFontCollectionRef collection) {
    CTFontCollection *c = (CTFontCollection *)collection;

    return c->_exclusions ? CFRetain(c->_exclusions) : NULL;
}

void CTFontCollectionSetExclusionDescriptors(CTMutableFontCollectionRef collection, CFArrayRef descriptors) {
    CTFontCollection *c = (CTFontCollection *)collection;

    if (c->_exclusions) CFRelease(c->_exclusions);
    c->_exclusions = descriptors ? CFArrayCreateCopy(NULL, descriptors) : NULL;
}

static bool recordMatches(const CTFontRecord *r, CFArrayRef descriptors) {
    for (CFIndex i = 0; descriptors && i < CFArrayGetCount(descriptors); i++) {
        CFArrayRef matches = CTFontDescriptorCreateMatchingFontDescriptors(CFArrayGetValueAtIndex(descriptors, i), NULL);
        bool hit = false;

        for (CFIndex j = 0; matches && j < CFArrayGetCount(matches) && !hit; j++) {
            CFStringRef name = CTFontDescriptorCopyAttribute(CFArrayGetValueAtIndex(matches, j), kCTFontNameAttribute);

            hit = name && CFEqual(name, r->postScriptName);
            if (name) CFRelease(name);
        }
        if (matches) CFRelease(matches);
        if (hit) return true;
    }
    return false;
}

static CFArrayRef copyMatches(CTFontCollection *c, CFStringRef family) {
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableSetRef seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    CFIndex count = CTRegistryCount();

    // query descriptors keep their order, all-fonts collections follow the registry
    if (c->_all) {
        for (CFIndex i = 0; i < count; i++) {
            const CTFontRecord *r = CTRegistryRecordAtIndex(i);

            if (r == NULL || CFSetContainsValue(seen, r->postScriptName)) continue;
            if (family && !CFEqual(family, r->familyName)) continue;
            if (c->_exclusions && recordMatches(r, c->_exclusions)) continue;
            CFSetAddValue(seen, r->postScriptName);

            CFDictionaryRef attrs = CTRecordCopyAttributes(r);
            CTFontDescriptorRef d = CTFontDescriptorCreateWithAttributes(attrs);

            CFArrayAppendValue(result, d);
            CFRelease(d);
            CFRelease(attrs);
        }
    } else {
        for (CFIndex i = 0; c->_query && i < CFArrayGetCount(c->_query); i++) {
            CFArrayRef matches = CTFontDescriptorCreateMatchingFontDescriptors(CFArrayGetValueAtIndex(c->_query, i), NULL);

            for (CFIndex j = 0; matches && j < CFArrayGetCount(matches); j++) {
                CTFontDescriptorRef d = CFArrayGetValueAtIndex(matches, j);
                CFStringRef name = CTFontDescriptorCopyAttribute(d, kCTFontNameAttribute);
                CFStringRef fam = family ? CTFontDescriptorCopyAttribute(d, kCTFontFamilyNameAttribute) : NULL;
                const CTFontRecord *r = name ? CTRegistryRecordForPostScriptName(name) : NULL;
                bool keep = name && !CFSetContainsValue(seen, name) && (!family || (fam && CFEqual(fam, family)));

                if (keep && r && c->_exclusions && recordMatches(r, c->_exclusions)) keep = false;
                if (keep) {
                    CFSetAddValue(seen, name);
                    CFArrayAppendValue(result, d);
                }
                if (fam) CFRelease(fam);
                if (name) CFRelease(name);
            }
            if (matches) CFRelease(matches);
        }
    }
    CFRelease(seen);
    if (CFArrayGetCount(result) == 0) {
        CFRelease(result);
        return NULL;
    }
    return result;
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptors(CTFontCollectionRef collection) {
    return copyMatches((CTFontCollection *)collection, NULL);
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsWithOptions(CTFontCollectionRef collection, CFDictionaryRef options) {
    return copyMatches((CTFontCollection *)collection, NULL);
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsForFamily(CTFontCollectionRef collection, CFStringRef familyName, CFDictionaryRef options) {
    return copyMatches((CTFontCollection *)collection, familyName);
}

typedef struct {
    CTFontCollectionSortDescriptorsCallback callback;
    void *refCon;
} SortContext;

static CFComparisonResult sortTrampoline(const void *a, const void *b, void *context) {
    SortContext *s = context;

    return s->callback((CTFontDescriptorRef)a, (CTFontDescriptorRef)b, s->refCon);
}

CFArrayRef CTFontCollectionCreateMatchingFontDescriptorsSortedWithCallback(CTFontCollectionRef collection, CTFontCollectionSortDescriptorsCallback sortCallback, void *refCon) {
    CFArrayRef matches = copyMatches((CTFontCollection *)collection, NULL);
    CFMutableArrayRef sorted;
    SortContext context = { sortCallback, refCon };

    if (matches == NULL || sortCallback == NULL) return matches;
    sorted = CFArrayCreateMutableCopy(NULL, 0, matches);
    CFRelease(matches);
    CFArraySortValues(sorted, CFRangeMake(0, CFArrayGetCount(sorted)), sortTrampoline, &context);
    return sorted;
}

CFArrayRef CTFontCollectionCopyFontAttribute(CTFontCollectionRef collection, CFStringRef attributeName, CTFontCollectionCopyOptions options) {
    CFArrayRef matches = copyMatches((CTFontCollection *)collection, NULL);
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableSetRef seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

    for (CFIndex i = 0; matches && i < CFArrayGetCount(matches); i++) {
        CFTypeRef value = CTFontDescriptorCopyAttribute(CFArrayGetValueAtIndex(matches, i), attributeName);

        if (value == NULL) continue;
        if (!(options & kCTFontCollectionCopyUnique) || !CFSetContainsValue(seen, value)) {
            CFSetAddValue(seen, value);
            CFArrayAppendValue(result, value);
        }
        CFRelease(value);
    }
    if (options & kCTFontCollectionCopyStandardSort)
        CFArraySortValues(result, CFRangeMake(0, CFArrayGetCount(result)), (CFComparatorFunction)CFStringCompare, NULL);
    if (matches) CFRelease(matches);
    CFRelease(seen);
    return result;
}

CFArrayRef CTFontCollectionCopyFontAttributes(CTFontCollectionRef collection, CFSetRef attributeNames, CTFontCollectionCopyOptions options) {
    CFArrayRef matches = copyMatches((CTFontCollection *)collection, NULL);
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFIndex nameCount = attributeNames ? CFSetGetCount(attributeNames) : 0;
    const void *names[nameCount > 0 ? nameCount : 1];

    if (attributeNames) CFSetGetValues(attributeNames, names);
    for (CFIndex i = 0; matches && i < CFArrayGetCount(matches); i++) {
        CFMutableDictionaryRef values = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        for (CFIndex j = 0; j < nameCount; j++) {
            CFTypeRef value = CTFontDescriptorCopyAttribute(CFArrayGetValueAtIndex(matches, i), names[j]);

            if (value) {
                CFDictionarySetValue(values, names[j], value);
                CFRelease(value);
            }
        }
        CFArrayAppendValue(result, values);
        CFRelease(values);
    }
    if (matches) CFRelease(matches);
    return result;
}
