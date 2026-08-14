/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_SECASSESSMENT
#define _H_SECASSESSMENT

#include <CoreFoundation/CoreFoundation.h>
#include <Security/CSCommon.h>

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @type SecAccessmentRef An assessment being performed.
 */
typedef struct _SecAssessment *SecAssessmentRef;


/*!
 * CF-standard type function
 */
CFTypeID SecAssessmentGetTypeID();


/*
 * Notifications sent when the policy authority database changes.
 * (Should move to /usr/include/notify_keys.h eventually.)
 */
#define kNotifySecAssessmentMasterSwitch "com.apple.security.assessment.masterswitch"
#define kNotifySecAssessmentUpdate "com.apple.security.assessment.update"
#define kNotifySecAssessmentRecordingChange "com.apple.security.assessment.UIRecordRejectDidChangeNotification"


/*!
 * Primary operation types. These are operations the system policy can express
 * opinions on. They are not operations *on* the system configuration itself.
 * (For those, see SecAssessmentUpdate below.)
 *
 * @constant kSecAssessmentContextKeyOperation Context key describing the type of operation
 *	being contemplated. The default varies depending on the API call used.
 * @constant kSecAssessmentOperationTypeExecute Value denoting the operation of running or executing
 *	code on the system.
 * @constant kSecAssessmentOperationTypeInstall Value denoting the operation of installing
 *	software into the system.
 * @constant kSecAssessmentOperationTypeOpenDocument Value denoting the operation of opening
 *	(in the LaunchServices sense) of documents.
 */
extern CFStringRef kSecAssessmentContextKeyOperation;	// proposed operation
extern CFStringRef kSecAssessmentOperationTypeExecute;	// .. execute code
extern CFStringRef kSecAssessmentOperationTypeInstall;	// .. install software
extern CFStringRef kSecAssessmentOperationTypeOpenDocument; // .. LaunchServices-level document open


/*!
	Operational flags for SecAssessment calls
	
	@type SecAssessmentFlags A mask of flag bits passed to SecAssessment calls to influence their
		operation.
	
	@constant kSecAssessmentDefaultFlags Pass this to indicate that default behavior is desired.
	@constant kSecAssessmentFlagIgnoreCache Do not use cached information; always perform a full
		evaluation of system policy. This may be substantially slower.
	@constant kSecAssessmentFlagNoCache Do not save any evaluation outcome in the system caches.
		Any content already there is left undisturbed. Independent of kSecAssessmentFlagIgnoreCache.
	@constant kSecAssessmentFlagEnforce Perform normal operations even if assessments have been
		globally bypassed (which would usually approve anything).
	@constant kSecAssessmentAllowWeak Allow signatures that contain known weaknesses, such as an
		insecure resource envelope.
	@constant kSecAssessmentIgnoreWhitelist Do not search the weak signature whitelist.
	@constant kSecAssessmentFlagIgnoreActiveAssessments Permit parallel re-assessment of the same target.
	@constant kSecAssessmentFlagLowPriority Run the assessment in low priority.

	Flags common to multiple calls are assigned from high-bit down. Flags for particular calls
	are assigned low-bit up, and are documented with that call.
 */
typedef uint64_t SecAssessmentFlags;
enum {
	kSecAssessmentDefaultFlags = 0,					// default behavior

	kSecAssessmentFlagDirect = 1 << 30,				// in-process evaluation
	kSecAssessmentFlagAsynchronous = 1 << 29,		// request asynchronous operation
	kSecAssessmentFlagIgnoreCache = 1 << 28,		// do not search cache
	kSecAssessmentFlagNoCache = 1 << 27,			// do not populate cache
	kSecAssessmentFlagEnforce = 1 << 26,			// force on (disable bypass switches)
	kSecAssessmentFlagAllowWeak = 1 << 25,			// allow weak signatures
	kSecAssessmentFlagIgnoreWhitelist = 1 << 24,	// do not search weak signature whitelist
    // 1 << 23 removed (was kSecAssessmentFlagDequarantine)
    kSecAssessmentFlagIgnoreActiveAssessments = 1 << 22, // permit parallel re-assessment of the same target
    kSecAssessmentFlagLowPriority = 1 << 21,        // run the assessment in low priority
};


/*!
	@function SecAssessmentCreate
	Ask the system for its assessment of a proposed operation.
	
	@param path CFURL describing the file central to the operation - the program
		to be executed, archive to be installed, plugin to be loaded, etc.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@param context Optional CFDictionaryRef containing additional information bearing
		on the requested assessment.
	@param errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to arrive at a judgment is.
	@result On success, a SecAssessment object that can be queried for its outcome.
		On error, NULL (with *errors set).
	
	Option flags:
	
	@constant kSecAssessmentFlagRequestOrigin Request additional work to produce information on
		the originator (signer) of the object being discussed.

	Context keys:

	@constant kSecAssessmentContextKeyOperation Type of operation (see overview above). This defaults
		to the kSecAssessmentOperationTypeExecute.
 */
extern CFStringRef kSecAssessmentContextKeyUTI;			// caller determination of UTI for primary assessment subject

extern CFStringRef kSecAssessmentContextKeyFeedback;	// feedback reporting block
typedef Boolean (^SecAssessmentFeedback)(CFStringRef type, CFDictionaryRef information);
extern CFStringRef kSecAssessmentFeedbackProgress;		// progress reporting feedback
extern CFStringRef kSecAssessmentFeedbackInfoCurrent;	// info key: current work progress
extern CFStringRef kSecAssessmentFeedbackInfoTotal;		// info key: total expected work
	
extern CFStringRef kSecAssessmentContextKeyPrimarySignature; // on document assessment, treat code signature as primary and return its status
	
extern CFStringRef kSecAssessmentAssessmentVerdict;		// CFBooleanRef: master result - allow or deny
extern CFStringRef kSecAssessmentAssessmentOriginator;	// CFStringRef: describing the signature originator
extern CFStringRef kSecAssessmentAssessmentAuthority;	// CFDictionaryRef: authority used to arrive at result
extern CFStringRef kSecAssessmentAssessmentSource;		// CFStringRef: primary source of authority
extern CFStringRef kSecAssessmentAssessmentFromCache;	// present if result is from cache
extern CFStringRef kSecAssessmentAssessmentWeakSignature; // present if result attributable to signature weakness
extern CFStringRef kSecAssessmentAssessmentCodeSigningError; // error code returned by code signing API
extern CFStringRef kSecAssessmentAssessmentAuthorityRow; // (internal)
extern CFStringRef kSecAssessmentAssessmentAuthorityOverride; // (internal)
extern CFStringRef kSecAssessmentAssessmentAuthorityOriginalVerdict; // (internal)
extern CFStringRef kSecAssessmentAssessmentAuthorityFlags; // (internal)
extern CFStringRef kSecAssessmentAssessmentNotarizationDate; // (internal)

extern CFStringRef kDisabledOverride;					// AuthorityOverride value for "Gatekeeper is disabled"

enum {
	kSecAssessmentFlagRequestOrigin = 1 << 0,		// request origin information (slower)
};

SecAssessmentRef SecAssessmentCreate(CFURLRef path,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors);


/*!
	@function SecAssessmentCopyResult

	Extract results from a completed assessment and return them as a CFDictionary.

	@param assessment A SecAssessmentRef created with SecAssessmentCreate.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to form a judgment is.
	@result On success, a CFDictionary describing the outcome and various corroborating
		data as requested by flags. The caller owns this dictionary and should release it
		when done with it. On error, NULL (with *errors set).

	Assessment result keys (dictionary keys returned on success):

	@constant kSecAssessmentAssessmentVerdict A CFBoolean value indicating whether the system policy
		allows (kCFBooleanTrue) or denies (kCFBooleanFalse) the proposed operation.
	@constant kSecAssessmentAssessmentAuthority A CFDictionary describing what sources of authority
		were used to arrive at this result.
	@constant kSecAssessmentAssessmentOriginator A human-readable CFString describing the originator
		of the signature securing the subject of the verdict. Requires kSecAssessmentFlagRequireOrigin.
		May be missing anyway if no reliable source of origin can be determined.
 */
CFDictionaryRef SecAssessmentCopyResult(SecAssessmentRef assessment,
	SecAssessmentFlags flags,
	CFErrorRef *errors);


/*!
	@function SecAssessmentCopyUpdate
	Make changes to the system policy configuration.
	
	@param path CFTypeRef describing the subject of the operation. Depending on the operation,
		this may be a CFURL denoting a (single) file or bundle; a SecRequirement describing
		a group of files; a CFNumber denoting an existing rule by rule number, or NULL to perform
		global changes.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@param context Required CFDictionaryRef containing information bearing
		on the requested assessment. Must at least contain the kSecAssessmentContextKeyEdit key.
	@param errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to form a judgment is.
	@result Returns On success, a CFDictionary containing information pertaining to the completed operation.
		Caller must CFRelease it when done. On failure, NULL, with *errors set if provided.
	
	Note: The SecAssessmentUpdate variant does not return data. It returns True on success, or False on error.
	
	Context keys and values:

	@constant kSecAssessmentContextKeyEdit Required context key describing the kind of change
		requested to the system policy configuration. Currently understood values:
	@constant kSecAssessmentUpdateOperationAdd Add a new rule to the assessment rule database.
	@constant kSecAssessmentUpdateOperationRemove Remove rules from the rule database.
	@constant kSecAssessmentUpdateOperationEnable (Re)enable rules in the rule database.
	@constant kSecAssessmentUpdateOperationDisable Disable rules in the rule database.
	@constant kSecAssessmentUpdateOperationFind Locate and return rules from the rule database.
		This operation does not change the database, and does not require authorization or privileges.
		
	@constant kSecAssessmentUpdateKeyAuthorization A CFData containing the external form of a
		system AuthorizationRef used to authorize the change. The call will automatically generate
		a suitable authorization if this is missing; however, if the request is on behalf of
		another client, an AuthorizationRef should be created there and passed along here.
	@constant kSecAssessmentUpdateKeyPriority CFNumber denoting a (floating point) priority
		for the rule(s) being processed.
	@constant kSecAssessmentUpdateKeyLabel CFString denoting a label string applied to the rule(s)
		being processed.
	@constant kSecAssessmentUpdateKeyExpires CFDate denoting an (absolute, future) expiration date
		for rule(s) being processed.
	@constant kSecAssessmentUpdateKeyAllow CFBoolean denoting whether a new rule allows or denies
		assessment. The default is to allow; set to kCFBooleanFalse to create a negative (denial) rule.
	@constant kSecAssessmentUpdateKeyRemarks CFString containing a colloquial description or comment
		about a newly created rule. This is mean to be human readable and is not used when evaluating rules.
	
	Keys returned as the result of a successful kSecAssessmentUpdateOperationFind operation:
	
	@constant kSecAssessmentRuleKeyID A CFNumber uniquely identifying a rule.
	@constant kSecAssessmentRuleKeyPriority A CFNumber indicating the rule's priority.
		This is a floating point number. Higher values indicate higher priority.
	@constant kSecAssessmentRuleKeyAllow A CFBoolean indicating whether the rule allows (true) or denies (false) the operation.
	@constant kSecAssessmentRuleKeyLabel An optional CFString labeling the rule. Multiple rules may have the same label;
		this can be used to group rules. Labels are not presented to the user. The label has no effect on evaluation.
	@constant kSecAssessmentRuleKeyRemarks An optional CFString containing user-readable text characterizing the rule's meaning.
		The remark has no effect on the evaluation.
	@constant kSecAssessmentRuleKeyRequirement A CFString containing the (text form of) the code requirement governing the rule's match.
	@constant kSecAssessmentRuleKeyType A CFString denoting the type of operation governed by the rule.
		One of the kSecAssessmentOperationType* constants.
	@constant kSecAssessmentRuleKeyExpires A CFDate indicating when the rule expires. Absent if the rule does not expire. Expired rules are never returned.
	@constant kSecAssessmentRuleKeyDisabled A CFNumber; non zero if temporarily disabled. Optional.
	@constant kSecAssessmentRuleKeyBookmark A CFData with the bookmark to the rule. Optional.
 */
extern CFStringRef kSecAssessmentContextKeyUpdate;		// proposed operation
extern CFStringRef kSecAssessmentUpdateOperationAdd;		// add rule to policy database
extern CFStringRef kSecAssessmentUpdateOperationRemove;	// remove rule from policy database
extern CFStringRef kSecAssessmentUpdateOperationEnable;	// enable rule(s) in policy database
extern CFStringRef kSecAssessmentUpdateOperationDisable;	// disable rule(s) in policy database
extern CFStringRef kSecAssessmentUpdateOperationFind;	// extract rule(s) from the policy database

extern CFStringRef kSecAssessmentUpdateKeyAuthorization;	// [CFData] external form of governing authorization

extern CFStringRef kSecAssessmentUpdateKeyPriority;		// rule priority
extern CFStringRef kSecAssessmentUpdateKeyLabel;			// rule label
extern CFStringRef kSecAssessmentUpdateKeyExpires;		// rule expiration
extern CFStringRef kSecAssessmentUpdateKeyAllow;			// rule outcome (allow/deny)
extern CFStringRef kSecAssessmentUpdateKeyRemarks;		// rule remarks (human readable)

extern CFStringRef kSecAssessmentUpdateKeyRow;			// rule identifier (CFNumber; add only)
extern CFStringRef kSecAssessmentUpdateKeyCount;			// count of changed rules (CFNumber)
extern CFStringRef kSecAssessmentUpdateKeyFound;			// set of found rules (CFArray of CFDictionaries)

extern CFStringRef kSecAssessmentRuleKeyID;				// rule content returned: rule ID
extern CFStringRef kSecAssessmentRuleKeyPriority;			// rule content returned: rule priority (floating point)
extern CFStringRef kSecAssessmentRuleKeyAllow;			// rule content returned: rule allows (boolean)
extern CFStringRef kSecAssessmentRuleKeyLabel;			// rule content returned: rule label (string; optional)
extern CFStringRef kSecAssessmentRuleKeyRemarks;			// rule content returned: rule remarks (string; optional)
extern CFStringRef kSecAssessmentRuleKeyRequirement;		// rule content returned: rule code requirement (string)
extern CFStringRef kSecAssessmentRuleKeyType;				// rule content returned: rule type (string)
extern CFStringRef kSecAssessmentRuleKeyExpires;			// rule content returned: rule expiration (CFDate; optional)
extern CFStringRef kSecAssessmentRuleKeyDisabled;			// rule content returned: rule disabled (CFNumber; nonzero means temporarily disabled)
extern CFStringRef kSecAssessmentRuleKeyBookmark;			// rule content returned: bookmark data (CFBookmark; optional)
	
CFDictionaryRef SecAssessmentCopyUpdate(CFTypeRef target,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors);

Boolean SecAssessmentUpdate(CFTypeRef target,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors);


/*!
	@function SecAssessmentControl
	Miscellaneous system policy operations.
	
	@param control A CFString indicating which operation is requested.
	@param arguments Arguments to the operation as documented for control.
	@param errors Standard CFErrorRef * argument to report errors.
	@result Returns True on success. Returns False on failure (and sets *errors).
 */
Boolean SecAssessmentControl(CFStringRef control, void *arguments, CFErrorRef *errors);

/*
 * SecAssessmentTicket SPI
 */
typedef uint64_t SecAssessmentTicketFlags;
enum {
	kSecAssessmentTicketFlagDefault = 0,				// default behavior, offline check
	kSecAssessmentTicketFlagForceOnlineCheck = 1 << 0,	// force an online check
	kSecAssessmentTicketFlagLegacyListCheck = 1 << 1, // Check the DeveloperID Legacy list
};
Boolean SecAssessmentTicketRegister(CFDataRef ticketData, CFErrorRef *errors);
Boolean SecAssessmentRegisterPackageTicket(CFURLRef packageURL, CFErrorRef* errors) API_AVAILABLE(macos(10.14.6));
Boolean SecAssessmentTicketLookup(CFDataRef hash, SecCSDigestAlgorithm hashType, SecAssessmentTicketFlags flags, double *date, CFErrorRef *errors);
Boolean SecAssessmentLegacyCheck(CFDataRef hash, SecCSDigestAlgorithm hashType, CFStringRef teamID, CFErrorRef *errors);

#ifdef __cplusplus
}
#endif

#endif //_H_SECASSESSMENT
