// NSString entry points Swift's String bridging calls (StringBridge.swift's
// _StringSelectorHolder), added to PD Foundation's NSString only where missing
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <objc/runtime.h>

static const char *pd_fastCStringContents(id self, SEL _cmd, signed char nul)
{
	return NULL;
}

static const unichar *pd_fastCharacterContents(id self, SEL _cmd)
{
	return NULL;
}

static signed char pd_isNSString_yes(id self, SEL _cmd)
{
	return 1;
}

static signed char pd_isNSString_no(id self, SEL _cmd)
{
	return 0;
}

static id pd_newTagged(id self, SEL _cmd, const char *bytes, NSInteger len)
{
	return (id)CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)bytes, len, kCFStringEncodingASCII, false);
}

static void pd_getCharactersRange(id self, SEL _cmd, unichar *buf, NSRange r)
{
	CFStringGetCharacters((CFStringRef)self, CFRangeMake(r.location, r.length), buf);
}

static signed char pd_getBytes(id self, SEL _cmd, void *buf, NSUInteger max, NSUInteger *used,
    NSUInteger enc, NSUInteger opts, NSRange r, NSRange *left)
{
	CFStringEncoding cfenc = CFStringConvertNSStringEncodingToEncoding(enc);
	CFIndex u = 0;
	CFIndex n = CFStringGetBytes((CFStringRef)self, CFRangeMake(r.location, r.length), cfenc, 0,
	    (opts & 2) != 0, buf, max, &u);

	if (used)
		*used = u;
	if (left)
		*left = NSMakeRange(r.location + n, r.length - n);
	return n > 0 || r.length == 0;
}

static NSInteger pd_compareLocale(id self, SEL _cmd, id other, NSUInteger opts, NSRange r, id locale)
{
	return CFStringCompareWithOptions((CFStringRef)self, (CFStringRef)other, CFRangeMake(r.location, r.length), opts);
}

static id pd_normalized(id self, CFStringNormalizationForm form)
{
	CFMutableStringRef m = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, (CFStringRef)self);

	CFStringNormalize(m, form);
	return [(id)m autorelease];
}

static id pd_decomposed(id self, SEL _cmd)
{
	return pd_normalized(self, kCFStringNormalizationFormD);
}

static id pd_precomposed(id self, SEL _cmd)
{
	return pd_normalized(self, kCFStringNormalizationFormC);
}

static void add(Class c, const char *sel, IMP imp, const char *types)
{
	SEL s = sel_registerName(sel);

	if (!class_getInstanceMethod(c, s))
		class_addMethod(c, s, imp, types);
}

__attribute__((constructor)) static void pd_swift_string_bridge_init(void)
{
	Class ns = objc_getClass("NSString");
	Class obj = objc_getClass("NSObject");

	if (!ns)
		return;
	add(ns, "_fastCStringContents:", (IMP)pd_fastCStringContents, "r*@:c");
	add(ns, "_fastCharacterContents", (IMP)pd_fastCharacterContents, "r^S@:");
	add(ns, "isNSString__", (IMP)pd_isNSString_yes, "c@:");
	add(obj, "isNSString__", (IMP)pd_isNSString_no, "c@:");
	add(ns, "getCharacters:range:", (IMP)pd_getCharactersRange, "v@:^S{_NSRange=QQ}");
	add(ns, "getBytes:maxLength:usedLength:encoding:options:range:remainingRange:", (IMP)pd_getBytes,
	    "c@:^vQ^QQQ{_NSRange=QQ}^{_NSRange=QQ}");
	add(ns, "compare:options:range:locale:", (IMP)pd_compareLocale, "q@:@Q{_NSRange=QQ}@");
	add(ns, "decomposedStringWithCanonicalMapping", (IMP)pd_decomposed, "@@:");
	add(ns, "precomposedStringWithCanonicalMapping", (IMP)pd_precomposed, "@@:");
	if (!class_getClassMethod(ns, sel_registerName("newTaggedNSStringWithASCIIBytes_:length_:")))
		class_addMethod(object_getClass(ns), sel_registerName("newTaggedNSStringWithASCIIBytes_:length_:"),
		    (IMP)pd_newTagged, "@@:r*q");
}
