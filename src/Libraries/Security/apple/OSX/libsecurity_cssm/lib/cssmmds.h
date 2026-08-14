/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
// cssmmds - MDS interface for CSSM & friends
//
#ifndef _H_CSSMMDS
#define _H_CSSMMDS

#include "cssmint.h"
#include <security_utilities/globalizer.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_client/mds_standard.h>


class MdsComponent {
public:
    MdsComponent(const Guid &guid);
    virtual ~MdsComponent();

    const Guid &myGuid() const { return mMyGuid; }
    
    CSSM_SERVICE_MASK services() const { return mCommon->serviceMask(); }
    bool supportsService(CSSM_SERVICE_TYPE t) const { return t & services(); }
    bool isThreadSafe() const { return !mCommon->singleThreaded(); }
    string path() const { return mCommon->path(); }
	string name() const { return mCommon->moduleName(); }
	string description() const { return mCommon->description(); }

private:
    const Guid mMyGuid;					// GUID of the component
	RefPointer<MDSClient::Common> mCommon; // MDS common record for this module
};


#endif //_H_CSSMMDS
