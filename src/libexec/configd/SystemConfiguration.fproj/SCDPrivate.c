/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#define SC_LOG_HANDLE	_SC_LOG_DEFAULT
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dlfcn.h>
#include <mach-o/dyld_priv.h>

#include <dispatch/dispatch.h>

#include <mach/mach.h>
#include <mach/notify.h>
#include <mach/mach_error.h>
#include <pthread.h>

#include <execinfo.h>
#include <unistd.h>

#if	TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#include <CoreFoundation/CFUserNotification.h>
#endif	// TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR

/* CrashReporter "Application Specific Information" */
#include <CrashReporterClient.h>

#define	N_QUICK	32


#pragma mark -
#pragma mark Miscellaneous


__private_extern__ char *
_SC_cfstring_to_cstring_ext(CFStringRef cfstr, char *buf, CFIndex bufLen, CFStringEncoding encoding, UInt8 lossByte, CFIndex *usedBufLen)
{
	CFIndex	converted;
	CFIndex	last	= 0;
	CFIndex	len;

	if (cfstr == NULL) {
		cfstr = CFSTR("");
	}
	len = CFStringGetLength(cfstr);

	/* how much buffer space will we really need? */
	converted = CFStringGetBytes(cfstr,
				     CFRangeMake(0, len),
				     encoding,
				     lossByte,
				     FALSE,
				     NULL,
				     0,
				     &last);
	if (converted < len) {
		/* if full string could not be converted */
		if (buf != NULL) {
			buf[0] = '\0';
		}
		return NULL;
	}

	if (buf != NULL) {
		if (bufLen < (last + 1)) {
			/* if the size of the provided buffer is too small */
			buf[0] = '\0';
			return NULL;
		}
	} else {
		/* allocate a buffer */
		bufLen = last + 1;
		buf = CFAllocatorAllocate(NULL, bufLen, 0);
		if (buf == NULL) {
			return NULL;
		}
	}

	(void)CFStringGetBytes(cfstr,
			       CFRangeMake(0, len),
			       encoding,
			       lossByte,
			       FALSE,
			       (UInt8 *)buf,
			       bufLen,
			       &last);
	buf[last] = '\0';

	if (usedBufLen != NULL) {
		*usedBufLen = last;
	}

	return buf;
}


char *
_SC_cfstring_to_cstring(CFStringRef cfstr, char *buf, CFIndex bufLen, CFStringEncoding encoding)
{
	return _SC_cfstring_to_cstring_ext(cfstr, buf, bufLen, encoding, 0, NULL);

}


void
_SC_sockaddr_to_string(const struct sockaddr *address, char *buf, size_t bufLen)
{
	union {
		const struct sockaddr		*sa;
		const struct sockaddr_in	*sin;
		const struct sockaddr_in6	*sin6;
		const struct sockaddr_dl	*sdl;
	} addr;

	addr.sa = address;

	memset(buf, 0, bufLen);
	switch (address->sa_family) {
		case AF_INET :
			(void)inet_ntop(addr.sin->sin_family,
					&addr.sin->sin_addr,
					buf,
					(socklen_t)bufLen);
			break;
		case AF_INET6 : {
			(void)inet_ntop(addr.sin6->sin6_family,
					&addr.sin6->sin6_addr,
					buf,
					(socklen_t)bufLen);
			if (addr.sin6->sin6_scope_id != 0) {
				size_t	n;

				n = strlen(buf);
				if ((n+IF_NAMESIZE+1) <= bufLen) {
					buf[n++] = '%';
					if_indextoname(addr.sin6->sin6_scope_id, &buf[n]);
				}
			}
			break;
		}
		default :
			snprintf(buf, bufLen, "unexpected address family %d", address->sa_family);
			break;
	}

	return;
}


struct sockaddr *
_SC_string_to_sockaddr(const char *str, sa_family_t af, void *buf, size_t bufLen)
{
	union {
		void			*buf;
		struct sockaddr		*sa;
		struct sockaddr_in	*sin;
		struct sockaddr_in6	*sin6;
	} addr;

	if (buf == NULL) {
		bufLen = sizeof(struct sockaddr_storage);
		addr.buf = CFAllocatorAllocate(NULL, bufLen, 0);
	} else {
		addr.buf = buf;
	}

	memset(addr.buf, 0, bufLen);
	if (((af == AF_UNSPEC) || (af == AF_INET)) &&
	    (bufLen >= sizeof(struct sockaddr_in)) &&
	    inet_aton(str, &addr.sin->sin_addr) == 1) {
		// if IPv4 address
		addr.sin->sin_len    = sizeof(struct sockaddr_in);
		addr.sin->sin_family = AF_INET;
	} else if (((af == AF_UNSPEC) || (af == AF_INET6)) &&
		   (bufLen >= sizeof(struct sockaddr_in6)) &&
		   inet_pton(AF_INET6, str, &addr.sin6->sin6_addr) == 1) {
		// if IPv6 address
		char	*p;

		addr.sin6->sin6_len    = sizeof(struct sockaddr_in6);
		addr.sin6->sin6_family = AF_INET6;

		p = strchr(str, '%');
		if (p != NULL) {
			addr.sin6->sin6_scope_id = if_nametoindex(p + 1);
		}

		if (IN6_IS_ADDR_LINKLOCAL(&addr.sin6->sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&addr.sin6->sin6_addr)) {
			uint16_t	if_index;

			if_index = ntohs(addr.sin6->sin6_addr.__u6_addr.__u6_addr16[1]);
			addr.sin6->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
			if (addr.sin6->sin6_scope_id == 0) {
				// use the scope id that was embedded in the [link local] IPv6 address
				addr.sin6->sin6_scope_id = if_index;
			}
		}
	} else {
		if (addr.buf != buf) {
			CFAllocatorDeallocate(NULL, addr.buf);
		}
		addr.buf = NULL;
	}

	return addr.sa;
}


void *
_SC_dlopen(const char *framework)
{
	void			*image;
	static dispatch_once_t	once;
	static const char	*suffix	= NULL;

	dispatch_once(&once, ^{
		if (!dyld_process_is_restricted()) {
			suffix = getenv("DYLD_IMAGE_SUFFIX");
			if ((suffix != NULL) &&
			    ((strlen(suffix) < 2) || (suffix[0] != '_'))) {
				// if too short or no leading "_"
				suffix = NULL;
			}
		}
	});

	if (suffix != NULL) {
		struct stat	statbuf;
		char		path[MAXPATHLEN];

		strlcpy(path, framework, sizeof(path));
		strlcat(path, suffix, sizeof(path));
		if (0 <= stat(path, &statbuf)) {
			image = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
			return image;
		}
	}

	image = dlopen(framework, RTLD_LAZY | RTLD_LOCAL);
	return image;
}


void
_SC_sendMachMessage(mach_port_t port, mach_msg_id_t msg_id)
{
	mach_msg_empty_send_t	msg;
	mach_msg_option_t	options;
	kern_return_t		status;

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_id = msg_id;
	options = MACH_SEND_TIMEOUT;
	status = mach_msg(&msg.header,			/* msg */
			  MACH_SEND_MSG|options,	/* options */
			  msg.header.msgh_size,		/* send_size */
			  0,				/* rcv_size */
			  MACH_PORT_NULL,		/* rcv_name */
			  0,				/* timeout */
			  MACH_PORT_NULL);		/* notify */
	if ((status == MACH_SEND_INVALID_DEST) || (status == MACH_SEND_TIMED_OUT)) {
		mach_msg_destroy(&msg.header);
	}

	return;
}


CFStringRef
_SC_trimDomain(CFStringRef domain)
{
	CFIndex	length;

	if (!isA_CFString(domain)) {
		return NULL;
	}

	// remove any leading/trailing dots
	length = CFStringGetLength(domain);
	if ((length > 0) &&
	    (CFStringFindWithOptions(domain,
				     CFSTR("."),
				     CFRangeMake(0, 1),
				     kCFCompareAnchored,
				     NULL) ||
	     CFStringFindWithOptions(domain,
				     CFSTR("."),
				     CFRangeMake(0, length),
				     kCFCompareAnchored|kCFCompareBackwards,
				     NULL))
	    ) {
	     CFMutableStringRef	trimmed;

	     trimmed = CFStringCreateMutableCopy(NULL, 0, domain);
	     CFStringTrim(trimmed, CFSTR("."));
	     domain = (CFStringRef)trimmed;
	     length = CFStringGetLength(domain);
	} else {
	     CFRetain(domain);
	}

	if (length == 0) {
		CFRelease(domain);
		domain = NULL;
	}

	return domain;
}


CFStringRef
_SC_hw_model(Boolean trim)
{
	static CFStringRef	model		= NULL;
	static CFStringRef	model_trimmed	= NULL;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		char	*cp;
		char	hwModel[64];
		int	mib[]		= { CTL_HW, HW_MODEL };
		size_t	n		= sizeof(hwModel);
		int	ret;

		// get HW model name
		memset(&hwModel, 0, sizeof(hwModel));
		ret = sysctl(mib, sizeof(mib) / sizeof(mib[0]), &hwModel, &n, NULL, 0);
		if (ret != 0) {
			SC_log(LOG_NOTICE, "sysctl() CTL_HW/HW_MODEL failed: %s", strerror(errno));
			return;
		}
		hwModel[sizeof(hwModel) - 1] = '\0';
		model = CFStringCreateWithCString(NULL, hwModel, kCFStringEncodingASCII);

		// and the "trimmed" name
		// ... remove everything after (and including) a comma
		// ... and then any trailing digits
		cp = index(hwModel, ',');
		if (cp != NULL) {
			*cp = '\0';
		}
		n = strlen(hwModel) - 1;
		while (n > 0) {
			if (!isdigit(hwModel[n])) {
				break;
			}
			hwModel[n--] = '\0';
		}
		model_trimmed = CFStringCreateWithCString(NULL, hwModel, kCFStringEncodingASCII);
	});

	return trim ? model_trimmed : model;
}


#pragma mark -
#pragma mark Serialization


static kern_return_t
__CFDataCopyVMData(CFDataRef data, const void **dataRef, CFIndex *dataLen)
{
	kern_return_t		kr;

	vm_address_t	vm_address;
	vm_size_t	vm_size;

	vm_address = (vm_address_t)(const void *)CFDataGetBytePtr(data);
	vm_size    = (vm_size_t)CFDataGetLength(data);
	kr = vm_allocate(mach_task_self(), &vm_address, vm_size, VM_FLAGS_ANYWHERE);
	if (kr != KERN_SUCCESS) {
		*dataRef = NULL;
		*dataLen = 0;
		return kr;
	}

	memcpy((void *)vm_address, CFDataGetBytePtr(data), vm_size);
	*dataRef = (void *)vm_address;
	*dataLen = vm_size;

	return kr;
}


Boolean
_SCSerialize(CFPropertyListRef obj, CFDataRef *xml, const void **dataRef, CFIndex *dataLen)
{
	CFDataRef		myXml;

	if ((xml == NULL) && ((dataRef == NULL) || (dataLen == NULL))) {
		/* if not keeping track of allocated space */
		return FALSE;
	}

	myXml = CFPropertyListCreateData(NULL,
					 obj,
					 kCFPropertyListBinaryFormat_v1_0,
					 0,
					 NULL);
	if (myXml == NULL) {
		SC_log(LOG_NOTICE, "CFPropertyListCreateData() failed");
		if (xml != NULL) {
			*xml = NULL;
		}
		if ((dataRef != NULL) && (dataLen != NULL)) {
			*dataLen = 0;
			*dataRef = NULL;
		}
		return FALSE;
	}

	if (xml != NULL) {
		*xml = myXml;
		if ((dataRef != NULL) && (dataLen != NULL)) {
			*dataRef = CFDataGetBytePtr(myXml);
			*dataLen = CFDataGetLength(myXml);
		}
	} else {
		kern_return_t	kr;

		kr = __CFDataCopyVMData(myXml, dataRef, dataLen);
		CFRelease(myXml);
		if (kr != KERN_SUCCESS) {
			SC_log(LOG_NOTICE, "__CFDataCreateVMData() failed: %s", mach_error_string(kr));
			return FALSE;
		}
	}

	return TRUE;
}


Boolean
_SCUnserialize(CFPropertyListRef *obj, CFDataRef xml, const void *dataRef, CFIndex dataLen)
{
	CFErrorRef	error	= NULL;

	if (xml == NULL) {
		kern_return_t	status;

		xml = CFDataCreateWithBytesNoCopy(NULL, dataRef, dataLen, kCFAllocatorNull);
		*obj = CFPropertyListCreateWithData(NULL, xml, kCFPropertyListImmutable, NULL, &error);
		CFRelease(xml);

		status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_NOTICE, "vm_deallocate() failed: %s", mach_error_string(status));
			/* non-fatal???, proceed */
		}
	} else {
		*obj = CFPropertyListCreateWithData(NULL, xml, kCFPropertyListImmutable, NULL, &error);
	}

	if (*obj == NULL) {
		if (error != NULL) {
			SC_log(LOG_NOTICE, "CFPropertyListCreateWithData() failed: %@", error);
			CFRelease(error);
		}
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


Boolean
_SCSerializeString(CFStringRef str, CFDataRef *data, const void **dataRef, CFIndex *dataLen)
{
	CFDataRef	myData;

	if (!isA_CFString(str)) {
		/* if not a CFString */
		return FALSE;
	}

	if ((data == NULL) && ((dataRef == NULL) || (dataLen == NULL))) {
		/* if not keeping track of allocated space */
		return FALSE;
	}

	myData = CFStringCreateExternalRepresentation(NULL, str, kCFStringEncodingUTF8, 0);
	if (myData == NULL) {
		SC_log(LOG_NOTICE, "CFStringCreateExternalRepresentation() failed");
		if (data != NULL) {
			*data = NULL;
		}
		if ((dataRef != NULL) && (dataLen != NULL)) {
			*dataRef = NULL;
			*dataLen = 0;
		}
		return FALSE;
	}

	if (data != NULL) {
		*data = myData;
		if ((dataRef != NULL) && (dataLen != NULL)) {
			*dataRef = CFDataGetBytePtr(myData);
			*dataLen = CFDataGetLength(myData);
		}
	} else {
		kern_return_t	kr;

		kr = __CFDataCopyVMData(myData, dataRef, dataLen);
		CFRelease(myData);
		if (kr != KERN_SUCCESS) {
			SC_log(LOG_NOTICE, "__CFDataCreateVMData() failed: %s", mach_error_string(kr));
			return FALSE;
		}
	}

	return TRUE;
}


Boolean
_SCUnserializeString(CFStringRef *str, CFDataRef utf8, const void *dataRef, CFIndex dataLen)
{
	if (utf8 == NULL) {
		kern_return_t	status;

		utf8 = CFDataCreateWithBytesNoCopy(NULL, dataRef, dataLen, kCFAllocatorNull);
		*str = CFStringCreateFromExternalRepresentation(NULL, utf8, kCFStringEncodingUTF8);
		CFRelease(utf8);

		status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_NOTICE, "vm_deallocate() failed: %s", mach_error_string(status));
			/* non-fatal???, proceed */
		}
	} else {
		*str = CFStringCreateFromExternalRepresentation(NULL, utf8, kCFStringEncodingUTF8);
	}

	if (*str == NULL) {
		SC_log(LOG_NOTICE, "CFStringCreateFromExternalRepresentation() failed");
		return FALSE;
	}

	return TRUE;
}


Boolean
_SCSerializeData(CFDataRef data, const void **dataRef, CFIndex *dataLen)
{
	kern_return_t	kr;

	if (!isA_CFData(data)) {
		/* if not a CFData */
		return FALSE;
	}

	kr = __CFDataCopyVMData(data, dataRef, dataLen);
	if (kr != KERN_SUCCESS) {
		SC_log(LOG_NOTICE, "__CFDataCreateVMData() failed: %s", mach_error_string(kr));
		return FALSE;
	}

	return TRUE;
}


Boolean
_SCUnserializeData(CFDataRef *data, const void *dataRef, CFIndex dataLen)
{
	kern_return_t		status;

	*data = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SC_log(LOG_NOTICE, "vm_deallocate() failed: %s", mach_error_string(status));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


CF_RETURNS_RETAINED CFDictionaryRef
_SCSerializeMultiple(CFDictionaryRef dict)
{
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			nElements;
	CFDictionaryRef		newDict		= NULL;
	const void *		pLists_q[N_QUICK];
	const void **		pLists		= pLists_q;
	const void *		values_q[N_QUICK];
	const void **		values		= values_q;

	nElements = CFDictionaryGetCount(dict);
	if (nElements > 0) {
		if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			pLists = CFAllocatorAllocate(NULL, nElements * sizeof(CFDataRef), 0);
		}
		memset(pLists, 0, nElements * sizeof(CFDataRef));

		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i = 0; i < nElements; i++) {
			pLists[i] = NULL;
			if (!_SCSerialize((CFPropertyListRef)values[i], (CFDataRef *)&pLists[i], NULL, NULL)) {
				goto done;
			}
		}
	}

	newDict = CFDictionaryCreate(NULL,
				     keys,
				     pLists,
				     nElements,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    done :

	if (nElements > 0) {
		for (i = 0; i < nElements; i++) {
			if (pLists[i] != NULL)	CFRelease((CFDataRef)pLists[i]);
		}

		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
			CFAllocatorDeallocate(NULL, pLists);
		}
	}

	return newDict;
}


CF_RETURNS_RETAINED
CFDictionaryRef
_SCUnserializeMultiple(CFDictionaryRef dict)
{
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			nElements;
	CFDictionaryRef		newDict		= NULL;
	const void *		pLists_q[N_QUICK];
	const void **		pLists		= pLists_q;
	const void *		values_q[N_QUICK];
	const void **		values		= values_q;

	nElements = CFDictionaryGetCount(dict);
	if (nElements > 0) {
		CFIndex	i;

		if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			pLists = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		}
		memset(pLists, 0, nElements * sizeof(CFTypeRef));

		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i = 0; i < nElements; i++) {
			if (!_SCUnserialize((CFPropertyListRef *)&pLists[i], values[i], NULL, 0)) {
				goto done;
			}
		}
	}

	newDict = CFDictionaryCreate(NULL,
				     keys,
				     pLists,
				     nElements,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    done :

	if (nElements > 0) {
		CFIndex	i;

		for (i = 0; i < nElements; i++) {
			if (pLists[i])	CFRelease(pLists[i]);
		}

		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
			CFAllocatorDeallocate(NULL, pLists);
		}
	}

	return newDict;
}


#pragma mark -


CFPropertyListRef
_SCCreatePropertyListFromResource(CFURLRef url)
{
	CFDictionaryRef	dict		= NULL;
	SInt64		fileLen		= 0;
	Boolean		ok;
	CFReadStreamRef readStream;
	CFNumberRef	val		= NULL;

	if (!CFURLCopyResourcePropertyForKey(url, kCFURLFileSizeKey, &val, NULL) ||
	    (val == NULL)) {
		// if size not available
		SC_log(LOG_NOTICE, "CFURLCopyResourcePropertyForKey() size not available: %@", url);
		return NULL;
	}

	ok = CFNumberGetValue(val, kCFNumberSInt64Type, &fileLen);
	CFRelease(val);
	if (!ok || (fileLen == 0)) {
		// if empty or size too large
		SC_log(LOG_INFO, "_SCCreatePropertyListFromResource() improper size: %@", url);
		return NULL;
	}

	readStream = CFReadStreamCreateWithFile(NULL, url);
	if (readStream != NULL) {
		if (CFReadStreamOpen(readStream)) {
			UInt8		*buffer;
			CFIndex		dataLen;

			buffer = CFAllocatorAllocate(NULL, (CFIndex)fileLen, 0);
			dataLen = CFReadStreamRead(readStream, buffer, (CFIndex)fileLen);
			if (dataLen == (CFIndex)fileLen) {
				CFDataRef	data;

				data = CFDataCreateWithBytesNoCopy(NULL, buffer, (CFIndex)fileLen, kCFAllocatorNull);
				if (data != NULL) {
					dict = CFPropertyListCreateWithData(NULL,
									    data,
									    kCFPropertyListImmutable,
									    NULL,
									    NULL);
					CFRelease(data);
				}
			}
			CFAllocatorDeallocate(NULL, buffer);
			CFReadStreamClose(readStream);
		}
		CFRelease(readStream);
	}

	return dict;
}


#pragma mark -
#pragma mark CFRunLoop scheduling


__private_extern__ void
_SC_signalRunLoop(CFTypeRef obj, CFRunLoopSourceRef rls, CFArrayRef rlList)
{
	CFRunLoopRef	rl	= NULL;
	CFRunLoopRef	rl1	= NULL;
	CFIndex		i;
	CFIndex		n	= CFArrayGetCount(rlList);

	if (n == 0) {
		return;
	}

	/* get first runLoop for this object */
	for (i = 0; i < n; i += 3) {
		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl1 = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		break;
	}

	if (rl1 == NULL) {
		/* if not scheduled */
		return;
	}

	/* check if we have another runLoop for this object */
	rl = rl1;
	for (i = i+3; i < n; i += 3) {
		CFRunLoopRef	rl2;

		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl2 = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		if (!CFEqual(rl1, rl2)) {
			/* we've got more than one runLoop */
			rl = NULL;
			break;
		}
	}

	if (rl != NULL) {
		/* if we only have one runLoop */
		CFRunLoopWakeUp(rl);
		return;
	}

	/* more than one different runLoop, so we must pick one */
	for (i = 0; i < n; i+=3) {
		CFStringRef	rlMode;

		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl     = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		rlMode = CFRunLoopCopyCurrentMode(rl);
		if (rlMode != NULL) {
			Boolean	waiting;

			waiting = (CFRunLoopIsWaiting(rl) && CFRunLoopContainsSource(rl, rls, rlMode));
			CFRelease(rlMode);
			if (waiting) {
				/* we've found a runLoop that's "ready" */
				CFRunLoopWakeUp(rl);
				return;
			}
		}
	}

	/* didn't choose one above, so choose first */
	CFRunLoopWakeUp(rl1);
	return;
}


__private_extern__ Boolean
_SC_isScheduled(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFIndex	i;
	CFIndex	n	= CFArrayGetCount(rlList);

	for (i = 0; i < n; i += 3) {
		if ((obj != NULL)         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}
		if ((runLoop != NULL)     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			continue;
		}
		if ((runLoopMode != NULL) && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			continue;
		}
		return TRUE;
	}

	return FALSE;
}


__private_extern__ void
_SC_schedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFArrayAppendValue(rlList, obj);
	CFArrayAppendValue(rlList, runLoop);
	CFArrayAppendValue(rlList, runLoopMode);

	return;
}


__private_extern__ Boolean
_SC_unschedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList, Boolean all)
{
	CFIndex	i	= 0;
	Boolean	found	= FALSE;
	CFIndex	n	= CFArrayGetCount(rlList);

	while (i < n) {
		if ((obj != NULL)         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			i += 3;
			continue;
		}
		if ((runLoop != NULL)     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			i += 3;
			continue;
		}
		if ((runLoopMode != NULL) && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			i += 3;
			continue;
		}

		found = TRUE;

		CFArrayRemoveValueAtIndex(rlList, i + 2);
		CFArrayRemoveValueAtIndex(rlList, i + 1);
		CFArrayRemoveValueAtIndex(rlList, i);

		if (!all) {
			return found;
		}

		n -= 3;
	}

	return found;
}


#pragma mark -
#pragma mark Bundle


CFStringRef
_SC_getApplicationBundleID(void)
{
	static CFStringRef	bundleID	= NULL;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		CFBundleRef	bundle;

		bundle = CFBundleGetMainBundle();
		if (bundle != NULL) {
			bundleID = CFBundleGetIdentifier(bundle);
			if (bundleID != NULL) {
				CFRetain(bundleID);
			} else {
				CFURLRef	url;

				url = CFBundleCopyExecutableURL(bundle);
				if (url != NULL) {
					bundleID = CFURLCopyPath(url);
					CFRelease(url);
				}
			}

			if (bundleID != NULL) {
				if (CFEqual(bundleID, CFSTR("/"))) {
					CFRelease(bundleID);
					bundleID = NULL;
				}
			}
		}
		if (bundleID == NULL) {
			bundleID = CFStringCreateWithFormat(NULL, NULL, CFSTR("Unknown(%d)"), getpid());
		}
	});

	return bundleID;
}


#define SYSTEMCONFIGURATION_BUNDLE_ID		CFSTR("com.apple.SystemConfiguration")
#define	SYSTEMCONFIGURATION_FRAMEWORK_PATH_LEN	(sizeof(SYSTEMCONFIGURATION_FRAMEWORK_PATH) - 1)

#define	SUFFIX_SYM				"~sym"
#define	SUFFIX_SYM_LEN				(sizeof(SUFFIX_SYM) - 1)

#define	SUFFIX_DST				"~dst"


CFBundleRef
_SC_CFBundleGet(void)
{
	static CFBundleRef	bundle	= NULL;
	char			*env;
	size_t			len;
	CFURLRef		url;

	if (bundle != NULL) {
		goto done;
	}

	bundle = CFBundleGetBundleWithIdentifier(SYSTEMCONFIGURATION_BUNDLE_ID);
	if (bundle != NULL) {
		CFRetain(bundle);	// we want to hold a reference to the bundle
		goto done;
	} else {
		SC_log(LOG_NOTICE, "could not get CFBundle for \"%@\". Trying harder...",
		       SYSTEMCONFIGURATION_BUNDLE_ID);
	}

	// if appropriate (e.g. when debugging), try a bit harder

	env = getenv("DYLD_FRAMEWORK_PATH");
	len = (env != NULL) ? strlen(env) : 0;

	if (len > 0) {	/* We are debugging */

		// trim any trailing slashes
		while (len > 1) {
			if (env[len - 1] != '/') {
				break;
			}
			len--;
		}

		// if DYLD_FRAMEWORK_PATH is ".../xxx~sym" than try ".../xxx~dst"
		if ((len > SUFFIX_SYM_LEN) &&
		    (strncmp(&env[len - SUFFIX_SYM_LEN], SUFFIX_SYM, SUFFIX_SYM_LEN) == 0) &&
		    ((len + SYSTEMCONFIGURATION_FRAMEWORK_PATH_LEN) < MAXPATHLEN)) {
			char		path[MAXPATHLEN];

			strlcpy(path, env, sizeof(path));
			strlcpy(&path[len - SUFFIX_SYM_LEN], SUFFIX_DST, sizeof(path) - (len - SUFFIX_SYM_LEN));
			strlcat(&path[len], SYSTEMCONFIGURATION_FRAMEWORK_PATH, sizeof(path) - len);

			url = CFURLCreateFromFileSystemRepresentation(NULL,
								      (UInt8 *)path,
								      len + SYSTEMCONFIGURATION_FRAMEWORK_PATH_LEN,
								      TRUE);
			bundle = CFBundleCreate(NULL, url);
			CFRelease(url);
		}
	}

	if (bundle == NULL) { /* Try a more "direct" route to get the bundle */

		url = CFURLCreateWithFileSystemPath(NULL,
						    CFSTR(SYSTEMCONFIGURATION_FRAMEWORK_PATH),
						    kCFURLPOSIXPathStyle,
						    TRUE);

		bundle = CFBundleCreate(NULL, url);
		CFRelease(url);
	}

	if (bundle == NULL) {
		SC_log(LOG_ERR, "could not get CFBundle for \"%@\"", SYSTEMCONFIGURATION_BUNDLE_ID);
	}

done:
	return bundle;
}


/*
 * cachedInfo
 *   <dict>
 *      <key>bundleID-tableName</key>
 *      <dict>
 *        ... property list from non-localized bundle URL
 *      </dict>
 *   </dict>
 */
static CFMutableDictionaryRef	cachedInfo	= NULL;


static dispatch_queue_t
_SC_CFBundleCachedInfoQueue()
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		q = dispatch_queue_create("_SC_CFBundleCachedInfo", NULL);
	});

	return q;
}


static CFStringRef
_SC_CFBundleCachedInfoCopyTableKey(CFBundleRef bundle, CFStringRef tableName)
{
	CFStringRef	bundleID;
	CFStringRef	tableKey;

	bundleID = CFBundleGetIdentifier(bundle);
	tableKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@"), bundleID, tableName);
	return tableKey;
}


static CFDictionaryRef
_SC_CFBundleCachedInfoCopyTable(CFBundleRef bundle, CFStringRef tableName)
{
	__block CFDictionaryRef		dict	= NULL;

	dispatch_sync(_SC_CFBundleCachedInfoQueue(), ^{
		if (cachedInfo != NULL) {
			CFStringRef	tableKey;

			tableKey = _SC_CFBundleCachedInfoCopyTableKey(bundle, tableName);
			dict = CFDictionaryGetValue(cachedInfo, tableKey);
			if (dict != NULL) {
				CFRetain(dict);
			}
			CFRelease(tableKey);
		}
	});

	return dict;
}


static void
_SC_CFBundleCachedInfoSaveTable(CFBundleRef bundle, CFStringRef tableName, CFDictionaryRef table)
{

	dispatch_sync(_SC_CFBundleCachedInfoQueue(), ^{
		CFStringRef	tableKey;

		tableKey = _SC_CFBundleCachedInfoCopyTableKey(bundle, tableName);
		SC_log(LOG_DEBUG, "Caching %@", tableKey);

		if (cachedInfo == NULL) {
			cachedInfo = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
		}
		CFDictionarySetValue(cachedInfo, tableKey, table);
		CFRelease(tableKey);
	});

	return;
}


CFStringRef
_SC_CFBundleCopyNonLocalizedString(CFBundleRef bundle, CFStringRef key, CFStringRef value, CFStringRef tableName)
{
	CFStringRef	str	= NULL;
	CFDictionaryRef	table	= NULL;
	CFURLRef	url;

	if ((tableName == NULL) || CFEqual(tableName, CFSTR(""))) {
		tableName = CFSTR("Localizable");
	}

	table = _SC_CFBundleCachedInfoCopyTable(bundle, tableName);
	if (table == NULL) {
		url = CFBundleCopyResourceURLForLocalization(bundle,
							     tableName,
							     CFSTR("strings"),
							     NULL,
							     CFSTR("en"));
		if (url != NULL) {
			table = _SCCreatePropertyListFromResource(url);
			CFRelease(url);
			if (table != NULL) {
				_SC_CFBundleCachedInfoSaveTable(bundle, tableName, table);
			}
		} else {
			SC_log(LOG_ERR, "failed to get resource url: {bundle:%@, table: %@}", bundle, tableName);
		}
	}

	if (table != NULL) {
		if (isA_CFDictionary(table)) {
			str = CFDictionaryGetValue(table, key);
			if (str != NULL) {
				CFRetain(str);
			}
		}

		CFRelease(table);
	}

	if (str == NULL) {
		str = CFRetain(value);
	}

	return str;
}


#pragma mark -
#pragma mark Mach port / CFMachPort management


CFMachPortRef
_SC_CFMachPortCreateWithPort(const char		*portDescription,
			     mach_port_t	portNum,
			     CFMachPortCallBack	callout,
			     CFMachPortContext	*context)
{
	CFMachPortRef	port;
	Boolean		shouldFree	= FALSE;

	port = CFMachPortCreateWithPort(NULL, portNum, callout, context, &shouldFree);
	if ((port == NULL) || shouldFree) {
		char		*crash_info;
		CFStringRef	err;

		SC_log(LOG_NOTICE, "%s: CFMachPortCreateWithPort() failed , port = %p",
		       portDescription,
		       (void *)(uintptr_t)portNum);
		if (port != NULL) {
			err = CFStringCreateWithFormat(NULL, NULL,
						       CFSTR("%s: CFMachPortCreateWithPort recycled, [old] port = %@"),
						       portDescription, port);
		} else {
			err = CFStringCreateWithFormat(NULL, NULL,
						       CFSTR("%s: CFMachPortCreateWithPort returned NULL"),
						       portDescription);
		}
		crash_info = _SC_cfstring_to_cstring(err, NULL, 0, kCFStringEncodingASCII);
		CFRelease(err);

		err = CFStringCreateWithFormat(NULL,
					       NULL,
					       CFSTR("A recycled mach_port has been detected by \"%s\"."),
					       getprogname());
		_SC_crash(crash_info, CFSTR("CFMachPort error"), err);
		CFAllocatorDeallocate(NULL, crash_info);
		CFRelease(err);
	}

	return port;
}


#pragma mark -
#pragma mark DOS encoding/codepage


#if	!TARGET_OS_IPHONE
void
_SC_dos_encoding_and_codepage(CFStringEncoding	macEncoding,
			      UInt32		macRegion,
			      CFStringEncoding	*dosEncoding,
			      UInt32		*dosCodepage)
{
	switch (macEncoding) {
	case kCFStringEncodingMacRoman:
		if (macRegion != 0) /* anything non-zero is not US */
		*dosEncoding = kCFStringEncodingDOSLatin1;
		else /* US region */
		*dosEncoding = kCFStringEncodingDOSLatinUS;
		break;

	case kCFStringEncodingMacJapanese:
		*dosEncoding = kCFStringEncodingDOSJapanese;
		break;

	case kCFStringEncodingMacChineseTrad:
		*dosEncoding = kCFStringEncodingDOSChineseTrad;
		break;

	case kCFStringEncodingMacKorean:
		*dosEncoding = kCFStringEncodingDOSKorean;
		break;

	case kCFStringEncodingMacArabic:
		*dosEncoding = kCFStringEncodingDOSArabic;
		break;

	case kCFStringEncodingMacHebrew:
		*dosEncoding = kCFStringEncodingDOSHebrew;
		break;

	case kCFStringEncodingMacGreek:
		*dosEncoding = kCFStringEncodingDOSGreek;
		break;

	case kCFStringEncodingMacCyrillic:
		*dosEncoding = kCFStringEncodingDOSCyrillic;
		break;

	case kCFStringEncodingMacThai:
		*dosEncoding = kCFStringEncodingDOSThai;
		break;

	case kCFStringEncodingMacChineseSimp:
		*dosEncoding = kCFStringEncodingDOSChineseSimplif;
		break;

	case kCFStringEncodingMacCentralEurRoman:
		*dosEncoding = kCFStringEncodingDOSLatin2;
		break;

	case kCFStringEncodingMacTurkish:
		*dosEncoding = kCFStringEncodingDOSTurkish;
		break;

	case kCFStringEncodingMacCroatian:
		*dosEncoding = kCFStringEncodingDOSLatin2;
		break;

	case kCFStringEncodingMacIcelandic:
		*dosEncoding = kCFStringEncodingDOSIcelandic;
		break;

	case kCFStringEncodingMacRomanian:
		*dosEncoding = kCFStringEncodingDOSLatin2;
		break;

	case kCFStringEncodingMacFarsi:
		*dosEncoding = kCFStringEncodingDOSArabic;
		break;

	case kCFStringEncodingMacUkrainian:
		*dosEncoding = kCFStringEncodingDOSCyrillic;
		break;

	default:
		*dosEncoding = kCFStringEncodingDOSLatin1;
		break;
	}

	*dosCodepage = CFStringConvertEncodingToWindowsCodepage(*dosEncoding);
	return;
}
#endif	// !TARGET_OS_IPHONE


#pragma mark -
#pragma mark Debugging


/*
 * print status of in-use mach ports
 */
void
_SC_logMachPortStatus(void)
{
	kern_return_t		status;
	mach_port_name_array_t	ports;
	mach_port_type_array_t	types;
	mach_msg_type_number_t	pi, pn, tn;
	CFMutableStringRef	str;

	SC_log(LOG_DEBUG, "----------");

	/* report on ALL mach ports associated with this task */
	status = mach_port_names(mach_task_self(), &ports, &pn, &types, &tn);
	if (status == MACH_MSG_SUCCESS) {
		str = CFStringCreateMutable(NULL, 0);
		for (pi = 0; pi < pn; pi++) {
			char	rights[16], *rp = &rights[0];

			if (types[pi] != MACH_PORT_TYPE_NONE) {
				*rp++ = ' ';
				*rp++ = '(';
				if (types[pi] & MACH_PORT_TYPE_SEND)
					*rp++ = 'S';
				if (types[pi] & MACH_PORT_TYPE_RECEIVE)
					*rp++ = 'R';
				if (types[pi] & MACH_PORT_TYPE_SEND_ONCE)
					*rp++ = 'O';
				if (types[pi] & MACH_PORT_TYPE_PORT_SET)
					*rp++ = 'P';
				if (types[pi] & MACH_PORT_TYPE_DEAD_NAME)
					*rp++ = 'D';
				*rp++ = ')';
			}
			*rp = '\0';
			CFStringAppendFormat(str, NULL, CFSTR(" %d%s"), ports[pi], rights);
		}
		SC_log(LOG_DEBUG, "Task ports (n=%d):%@", pn, str);
		CFRelease(str);
	}

	return;
}


static kern_return_t
_SC_getMachPortReferences(mach_port_t		port,
			  mach_port_type_t	*pt,
			  mach_port_urefs_t	*refs_send,
			  mach_port_urefs_t	*refs_recv,
			  mach_port_status_t	*recv_status,
			  mach_port_urefs_t	*refs_once,
			  mach_port_urefs_t	*refs_pset,
			  mach_port_urefs_t	*refs_dead,
			  const char		*err_prefix)
{
	kern_return_t		status;

	status = mach_port_type(mach_task_self(), port, pt);
	if (status != KERN_SUCCESS) {
		SC_log(LOG_DEBUG, "%smach_port_type(..., 0x%x): %s",
		       err_prefix,
		       port,
		       mach_error_string(status));
		return status;
	}

	if ((refs_send != NULL) && ((*pt & MACH_PORT_TYPE_SEND) != 0)) {
		status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, refs_send);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_refs(..., 0x%x, MACH_PORT_RIGHT_SEND): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}
	}

	if ((refs_recv != NULL) && (recv_status != NULL) && ((*pt & MACH_PORT_TYPE_RECEIVE) != 0)) {
		mach_msg_type_number_t	count;

		status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, refs_recv);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_refs(..., 0x%x, MACH_PORT_RIGHT_RECEIVE): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}

		count = MACH_PORT_RECEIVE_STATUS_COUNT;
		status = mach_port_get_attributes(mach_task_self(),
						  port,
						  MACH_PORT_RECEIVE_STATUS,
						  (mach_port_info_t)recv_status,
						  &count);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_attributes(..., 0x%x, MACH_PORT_RECEIVE_STATUS): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}
	}

	if ((refs_once != NULL) && ((*pt & MACH_PORT_TYPE_SEND_ONCE) != 0)) {
		status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND_ONCE, refs_once);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_refs(..., 0x%x, MACH_PORT_RIGHT_SEND_ONCE): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}
	}

	if ((refs_pset != NULL) && ((*pt & MACH_PORT_TYPE_PORT_SET) != 0)) {
		status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_PORT_SET, refs_pset);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_refs(..., 0x%x, MACH_PORT_RIGHT_PORT_SET): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}
	}

	if ((refs_dead != NULL) && ((*pt & MACH_PORT_TYPE_DEAD_NAME) != 0)) {
		status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, refs_dead);
		if (status != KERN_SUCCESS) {
			SC_log(LOG_DEBUG, "%smach_port_get_refs(..., 0x%x, MACH_PORT_RIGHT_DEAD_NAME): %s",
			       err_prefix,
			       port,
			       mach_error_string(status));
			return status;
		}
	}

	return KERN_SUCCESS;
}


Boolean
_SC_checkMachPortReceive(const char *err_prefix, mach_port_t port)
{
	mach_port_type_t	pt;
	mach_port_status_t	recv_status	= { .mps_nsrequest = 0, };
	mach_port_urefs_t	refs_recv	= 0;
	kern_return_t		status;

	status = _SC_getMachPortReferences(port,
					   &pt,
					   NULL,
					   &refs_recv,
					   &recv_status,
					   NULL,
					   NULL,
					   NULL,
					   err_prefix);
	if (status != KERN_SUCCESS) {
		return FALSE;
	}

	if (refs_recv == 0)  {
		// if no receive rights
		return FALSE;
	}

	return TRUE;
}


Boolean
_SC_checkMachPortSend(const char *err_prefix, mach_port_t port)
{
	mach_port_type_t	pt;
	mach_port_urefs_t	refs_send	= 0;
	mach_port_urefs_t	refs_once	= 0;
	mach_port_urefs_t	refs_dead	= 0;
	kern_return_t		status;

	status = _SC_getMachPortReferences(port,
					   &pt,
					   &refs_send,
					   NULL,
					   NULL,
					   &refs_once,
					   NULL,
					   &refs_dead,
					   err_prefix);
	if (status != KERN_SUCCESS) {
		return FALSE;
	}

	if ((refs_send == 0) && (refs_once == 0) && (refs_dead == 0)) {
		// if no send right
		return FALSE;
	}

	return TRUE;
}


void
_SC_logMachPortReferences(const char *log_prefix, mach_port_t port)
{
	const char		*blanks		= "                                                            ";
	char			buf[60];
	mach_port_type_t	pt;
	mach_port_status_t	recv_status	= { .mps_nsrequest = 0, };
	mach_port_urefs_t	refs_send	= 0;
	mach_port_urefs_t	refs_recv	= 0;
	mach_port_urefs_t	refs_once	= 0;
	mach_port_urefs_t	refs_pset	= 0;
	mach_port_urefs_t	refs_dead	= 0;
	kern_return_t		status;

	buf[0] = '\0';
	if (log_prefix != NULL) {
		// add provided string
		strlcpy(buf, log_prefix, sizeof(buf));

		// fill
		strlcat(buf, blanks, sizeof(buf));
		if (strcmp(&buf[sizeof(buf) - 3], "  ") == 0) {
			buf[sizeof(buf) - 3] = ':';
		}
	}

	status = _SC_getMachPortReferences(port,
					   &pt,
					   &refs_send,
					   &refs_recv,
					   &recv_status,
					   &refs_once,
					   &refs_pset,
					   &refs_dead,
					   buf);
	if (status != KERN_SUCCESS) {
		return;
	}

	SC_log(LOG_DEBUG, "%smach port 0x%x (%d): send=%d, receive=%d, send once=%d, port set=%d, dead name=%d%s%s",
	       buf,
	       port,
	       port,
	       refs_send,
	       refs_recv,
	       refs_once,
	       refs_pset,
	       refs_dead,
	       recv_status.mps_nsrequest ? ", no more senders"   : "",
	       ((pt & MACH_PORT_TYPE_DEAD_NAME) != 0) ? ", dead name request" : "");

	return;
}


CFStringRef
_SC_copyBacktrace()
{
	int			n;
	void			*stack[64];
	char			**symbols;
	CFMutableStringRef	trace;

	n = backtrace(stack, sizeof(stack)/sizeof(stack[0]));
	if (n == -1) {
		SC_log(LOG_NOTICE, "backtrace() failed: %s", strerror(errno));
		return NULL;
	}

	trace = CFStringCreateMutable(NULL, 0);

	symbols = backtrace_symbols(stack, n);
	if (symbols != NULL) {
		int	i;

		for (i = 0; i < n; i++) {
			CFStringAppendFormat(trace, NULL, CFSTR("%s\n"), symbols[i]);
		}

		free(symbols);
	}

	return trace;
}


static void
_SC_ReportCrash(CFStringRef notifyHeader, CFStringRef notifyMessage)
{
#if	!TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#pragma unused(notifyHeader)
#pragma unused(notifyMessage)
#endif	// !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
	static Boolean	warned	= FALSE;

	if (!warned) {
#if	TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		CFStringRef	displayMessage;

		displayMessage = CFStringCreateWithFormat(NULL,
							  NULL,
							  CFSTR("%@\n\nPlease collect the crash report and file a Radar."),
							  notifyMessage);
		CFUserNotificationDisplayNotice(0,
						kCFUserNotificationStopAlertLevel,
						NULL,
						NULL,
						NULL,
						notifyHeader,
						displayMessage,
						NULL);
		CFRelease(displayMessage);
#endif	// TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		warned = TRUE;
	}

	return;
}


void
_SC_crash(const char *crash_info, CFStringRef notifyHeader, CFStringRef notifyMessage)
{
	if (_SC_isAppleInternal()) {
		if (crash_info == NULL) {
			crash_info = "_SC_crash() called w/o \"crash_info\"";
		}

		// augment the crash log message
		CRSetCrashLogMessage(crash_info);

		// simulate a crash report
		os_log_with_type(SC_LOG_HANDLE(), OS_LOG_TYPE_FAULT, "%s", crash_info);

		// report the crash to the user
		if ((notifyHeader != NULL) && (notifyMessage != NULL)) {
			_SC_ReportCrash(notifyHeader, notifyMessage);
		}

		// ... and cleanup after the crash report has been generated
		CRSetCrashLogMessage(NULL);
	}

	return;
}


Boolean
_SC_getconninfo(int socket, struct sockaddr_storage *src_addr, struct sockaddr_storage *dest_addr, int *if_index, uint32_t *flags)
{
	struct so_cinforeq	request;

	memset(&request, 0, sizeof(request));

	if (src_addr != NULL) {
		memset(src_addr, 0, sizeof(*src_addr));
		request.scir_src = (struct sockaddr *)src_addr;
		request.scir_src_len = sizeof(*src_addr);
	}

	if (dest_addr != NULL) {
		memset(dest_addr, 0, sizeof(*dest_addr));
		request.scir_dst = (struct sockaddr *)dest_addr;
		request.scir_dst_len = sizeof(*dest_addr);
	}

	if (ioctl(socket, SIOCGCONNINFO, &request) != 0) {
		SC_log(LOG_NOTICE, "SIOCGCONNINFO failed: %s", strerror(errno));
		return FALSE;
	}

	if (if_index != NULL) {
		*if_index = 0;
		*if_index = request.scir_ifindex;
	}

	if (flags != NULL) {
		*flags = 0;
		*flags = request.scir_flags;
	}

	return TRUE;
}
