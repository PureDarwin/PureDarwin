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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <sys/time.h>

#define OS_LOG_PACK_SPI
#include <os/log.h>
#include <os/log_private.h>

#include "SCDynamicStoreInternal.h"
#include "SCD.h"
#include "config.h"		/* MiG generated file */

#define INSTALL_ENVIRONMENT	"__OSINSTALL_ENVIRONMENT"

/* framework variables */
int			_sc_debug	= FALSE;	/* non-zero if debugging enabled */
int			_sc_verbose	= FALSE;	/* non-zero if verbose logging enabled */
_SCLogDestination	_sc_log		= kSCLogDestinationDefault;


#pragma mark -
#pragma mark Thread specific data


static pthread_once_t	tsKeyInitialized	= PTHREAD_ONCE_INIT;
static pthread_key_t	tsDataKey;


static void
__SCThreadSpecificDataFinalize(void *arg)
{
	__SCThreadSpecificDataRef	tsd = (__SCThreadSpecificDataRef)arg;

	if (tsd != NULL) {
		if (tsd->_sc_interface_cache != NULL) {
			CFRelease(tsd->_sc_interface_cache);
		}
		if (tsd->_sc_store != NULL) {
			CFRelease(tsd->_sc_store);
		}
		CFAllocatorDeallocate(kCFAllocatorSystemDefault, tsd);
	}
	return;
}


static void
__SCThreadSpecificKeyInitialize()
{
	pthread_key_create(&tsDataKey, __SCThreadSpecificDataFinalize);
	return;
}


__private_extern__
__SCThreadSpecificDataRef
__SCGetThreadSpecificData()
{
	__SCThreadSpecificDataRef	tsd;
	pthread_once(&tsKeyInitialized, __SCThreadSpecificKeyInitialize);

	tsd = pthread_getspecific(tsDataKey);
	if (tsd == NULL) {
		tsd = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__SCThreadSpecificData), 0);
		tsd->_sc_error = kSCStatusOK;
		tsd->_sc_interface_cache = NULL;
		tsd->_sc_store = NULL;
		pthread_setspecific(tsDataKey, tsd);
	}

	return tsd;
}


#pragma mark -
#pragma mark Logging


#define	ENABLE_SC_FORMATTING
#ifdef	ENABLE_SC_FORMATTING
// from <CoreFoundation/ForFoundationOnly.h>
extern CFStringRef _CFStringCreateWithFormatAndArgumentsAux(CFAllocatorRef alloc, CFStringRef (*copyDescFunc)(CFTypeRef, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef format, va_list arguments);
#endif	/* ENABLE_SC_FORMATTING */


static CFComparisonResult
compare_CFDate(const void *val1, const void *val2, void *context)
{
#pragma unused(context)
	CFDateRef		str1	= (CFDateRef)val1;
	CFDateRef		str2	= (CFDateRef)val2;

	return CFDateCompare(str1, str2, 0);
}


static CFComparisonResult
compare_CFNumber(const void *val1, const void *val2, void *context)
{
#pragma unused(context)
	CFNumberRef		str1	= (CFNumberRef)val1;
	CFNumberRef		str2	= (CFNumberRef)val2;

	return CFNumberCompare(str1, str2, 0);
}


static CFComparisonResult
compare_CFString(const void *val1, const void *val2, void *context)
{
#pragma unused(context)
	CFStringRef		str1	= (CFStringRef)val1;
	CFStringRef		str2	= (CFStringRef)val2;

	return CFStringCompare(str1, str2, 0);
}


CFStringRef
_SCCopyDescription(CFTypeRef cf, CFDictionaryRef formatOptions)
{
#ifdef	ENABLE_SC_FORMATTING
	CFMutableDictionaryRef	nFormatOptions;
	CFStringRef		prefix1;
	CFStringRef		prefix2;
	CFTypeID		type	= CFGetTypeID(cf);

	if (!formatOptions ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX1"), (const void **)&prefix1)) {
		prefix1 = CFSTR("");
	}

	if (type == CFStringGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%@"),
						prefix1,
						cf);
	}

	if (type == CFBooleanGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%s"),
						prefix1,
						CFBooleanGetValue(cf) ? "TRUE" : "FALSE");
	}

	if (type == CFDataGetTypeID()) {
		const uint8_t		*data;
		CFIndex			dataLen;
		CFIndex			i;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<data> 0x"), prefix1);

		data    = CFDataGetBytePtr(cf);
		dataLen = CFDataGetLength(cf);
		for (i = 0; i < dataLen; i++) {
			CFStringAppendFormat(str, NULL, CFSTR("%02x"), data[i]);
		}

		return str;
	}

	if (type == CFNumberGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%@"),
						prefix1,
						cf);
	}

	if (type == CFDateGetTypeID()) {
		CFCalendarRef	calendar;
		CFStringRef	str;
		CFTimeZoneRef	tz;
		int		MM, DD, YYYY, hh, mm, ss;

		calendar = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
		tz = CFTimeZoneCopySystem();
		CFCalendarSetTimeZone(calendar, tz);
		CFRelease(tz);
		CFCalendarDecomposeAbsoluteTime(calendar,
						CFDateGetAbsoluteTime(cf),
						"MdyHms",
						&MM, &DD, &YYYY, &hh, &mm, &ss);
		CFRelease(calendar);

		str = CFStringCreateWithFormat(NULL,
					       formatOptions,
					       CFSTR("%@%02d/%02d/%04d %02d:%02d:%02d"),
					       prefix1,
					       MM, DD, YYYY, hh, mm, ss);
		return str;
	}

	if ((formatOptions == NULL) ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX2"), (const void **)&prefix2)) {
		prefix2 = prefix1;
	}

	if (formatOptions != NULL) {
		nFormatOptions = CFDictionaryCreateMutableCopy(NULL, 0, formatOptions);
	} else {
		nFormatOptions = CFDictionaryCreateMutable(NULL,
							   0,
							   &kCFTypeDictionaryKeyCallBacks,
							   &kCFTypeDictionaryValueCallBacks);
	}
	assert(nFormatOptions != NULL);

#define	N_QUICK	32

	if (type == CFArrayGetTypeID()) {
		const void *		elements_q[N_QUICK];
		const void **		elements	= elements_q;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<array> {"), prefix1);

		nElements = CFArrayGetCount(cf);
		if (nElements > 0) {
			if (nElements > (CFIndex)(sizeof(elements_q)/sizeof(CFTypeRef)))
				elements  = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			CFArrayGetValues(cf, CFRangeMake(0, nElements), elements);
			for (i = 0; i < nElements; i++) {
				CFMutableStringRef	nPrefix1;
				CFMutableStringRef	nPrefix2;
				CFStringRef		nStr;
				CFStringRef		vStr;

				nStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%ld"), i);

				nPrefix1 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix1,
						     formatOptions,
						     CFSTR("%@  %@ : "),
						     prefix2,
						     nStr);
				nPrefix2 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix2,
						     formatOptions,
						     CFSTR("%@  "),
						     prefix2);

				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX1"), nPrefix1);
				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX2"), nPrefix2);
				CFRelease(nPrefix1);
				CFRelease(nPrefix2);
				CFRelease(nStr);

				vStr = _SCCopyDescription((CFTypeRef)elements[i], nFormatOptions);
				CFStringAppendFormat(str,
						     formatOptions,
						     CFSTR("\n%@"),
						     vStr);
				CFRelease(vStr);
			}
			if (elements != elements_q) CFAllocatorDeallocate(NULL, elements);
		}
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	if (type == CFDictionaryGetTypeID()) {
		const void *		keys_q[N_QUICK];
		const void **		keys	= keys_q;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	nPrefix1;
		CFMutableStringRef	nPrefix2;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<dictionary> {"), prefix1);

		nElements = CFDictionaryGetCount(cf);
		if (nElements > 0) {
			CFComparatorFunction	compFunc	= NULL;
			CFMutableArrayRef	sortedKeys;

			if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
				keys = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			}
			CFDictionaryGetKeysAndValues(cf, keys, NULL);

			sortedKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			for (i = 0; i < nElements; i++) {
				CFArrayAppendValue(sortedKeys, (CFStringRef)keys[i]);
			}

			if (isA_CFString(keys[0])) {
				compFunc = compare_CFString;
			}
			else if (isA_CFNumber(keys[0])) {
				compFunc = compare_CFNumber;
			}
			else if (isA_CFDate(keys[0])) {
				compFunc = compare_CFDate;
			}

			if (compFunc != NULL) {
				CFArraySortValues(sortedKeys,
						  CFRangeMake(0, nElements),
						  compFunc,
						  NULL);
			}

			for (i = 0; i < nElements; i++) {
				CFStringRef		key;
				CFStringRef		kStr;
				CFTypeRef		val;
				CFStringRef		vStr;

				key  = CFArrayGetValueAtIndex(sortedKeys, i);
				kStr = _SCCopyDescription((CFTypeRef)key, NULL);

				nPrefix1 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix1,
						     formatOptions,
						     CFSTR("%@  %@ : "),
						     prefix2,
						     kStr);
				nPrefix2 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix2,
						     formatOptions,
						     CFSTR("%@  "),
						     prefix2);

				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX1"), nPrefix1);
				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX2"), nPrefix2);
				CFRelease(nPrefix1);
				CFRelease(nPrefix2);
				CFRelease(kStr);

				val  = CFDictionaryGetValue(cf, key);
				vStr = _SCCopyDescription((CFTypeRef)val, nFormatOptions);
				CFStringAppendFormat(str,
						     formatOptions,
						     CFSTR("\n%@"),
						     vStr);
				CFRelease(vStr);
			}

			CFRelease(sortedKeys);

			if (keys != keys_q) {
				CFAllocatorDeallocate(NULL, keys);
			}
		}
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	CFRelease(nFormatOptions);
#endif	/* ENABLE_SC_FORMATTING */

	return CFStringCreateWithFormat(NULL,
					formatOptions,
					CFSTR("%@%@"),
					prefix1,
					cf);
}


static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

Boolean
_SC_isInstallEnvironment() {
	static dispatch_once_t	once;
	static Boolean		is_install;

	dispatch_once(&once, ^{
		is_install = (getenv(INSTALL_ENVIRONMENT) != NULL);
	});

	return is_install;
}


os_log_t
_SC_LOG_DEFAULT(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "");
	}

	return log;
}


os_log_type_t
_SC_syslog_os_log_mapping(int level)
{
	if (level < 0) {
		level = ~level;
	}

	switch (level) {
		case LOG_EMERG :
		case LOG_ALERT :
		case LOG_CRIT :
			return OS_LOG_TYPE_ERROR;

		case LOG_ERR :
		case LOG_WARNING :
		case LOG_NOTICE :
			return OS_LOG_TYPE_DEFAULT;

		case LOG_INFO :
			return OS_LOG_TYPE_INFO;

		case LOG_DEBUG :
			return OS_LOG_TYPE_DEBUG;
	}

	return OS_LOG_TYPE_DEFAULT;
}

static void
__SCLog(void *ret_addr, os_log_type_t type, const char *formatString, va_list formatArguments)
{
	os_log_with_args(_SC_LOG_DEFAULT(),
			 type,
			 formatString,
			 formatArguments,
			 ret_addr);
	return;
}


static void
__SCPrint(FILE *stream, CFStringRef formatString, va_list formatArguments, Boolean addTime, Boolean addNL)
{
	char		*line;
	CFStringRef	str;
	CFIndex		usedBufLen;

#ifdef	ENABLE_SC_FORMATTING
	str = _CFStringCreateWithFormatAndArgumentsAux(NULL,
						       _SCCopyDescription,
						       NULL,
						       formatString,
						       formatArguments);
#else	/* ENABLE_SC_FORMATTING */
	str =  CFStringCreateWithFormatAndArguments   (NULL,
						       NULL,
						       formatString,
						       formatArguments);
#endif	/* !ENABLE_SC_FORMATTING */

	line =_SC_cfstring_to_cstring_ext(str,
					  NULL,
					  0,
					  kCFStringEncodingUTF8,
					  (UInt8)'?',
					  &usedBufLen);
	CFRelease(str);
	if (!line) {
		return;
	}

	pthread_mutex_lock(&lock);
	if (addTime) {
		struct tm	tm_now;
		struct timeval	tv_now;

		(void)gettimeofday(&tv_now, NULL);
		(void)localtime_r(&tv_now.tv_sec, &tm_now);
		(void)fprintf(stream, "%2d:%02d:%02d.%03d ",
			      tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv_now.tv_usec / 1000);
	}
	(void)fwrite((const void *)line, usedBufLen, 1, stream);
	if (addNL) {
		(void)fputc('\n', stream);
	}
	fflush (stream);
	pthread_mutex_unlock(&lock);
	CFAllocatorDeallocate(NULL, line);

	return;
}


/*
 * NOTE: We need to keep this function in place (for a least a while) to ensure
 *       that any [old] code that was using an earlier version of SC_log() will
 *       have the needed support code to perform the actual logging.  Newly
 *       compiled code uses the new/replacement _SC_log_send() function.
 */
void
__SC_Log(int level, CFStringRef format_CF, os_log_t log, os_log_type_t type, const char *format, ...)
{
#pragma unused(level)
	Boolean		do_log		= FALSE;
	Boolean		do_print	= FALSE;
	va_list		args_log	= { 0 };
	va_list		args_print	= { 0 };

	if (_sc_log > kSCLogDestinationFile) {
		do_log = TRUE;			// log requested
		va_start(args_log, format);

		if (_sc_log >= kSCLogDestinationBoth) {
			do_print = TRUE;	// log AND print requested
			va_copy(args_print, args_log);
		}
	} else {
		do_print = TRUE;		// print requested
		va_start(args_print, format);
	}

	if (do_log) {
		os_log_with_args(log,
				 type,
				 format,
				 args_log,
				 __builtin_return_address(0));
		va_end(args_log);
	}

	if (do_print) {
		__SCPrint(stdout,
			  format_CF,
			  args_print,
			  (_sc_log == kSCLogDestinationBoth),	// trace
			  TRUE);				// add newline
		va_end(args_print);
	}

	return;
}


Boolean
__SC_log_enabled(int level, os_log_t log, os_log_type_t type)
{
	if (os_log_type_enabled(log, type)) {
		return TRUE;
	}

	if (_sc_log != kSCLogDestinationDefault) {
		// if os_log'ing not enabled and the messages is targeted to stdout/stderr
		if (level < LOG_INFO) {
			// if not LOG_INFO/LOG_DEBUG message, print
			return TRUE;
		} else if ((level == LOG_INFO) && _sc_verbose) {
			// if LOG_INFO and _sc_verbose, print
			return TRUE;
		} else if (_sc_debug) {
			// if _sc_debug, print
			return TRUE;
		}
	}

	if (_SC_isInstallEnvironment()) {
		// if OSInstaller environment
		if (level < LOG_INFO) {
			// if not LOG_INFO/LOG_DEBUG message, syslog
			return TRUE;
		} else if ((level == LOG_INFO) && _SC_isAppleInternal()) {
			// if LOG_INFO and internal, syslog
			return TRUE;
		} else if (_sc_debug) {
			// if _sc_debug, syslog
			return TRUE;
		}
	}

	return FALSE;
}


void
__SC_log_send(int level, os_log_t log, os_log_type_t type, os_log_pack_t pack)
{
	Boolean		addTime		= (_sc_log == kSCLogDestinationBoth);
	char		buffer[256];
	const char	*buffer_ptr	= buffer;
	char		*composed	= NULL;
	Boolean		do_log		= FALSE;
	Boolean		do_print	= FALSE;
	Boolean		do_syslog	= FALSE;

	if (_sc_log > kSCLogDestinationFile) {
		do_log = TRUE;

#ifdef	USE_SYSLOG_FOR_INSTALL
		if (_SC_isInstallEnvironment()) {
			/*
			 * os_log(3) messages are not persisted in the
			 * install environment.  But, the installer does
			 * capture syslog(3) messages.
			 */
			do_syslog = TRUE;
		}
#endif	// USE_SYSLOG_FOR_INSTALL

		if (_sc_log >= kSCLogDestinationBoth) {
			do_print = TRUE;	// log AND print requested
		}
	} else {
		do_print = TRUE;		// print requested
	}

	if (do_log) {
		if (!do_print && !do_syslog) {
			// if only os_log requested
			os_log_pack_send(pack, log, type);
		} else {
			// if os_log and print (or syslog) requested
			composed = os_log_pack_send_and_compose(pack, log, type, buffer, sizeof(buffer));
		}
	} else {
		// if print-only requested
		mach_get_times(NULL, &pack->olp_continuous_time, &pack->olp_wall_time);
		composed = os_log_pack_compose(pack, log, type, buffer, sizeof(buffer));
	}

	if (do_print &&
	    (
	     (level < LOG_INFO)				||	// print most messages
	     ((level == LOG_INFO) && _sc_verbose)	||	// with _sc_verbose, include LOG_INFO
	     _sc_debug						// with _sc_debug, include LOG_DEBUG
	    )
	   ) {
		// if printing
		pthread_mutex_lock(&lock);
		if (addTime) {
			struct tm	tm_now;
			struct timeval	tv_now;

			tv_now.tv_sec = (time_t)&pack->olp_wall_time.tv_sec;
			tv_now.tv_usec = (suseconds_t)((uint64_t)&pack->olp_wall_time.tv_nsec / NSEC_PER_USEC);
			(void)localtime_r(&tv_now.tv_sec, &tm_now);
			(void)fprintf(stdout, "%2d:%02d:%02d.%03d ",
				      tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv_now.tv_usec / 1000);
		}
		(void)fprintf(stdout, "%s\n", composed);
		fflush (stdout);
		pthread_mutex_unlock(&lock);
	}

	if (do_syslog &&
	    (
	     (level < LOG_INFO) ||
	     ((level == LOG_INFO) && _SC_isAppleInternal()) ||
	     _sc_debug
	    )
	   ) {
		// if [install/upgrade] syslog'ing
		syslog(level | LOG_INSTALL, "%s", composed);
	}

	if (composed != buffer_ptr) {
		free(composed);
	}

	return;
}


void
SCLog(Boolean condition, int level, CFStringRef formatString, ...)
{
	va_list		formatArguments		= { 0 };
	va_list		formatArguments_print	= { 0 };
	Boolean		log			= FALSE;
	Boolean		print			= FALSE;

	if (!condition) {
		return;
	}

	/*
	 * Note: The following are the expected values for _sc_log
	 *
	 * 0 if SC messages should be written to stdout/stderr
	 * 1 if SC messages should be logged w/os_log(3)
	 * 2 if SC messages should be written to stdout/stderr AND logged
	 * 3 if SC messages should be logged AND written to stdout/stderr (w/o timestamp)
	 */

	if (_sc_log > kSCLogDestinationFile) {
		log = TRUE;		// log requested
		va_start(formatArguments, formatString);

		if (_sc_log >= kSCLogDestinationBoth) {
			print = TRUE;	// log AND print requested
			va_copy(formatArguments_print, formatArguments);
		}
	} else {
		print = TRUE;		// print requested
		va_start(formatArguments_print, formatString);
	}

	if (log) {
		const char	*__format;

		__format = CFStringGetCStringPtr(formatString, kCFStringEncodingUTF8);
		if (__format != NULL) {
			os_log_type_t	__type;

			__type = _SC_syslog_os_log_mapping(level);
			__SCLog(__builtin_return_address(0), __type, __format, formatArguments);
		}
		va_end(formatArguments);
	}

	if (print) {
		__SCPrint((LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout,
			  formatString,
			  formatArguments_print,
			  (_sc_log == kSCLogDestinationBoth),	// trace
			  TRUE);				// add newline
		va_end(formatArguments_print);
	}

	return;
}


void
SCPrint(Boolean condition, FILE *stream, CFStringRef formatString, ...)
{
	va_list		formatArguments;

	if (!condition) {
		return;
	}

	va_start(formatArguments, formatString);
	__SCPrint(stream, formatString, formatArguments, FALSE, FALSE);
	va_end(formatArguments);

	return;
}


#pragma mark -
#pragma mark SC error handling / logging


const CFStringRef kCFErrorDomainSystemConfiguration	= CFSTR("com.apple.SystemConfiguration");


static const struct sc_errmsg {
	int	status;
	char	*message;
} sc_errmsgs[] = {
	{ kSCStatusAccessError,		"Permission denied" },
	{ kSCStatusConnectionIgnore,	"Network connection information not available at this time" },
	{ kSCStatusConnectionNoService,	"Network service for connection not available" },
	{ kSCStatusFailed,		"Failed!" },
	{ kSCStatusInvalidArgument,	"Invalid argument" },
	{ kSCStatusKeyExists,		"Key already defined" },
	{ kSCStatusLocked,		"Lock already held" },
	{ kSCStatusMaxLink,		"Maximum link count exceeded" },
	{ kSCStatusNeedLock,		"Lock required for this operation" },
	{ kSCStatusNoStoreServer,	"Configuration daemon not (no longer) available" },
	{ kSCStatusNoStoreSession,	"Configuration daemon session not active" },
	{ kSCStatusNoConfigFile,	"Configuration file not found" },
	{ kSCStatusNoKey,		"No such key" },
	{ kSCStatusNoLink,		"No such link" },
	{ kSCStatusNoPrefsSession,	"Preference session not active" },
	{ kSCStatusNotifierActive,	"Notifier is currently active" },
	{ kSCStatusOK,			"Success!" },
	{ kSCStatusPrefsBusy,		"Preferences update currently in progress" },
	{ kSCStatusReachabilityUnknown,	"Network reachability cannot be determined" },
	{ kSCStatusStale,		"Write attempted on stale version of object" },
};
#define nSC_ERRMSGS (sizeof(sc_errmsgs)/sizeof(struct sc_errmsg))

void
_SCErrorSet(int error)
{
	__SCThreadSpecificDataRef	tsd;

	tsd = __SCGetThreadSpecificData();
	tsd->_sc_error = error;
	return;
}


CFErrorRef
SCCopyLastError(void)
{
	CFStringRef			domain;
	CFErrorRef			error;
	int				i;
	int				code;
	__SCThreadSpecificDataRef	tsd;
	CFMutableDictionaryRef		userInfo	= NULL;

	tsd = __SCGetThreadSpecificData();
	code =tsd->_sc_error;

	for (i = 0; i < (int)nSC_ERRMSGS; i++) {
		if (sc_errmsgs[i].status == code) {
			CFStringRef	str;

			domain = kCFErrorDomainSystemConfiguration;
			userInfo = CFDictionaryCreateMutable(NULL,
							     0,
							     &kCFCopyStringDictionaryKeyCallBacks,
							     &kCFTypeDictionaryValueCallBacks);
			str = CFStringCreateWithCString(NULL,
							sc_errmsgs[i].message,
							kCFStringEncodingASCII);
			CFDictionarySetValue(userInfo, kCFErrorDescriptionKey, str);
			CFRelease(str);
			goto done;
		}
	}

	if ((code > 0) && (code <= ELAST)) {
		domain = kCFErrorDomainPOSIX;
		goto done;
	}

	domain = kCFErrorDomainMach;

	done :

	error = CFErrorCreate(NULL, domain, code, userInfo);
	if (userInfo != NULL) CFRelease(userInfo);
	return error;
}


int
SCError(void)
{
	__SCThreadSpecificDataRef	tsd;

	tsd = __SCGetThreadSpecificData();
	return tsd->_sc_error;
}


const char *
SCErrorString(int status)
{
	const char	*err;
	int		i;

	for (i = 0; i < (int)nSC_ERRMSGS; i++) {
		if (sc_errmsgs[i].status == status) {
			return sc_errmsgs[i].message;
		}
	}

	if ((status > 0) && (status <= ELAST)) {
		return strerror(status);
	}

	if ((status >= BOOTSTRAP_SUCCESS) && (status <= BOOTSTRAP_NO_MEMORY)) {
		return bootstrap_strerror(status);
	}

	err = mach_error_string(status);
	if (err == NULL) {
		err = strerror(status);		// return unknown error
	}

	return err;
}
