/*
 * Copyright (c) 2000-2004,2013-2014 Apple Inc. All Rights Reserved.
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

/*!
 @header SecPasswordGenerate
 SecPassword implements logic to use the system facilities for acquiring a password,
 optionally stored and retrieved from the user's keychain.
 */

#ifndef _SECURITY_SECPASSWORDGENERATE_H_
#define _SECURITY_SECPASSWORDGENERATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>

typedef uint32_t SecPasswordType;
enum {
    kSecPasswordTypeSafari = 0,
    kSecPasswordTypeiCloudRecovery = 1,
    kSecPasswordTypeWifi = 2,
    kSecPasswordTypePIN = 3,
    kSecPasswordTypeiCloudRecoveryKey __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.4) = 4,
} __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

// Keys for external dictionaries with password generation requirements we read from plist.
extern CFStringRef kSecPasswordDefaultForType
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

extern CFStringRef kSecPasswordMinLengthKey
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordMaxLengthKey
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordAllowedCharactersKey
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordRequiredCharactersKey
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

extern CFStringRef kSecPasswordDisallowedCharacters
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordCantStartWithChars
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordCantEndWithChars
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
    
extern CFStringRef kSecPasswordContainsNoMoreThanNSpecificCharacters
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordContainsAtLeastNSpecificCharacters
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
    
extern CFStringRef kSecPasswordCharacters
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordCharacterCount
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
    
extern CFStringRef kSecPasswordGroupSize
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordNumberOfGroups
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFStringRef kSecPasswordSeparator
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

    
/*
    @function SecPasswordCopyDefaultPasswordLength
    @abstract Returns the default length/number of tuples of a defaultly generated password
    @param type: default password types kSecPasswordTypeSafari, kSecPasswordTypeiCloudRecovery, kSecPasswordTypeWifi, kSecPasswordTypePIN
    @param error: An error code will be returned if an unrecognized password type is passed to the routine.
    @result Dictionary consisting of length of tuple and number of tuples or a NULL if the passed type isn't recognized.
*/
CFDictionaryRef SecPasswordCopyDefaultPasswordLength(SecPasswordType type, CFErrorRef *error)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

/*
 * Check that password is propery formated (groups, checksum). Make no claim about password quality.
 */
bool
SecPasswordValidatePasswordFormat(SecPasswordType type, CFStringRef password, CFErrorRef *error)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.4) __WATCHOS_AVAILABLE(3.4) __TVOS_AVAILABLE(10.4);

/*
 @function SecPasswordIsPasswordWeak
 @abstract Evalutes the weakness of a passcode. This function can take any type of passcode.  Currently
    the function evaluates passcodes with only ASCII characters
 @param passcode a string of any length and type (4 or 6 digit digit PIN, complex passcode)
 @result True if the password is weak, False if the password is strong.
 */

bool SecPasswordIsPasswordWeak(CFStringRef passcode)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

/*
@function SecPasswordIsPasswordWeak2
@abstract Evalutes the weakness of a passcode. This function can take any type of passcode.  Currently
the function evaluates passcodes with only ASCII characters
 ***conditions in which a passcode will be evaluated as weak***
 * all repeating characters
 * repeating 2 digits
 * is found in the black list of the top 10 most commonly used passcodes
 * incrementing digits
 * decrementing digits (including 0987)
 * low enough levels of entropy (complex passcodes)
@param passcode a string of any length and type (4 or 6 digit PIN, complex passcode)
@param isSimple is to indicate whether we're evaluating a 4 or 6 digit PIN or a complex passcode
@result True if the password is weak, False if the password is strong.
 */
    
bool SecPasswordIsPasswordWeak2(bool isSimple, CFStringRef passcode)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
    
/*
 @function SecPasswordGenerate.  Supports generating passwords for Safari, iCloud, Personal
 Hotspot clients.  Will also generate 4 or 6 digit pins.
 @abstract Returns a generated password based on a set of constraints
 @param type: type of password to generate. Pass enum types
 kSecPasswordTypeSafari, kSecPasswordTypeiCloudRecovery, kSecPasswordTypeWifi, or kSecPasswordTypePIN
 @param error: An error code will be returned if an error is encountered.  Check SecBase.h for the list of codes.
 @param passwordRequirements: a dictionary containing a set of password requirements.
 ex: password type 'safari' requires at least: minLength, maxLength, string
 of allowed characters, required characters
 @return NULL or a CFStringRef password

 *Note: This parameters is not required if kSecPasswordTypeiCloudRecovery or kSecPasswordTypePIN is supplied as the type.
 If kSecPasswordTypeSafari or kSecPasswordTypeWifi is supplied, you must include these dictionary key/value pairs:
 kSecPasswordMinLengthKey / CFNumberRef
 kSecPasswordMaxLengthKey / CFNumberRef
 kSecPasswordAllowedCharactersKey / CFStringRef
 kSecPasswordRequiredCharactersKey / CFArrayRef of CFCharacterSetRefs
 
 *Note: *If you would like a custom password type, file a bug in Sec Utilities requesting
 a new type along with generation specifications (ex. should contain one upper case, one lower case etc)
 
 *Note: Be sure to release the returned password when done using it.
 */
CF_RETURNS_RETAINED CFStringRef SecPasswordGenerate(SecPasswordType type, CFErrorRef *error, CFDictionaryRef passwordRequirements)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
    
CFStringRef SecPasswordCreateWithRandomDigits(int n, CFErrorRef *error)
     __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

#ifdef __cplusplus
}
#endif

#endif /* !_SECURITY_SECPASSWORDGENERATE_H_ */
