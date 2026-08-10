/*
 * CFXPCBridge.c - conversion between CF objects and XPC objects.
 */

#include <CoreFoundation/CFXPCBridge.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFUUID.h>

#include <stdlib.h>
#include <string.h>

#define NSEC_PER_SEC_D 1000000000.0

/* Copy a CFString out as UTF-8. Returns a malloc'd buffer the caller frees, or
 * NULL if the string is not representable. */
static char *cfstring_copy_utf8(CFStringRef str)
{
    CFIndex len = CFStringGetLength(str);
    CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    char *buf = malloc((size_t)max);

    if (buf == NULL) {
        return NULL;
    }
    if (!CFStringGetCString(str, buf, max, kCFStringEncodingUTF8)) {
        free(buf);
        return NULL;
    }
    return buf;
}

static xpc_object_t xpc_from_cf_number(CFNumberRef num)
{
    if (CFNumberIsFloatType(num)) {
        double d = 0;
        if (!CFNumberGetValue(num, kCFNumberDoubleType, &d)) {
            return NULL;
        }
        return xpc_double_create(d);
    } else {
        int64_t i = 0;
        if (!CFNumberGetValue(num, kCFNumberSInt64Type, &i)) {
            return NULL;
        }
        return xpc_int64_create(i);
    }
}

/* Context for the CFDictionary apply callback, which cannot itself stop early. */
struct dict_ctx {
    xpc_object_t xdict;
    bool failed;
};

static void dict_entry_to_xpc(const void *key, const void *value, void *raw)
{
    struct dict_ctx *ctx = raw;
    char *ckey;
    xpc_object_t xval;

    if (ctx->failed) {
        return;
    }
    /* XPC dictionaries are keyed by C string, so only CFString keys can map. */
    if (key == NULL || CFGetTypeID(key) != CFStringGetTypeID()) {
        ctx->failed = true;
        return;
    }
    ckey = cfstring_copy_utf8((CFStringRef)key);
    if (ckey == NULL) {
        ctx->failed = true;
        return;
    }

    xval = _CFXPCCreateXPCObjectFromCFObject((CFTypeRef)value);
    if (xval == NULL) {
        free(ckey);
        ctx->failed = true;
        return;
    }
    xpc_dictionary_set_value(ctx->xdict, ckey, xval);
    xpc_release(xval);
    free(ckey);
}

xpc_object_t _CFXPCCreateXPCObjectFromCFObject(CFTypeRef cf)
{
    CFTypeID type;

    if (cf == NULL) {
        return NULL;
    }
    type = CFGetTypeID(cf);

    if (type == CFStringGetTypeID()) {
        char *s = cfstring_copy_utf8((CFStringRef)cf);
        xpc_object_t x;
        if (s == NULL) {
            return NULL;
        }
        x = xpc_string_create(s);
        free(s);
        return x;
    }

    if (type == CFNumberGetTypeID()) {
        return xpc_from_cf_number((CFNumberRef)cf);
    }

    if (type == CFBooleanGetTypeID()) {
        return xpc_bool_create(CFBooleanGetValue((CFBooleanRef)cf));
    }

    if (type == CFDataGetTypeID()) {
        return xpc_data_create(CFDataGetBytePtr((CFDataRef)cf),
                               (size_t)CFDataGetLength((CFDataRef)cf));
    }

    if (type == CFDateGetTypeID()) {
        CFAbsoluteTime at = CFDateGetAbsoluteTime((CFDateRef)cf);
        double since1970 = at + kCFAbsoluteTimeIntervalSince1970;
        return xpc_date_create((int64_t)(since1970 * NSEC_PER_SEC_D));
    }

    if (type == CFNullGetTypeID()) {
        return xpc_null_create();
    }

    if (type == CFUUIDGetTypeID()) {
        CFUUIDBytes b = CFUUIDGetUUIDBytes((CFUUIDRef)cf);
        uuid_t raw;
        /* CFUUIDBytes is already the 16 bytes in network order. */
        memcpy(raw, &b, sizeof(raw));
        return xpc_uuid_create(raw);
    }

    if (type == CFArrayGetTypeID()) {
        CFArrayRef arr = (CFArrayRef)cf;
        CFIndex i, n = CFArrayGetCount(arr);
        xpc_object_t xarr = xpc_array_create(NULL, 0);

        if (xarr == NULL) {
            return NULL;
        }
        for (i = 0; i < n; i++) {
            xpc_object_t xval =
                _CFXPCCreateXPCObjectFromCFObject(CFArrayGetValueAtIndex(arr, i));
            if (xval == NULL) {
                xpc_release(xarr);
                return NULL;
            }
            xpc_array_append_value(xarr, xval);
            xpc_release(xval);
        }
        return xarr;
    }

    if (type == CFDictionaryGetTypeID()) {
        struct dict_ctx ctx;

        ctx.xdict = xpc_dictionary_create(NULL, NULL, 0);
        ctx.failed = false;
        if (ctx.xdict == NULL) {
            return NULL;
        }
        CFDictionaryApplyFunction((CFDictionaryRef)cf, dict_entry_to_xpc, &ctx);
        if (ctx.failed) {
            xpc_release(ctx.xdict);
            return NULL;
        }
        return ctx.xdict;
    }

    return NULL;
}

CFTypeRef _CFXPCCreateCFObjectFromXPCObject(xpc_object_t xpc)
{
    xpc_type_t type;

    if (xpc == NULL) {
        return NULL;
    }
    type = xpc_get_type(xpc);

    if (type == XPC_TYPE_STRING) {
        const char *s = xpc_string_get_string_ptr(xpc);
        if (s == NULL) {
            return NULL;
        }
        return CFStringCreateWithCString(kCFAllocatorDefault, s,
                                         kCFStringEncodingUTF8);
    }

    if (type == XPC_TYPE_INT64) {
        int64_t v = xpc_int64_get_value(xpc);
        return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &v);
    }

    if (type == XPC_TYPE_UINT64) {
        /* CFNumber has no unsigned types; the 64-bit signed slot is the widest
         * container available, matching what Apple's bridge does. */
        int64_t v = (int64_t)xpc_uint64_get_value(xpc);
        return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &v);
    }

    if (type == XPC_TYPE_DOUBLE) {
        double v = xpc_double_get_value(xpc);
        return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &v);
    }

    if (type == XPC_TYPE_BOOL) {
        return CFRetain(xpc_bool_get_value(xpc) ? kCFBooleanTrue : kCFBooleanFalse);
    }

    if (type == XPC_TYPE_DATA) {
        const void *bytes = xpc_data_get_bytes_ptr(xpc);
        size_t len = xpc_data_get_length(xpc);
        if (bytes == NULL && len != 0) {
            return NULL;
        }
        return CFDataCreate(kCFAllocatorDefault, bytes, (CFIndex)len);
    }

    if (type == XPC_TYPE_DATE) {
        double since1970 = (double)xpc_date_get_value(xpc) / NSEC_PER_SEC_D;
        return CFDateCreate(kCFAllocatorDefault,
                            since1970 - kCFAbsoluteTimeIntervalSince1970);
    }

    if (type == XPC_TYPE_NULL) {
        return CFRetain(kCFNull);
    }

    if (type == XPC_TYPE_UUID) {
        const uint8_t *b = xpc_uuid_get_bytes(xpc);
        CFUUIDBytes bytes;
        if (b == NULL) {
            return NULL;
        }
        memcpy(&bytes, b, sizeof(bytes));
        return CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, bytes);
    }

    if (type == XPC_TYPE_ARRAY) {
        CFMutableArrayRef arr =
            CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        __block bool failed = false;

        if (arr == NULL) {
            return NULL;
        }
        xpc_array_apply(xpc, ^bool(size_t index __attribute__((unused)),
                                   xpc_object_t value) {
            CFTypeRef cf = _CFXPCCreateCFObjectFromXPCObject(value);
            if (cf == NULL) {
                failed = true;
                return false;           /* stop iterating */
            }
            CFArrayAppendValue(arr, cf);
            CFRelease(cf);
            return true;
        });
        if (failed) {
            CFRelease(arr);
            return NULL;
        }
        return arr;
    }

    if (type == XPC_TYPE_DICTIONARY) {
        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        __block bool failed = false;

        if (dict == NULL) {
            return NULL;
        }
        xpc_dictionary_apply(xpc, ^bool(const char *key, xpc_object_t value) {
            CFStringRef cfkey;
            CFTypeRef cfval;

            cfkey = CFStringCreateWithCString(kCFAllocatorDefault, key,
                                              kCFStringEncodingUTF8);
            if (cfkey == NULL) {
                failed = true;
                return false;
            }
            cfval = _CFXPCCreateCFObjectFromXPCObject(value);
            if (cfval == NULL) {
                CFRelease(cfkey);
                failed = true;
                return false;
            }
            CFDictionarySetValue(dict, cfkey, cfval);
            CFRelease(cfkey);
            CFRelease(cfval);
            return true;
        });
        if (failed) {
            CFRelease(dict);
            return NULL;
        }
        return dict;
    }

    return NULL;
}
