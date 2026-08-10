/*
 * Copyright (c) 2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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
#ifndef _SECURITYD_SPI_H_
#define _SECURITYD_SPI_H_

#include "utilities/SecCFError.h"
#include <xpc/xpc.h>
#include <CoreFoundation/CFURL.h>

__BEGIN_DECLS

/* Calling this function initializes the spi interface in the library to call
   directly into the backend. It uses home_dir for root of files if specified.
   This function only initializes the trust spi interface if libtrustd is linked
   by the caller and LIBTRUSTD=1 is specified.  */
void securityd_init(CFURLRef home_dir);

// Don't call either of these functions unless you are really securityd
void securityd_init_server(void);
void securityd_init_local_spi(void);

__END_DECLS

#endif /* _SECURITYD_SPI_H_ */
