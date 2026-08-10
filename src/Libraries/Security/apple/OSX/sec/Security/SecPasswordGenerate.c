/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 * SecPasswordStrength.c
 */

#include <limits.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <Security/SecRandom.h>
#include "SecPasswordGenerate.h"
#include <AssertMacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include "SecCFAllocator.h"


// Keys for external dictionaries with password generation requirements we read from plist.
CFStringRef kSecPasswordMinLengthKey = CFSTR("PasswordMinLength");
CFStringRef kSecPasswordMaxLengthKey = CFSTR("PasswordMaxLength");
CFStringRef kSecPasswordAllowedCharactersKey = CFSTR("PasswordAllowedCharacters");
CFStringRef kSecPasswordRequiredCharactersKey = CFSTR("PasswordRequiredCharacters");
CFStringRef kSecPasswordDefaultForType = CFSTR("PasswordDefaultForType");

CFStringRef kSecPasswordDisallowedCharacters = CFSTR("PasswordDisallowedCharacters");
CFStringRef kSecPasswordCantStartWithChars = CFSTR("PasswordCantStartWithChars");
CFStringRef kSecPasswordCantEndWithChars = CFSTR("PasswordCantEndWithChars");
CFStringRef kSecPasswordContainsNoMoreThanNSpecificCharacters = CFSTR("PasswordContainsNoMoreThanNSpecificCharacters");
CFStringRef kSecPasswordContainsAtLeastNSpecificCharacters = CFSTR("PasswordContainsAtLeastNSpecificCharacters");
CFStringRef kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters = CFSTR("PasswordContainsNoMoreThanNConsecutiveIdenticalCharacters");
CFStringRef kSecPasswordCharacterCount = CFSTR("PasswordCharacterCount");
CFStringRef kSecPasswordCharacters = CFSTR("PasswordCharacters");

CFStringRef kSecPasswordGroupSize = CFSTR("PasswordGroupSize");
CFStringRef kSecPasswordNumberOfGroups = CFSTR("PasswordNumberOfGroups");
CFStringRef kSecPasswordSeparator = CFSTR("SecPasswordSeparator");

// Keys for internally used dictionaries with password generation parameters (never exposed to external API).
static CFStringRef kSecUseDefaultPasswordFormatKey = CFSTR("UseDefaultPasswordFormat");
static CFStringRef kSecNumberOfRequiredRandomCharactersKey = CFSTR("NumberOfRequiredRandomCharacters");
static CFStringRef kSecNumberOfChecksumCharactersKey = CFSTR("NumberOfChecksumCharacters");
static CFStringRef kSecAllowedCharactersKey = CFSTR("AllowedCharacters");
static CFStringRef kSecRequiredCharacterSetsKey = CFSTR("RequiredCharacterSets");

static CFIndex defaultNumberOfRandomCharacters = 20;
static CFIndex defaultPINLength = 4;
static CFIndex defaultiCloudPasswordLength = 24;
static CFIndex defaultWifiPasswordLength = 12;

static CFStringRef defaultWifiCharacters = CFSTR("abcdefghijklmnopqrstuvwxyz1234567890");
static CFStringRef defaultPINCharacters = CFSTR("0123456789");
static CFStringRef defaultiCloudCharacters = CFSTR("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
static CFStringRef defaultCharacters = CFSTR("abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ123456789");

static CFCharacterSetRef uppercaseLetterCharacterSet;
static CFCharacterSetRef lowercaseLetterCharacterSet;
static CFCharacterSetRef decimalDigitCharacterSet;
static CFCharacterSetRef punctuationCharacterSet;

static CFIndex alphabetSetSize = 26;
static CFIndex decimalSetSize = 10;
static CFIndex punctuationSetSize = 33;
static double entropyStrengthThreshold = 35.0;

/*
 generated with ruby badpins.rb | gperf
 See this for PIN list:
 A birthday present every eleven wallets? The security of customer-chosen banking PINs (2012),  by Joseph Bonneau , Sören Preibusch , Ross Anderson
 */
const char *in_word_set (const char *str, unsigned int len);

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
&& ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
&& (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
&& ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
&& ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
&& ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
&& ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
&& ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
&& ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
&& ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
&& ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
&& ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
&& ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
&& ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
&& ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
&& ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
&& ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
&& ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
&& ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
&& ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
&& ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
&& ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
&& ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif


#define TOTAL_KEYWORDS 100
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 4
#define MIN_HASH_VALUE 21
#define MAX_HASH_VALUE 275
/* maximum key range = 255, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int pinhash (const char *str, unsigned int len)
{
    static unsigned short asso_values[] =
    {
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276,   5,   0,
        10,  10,  30,  50, 100, 120,  70,  25,  57,  85,
        2,   4,   1,  19,  14,  11,  92, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276, 276, 276, 276, 276, 276,
        276, 276, 276, 276, 276
    };
    return len + asso_values[(unsigned char)str[3]+9] + asso_values[(unsigned char)str[2]] + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]+3];
}

///<://problem/29089896> warn against using these common PINs
static bool isTopTenSixDigitPasscode(CFStringRef passcode){
    bool result = false;
    CFMutableArrayRef topTen = CFArrayCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                  CFSTR("030379"),
                                                                  CFSTR("101471"),
                                                                  CFSTR("112233"),
                                                                  CFSTR("123123"),
                                                                  CFSTR("123321"),
                                                                  CFSTR("123654"),
                                                                  CFSTR("147258"),
                                                                  CFSTR("159753"),
                                                                  CFSTR("321654"),
                                                                  CFSTR("520131"),
                                                                  CFSTR("520520"),
                                                                  CFSTR("789456"), NULL);

    for(CFIndex i = 0; i < CFArrayGetCount(topTen); i++){
        if(CFEqualSafe(passcode, CFArrayGetValueAtIndex(topTen, i))){
            result = true;
            break;
        }
    }
    CFReleaseNull(topTen);
    return result;
}

CFStringRef SecPasswordCreateWithRandomDigits(int n, CFErrorRef *error){
    int min = n;
    int max = n;

    uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
    lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
    decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
    punctuationCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetPunctuation);

    CFNumberRef minRef = CFNumberCreate(NULL, kCFNumberIntType, &min);
    CFNumberRef maxRef = CFNumberCreate(NULL, kCFNumberIntType, &max);

    CFMutableDictionaryRef passwordRequirements = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMinLengthKey, minRef);
    CFDictionaryAddValue(passwordRequirements, kSecPasswordMaxLengthKey, maxRef);
    CFStringRef allowedCharacters = CFSTR("0123456789");

    CFDictionaryAddValue(passwordRequirements, kSecPasswordAllowedCharactersKey, allowedCharacters);

    CFStringRef password = SecPasswordGenerate(kSecPasswordTypePIN, error, passwordRequirements);

    CFReleaseNull(minRef);
    CFReleaseNull(maxRef);
    CFReleaseNull(passwordRequirements);

    return password;
}



//pins that reached the top 20 list
static const char *blacklist[] = {"1234", "1004", "2000", "1122", "4321", "2001", "2580"};
bool SecPasswordIsPasswordWeak(CFStringRef passcode)
{
    uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
    lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
    decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
    punctuationCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetPunctuation);

    bool isNumber = true;
    char* pin = NULL;

    //length checks
    if( CFStringGetLength(passcode) < 4 ){
        return true; //weak password
    }
    //check to see if passcode is a number
    for(CFIndex i = 0; i < CFStringGetLength(passcode); i++){
        if( CFStringFindCharacterFromSet(passcode, decimalDigitCharacterSet, CFRangeMake(i,1), 0, NULL))
            continue;
        else {
            isNumber = false;
            break;
        }
    }
    //checking to see if it's a 4 digit pin
    if(isNumber && CFStringGetLength(passcode) == 4){

        pin = CFStringToCString(passcode);
        if(in_word_set(pin, 4)){
            free(pin);
            return true;
        }

        CFIndex blacklistLength = (CFIndex)sizeof(blacklist)/sizeof(blacklist[0]);

        //not all the same number
        if(pin[0] == pin[1] == pin[2] == pin[3]){
            free(pin);
            return true; //weak password
        }
        //first two digits being the same and the last two digits being the same
        if ( pin[0] == pin[1] && pin[2] == pin[3]){
            free(pin);
            return true; //weak password
        }
        //first two digits not being the same as the last two digits
        if(pin[0] == pin[2] && pin[1] == pin[3]){
            free(pin);
            return true; //weak password
        }
        //check if PIN is a bunch of incrementing numbers
        for(int i = 0; i < CFStringGetLength(passcode); i++){
            if(i == CFStringGetLength(passcode)-1){
                free(pin);
                return true;
            }
            else if ((pin[i] + 1) == pin[i+1])
                continue;
            else
                break;
        }
        //check if PIN is a bunch of decrementing numbers
        for(int i = 0; i < CFStringGetLength(passcode); i++){
            if(i == CFStringGetLength(passcode)-1){
                free(pin);
                return true;
            }
            else if ((pin[i]) == (pin[i+1] +1))
                continue;
            else if ((i == 0) && (pin[i] == '0') && (pin[i+1] == '9'))
                continue;
            else
                break;
        }

        //not in this list
        for(CFIndex i = 0; i < blacklistLength; i++)
        {
            const char* blackCode = blacklist[i];
            if(0 == strcmp(blackCode, pin))
            {
                free(pin);
                return true; //weak password
            }
        }
    }
    else if(isNumber){ //dealing with a numeric PIN
        pin = CFStringToCString(passcode);
        //check if PIN is all the same number
        for(int i = 0; i < CFStringGetLength(passcode); i++){
            if(i+1 >= CFStringGetLength(passcode)){
                free(pin);
                return true;
            }
            else if (pin[i] == pin[i+1])
                continue;
            else
                break;
        }
        //check if PIN is a bunch of incrementing numbers
        for(int i = 0; i < CFStringGetLength(passcode); i++){
            if(i == CFStringGetLength(passcode)-1){
                free(pin);
                return true;
            }
            else if ((pin[i] + 1) == pin[i+1])
                continue;
            else
                break;
        }
        //check if PIN is a bunch of decrementing numbers
        for(int i = 0; i < CFStringGetLength(passcode); i++){
            if(i == CFStringGetLength(passcode)-1){
                free(pin);
                return true;
            }
            else if ((pin[i]) == (pin[i+1] +1))
                continue;
            else if ((i == 0) && (pin[i] == '0') && (pin[i+1] == '9'))
                continue;
            else
                break;
        }
    }
    else{ // password is complex, evaluate entropy
        int u = 0;
        int l = 0;
        int d = 0;
        int p = 0;
        int characterSet = 0;

        //calculate new entropy
        for(CFIndex i = 0; i < CFStringGetLength(passcode); i++){

            if( CFStringFindCharacterFromSet(passcode, uppercaseLetterCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                u++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, lowercaseLetterCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                l++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, decimalDigitCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                d++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, punctuationCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                p++;
                continue;
            }

        }
        if(u > 0){
            characterSet += alphabetSetSize;
        }
        if(l > 0){
            characterSet += alphabetSetSize;
        }
        if(d > 0){
            characterSet += decimalSetSize;
        }
        if(p > 0){
            characterSet += punctuationSetSize;
        }

        double strength = CFStringGetLength(passcode)*log2(characterSet);

        if(strength < entropyStrengthThreshold){
            return true; //weak
        }
        else
            return false; //strong
    }
    if(pin)
        free(pin);

    return false; //strong password

}

static bool SecPasswordIsPasscodeIncrementingOrDecrementingDigits(CFStringRef passcode)
{
    char* pin = CFStringToCString(passcode);

    //check if PIN is a bunch of incrementing numbers
    for(int i = 0; i < CFStringGetLength(passcode); i++){
        if(i == CFStringGetLength(passcode)-1){
            free(pin);
            return true;
        }
        else if ((pin[i] + 1) == pin[i+1])
            continue;
        else
            break;
    }
    //check if PIN is a bunch of decrementing numbers
    for(int i = 0; i < CFStringGetLength(passcode); i++){
        if(i == CFStringGetLength(passcode)-1){
            free(pin);
            return true;
        }
        else if ((pin[i]) == (pin[i+1] +1))
            continue;
        else if ((i == 0) && (pin[i] == '0') && (pin[i+1] == '9'))
            continue;
        else
            break;
    }
    free(pin);
    return false;
}

static bool SecPasswordIsPasswordRepeatingTwoNumbers(CFStringRef passcode){
    char* pin = CFStringToCString(passcode);

    for(int i = 0; i < CFStringGetLength(passcode); i++)
    {
        if(i+2 == CFStringGetLength(passcode)-1){
            free(pin);
            return true;
        }
        else if(pin[i] == pin[i+2])
            continue;
        else
            break;
    }
    
    free(pin);
    return false;
}

static int SecPasswordNumberOfRepeatedDigits(CFStringRef passcode){
    int repeating = 1;
    CFIndex length = CFStringGetLength(passcode);
    CFNumberRef highest = NULL;
    CFMutableArrayRef highestRepeatingcount = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    for(int i = 0; i < length; i++){

        if(i+1 == length){
            CFNumberRef newRepeatingAddition = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &repeating);
            CFArrayAppendValue(highestRepeatingcount, newRepeatingAddition);
            CFReleaseNull(newRepeatingAddition);
            break;
        }
        if(CFStringGetCharacterAtIndex(passcode, i) == CFStringGetCharacterAtIndex(passcode,i+1))
            repeating++;
        else{
            if(repeating != 1)
            {
                CFNumberRef newRepeatingAddition = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &repeating);
                CFArrayAppendValue(highestRepeatingcount, newRepeatingAddition);
                CFReleaseNull(newRepeatingAddition);
            }
            repeating = 1;

        }
    }

    for(int i =0; i< CFArrayGetCount(highestRepeatingcount); i++){
        if(i == 0){
            highest = CFArrayGetValueAtIndex(highestRepeatingcount, i);
            continue;
        }
        else{
            CFNumberRef competitor = CFArrayGetValueAtIndex(highestRepeatingcount, i);
            if(CFNumberCompare(competitor, highest, NULL) == kCFCompareGreaterThan){
                highest = competitor;
            }
        }
    }
    int finalRepeating = 0;
    if(highest != NULL)
        CFNumberGetValue(highest, kCFNumberIntType, &finalRepeating);

    CFReleaseNull(highestRepeatingcount);
    return finalRepeating;
}

static bool SecPasswordIsPalindrome(CFStringRef passcode){
    char* pin = CFStringToCString(passcode);
    long length = CFStringGetLength(passcode);
    long j = length-1;

    for(int i = 0; i < CFStringGetLength(passcode); i++)
    {
        if(length%2 == 1 && i == j){
            free(pin);
            return true;
        }
        else if(length%2 == 0 && i == j-1){
            if(pin[i] == pin[j]){
                free(pin);
                return true;
            }
            else
                break;
        }
        else if(pin[i] == pin[j]){
            j--;
            continue;
        }
        else
            break;
    }
    free(pin);
    return false;
}

static bool SecPasswordHasRepeatingGroups(CFStringRef passcode){
    char* pin = CFStringToCString(passcode);

    for(int i = 0; i < CFStringGetLength(passcode); i++)
    {
        if(i+4 == CFStringGetLength(passcode)){
            if(pin[i] == pin[i+3]){
                free(pin);
                return true;
            }
            else
                break;
        }
        else if(pin[i] == pin[i+3])
            continue;
        else
            break;
    }

    free(pin);

    return false;
}

bool SecPasswordIsPasswordWeak2(bool isSimple, CFStringRef passcode)
{
    uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
    lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
    decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
    punctuationCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetPunctuation);


    char* pin = NULL;

    //length checks
    if( CFStringGetLength(passcode) < 4 ){
        return true; //weak password
    }

    bool isPasscodeNumber = true;
    //check to see if passcode is a number
    for(CFIndex i = 0; i < CFStringGetLength(passcode); i++){
        if( CFStringFindCharacterFromSet(passcode, decimalDigitCharacterSet, CFRangeMake(i,1), 0, NULL))
            continue;
        else {
            isPasscodeNumber = false;
            break;
        }
    }

    if(isSimple){
        //checking to see if it's a 4 digit pin
        if(isPasscodeNumber && CFStringGetLength(passcode) == 4){

            pin = CFStringToCString(passcode);
            if(in_word_set(pin, 4)){
                free(pin);
                return true;
            }

            CFIndex blacklistLength = (CFIndex)sizeof(blacklist)/sizeof(blacklist[0]);

            //not all the same number
            if(pin[0] == pin[1] == pin[2] == pin[3]){
                free(pin);
                return true; //weak password
            }
            //first two digits being the same and the last two digits being the same
            if ( pin[0] == pin[1] && pin[2] == pin[3]){
                free(pin);
                return true; //weak password
            }
            //first two digits not being the same as the last two digits
            if(pin[0] == pin[2] && pin[1] == pin[3]){
                free(pin);
                return true; //weak password
            }
            //not in this list
            for(CFIndex i = 0; i < blacklistLength; i++)
            {
                const char* blackCode = blacklist[i];
                if(0 == strcmp(blackCode, pin))
                {
                    free(pin);
                    return true; //weak password
                }
            }
        }
        else if(isPasscodeNumber && CFStringGetLength(passcode) == 6){
            pin = CFStringToCString(passcode);

            //not all the same number
            for(int i = 0; i < CFStringGetLength(passcode); i++){
                if(i == CFStringGetLength(passcode)-1){
                    free(pin);
                    return true;
                }
                else if ((pin[i]) == pin[i+1])
                    continue;
                else
                    break;
            }
            //not in the top 10
            if(isTopTenSixDigitPasscode(passcode)){
                free(pin);
                return true;
            }
            //palindrome test
            if(SecPasswordIsPalindrome(passcode)){
                free(pin);
                return true;
            }

            //2 identical groups
            if(SecPasswordHasRepeatingGroups(passcode)){
                free(pin);
                return true;
            }
            //passcode is incrementing ex 123456 or 654321
            if(SecPasswordIsPasscodeIncrementingOrDecrementingDigits(passcode)) {
                free(pin);
                return true;
            }
            //passcode does not consist of 2 repeating digits
            if(SecPasswordIsPasswordRepeatingTwoNumbers(passcode)){
                free(pin);
                return true;
            }
        }
        else//should be a 4 or 6digit number
            return false;
    }
    else if(isPasscodeNumber && !isSimple){ //dealing with a complex numeric passcode
        pin = CFStringToCString(passcode);
        //check if PIN is all the same number
        int repeatingDigits = SecPasswordNumberOfRepeatedDigits(passcode);
        if(repeatingDigits >= (CFStringGetLength(passcode)/2)){
            free(pin);
            return true;
        }
        //palindrome test
        if(SecPasswordIsPalindrome(passcode)){
            free(pin);
            return true;
        }
        //not in the top 10
        if(isTopTenSixDigitPasscode(passcode)){
            free(pin);
            return true;
        }
        //2 identical groups
        if(SecPasswordHasRepeatingGroups(passcode) && CFStringGetLength(passcode) >= 6){
            free(pin);
            return true;
        }

        if(SecPasswordIsPasscodeIncrementingOrDecrementingDigits(passcode)) {
            free(pin);
            return true;
        }
        if(SecPasswordIsPasswordRepeatingTwoNumbers(passcode)){
            free(pin);
            return true;
        }

    }
    else{ // password is complex, evaluate entropy
        int u = 0;
        int l = 0;
        int d = 0;
        int p = 0;
        int characterSet = 0;

        //calculate new entropy
        for(CFIndex i = 0; i < CFStringGetLength(passcode); i++){

            if( CFStringFindCharacterFromSet(passcode, uppercaseLetterCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                u++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, lowercaseLetterCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                l++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, decimalDigitCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                d++;
                continue;
            }
            if( CFStringFindCharacterFromSet(passcode, punctuationCharacterSet, CFRangeMake(i,1), kCFCompareBackwards, NULL)){
                p++;
                continue;
            }

        }
        if(u > 0){
            characterSet += alphabetSetSize;
        }
        if(l > 0){
            characterSet += alphabetSetSize;
        }
        if(d > 0){
            characterSet += decimalSetSize;
        }
        if(p > 0){
            characterSet += punctuationSetSize;
        }

        double strength = CFStringGetLength(passcode)*log2(characterSet);

        if(strength < entropyStrengthThreshold){
            return true; //weak
        }
        else
            return false; //strong
    }
    if(pin)
        free(pin);

    return false; //strong password

}

static void getUniformRandomNumbers(uint8_t* buffer, size_t numberOfDesiredNumbers, uint8_t upperBound)
{

    // The values returned by SecRandomCopyBytes are uniformly distributed in the range [0, 255]. If we try to map
    // these values onto a smaller range using modulo we will introduce a bias towards lower numbers in situations
    // where our smaller range doesn’t evenly divide in to [0, 255]. For example, with the desired range of [0, 54]
    // the ranges 0..54, 55..109, 110..164, and 165..219 are uniformly distributed, but the range 220..255 modulo 55
    // is only distributed over [0, 35], giving significant bias to these lower values. So, we ignore random numbers
    // that would introduce this bias.
    uint8_t limitAvoidingModuloBias = UCHAR_MAX - (UCHAR_MAX % upperBound);

    for (size_t numberOfAcceptedNumbers = 0; numberOfAcceptedNumbers < numberOfDesiredNumbers; ) {
        if (SecRandomCopyBytes(kSecRandomDefault, numberOfDesiredNumbers - numberOfAcceptedNumbers, buffer + numberOfAcceptedNumbers) == -1)
            continue;
        for (size_t i = numberOfAcceptedNumbers; i < numberOfDesiredNumbers; ++i) {
            if (buffer[i] < limitAvoidingModuloBias)
                buffer[numberOfAcceptedNumbers++] = buffer[i] % upperBound;
        }
    }
}

static bool passwordContainsRequiredCharacters(CFStringRef password, CFArrayRef requiredCharacterSets)
{
    CFCharacterSetRef characterSet;

    for (CFIndex i = 0; i< CFArrayGetCount(requiredCharacterSets); i++) {
        characterSet = CFArrayGetValueAtIndex(requiredCharacterSets, i);
        CFRange rangeToSearch = CFRangeMake(0, CFStringGetLength(password));
        require_quiet(CFStringFindCharacterFromSet(password, characterSet, rangeToSearch, 0, NULL), fail);
    }
    return true;

fail:
    return false;

}

static bool passwordContainsLessThanNIdenticalCharacters(CFStringRef password, CFIndex identicalCount)
{
    unsigned char Char, nextChar;
    int repeating = 0;

    for(CFIndex i = 0; i < CFStringGetLength(password); i++){
        Char = CFStringGetCharacterAtIndex(password, i);
        for(CFIndex j = i; j< CFStringGetLength(password); j++){
            nextChar = CFStringGetCharacterAtIndex(password, j);
            require_quiet(repeating <= identicalCount, fail);
            if(Char == nextChar){
                repeating++;
            }else{
                repeating = 0;
                break;
            }
        }
    }
    return true;
fail:
    return false;
}

static bool passwordContainsAtLeastNCharacters(CFStringRef password, CFStringRef characters, CFIndex N)
{
    CFCharacterSetRef characterSet = NULL;
    characterSet = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, characters);
    CFIndex counter = 0;

    for(CFIndex i = 0; i < CFStringGetLength(password); i++){
        if(CFStringFindCharacterFromSet(password, characterSet, CFRangeMake(i, 1), 0, NULL))
            counter++;
    }
    CFReleaseNull(characterSet);
    if(counter < N)
        return false;
    else
        return true;
}

static bool passwordContainsLessThanNCharacters(CFStringRef password, CFStringRef characters, CFIndex N)
{
    CFCharacterSetRef characterSet = NULL;
    characterSet = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, characters);
    CFIndex counter = 0;

    for(CFIndex i = 0; i < CFStringGetLength(password); i++){
        if(CFStringFindCharacterFromSet(password, characterSet, CFRangeMake(i, 1), 0, NULL))
            counter++;
    }
    CFReleaseNull(characterSet);
    if(counter > N)
        return false;
    else
        return true;
}

static bool passwordDoesNotContainCharacters(CFStringRef password, CFStringRef prohibitedCharacters)
{
    CFCharacterSetRef characterSet = NULL;
    characterSet = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, prohibitedCharacters);
    CFRange rangeToSearch = CFRangeMake(0, CFStringGetLength(password));

    require_quiet(!CFStringFindCharacterFromSet(password, characterSet, rangeToSearch, 0, NULL), fail);
    CFReleaseNull(characterSet);
    return true;
fail:
    CFReleaseNull(characterSet);
    return false;
}

static OSStatus getPasswordRandomCharacters(CFStringRef *returned, CFDictionaryRef requirements, CFIndex *numberOfRandomCharacters, CFStringRef allowedCharacters)
{
    uint8_t *randomNumbers = malloc(*numberOfRandomCharacters);
    unsigned char *randomCharacters = malloc(*numberOfRandomCharacters);
    
    if (randomNumbers == NULL || randomCharacters == NULL) {
        free(randomNumbers);
        free(randomCharacters);
        return errSecMemoryError;
    }
    
    getUniformRandomNumbers(randomNumbers, *numberOfRandomCharacters, CFStringGetLength(allowedCharacters));

    CFTypeRef prohibitedCharacters = NULL;
    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordDisallowedCharacters, &prohibitedCharacters))
        prohibitedCharacters = NULL;

    //it's faster for long characters to check each character produced for these cases
    for (CFIndex i = 0; i < *numberOfRandomCharacters; ++i){
        //check prohibited characters
        UniChar randomChar[1];
        randomChar[0] = CFStringGetCharacterAtIndex(allowedCharacters, randomNumbers[i]);
        if (prohibitedCharacters != NULL)
        {
            CFStringRef temp = CFStringCreateWithCharacters(kCFAllocatorDefault, randomChar, 1);
            bool pwdncc = passwordDoesNotContainCharacters(temp, prohibitedCharacters);
            CFReleaseSafe(temp);
            if (!pwdncc) {
                //change up the random numbers so we don't get the same index into allowed
                getUniformRandomNumbers(randomNumbers, *numberOfRandomCharacters, CFStringGetLength(allowedCharacters));
                i--;
                continue;
            }
        }
        randomCharacters[i] = (unsigned char)randomChar[0];
    }

    *returned = CFStringCreateWithBytes(kCFAllocatorDefault, randomCharacters, *numberOfRandomCharacters, kCFStringEncodingUTF8, false);
    
    free(randomCharacters);
    free(randomNumbers);
    
    return errSecSuccess;
}

static bool doesPasswordEndWith(CFStringRef password, CFStringRef prohibitedCharacters)
{
    CFCharacterSetRef characterSet = NULL;
    characterSet = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, prohibitedCharacters);

    CFRange rangeToSearch = CFRangeMake(CFStringGetLength(password) - CFStringGetLength(prohibitedCharacters), CFStringGetLength(prohibitedCharacters));
    require_quiet(0 == CFStringCompareWithOptions(password, prohibitedCharacters, rangeToSearch, 0), fail);
    CFReleaseNull(characterSet);
    return false;
fail:
    CFReleaseNull(characterSet);
    return true;
}

static bool doesPasswordStartWith(CFStringRef password, CFStringRef prohibitedCharacters)
{
    CFCharacterSetRef characterSet = NULL;
    characterSet = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, prohibitedCharacters);

    CFRange rangeToSearch = CFRangeMake(0, CFStringGetLength(prohibitedCharacters));
    require_quiet(0 == CFStringCompareWithOptions(password, prohibitedCharacters, rangeToSearch, 0), fail);
    CFReleaseNull(characterSet);
    return false; //does not start with prohibitedCharacters
fail:
    CFReleaseNull(characterSet);
    return true;
}

static CFDictionaryRef passwordGenerateCreateDefaultParametersDictionary(SecPasswordType type, CFDictionaryRef requirements){

    CFMutableArrayRef requiredCharacterSets = NULL;
    CFNumberRef numReqChars = NULL, checksumChars = NULL;
    CFStringRef defaultPasswordFormat = NULL;
    requiredCharacterSets = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    defaultPasswordFormat = CFSTR("true");
    CFTypeRef groupSizeRef = NULL, numberOfGroupsRef = NULL;
    CFIndex groupSize, numberOfGroups, checksumSize = 0;
    CFDictionaryRef returned = NULL;

    switch(type){
        case kSecPasswordTypeiCloudRecoveryKey:
            groupSize = 4;
            numberOfGroups = 7;
            checksumSize = 2;
            numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, (groupSize * numberOfGroups) - checksumSize);
            checksumChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, checksumSize);
            groupSizeRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &groupSize);
            numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &numberOfGroups);

            uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
            CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
            returned = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                    kSecUseDefaultPasswordFormatKey,   defaultPasswordFormat,
                                                    kSecNumberOfRequiredRandomCharactersKey, numReqChars,
                                                    kSecNumberOfChecksumCharactersKey, checksumChars,
                                                    kSecAllowedCharactersKey,   defaultiCloudCharacters,
                                                    kSecRequiredCharacterSetsKey, requiredCharacterSets,
                                                    kSecPasswordGroupSize, groupSizeRef,
                                                    kSecPasswordNumberOfGroups, numberOfGroupsRef,
                                                    NULL);
            break;
        case(kSecPasswordTypeiCloudRecovery):
            numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, defaultiCloudPasswordLength);
            groupSize = 4;
            numberOfGroups = 6;
            groupSizeRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &groupSize);
            numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &numberOfGroups);

            uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
            CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
            returned = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                    kSecUseDefaultPasswordFormatKey,   defaultPasswordFormat,
                                                    kSecNumberOfRequiredRandomCharactersKey, numReqChars,
                                                    kSecAllowedCharactersKey,   defaultiCloudCharacters,
                                                    kSecRequiredCharacterSetsKey, requiredCharacterSets,
                                                    kSecPasswordGroupSize, groupSizeRef,
                                                    kSecPasswordNumberOfGroups, numberOfGroupsRef,
                                                    NULL);
            break;

        case(kSecPasswordTypePIN):
            numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, defaultPINLength);
            groupSize = 4;
            numberOfGroups = 1;
            groupSizeRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &groupSize);
            numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &numberOfGroups);

            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
            returned = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                         kSecUseDefaultPasswordFormatKey,   defaultPasswordFormat,
                                                                         kSecNumberOfRequiredRandomCharactersKey, numReqChars,
                                                                         kSecAllowedCharactersKey,   defaultPINCharacters,
                                                                         kSecRequiredCharacterSetsKey, requiredCharacterSets,
                                                                         kSecPasswordGroupSize, groupSizeRef,
                                                                         kSecPasswordNumberOfGroups, numberOfGroupsRef,
                                                                         NULL);
            break;

        case(kSecPasswordTypeWifi):
            groupSize = 4;
            numberOfGroups = 3;
            groupSizeRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &groupSize);
            numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &numberOfGroups);

            lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);

            numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, defaultWifiPasswordLength);
            CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
            returned = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                         kSecUseDefaultPasswordFormatKey,   defaultPasswordFormat,
                                                                         kSecNumberOfRequiredRandomCharactersKey, numReqChars,
                                                                         kSecAllowedCharactersKey,   defaultWifiCharacters,
                                                                         kSecRequiredCharacterSetsKey, requiredCharacterSets,
                                                                         kSecPasswordGroupSize, groupSizeRef,
                                                                         kSecPasswordNumberOfGroups, numberOfGroupsRef,
                                                                         NULL);
            break;

        default:
            groupSize = 4;
            numberOfGroups = 6;
            groupSizeRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &groupSize);
            numberOfGroupsRef = CFNumberCreate(NULL, kCFNumberCFIndexType, &numberOfGroups);
            uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
            lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
            CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);

            numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, defaultNumberOfRandomCharacters);
            returned = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                         kSecUseDefaultPasswordFormatKey,   defaultPasswordFormat,
                                                                         kSecNumberOfRequiredRandomCharactersKey, numReqChars,
                                                                         kSecAllowedCharactersKey,   defaultCharacters,
                                                                         kSecRequiredCharacterSetsKey, requiredCharacterSets,
                                                                         kSecPasswordGroupSize, groupSizeRef,
                                                                         kSecPasswordNumberOfGroups, numberOfGroupsRef,
                                                                         NULL);



            break;
    }

    CFReleaseNull(numReqChars);
    CFReleaseNull(requiredCharacterSets);
    CFReleaseNull(groupSizeRef);
    CFReleaseNull(numberOfGroupsRef);
    CFReleaseNull(checksumChars);
    return returned;
}
static CFDictionaryRef passwordGenerationCreateParametersDictionary(SecPasswordType type, CFDictionaryRef requirements)
{
    CFMutableArrayRef requiredCharacterSets = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFNumberRef numReqChars = NULL;
    CFIndex numberOfRequiredRandomCharacters;
    CFStringRef allowedCharacters = NULL, useDefaultPasswordFormat = NULL;
    uint64_t valuePtr;
    CFTypeRef prohibitedCharacters = NULL, endWith = NULL, startWith = NULL,
            groupSizeRef = NULL, numberOfGroupsRef = NULL, separatorRef = NULL,
            atMostCharactersRef = NULL,atLeastCharactersRef = NULL, identicalRef = NULL;

    CFNumberRef min = (CFNumberRef)CFDictionaryGetValue(requirements, kSecPasswordMinLengthKey);
    CFNumberRef max = (CFNumberRef)CFDictionaryGetValue(requirements, kSecPasswordMaxLengthKey);

    CFNumberGetValue(min, kCFNumberSInt64Type, &valuePtr);
    CFIndex minPasswordLength = (long)valuePtr;
    CFNumberGetValue(max, kCFNumberSInt64Type, &valuePtr);
    CFIndex maxPasswordLength = (long)valuePtr;

    // If requirements allow, we will generate the password in default format.
    useDefaultPasswordFormat = CFSTR("true");
    numberOfRequiredRandomCharacters = defaultNumberOfRandomCharacters;

    if(type == kSecPasswordTypePIN)
    {
        if( maxPasswordLength && minPasswordLength )
            numberOfRequiredRandomCharacters = maxPasswordLength;
        else if( !maxPasswordLength && minPasswordLength )
            numberOfRequiredRandomCharacters = minPasswordLength;
        else if( !minPasswordLength && maxPasswordLength )
            numberOfRequiredRandomCharacters = maxPasswordLength;
        else
            numberOfRequiredRandomCharacters = defaultPINLength;

        allowedCharacters = CFSTR("0123456789");
        CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
        useDefaultPasswordFormat = CFSTR("false");
    }
    else{
        CFArrayRef requiredCharactersArray = NULL;

        if (minPasswordLength && minPasswordLength > defaultNumberOfRandomCharacters) {
            useDefaultPasswordFormat = CFSTR("false");
            numberOfRequiredRandomCharacters = minPasswordLength;
        }
        if (maxPasswordLength && maxPasswordLength < defaultNumberOfRandomCharacters) {
            useDefaultPasswordFormat = CFSTR("false");
            numberOfRequiredRandomCharacters = maxPasswordLength;
        }
        if (maxPasswordLength && minPasswordLength && maxPasswordLength == minPasswordLength && maxPasswordLength != defaultNumberOfRandomCharacters){
            useDefaultPasswordFormat = CFSTR("false");
            numberOfRequiredRandomCharacters = maxPasswordLength;
        }
        allowedCharacters = (CFStringRef)CFRetainSafe(CFDictionaryGetValue(requirements, kSecPasswordAllowedCharactersKey));
        requiredCharactersArray = (CFArrayRef)CFDictionaryGetValue(requirements, kSecPasswordRequiredCharactersKey);

        if (requiredCharactersArray) {
            for (CFIndex i = 0; i < CFArrayGetCount(requiredCharactersArray); i++){
                CFCharacterSetRef stringWithRequiredCharacters = CFArrayGetValueAtIndex(requiredCharactersArray, i);
                if(stringWithRequiredCharacters && CFStringFindCharacterFromSet(allowedCharacters, stringWithRequiredCharacters, CFRangeMake(0, CFStringGetLength(allowedCharacters)), 0, NULL)){
                    CFArrayAppendValue(requiredCharacterSets, stringWithRequiredCharacters);
                }
            }
        } else{
            uppercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetUppercaseLetter);
            lowercaseLetterCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetLowercaseLetter);
            decimalDigitCharacterSet = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
            CFArrayAppendValue(requiredCharacterSets, uppercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, lowercaseLetterCharacterSet);
            CFArrayAppendValue(requiredCharacterSets, decimalDigitCharacterSet);
        }
    }
    
    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordDisallowedCharacters, &prohibitedCharacters))
        prohibitedCharacters = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordCantEndWithChars, &endWith))
        endWith = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordCantStartWithChars, &startWith))
        startWith = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordGroupSize, &groupSizeRef))
        groupSizeRef = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordNumberOfGroups, &numberOfGroupsRef))
        numberOfGroupsRef = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordSeparator, &separatorRef))
        separatorRef = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, &atMostCharactersRef))
        atMostCharactersRef = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsAtLeastNSpecificCharacters, &atLeastCharactersRef))
        atLeastCharactersRef = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, &identicalRef))
        identicalRef = NULL;

    if (allowedCharacters) {
        if( false == CFStringFindWithOptions(allowedCharacters, CFSTR("-"), CFRangeMake(0, CFStringGetLength(allowedCharacters)), kCFCompareCaseInsensitive, NULL))
            useDefaultPasswordFormat = CFSTR("false");
    } else
        allowedCharacters = CFRetainSafe(defaultCharacters);

    // In default password format, we use dashes only as separators, not as symbols you can encounter at a random position.
    if (useDefaultPasswordFormat == CFSTR("false")){
        CFMutableStringRef mutatedAllowedCharacters = CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(allowedCharacters), allowedCharacters);
        CFStringFindAndReplace (mutatedAllowedCharacters, CFSTR("-"), CFSTR(""), CFRangeMake(0, CFStringGetLength(allowedCharacters)),kCFCompareCaseInsensitive);
        CFReleaseSafe(allowedCharacters);
        allowedCharacters = mutatedAllowedCharacters;
    }

    if (CFArrayGetCount(requiredCharacterSets) > numberOfRequiredRandomCharacters) {
        CFReleaseNull(requiredCharacterSets);
        requiredCharacterSets = NULL;
    }
    //create new CFDictionary
    numReqChars = CFNumberCreateWithCFIndex(kCFAllocatorDefault, numberOfRequiredRandomCharacters);
    CFMutableDictionaryRef updatedConstraints = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(updatedConstraints, kSecUseDefaultPasswordFormatKey, useDefaultPasswordFormat);
    CFDictionarySetValue(updatedConstraints, kSecNumberOfRequiredRandomCharactersKey, numReqChars);
    CFDictionaryAddValue(updatedConstraints, kSecAllowedCharactersKey, allowedCharacters);
    if(requiredCharacterSets)
        CFDictionaryAddValue(updatedConstraints, kSecRequiredCharacterSetsKey, requiredCharacterSets);

    //add the prohibited characters string if it exists to the new dictionary
    if(prohibitedCharacters)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordDisallowedCharacters, prohibitedCharacters);

    //add the characters the password can't end with if it exists to the new dictionary
    if(endWith)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordCantEndWithChars, endWith);

    //add the characters the password can't start with if it exists to the new dictionary
    if(startWith)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordCantStartWithChars, startWith);

    if(groupSizeRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordGroupSize, groupSizeRef);

    if(numberOfGroupsRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordNumberOfGroups, numberOfGroupsRef);

    if(separatorRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordSeparator, separatorRef);

    if(atMostCharactersRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordContainsNoMoreThanNSpecificCharacters, atMostCharactersRef);

    if(atLeastCharactersRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordContainsAtLeastNSpecificCharacters, atLeastCharactersRef);

    if(identicalRef)
        CFDictionaryAddValue(updatedConstraints, kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, identicalRef);

    CFReleaseNull(useDefaultPasswordFormat);
    CFReleaseNull(numReqChars);
    CFReleaseNull(allowedCharacters);
    CFReleaseNull(requiredCharacterSets);

    return updatedConstraints;
}

static bool isDictionaryFormattedProperly(SecPasswordType type, CFDictionaryRef passwordRequirements, CFErrorRef *error){

    CFTypeRef defaults = NULL;
    CFErrorRef tempError = NULL;
    if(passwordRequirements == NULL){
        return true;
    }

    if( CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordDefaultForType, &defaults) ){
        if(isString(defaults) == true && 0 == CFStringCompare(defaults, CFSTR("true"), 0)){
            return true;
        }
    }
    //only need to check max and min pin length formatting
    if(type == kSecPasswordTypePIN){
        CFTypeRef minTest = NULL, maxTest = NULL;
        uint64_t valuePtr;
        CFIndex minPasswordLength = 0, maxPasswordLength= 0;

        if( CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordDefaultForType, &defaults) ){
            if(isString(defaults) == true && 0 == CFStringCompare(defaults, CFSTR("true"), 0)){
                return true;
            }
        }
        //check if the values exist!
        if( CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordMaxLengthKey, &maxTest) ){
            require_action_quiet(isNull(maxTest)!= true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a max length"), (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(maxTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's max length must be a CFNumberRef"), (CFIndex)errSecBadReq, NULL));

        }
        if (CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordMinLengthKey, &minTest) ){
            require_action_quiet(isNull(minTest)!= true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a min length"), (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(minTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's min length must be a CFNumberRef"), (CFIndex)errSecBadReq, NULL));
        }
        //check if the values exist!
        if(maxTest){
            CFNumberRef max = (CFNumberRef)maxTest;
            CFNumberGetValue(max, kCFNumberSInt64Type, &valuePtr);
            maxPasswordLength = (long)valuePtr;
        }
        if(minTest){
            CFNumberRef min = (CFNumberRef)minTest;
            CFNumberGetValue(min, kCFNumberSInt64Type, &valuePtr);
            minPasswordLength = (long)valuePtr;
        }
        //make sure min and max make sense respective to each other and that they aren't less than 4 digits.
        require_action_quiet(minPasswordLength && maxPasswordLength && minPasswordLength <= maxPasswordLength, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's length parameters make no sense ( is max < min ?)"),  (CFIndex)errSecBadReq, NULL));
        require_action_quiet((minPasswordLength && minPasswordLength >= 4) || (maxPasswordLength && maxPasswordLength >= 4), fail, tempError = CFErrorCreate(kCFAllocatorDefault,  CFSTR("The password's length parameters make no sense ( is max < min ?)"),  (CFIndex)errSecBadReq, NULL));
    }
    else{
        CFTypeRef allowedTest, maxTest, minTest, requiredTest, prohibitedCharacters, endWith, startWith,
        groupSizeRef, numberOfGroupsRef, separatorRef, atMostCharactersRef,
        atLeastCharactersRef, thresholdRef, identicalRef, characters;
        uint64_t valuePtr;

        //check if the values exist!
        require_action_quiet(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordAllowedCharactersKey, &allowedTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need a string of characters; password must only contain characters in this string"),  (CFIndex)errSecBadReq, NULL));
        require_action_quiet( CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordMaxLengthKey, &maxTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a max length"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet( CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordMinLengthKey, &minTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a min length"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordRequiredCharactersKey, &requiredTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need an array of character sets, password must have at least 1 character from each set"), (CFIndex)errSecBadReq, NULL));

        //check if values are null?
        require_action_quiet(isNull(allowedTest) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need a string of characters; password must only contain characters in this string"),  (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isNull(maxTest)!= true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a max length"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isNull(minTest)!= true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("To generate a password, need a min length"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isNull(requiredTest)!= true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need an array of character sets, password must have at least 1 character from each set"), (CFIndex)errSecBadReq, NULL));

        //check if the values are correct
        require_action_quiet(isString(allowedTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's allowed characters must be a CFStringRef"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isNumber(maxTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's max length must be a CFNumberRef"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isNumber(minTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's min length must be a CFNumberRef"), (CFIndex)errSecBadReq, NULL));
        require_action_quiet(isArray(requiredTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's required characters must be an array of CFCharacterSetRefs"), (CFIndex)errSecBadReq, NULL));

        CFNumberGetValue(minTest, kCFNumberSInt64Type, &valuePtr);
        CFIndex minPasswordLength = (long)valuePtr;
        CFNumberGetValue(maxTest, kCFNumberSInt64Type, &valuePtr);
        CFIndex maxPasswordLength = (long)valuePtr;

        require_action_quiet(minPasswordLength <= maxPasswordLength, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The password's length parameters make no sense ( is max < min ?)"),  (CFIndex)errSecBadReq, NULL));

        require_action_quiet(CFStringGetLength((CFStringRef)allowedTest) != 0, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need a string of characters; password must only contain characters in this string"),  (CFIndex)errSecBadReq, NULL));
        require_action_quiet(CFArrayGetCount((CFArrayRef)requiredTest), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Need an array of character sets, password must have at least 1 character from each set"), (CFIndex)errSecBadReq, NULL));

        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordDisallowedCharacters, &prohibitedCharacters)){
            require_action_quiet(isNull(prohibitedCharacters) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Disallowed Characters dictionary parameter is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(prohibitedCharacters), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("Disallowed Characters dictionary parameter is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }
        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordCantEndWithChars, &endWith)){
            require_action_quiet(isNull(endWith) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'EndWith' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(endWith), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'EndWith' is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }
        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordCantStartWithChars, &startWith)){
            require_action_quiet(isNull(startWith) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'StartWith' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(startWith), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'StartWith' is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }
        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordGroupSize, &groupSizeRef)){
            require_action_quiet(isNull(groupSizeRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'groupsize' is either null or not a number"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(groupSizeRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'groupsize' is either null or not a number"), (CFIndex)errSecBadReq, NULL));
        }
        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordNumberOfGroups, &numberOfGroupsRef)){
            require_action_quiet(isNull(numberOfGroupsRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'number of groupds' is either null or not a number"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(numberOfGroupsRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'number of groupds' is either null or not a number"), (CFIndex)errSecBadReq, NULL));
        }
        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordSeparator, &separatorRef)){
            require_action_quiet(isNull(separatorRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'password separator character' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(separatorRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'password separator character' is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }

        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, &atMostCharactersRef)){
            require_action_quiet(isNull(atMostCharactersRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Most N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isDictionary(atMostCharactersRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Most N Characters' is either null or not a string"), (CFIndex)errSecBadReq, NULL));

            require_action_quiet(CFDictionaryGetValueIfPresent(atMostCharactersRef, kSecPasswordCharacterCount, &thresholdRef) != false, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Most N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNull(thresholdRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'characters' is either null or not a number"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(thresholdRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'characters' is either null or not a number"), (CFIndex)errSecBadReq, NULL));

            require_action_quiet(CFDictionaryGetValueIfPresent(atMostCharactersRef, kSecPasswordCharacters, &characters)!= false, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Most N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNull(characters) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(characters), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Characters' is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }

        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordContainsAtLeastNSpecificCharacters, &atLeastCharactersRef)){
            require_action_quiet(isNull(atLeastCharactersRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Least N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isDictionary(atLeastCharactersRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Least N Characters' is either null or not a string"), (CFIndex)errSecBadReq, NULL));

            require_action_quiet(CFDictionaryGetValueIfPresent(atLeastCharactersRef, kSecPasswordCharacterCount, &thresholdRef) != false, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Least N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));

            require_action_quiet(isNull(thresholdRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'characters' is either null or not a number"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(thresholdRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'characters' is either null or not a number"), (CFIndex)errSecBadReq, NULL));

            require_action_quiet(CFDictionaryGetValueIfPresent(atLeastCharactersRef, kSecPasswordCharacters, &characters) != false, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'At Least N Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNull(characters) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Characters' is either null or not a string"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isString(characters), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Characters' is either null or not a string"), (CFIndex)errSecBadReq, NULL));
        }

        if(CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, &identicalRef)){
            require_action_quiet(isNull(identicalRef) != true, fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Identical Consecutive Characters' is either null or not a number"),  (CFIndex)errSecBadReq, NULL));
            require_action_quiet(isNumber(identicalRef), fail, tempError = CFErrorCreate(kCFAllocatorDefault, CFSTR("The dictionary parameter 'Identical Consecutive Characters' is either null or not a number"), (CFIndex)errSecBadReq, NULL));
        }
    }

fail:
    {
        bool result = true;
    if (tempError != NULL) {
            if (error)
                *error = CFRetainSafe(tempError);
            result = false;
    }

    CFReleaseNull(tempError);
        return result;
}
}

static bool doesFinalPasswordPass(bool isSimple, CFStringRef password, CFDictionaryRef requirements){

    CFTypeRef characters, identicalRef = NULL, NRef = NULL, endWith= NULL, startWith= NULL, atLeastCharacters= NULL, atMostCharacters = NULL;
    uint64_t valuePtr;
    CFIndex N, identicalCount = 0;
    CFArrayRef requiredCharacterSet = (CFArrayRef)CFDictionaryGetValue(requirements, kSecRequiredCharacterSetsKey);

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordCantEndWithChars, &endWith))
        endWith = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordCantStartWithChars, &startWith))
        startWith = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsAtLeastNSpecificCharacters, &atLeastCharacters))
        atLeastCharacters = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsNoMoreThanNSpecificCharacters, &atMostCharacters))
        atMostCharacters = NULL;

    if(!CFDictionaryGetValueIfPresent(requirements, kSecPasswordContainsNoMoreThanNConsecutiveIdenticalCharacters, &identicalRef))
        identicalRef = NULL;
    else{
        CFNumberGetValue((CFNumberRef)identicalRef, kCFNumberSInt64Type, &valuePtr);
        identicalCount = (long)valuePtr;
    }
    if(endWith != NULL)
    {
        if(!doesPasswordEndWith(password, endWith))
            return false;
    }
    if(startWith != NULL){
        if(!doesPasswordStartWith(password, startWith))
            return false;
    }
    if(atLeastCharacters != NULL){
        NRef = CFDictionaryGetValue(atLeastCharacters, kSecPasswordCharacterCount);
        characters = CFDictionaryGetValue(atLeastCharacters, kSecPasswordCharacters);
        CFNumberGetValue((CFNumberRef)NRef, kCFNumberSInt64Type, &valuePtr);
        N = (long)valuePtr;
        if(!passwordContainsAtLeastNCharacters(password, characters, N))
            return false;
    }
    if(atMostCharacters != NULL){
        NRef = CFDictionaryGetValue(atMostCharacters, kSecPasswordCharacterCount);
        characters = CFDictionaryGetValue(atMostCharacters, kSecPasswordCharacters);
        CFNumberGetValue((CFNumberRef)NRef, kCFNumberSInt64Type, &valuePtr);
        N = (long)valuePtr;
        if(!passwordContainsLessThanNCharacters(password, characters, N))
            return false;
    }
    if(identicalRef != NULL){
        if(!passwordContainsLessThanNIdenticalCharacters(password, identicalCount))
            return false;
    }
    if (!passwordContainsRequiredCharacters(password, requiredCharacterSet))
        return false;

    if(true == SecPasswordIsPasswordWeak2(isSimple, password))
        return false;

    return true;
}

static CFStringRef
CreateChecksum(SecPasswordType type, CFStringRef password, CFIndex length, CFStringRef allowedChars)
{
    if (type != kSecPasswordTypeiCloudRecoveryKey)
        return NULL;

    CFMutableStringRef checksum = NULL;
    uint8_t digest[CCSHA256_OUTPUT_SIZE];
    if (length > (CFIndex)sizeof(digest))
        return NULL;

    CFDataRef data = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), password, kCFStringEncodingUTF8, 0);
    if (data == NULL)
        return NULL;

    ccdigest(ccsha256_di(), CFDataGetLength(data), CFDataGetBytePtr(data), digest);
    CFReleaseNull(data);

    CFIndex allowedCharLength = CFStringGetLength(allowedChars);

    checksum = CFStringCreateMutable(SecCFAllocatorZeroize(), 0);
    for (CFIndex n = 0; n < length; n++) {
        CFIndex selection = digest[n] % allowedCharLength;
        UniChar c = CFStringGetCharacterAtIndex(allowedChars, selection);
        CFStringAppendCharacters(checksum, &c, 1);
    }

    return checksum;
}

//entry point into password generation
CF_RETURNS_RETAINED CFStringRef SecPasswordGenerate(SecPasswordType type, CFErrorRef *error, CFDictionaryRef passwordRequirements){
    bool check = false, isSimple = false;
    CFTypeRef separator = NULL, defaults = NULL, groupSizeRef = NULL, numberOfGroupsRef = NULL;
    CFDictionaryRef properlyFormattedRequirements = NULL;
    CFErrorRef localError = NULL;
    uint64_t valuePtr, groupSize = 0, numberOfGroups, checksumChars = 0;
    CFNumberRef numberOfRequiredRandomCharacters, checksumCharacters;
    CFIndex requiredCharactersSize = 0;
    CFStringRef randomCharacters = NULL, password = NULL, allowedChars = NULL;
    CFMutableStringRef finalPassword = NULL;

    if(type == kSecPasswordTypePIN)
        isSimple = true;
    else
        isSimple = false;
    check = isDictionaryFormattedProperly(type, passwordRequirements, &localError);
    require_quiet(check != false, fail);

    //should we generate defaults?
    if(passwordRequirements == NULL || (CFDictionaryGetValueIfPresent(passwordRequirements, kSecPasswordDefaultForType, &defaults) && isString(defaults) == true && 0 == CFStringCompare(defaults, CFSTR("true"), 0) ))
        properlyFormattedRequirements = passwordGenerateCreateDefaultParametersDictionary(type, passwordRequirements);
    else
        properlyFormattedRequirements = passwordGenerationCreateParametersDictionary(type, passwordRequirements);

    require_quiet(localError == NULL && properlyFormattedRequirements != NULL, fail);

    numberOfRequiredRandomCharacters = (CFNumberRef)CFDictionaryGetValue(properlyFormattedRequirements, kSecNumberOfRequiredRandomCharactersKey);
    if (isNumber(numberOfRequiredRandomCharacters) && CFNumberGetValue(numberOfRequiredRandomCharacters, kCFNumberSInt64Type, &valuePtr))
        requiredCharactersSize = (long)valuePtr;

    checksumCharacters = (CFNumberRef)CFDictionaryGetValue(properlyFormattedRequirements, kSecNumberOfChecksumCharactersKey);
    if (isNumber(checksumCharacters) && CFNumberGetValue(checksumCharacters, kCFNumberSInt64Type, &valuePtr))
        checksumChars = (long)valuePtr;


    if(!CFDictionaryGetValueIfPresent(properlyFormattedRequirements, kSecPasswordGroupSize, &groupSizeRef)){
        groupSizeRef = NULL;
    }
    else
        CFNumberGetValue((CFNumberRef)groupSizeRef, kCFNumberSInt64Type, &groupSize);

    if(!CFDictionaryGetValueIfPresent(properlyFormattedRequirements, kSecPasswordNumberOfGroups, &numberOfGroupsRef)){
        numberOfGroupsRef = NULL;
    }
    else
        CFNumberGetValue((CFNumberRef)numberOfGroupsRef, kCFNumberSInt64Type, &numberOfGroups);

    require(requiredCharactersSize, fail);

    while (true) {
        allowedChars = CFDictionaryGetValue(properlyFormattedRequirements, kSecAllowedCharactersKey);
        require_noerr(getPasswordRandomCharacters(&randomCharacters, properlyFormattedRequirements, &requiredCharactersSize, allowedChars), fail);

        if(numberOfGroupsRef && groupSizeRef){
            finalPassword = CFStringCreateMutable(kCFAllocatorDefault, 0);

            if(!CFDictionaryGetValueIfPresent(properlyFormattedRequirements, kSecPasswordSeparator, &separator))
                separator = NULL;

            if(separator == NULL)
                separator = CFSTR("-");

            CFIndex i = 0;
            while( i != requiredCharactersSize){
                if((i + (CFIndex)groupSize) < requiredCharactersSize){
                    CFStringRef subString = CFStringCreateWithSubstring(kCFAllocatorDefault, randomCharacters, CFRangeMake(i, (CFIndex)groupSize));
                    CFStringAppend(finalPassword, subString);
                    CFStringAppend(finalPassword, separator);
                    CFReleaseSafe(subString);
                    i+=groupSize;
                }
                else if((i+(CFIndex)groupSize) == requiredCharactersSize){
                    CFStringRef subString = CFStringCreateWithSubstring(kCFAllocatorDefault, randomCharacters, CFRangeMake(i, (CFIndex)groupSize));
                    CFStringAppend(finalPassword, subString);
                    CFReleaseSafe(subString);
                    i+=groupSize;
                }
                else {
                    CFStringRef subString = CFStringCreateWithSubstring(kCFAllocatorDefault, randomCharacters, CFRangeMake(i, requiredCharactersSize - i));
                    CFStringAppend(finalPassword, subString);
                    CFReleaseSafe(subString);
                    i+=(requiredCharactersSize - i);
                }
            }
            if (checksumChars) {
                CFStringRef checksum = CreateChecksum(type, randomCharacters, (CFIndex)checksumChars, allowedChars);
                CFStringAppend(finalPassword, checksum);
                CFReleaseNull(checksum);
            }
            password = CFStringCreateCopy(kCFAllocatorDefault, finalPassword);
            CFReleaseNull(finalPassword);
        }
        //no fancy formatting
        else {
            password = CFStringCreateCopy(kCFAllocatorDefault, randomCharacters);
        }

        CFReleaseNull(randomCharacters);
        require_quiet(doesFinalPasswordPass(isSimple, password, properlyFormattedRequirements), no_pass);
        CFReleaseNull(properlyFormattedRequirements);
        return password;

    no_pass:
        CFReleaseNull(password);
    }

fail:
    if (error && localError) {
        *error = localError;
        localError = NULL;
    }

    CFReleaseSafe(localError);
    CFReleaseNull(properlyFormattedRequirements);
    return NULL;
}

const char *in_word_set (const char *str, unsigned int len){
    static const char * wordlist[] =
    {
        "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
        "", "", "0103", "", "", "", "", "0123", "", "", "", "", "0303", "", "", "",
        "", "", "", "", "0110", "", "1103", "", "", "", "", "1123", "", "", "0000",
        "", "1203", "", "0404", "", "", "", "", "1234", "1110", "2015", "2013", "",
        "2014", "1010", "2005", "2003", "", "2004", "1210", "0505", "0111", "", "",
        "", "2008", "0101", "", "2007", "", "", "", "", "2006", "2010", "1995", "1993",
        "", "1994", "2000", "", "1111", "", "", "", "1998", "1101", "", "1997", "",
        "0808", "1211", "", "1996", "0102", "", "1201", "", "", "1990", "", "", "",
        "", "0202", "", "2011", "", "", "1112", "1958", "2001", "", "1957", "1102",
        "", "3333", "", "1956", "1212", "1985", "1983", "", "1984", "1202", "", "0909",
        "", "0606", "", "1988", "1991", "", "1987", "2012", "", "", "", "1986", "2002",
        "", "", "", "0707", "1980", "", "2009", "", "", "2222", "1965", "1963", "",
        "1964", "", "", "2229", "", "", "1992", "1968", "", "", "1967", "", "", "1999",
        "", "1966", "", "1975", "1973", "", "1974", "1960", "", "1981", "", "4444",
        "", "1978", "", "7465", "1977", "", "", "", "", "1976", "2580", "", "1959",
        "", "", "1970", "", "", "", "", "", "", "", "", "", "1982", "", "1961", "",
        "", "5252", "", "1989", "", "", "", "", "", "", "", "", "", "", "", "", "",
        "", "1971", "", "", "", "", "", "", "", "1962", "", "5683", "", "6666", "",
        "", "1969", "", "", "", "", "", "", "", "", "", "", "", "", "1972", "", "",
        "", "", "", "", "1979", "", "", "", "7667"
    };

    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
        register int key = pinhash (str, len);

        if (key <= MAX_HASH_VALUE && key >= 0)
        {
            register const char *s = wordlist[key];
            if (*str == *s && !strcmp (str + 1, s + 1))
                return s;
        }
    }
    return 0;
}
CFDictionaryRef SecPasswordCopyDefaultPasswordLength(SecPasswordType type, CFErrorRef *error){

    CFIndex tupleLengthInt = 0, numOfTuplesInt = 0;
    CFNumberRef tupleLength = NULL;
    CFNumberRef numOfTuples = NULL;

    CFMutableDictionaryRef passwordLengthDefaults = NULL;
    CFDictionaryRef result = NULL;

    switch(type){
        case(kSecPasswordTypeiCloudRecoveryKey):
            tupleLengthInt = 4;
            numOfTuplesInt = 7;
            break;

        case(kSecPasswordTypeiCloudRecovery):
            tupleLengthInt = 4;
            numOfTuplesInt = 6;
            break;

        case(kSecPasswordTypePIN):
            tupleLengthInt = 4;
            numOfTuplesInt = 1;
            break;

        case(kSecPasswordTypeSafari):
            tupleLengthInt = 4;
            numOfTuplesInt = 5;
            break;

        case(kSecPasswordTypeWifi):
            tupleLengthInt = 4;
            numOfTuplesInt = 3;
            break;

        default:
            if(SecError(errSecBadReq, error, CFSTR("Password type does not exist.")) == false)
            {
                secdebug("secpasswordcopydefaultpasswordlength", "could not create error!");
            }
    }

    if (tupleLengthInt != 0 && numOfTuplesInt != 0) {
        tupleLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &tupleLengthInt);
        numOfTuples = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &numOfTuplesInt);
        passwordLengthDefaults = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(passwordLengthDefaults, kSecPasswordGroupSize, tupleLength);
        CFDictionaryAddValue(passwordLengthDefaults, kSecPasswordNumberOfGroups, numOfTuples);
        result = CFDictionaryCreateCopy(kCFAllocatorDefault, passwordLengthDefaults);
    }

    CFReleaseSafe(tupleLength);
    CFReleaseSafe(numOfTuples);
    CFReleaseSafe(passwordLengthDefaults);
    return result;
}

bool
SecPasswordValidatePasswordFormat(SecPasswordType type, CFStringRef password, CFErrorRef *error)
{
    CFIndex tupleLengthInt = 0, numOfTuplesInt = 0, checkSumChars = 0;
    CFStringRef checksum = NULL, madeChecksum = NULL, passwordNoChecksum = NULL;
    CFMutableStringRef randomChars = NULL;
    CFStringRef allowedChars = NULL;
    bool res = false;

    switch (type) {
        case kSecPasswordTypeiCloudRecoveryKey:
            tupleLengthInt = 4;
            numOfTuplesInt = 7;
            checkSumChars = 2;
            allowedChars = defaultiCloudCharacters;
            break;
        case kSecPasswordTypeiCloudRecovery:
            tupleLengthInt = 4;
            numOfTuplesInt = 6;
            break;
        case(kSecPasswordTypePIN):
            tupleLengthInt = 4;
            numOfTuplesInt = 1;
            break;
        default:
            SecError(errSecBadReq, error, CFSTR("Password type does not exist."));
            return false;
    }


    if (numOfTuplesInt < 1)
        return false;
    if (checkSumChars > tupleLengthInt)
        return false;

    CFIndex numberOfChars = numOfTuplesInt * tupleLengthInt + (numOfTuplesInt - 1);

    /*
     * First check expected length
     */

    require(CFStringGetLength(password) == numberOfChars, out); /* N groups of M with (N-1) seperator - in-between */

    randomChars = CFStringCreateMutable(SecCFAllocatorZeroize(), 0);
    require(randomChars, out);

    /*
     * make sure dash-es are at the expected spots
     */

    for (CFIndex n = 0; n < numOfTuplesInt; n++) {
        if (n != 0) {
            UniChar c = CFStringGetCharacterAtIndex(password, (n * (tupleLengthInt + 1)) - 1);
            require(c == '-', out);
        }
        CFStringRef substr = CFStringCreateWithSubstring(SecCFAllocatorZeroize(), password, CFRangeMake(n * (tupleLengthInt + 1), tupleLengthInt));
        CFStringAppend(randomChars, substr);
        CFReleaseNull(substr);
    }

    if (checkSumChars) {
        /*
         * Pull apart and password and checksum
         */

        checksum = CFStringCreateWithSubstring(SecCFAllocatorZeroize(), randomChars, CFRangeMake((numOfTuplesInt * tupleLengthInt) - checkSumChars, checkSumChars));
        require(checksum, out);

        passwordNoChecksum = CFStringCreateWithSubstring(SecCFAllocatorZeroize(), randomChars, CFRangeMake(0, (numOfTuplesInt * tupleLengthInt) - checkSumChars));
        require(passwordNoChecksum, out);

        /*
         * Validate checksum
         */

        madeChecksum = CreateChecksum(type, passwordNoChecksum, checkSumChars, allowedChars);
        require(madeChecksum, out);

        require(CFEqual(madeChecksum, checksum), out);
    }

    res = true;
out:
    CFReleaseNull(randomChars);
    CFReleaseNull(madeChecksum);
    CFReleaseNull(checksum);
    CFReleaseNull(passwordNoChecksum);

    return res;
}
