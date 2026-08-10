/*
 * Copyright (c) 2007-2010,2012-2017 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * SecCertificatePath.c - CoreFoundation based certificate path object
 */

#include "SecCertificatePath.h"

#include <Security/SecTrust.h>
#include <Security/SecTrustStore.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <utilities/SecIOFormat.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <AssertMacros.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <Security/SecBase.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <utilities/debugging.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

// MARK: -
// MARK: SecCertificatePath
/********************************************************
 ************* SecCertificatePath object ****************
 ********************************************************/
struct SecCertificatePath {
    CFRuntimeBase		_base;
	CFIndex				count;

	SecCertificateRef	certificates[];
};

CFGiblisWithHashFor(SecCertificatePath)

static void SecCertificatePathDestroy(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
		CFRelease(certificatePath->certificates[ix]);
    }
}

static Boolean SecCertificatePathCompare(CFTypeRef cf1, CFTypeRef cf2) {
	SecCertificatePathRef cp1 = (SecCertificatePathRef) cf1;
	SecCertificatePathRef cp2 = (SecCertificatePathRef) cf2;
	if (cp1->count != cp2->count)
		return false;
	CFIndex ix;
	for (ix = 0; ix < cp1->count; ++ix) {
		if (!CFEqual(cp1->certificates[ix], cp2->certificates[ix]))
			return false;
	}

	return true;
}

static CFHashCode SecCertificatePathHash(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
	CFHashCode hashCode = 0;
	// hashCode = 31 * SecCertificatePathGetTypeID();
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
		hashCode += CFHash(certificatePath->certificates[ix]);
	}
	return hashCode;
}

static CFStringRef SecCertificatePathCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
        CFSTR("<%@ certs: "), typeStr);
    CFRelease(typeStr);
    CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
        if (ix > 0) {
            CFStringAppend(desc, CFSTR(", "));
        }
        CFStringRef str = CFCopyDescription(certificatePath->certificates[ix]);
        CFStringAppend(desc, str);
        CFRelease(str);
	}
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}


/* Create a new certificate path from an xpc_array of data. */
SecCertificatePathRef SecCertificatePathCreateWithXPCArray(xpc_object_t xpc_path, CFErrorRef *error) {
    SecCertificatePathRef result = NULL;
    require_action_quiet(xpc_path, exit, SecError(errSecParam, error, CFSTR("xpc_path is NULL")));
    require_action_quiet(xpc_get_type(xpc_path) == XPC_TYPE_ARRAY, exit, SecError(errSecDecode, error, CFSTR("xpc_path value is not an array")));
    size_t count;
    require_action_quiet(count = xpc_array_get_count(xpc_path), exit, SecError(errSecDecode, error, CFSTR("xpc_path array count == 0")));
    size_t size = sizeof(struct SecCertificatePath) + count * sizeof(SecCertificateRef);
    require_action_quiet(result = (SecCertificatePathRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0), exit, SecError(errSecDecode, error, CFSTR("_CFRuntimeCreateInstance returned NULL")));

    result->count = count;

	size_t ix;
	for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithXPCArrayAtIndex(xpc_path, ix, error);
        if (certificate) {
            result->certificates[ix] = certificate;
        } else {
            result->count = ix; // total allocated
            CFReleaseNull(result);
            break;
        }
	}

exit:
    return result;
}

SecCertificatePathRef SecCertificatePathCreateDeserialized(CFArrayRef certificates, CFErrorRef *error) {
    SecCertificatePathRef result = NULL;
    require_action_quiet(isArray(certificates), exit,
                         SecError(errSecParam, error, CFSTR("certificates is not an array")));
    size_t count = 0;
    require_action_quiet(count = CFArrayGetCount(certificates), exit,
                         SecError(errSecDecode, error, CFSTR("certificates array count == 0")));
    size_t size = sizeof(struct SecCertificatePath) + count * sizeof(SecCertificateRef);
    require_action_quiet(result = (SecCertificatePathRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0), exit,
                         SecError(errSecDecode, error, CFSTR("_CFRuntimeCreateInstance returned NULL")));

    result->count = count;

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithData(NULL, CFArrayGetValueAtIndex(certificates, ix));
        if (certificate) {
            result->certificates[ix] = certificate;
        } else {
            result->count = ix; // total allocated
            CFReleaseNull(result);
            break;
        }
    }

exit:
    return result;
}

/* Create an array of CFDataRefs from a certificate path. */
xpc_object_t SecCertificatePathCopyXPCArray(SecCertificatePathRef path, CFErrorRef *error) {
    xpc_object_t xpc_chain = NULL;
    size_t ix, count = path->count;
    require_action_quiet(xpc_chain = xpc_array_create(NULL, 0), exit, SecError(errSecParam, error, CFSTR("xpc_array_create failed")));
	for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        if (!SecCertificateAppendToXPCArray(cert, xpc_chain, error)) {
            xpc_release(xpc_chain);
            return NULL;
        }
    }

exit:
    return xpc_chain;
}

/* Create an array of SecCertificateRefs from a certificate path. */
CFArrayRef SecCertificatePathCopyCertificates(SecCertificatePathRef path, CFErrorRef *error) {
    CFMutableArrayRef outCerts = NULL;
    size_t ix, count = path->count;
    require_action_quiet(outCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecParam, error, CFSTR("CFArray failed to create")));
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        if (cert) {
            CFArrayAppendValue(outCerts, cert);
        }
   }
exit:
    return outCerts;
}

SecCertificatePathRef SecCertificatePathCreateWithCertificates(CFArrayRef certificates, CFErrorRef *error) {
    SecCertificatePathRef result = NULL;
    require_action_quiet(isArray(certificates), exit,
                         SecError(errSecParam, error, CFSTR("certificates is not an array")));
    size_t count = 0;
    require_action_quiet(count = CFArrayGetCount(certificates), exit,
                         SecError(errSecDecode, error, CFSTR("certificates array count == 0")));
    size_t size = sizeof(struct SecCertificatePath) + count * sizeof(SecCertificateRef);
    require_action_quiet(result = (SecCertificatePathRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0), exit,
                         SecError(errSecDecode, error, CFSTR("_CFRuntimeCreateInstance returned NULL")));

    result->count = count;

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, ix);
        if (certificate) {
            result->certificates[ix] = CFRetainSafe(certificate);
        } else {
            result->count = ix; // total allocated
            CFReleaseNull(result);
            break;
        }
    }

exit:
    return result;
}

CFArrayRef SecCertificatePathCreateSerialized(SecCertificatePathRef path, CFErrorRef *error) {
    CFMutableArrayRef serializedCerts = NULL;
    require_quiet(path, exit);
    size_t ix, count = path->count;
    require_action_quiet(serializedCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecParam, error, CFSTR("CFArray failed to create")));
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        CFDataRef certData = SecCertificateCopyData(cert);
        if (certData) {
            CFArrayAppendValue(serializedCerts, certData);
            CFRelease(certData);
        }
    }
exit:
    return serializedCerts;
}

CFIndex SecCertificatePathGetCount(
	SecCertificatePathRef certificatePath) {
	check(certificatePath);
	return certificatePath ? certificatePath->count : 0;
}

SecCertificateRef SecCertificatePathGetCertificateAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix) {
	check(certificatePath && ix >= 0 && ix < certificatePath->count);
	return certificatePath->certificates[ix];
}

CFIndex SecCertificatePathGetIndexOfCertificate(SecCertificatePathRef path,
    SecCertificateRef certificate) {
    CFIndex ix, count = path->count;
	for (ix = 0; ix < count; ++ix) {
        if (CFEqual(path->certificates[ix], certificate))
            return ix;
	}
    return kCFNotFound;
}

SecKeyRef SecCertificatePathCopyPublicKeyAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix) {
	SecCertificateRef certificate =
        SecCertificatePathGetCertificateAtIndex(certificatePath, ix);
#if TARGET_OS_OSX
    return SecCertificateCopyPublicKey_ios(certificate);
#else
    return SecCertificateCopyPublicKey(certificate);
#endif
}
