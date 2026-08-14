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
// modload_static - pseudo-loading of statically linked plugins
//
#ifndef _H_MODLOAD_STATIC
#define _H_MODLOAD_STATIC

#include "modloader.h"
#include "cssmint.h"
#include <Security/cssmspi.h>
#include <security_cdsa_utilities/callback.h>


namespace Security {


//
// A "plugin" implementation that uses statically linked entry points
//
class StaticPlugin : public Plugin {
public: 
	StaticPlugin(const PluginFunctions &funcs) : entries(funcs) { }

    void load()				{ }
    void unload()			{ }
    bool isLoaded() const	{ return true; }

	CSSM_SPI_ModuleLoadFunction load;
	CSSM_SPI_ModuleUnloadFunction unload;
	CSSM_SPI_ModuleAttachFunction attach;
	CSSM_SPI_ModuleDetachFunction detach;
    
private:
	const PluginFunctions &entries;
};


} // end namespace Security


#endif //_H_MODLOAD_STATIC
