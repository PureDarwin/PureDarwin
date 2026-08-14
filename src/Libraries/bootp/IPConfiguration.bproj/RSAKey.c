/*
 * Copyright (c) 2013-2015 Apple Inc. All rights reserved.
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
 * RSAKey.c
 * - generate RSA key pair
 */
/* 
 * Modification History
 *
 * April 11, 2013 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <CommonCrypto/CommonRSACryptor.h>
#include "symbol_scope.h"
#include "RSAKey.h"
#include "cfutil.h"
#include "mylog.h"

STATIC CCCryptorStatus
my_CCRSACryptorCopyData(CCRSACryptorRef key, int key_size,
			CFDataRef * ret_data)
{
    uint8_t		buf[key_size]; /* very rough size estimate */
    size_t		buf_size;
    CCCryptorStatus 	status;

    buf_size = sizeof(buf);
    status = CCRSACryptorExport(key, buf, &buf_size);
    if (status != kCCSuccess) {
	*ret_data = NULL;
	return (status);
    }
    *ret_data = CFDataCreate(NULL, buf, buf_size);
    return (kCCSuccess);
}

STATIC CCCryptorStatus
my_CCRSACryptorGeneratePair(int key_size, 
			    CFDataRef * ret_priv, CFDataRef * ret_pub)
{
    CCRSACryptorRef 	priv = NULL;
    CFDataRef		priv_data = NULL;
    CCRSACryptorRef 	pub = NULL;
    CFDataRef		pub_data = NULL;
    CCCryptorStatus 	status;

    status = CCRSACryptorGeneratePair(key_size, 65537, &pub, &priv);
    if (status != kCCSuccess) {
	goto done;
    }
    status = my_CCRSACryptorCopyData(priv, key_size, &priv_data);
    if (status != kCCSuccess) {
	goto done;
    }
    status = my_CCRSACryptorCopyData(pub, key_size, &pub_data);
    if (status != kCCSuccess) {
	goto done;
    }
 done:
    if (pub != NULL) {
	CCRSACryptorRelease(pub);
    }
    if (priv != NULL) {
	CCRSACryptorRelease(priv);
    }
    if (status != kCCSuccess) {
	my_CFRelease(&priv_data);
	my_CFRelease(&pub_data);
    }
    *ret_priv = priv_data;
    *ret_pub = pub_data;
    return (status);
}

CF_RETURNS_RETAINED PRIVATE_EXTERN CFDataRef
RSAKeyPairGenerate(int key_size, CFDataRef * ret_pub)
{
    CFDataRef		priv;
    CCCryptorStatus 	status;

    status = my_CCRSACryptorGeneratePair(key_size, &priv, ret_pub);
    if (status != kCCSuccess) {
	my_log_fl(LOG_NOTICE, "my_CCRSACryptorGeneratePair failed, %ld\n",
		  (long)status);
    }
    return (priv);

}

#ifdef TEST_RSAKEY
int
main(int argc, char * argv[])
{
    CFDataRef		pub;
    CFDataRef		priv;
    int			key_size = 1024;
    
    if (argc > 1) {
	key_size = strtoul(argv[1], NULL, 0);
    }
    priv = RSAKeyPairGenerate(key_size, &pub);
    if (priv != NULL) {
	printf("key pair generated: pub len %d priv len %d\n",
	       (int)CFDataGetLength(pub), (int)CFDataGetLength(priv));
	CFRelease(pub);
	CFRelease(priv);
    }
    exit(0);
}
#endif /* TEST_RSAKEY */
