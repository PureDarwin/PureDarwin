/*
 * Copyright (c) 2000-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// module - CSSM Module objects
//
#ifndef _H_MODULE
#define _H_MODULE

#include "cssmint.h"
#include "cssmmds.h"
#include <security_cdsa_utilities/callback.h>
#include "modloader.h"
#include <set>

#include <unordered_map>


//
// This type represents a loaded plugin module of some kind. For each CssmManager
// instance and each one plugin, there is only (at most) one Module object to
// represent it.
//
class Module : public MdsComponent {
    NOCOPY(Module)
public:
    Module(CssmManager *mgr, const MdsComponent &info, Plugin *plugin);
    virtual ~Module();

    CssmManager &cssm;

    bool unload(const ModuleCallback &callback);

    CSSM_HANDLE attach(const CSSM_VERSION &version,
                       uint32 subserviceId,
                       CSSM_SERVICE_TYPE subserviceType,
                       const CSSM_API_MEMORY_FUNCS &memoryOps,
                       CSSM_ATTACH_FLAGS attachFlags,
                       CSSM_KEY_HIERARCHY keyHierarchy,
                       CSSM_FUNC_NAME_ADDR *functionTable,
                       uint32 functionTableSize);
    void detach(Attachment *attachment);

    void add(const ModuleCallback &cb) { callbackSet.insert(cb); }
    void remove(const ModuleCallback &cb) { callbackSet.erase(cb); }

    unsigned int callbackCount() const { return callbackSet.size(); }
    unsigned int attachmentCount() const { return (int)attachmentMap.size(); }

	void safeLock()		{ if (!isThreadSafe()) mLock.lock(); }
	void safeUnlock()	{ if (!isThreadSafe()) mLock.unlock(); }
    
public:
    typedef std::unordered_map<CSSM_HANDLE, Attachment *> AttachmentMap;
    
    Plugin *plugin;					// our loader module
	
private:
    void spiEvent(CSSM_MODULE_EVENT event,
                         const Guid &guid,
                         uint32 subserviceId,
                         CSSM_SERVICE_TYPE serviceType);

    static CSSM_RETURN spiEventRelay(const CSSM_GUID *ModuleGuid,
                                     void *Context,
                                     uint32 SubserviceId,
                                     CSSM_SERVICE_TYPE ServiceType,
                                     CSSM_MODULE_EVENT EventType);

private:
    AttachmentMap attachmentMap;	// map of all outstanding attachment handles
    ModuleCallbackSet callbackSet;	// set of registered callbacks

    Mutex mLock;
};

#endif //_H_MODULE
