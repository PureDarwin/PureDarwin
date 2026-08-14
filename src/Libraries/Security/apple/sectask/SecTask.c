/*
 * Copyright (c) 2008,2010-2013 Apple Inc. All Rights Reserved.
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

#include "SecTask.h"
#include "SecTaskPriv.h"

#include <utilities/debugging.h>
#include <utilities/entitlements.h>

#include <AssertMacros.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <System/sys/codesign.h>
#include <bsm/libbsm.h>
#include <inttypes.h>
#include <syslog.h>
#include <utilities/SecCFWrappers.h>
#include <xpc/private.h>

#include <sys/sysctl.h>

#if TARGET_OS_OSX
/* These won't exist until we unify codesigning */
#include <Security/SecCode.h>
#include <Security/SecCodePriv.h>
#include <Security/SecRequirement.h>
#endif /* TARGET_OS_OSX */

struct __SecTask {
    CFRuntimeBase base;
    
    audit_token_t token;
    
    /* Track whether we've loaded entitlements independently since after the
     * load, entitlements may legitimately be NULL */
    Boolean entitlementsLoaded;
    CFDictionaryRef entitlements;
	
	/* for debugging only, shown by debugDescription */
	int lastFailure;
};

static bool check_task(SecTaskRef task) {
    return SecTaskGetTypeID() == CFGetTypeID(task);
}

static void SecTaskFinalize(CFTypeRef cfTask)
{
    SecTaskRef task = (SecTaskRef) cfTask;
    CFReleaseNull(task->entitlements);
}


// Define PRIdPID (proper printf format string for pid_t)
#define PRIdPID PRId32

static CFStringRef SecTaskCopyDebugDescription(CFTypeRef cfTask)
{
    SecTaskRef task = (SecTaskRef) cfTask;
    pid_t pid;
    audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);

    const char *task_name;
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
        task_name = strerror(errno);
    else
        task_name = kp.kp_proc.p_comm;

    return CFStringCreateWithFormat(CFGetAllocator(task), NULL, CFSTR("%s[%" PRIdPID "]/%d#%d LF=%d"), task_name, pid,
									task->entitlementsLoaded, task->entitlements ? (int)CFDictionaryGetCount(task->entitlements) : -1, task->lastFailure);
}

CFGiblisWithFunctions(SecTask, NULL, NULL, SecTaskFinalize, NULL, NULL, NULL, SecTaskCopyDebugDescription, NULL, NULL, NULL)

static SecTaskRef init_task_ref(CFAllocatorRef allocator)
{
    CFIndex extra = sizeof(struct __SecTask) - sizeof(CFRuntimeBase);
    return (SecTaskRef) _CFRuntimeCreateInstance(allocator, SecTaskGetTypeID(), extra, NULL);
}

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator)
{
    SecTaskRef task = init_task_ref(allocator);
    if (task != NULL) {

        kern_return_t kr = KERN_FAILURE;
        mach_msg_type_number_t autoken_cnt = TASK_AUDIT_TOKEN_COUNT;
        kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)&task->token, &autoken_cnt);
        if (kr == KERN_SUCCESS) {
            task->entitlementsLoaded = false;
            task->entitlements = NULL;
        } else {
            CFReleaseNull(task);
        }
    }

    return task;
}

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef allocator, audit_token_t token)
{
    SecTaskRef task = init_task_ref(allocator);
    if (task != NULL) {
        
        memcpy(&task->token, &token, sizeof(token));
        task->entitlementsLoaded = false;
        task->entitlements = NULL;
    }
    
    return task;
}

_Nullable SecTaskRef
SecTaskCreateWithXPCMessage(xpc_object_t _Nonnull message)
{
    audit_token_t token;

    if (message == NULL || xpc_get_type(message) != XPC_TYPE_DICTIONARY) {
        return NULL;
    }
    xpc_dictionary_get_audit_token(message, &token);

    return SecTaskCreateWithAuditToken(NULL, token);
}



struct csheader {
        uint32_t magic;
        uint32_t length;
};

static int
csops_task(SecTaskRef task, int ops, void *blob, size_t size)
{
	int rc;
    
    pid_t pid;
    audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
    rc = csops_audittoken(pid, ops, blob, size, &task->token);

	task->lastFailure = (rc == -1) ? errno : 0;
	return rc;
}

static CFStringRef
SecTaskCopyIdentifier(SecTaskRef task, int op, CFErrorRef *error)
{
        CFStringRef signingId = NULL;
        char *data = NULL;
        struct csheader header;
        uint32_t bufferlen;
        int ret;

        ret = csops_task(task, op, &header, sizeof(header));
        if (ret != -1 || errno != ERANGE)
                return NULL;

        bufferlen = ntohl(header.length);
        /* check for insane values */
        if (bufferlen > 1024 * 1024 || bufferlen < 8) {
                ret = EINVAL;
                goto out;
        }
        data = malloc(bufferlen + 1);
        if (data == NULL) {
                ret = ENOMEM;
                goto out;
        }
        ret = csops_task(task, op, data, bufferlen);
        if (ret) {
                ret = errno;
                goto out;
        }
        data[bufferlen] = '\0';

        signingId = CFStringCreateWithCString(NULL, data + 8, kCFStringEncodingUTF8);

 out:
        if (data)
                free(data);
        if (ret && error)
                *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ret, NULL);

        return signingId;
}

CFStringRef
SecTaskCopySigningIdentifier(SecTaskRef task, CFErrorRef *error)
{
    return SecTaskCopyIdentifier(task, CS_OPS_IDENTITY, error);
}

CFStringRef
SecTaskCopyTeamIdentifier(SecTaskRef task, CFErrorRef *error)
{
    return SecTaskCopyIdentifier(task, CS_OPS_TEAMID, error);
}

uint32_t
SecTaskGetCodeSignStatus(SecTaskRef task)
{
    uint32_t flags = 0;
    if (csops_task(task, CS_OPS_STATUS, &flags, sizeof(flags)) != 0)
        return 0;
    return flags;
}

static bool SecTaskLoadEntitlements(SecTaskRef task, CFErrorRef *error)
{
    CFMutableDictionaryRef entitlements = NULL;
    struct csheader header;
    uint8_t *buffer = NULL;
    uint32_t bufferlen;
    int ret;

    ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header));
    /* Any other combination means no entitlements */
    if (ret == -1) {
        if (errno != ERANGE) {
            int entitlementErrno = errno;

            uint32_t cs_flags = -1;
            if (-1 == csops_task(task, CS_OPS_STATUS, &cs_flags, sizeof(cs_flags))) {
                syslog(LOG_NOTICE, "Failed to get cs_flags, error=%d", errno);
            }

            if (cs_flags != 0) {	// was signed
                pid_t pid;
                audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
                syslog(LOG_NOTICE, "SecTaskLoadEntitlements failed error=%d cs_flags=%x, pid=%d", entitlementErrno, cs_flags, pid);	// to ease diagnostics

                CFStringRef description = SecTaskCopyDebugDescription(task);
                char *descriptionBuf = NULL;
                CFIndex descriptionSize = CFStringGetLength(description) * 4;
                descriptionBuf = (char *)malloc(descriptionSize);
                if (!CFStringGetCString(description, descriptionBuf, descriptionSize, kCFStringEncodingUTF8)) {
                    descriptionBuf[0] = 0;
                }

                syslog(LOG_NOTICE, "SecTaskCopyDebugDescription: %s", descriptionBuf);
                CFReleaseNull(description);
                free(descriptionBuf);
            }
            task->lastFailure = entitlementErrno;	// was overwritten by csops_task(CS_OPS_STATUS) above

            // EINVAL is what the kernel says for unsigned code, so we'll have to let that pass
            if (entitlementErrno == EINVAL) {
                task->entitlementsLoaded = true;
                return true;
            }
            ret = entitlementErrno;	// what really went wrong
            goto out;		// bail out
        }
        bufferlen = ntohl(header.length);
        /* check for insane values */
        if (bufferlen > 1024 * 1024 || bufferlen < 8) {
            ret = E2BIG;
            goto out;
        }
        buffer = malloc(bufferlen);
        if (buffer == NULL) {
            ret = ENOMEM;
            goto out;
        }
        ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, buffer, bufferlen);
        if (ret) {
            ret = errno;
            goto out;
        }

        CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer+8, bufferlen-8, kCFAllocatorNull);
        entitlements = (CFMutableDictionaryRef) CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListMutableContainers, NULL, error);
        CFReleaseNull(data);

        if ((entitlements==NULL) || (CFGetTypeID(entitlements)!=CFDictionaryGetTypeID())){
            ret = EDOM;	// don't use EINVAL here; it conflates problems with syscall error returns
            goto out;
        }

        bool entitlementsModified = updateCatalystEntitlements(entitlements);
        if (entitlementsModified) {
            pid_t pid;
            audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
            secinfo("SecTask", "Fixed catalyst entitlements for process %d", pid);
        }
    }

    task->entitlements = entitlements ? CFRetain(entitlements) : NULL;
    task->entitlementsLoaded = true;

out:
    CFReleaseNull(entitlements);
    if(buffer)
        free(buffer);
    if (ret && error && *error==NULL)
        *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ret, NULL);
    return ret == 0;
}


CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error)
{
    CFTypeRef value = NULL;
    require(check_task(task), out);

    /* Load entitlements if necessary */
    if (task->entitlementsLoaded == false) {
        require_quiet(SecTaskLoadEntitlements(task, error), out);
    }

    if (task->entitlements != NULL) {
        value = CFDictionaryGetValue(task->entitlements, entitlement);
        
        /* Return something the caller must release */
        if (value != NULL) {
            CFRetain(value);
        }
    }
out:
    return value;
}

CFDictionaryRef SecTaskCopyValuesForEntitlements(SecTaskRef task, CFArrayRef entitlements, CFErrorRef *error)
{
    CFMutableDictionaryRef values = NULL;
    require(check_task(task), out);

    /* Load entitlements if necessary */
    if (task->entitlementsLoaded == false) {
        SecTaskLoadEntitlements(task, error);
    }
    
    /* Iterate over the passed in entitlements, populating the dictionary
     * If entitlements were loaded but none were present, return an empty
     * dictionary */
    if (task->entitlementsLoaded == true) {
        
        CFIndex i, count = CFArrayGetCount(entitlements);
        values = CFDictionaryCreateMutable(CFGetAllocator(task), count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (task->entitlements != NULL) {
            for (i = 0; i < count; i++) {
                CFStringRef entitlement = CFArrayGetValueAtIndex(entitlements, i);
                CFTypeRef value = CFDictionaryGetValue(task->entitlements, entitlement);
                if (value != NULL) {
                    CFDictionarySetValue(values, entitlement, value);
                }
            }
        }
    }
out:
    return values;
}

#if SEC_OS_OSX
/*
 * Determine if the given task meets a specified requirement.
 */
OSStatus
SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement)
{
    OSStatus status;
    SecCodeRef code = NULL;
    SecRequirementRef req = NULL;

    CFMutableDictionaryRef codeDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef auditData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)&task->token, sizeof(audit_token_t));
    CFDictionarySetValue(codeDict, kSecGuestAttributeAudit, auditData);
    status = SecCodeCopyGuestWithAttributes(NULL, codeDict, kSecCSDefaultFlags, &code);
    CFReleaseNull(codeDict);
    CFReleaseNull(auditData);

    if (!status) {
        status = SecRequirementCreateWithString(requirement,
                                                kSecCSDefaultFlags, &req);
    }
    if (!status) {
        status = SecCodeCheckValidity(code, kSecCSDefaultFlags, req);
    }

    CFReleaseNull(req);
    CFReleaseNull(code);

    return status;
}
#endif /* SEC_OS_OSX */

Boolean SecTaskEntitlementsValidated(SecTaskRef task) {
    // TODO: Cache the result
    uint32_t csflags = 0;
    const uint32_t mask = CS_VALID | CS_KILL | CS_ENTITLEMENTS_VALIDATED;
	const uint32_t debug_mask = CS_DEBUGGED | CS_ENTITLEMENTS_VALIDATED;
    int rc = csops_task(task, CS_OPS_STATUS, &csflags, sizeof(csflags));
	// Allow debugged processes that were valid to continue being treated as valid
	// We need this all the time (not just on internal) because third parties may need to debug their entitled process in xcode
    return (rc != -1) && ((mask & csflags) == mask || (debug_mask & csflags) == debug_mask);
}

