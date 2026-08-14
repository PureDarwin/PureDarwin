/*
 * Copyright (c) 2000-2001,2004,2011,2014 Apple Inc. All Rights Reserved.
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
// attachfactory - industrial grade production of Attachment objects
//
#ifndef _H_ATTACHFACTORY
#define _H_ATTACHFACTORY

#include "cssmint.h"
#include "attachment.h"

#ifdef _CPP_ATTACHFACTORY
# pragma export on
#endif


//
// An AttachmentMaker can create an Attachment object for a particular service
// type when asked nicely.
// 
class AttachmentMaker {
public:
    AttachmentMaker(CSSM_SERVICE_TYPE type) : mType(type) { }
    virtual ~AttachmentMaker();

    virtual Attachment *make(Module *module,
                             const CSSM_VERSION &version,
                             uint32 subserviceId,
                             CSSM_SERVICE_TYPE subserviceType,
                             const CSSM_API_MEMORY_FUNCS &memoryOps,
                             CSSM_ATTACH_FLAGS attachFlags,
                             CSSM_KEY_HIERARCHY keyHierarchy,
                             CSSM_FUNC_NAME_ADDR *functionTable,
                             uint32 functionTableSize) = 0;

    CSSM_SERVICE_TYPE factoryType() const { return mType; }
    
private:
    CSSM_SERVICE_TYPE mType;
};


//
// An AttachmentFactory contains a registry of AttachmentMakers for different
// service types, and produces the needed one on request.
//
class AttachmentFactory {
public:
    AttachmentFactory();
    
    AttachmentMaker *attachmentMakerFor(CSSM_SERVICE_TYPE type) const;

private:
    typedef map<CSSM_SERVICE_TYPE, AttachmentMaker *> AttachFactories;
    AttachFactories factories;
};

#ifdef _CPP_ATTACHFACTORY
# pragma export off
#endif


#endif //_H_ATTACHFACTORY
