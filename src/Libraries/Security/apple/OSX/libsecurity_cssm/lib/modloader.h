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
// modloader.h - CSSM module loader interface
//
// This is a thin abstraction of plugin module loading/handling for CSSM.
// The resulting module ("Plugin") notion is specific to CSSM plugin modules.
// This implementation uses MacOS X bundles.
//
#ifndef _H_MODLOADER
#define _H_MODLOADER

#include <exception>
#include <security_utilities/osxcode.h>
#include "cssmint.h"
#include <map>
#include <string>


namespace Security {


//
// A collection of canonical plugin entry points (aka CSSM module SPI)
//
struct PluginFunctions {
	CSSM_SPI_ModuleLoadFunction *load;
	CSSM_SPI_ModuleUnloadFunction *unload;
	CSSM_SPI_ModuleAttachFunction *attach;
	CSSM_SPI_ModuleDetachFunction *detach;
};


//
// An abstract representation of a loadable plugin.
// Note that "loadable" doesn't mean that actual code loading
// is necessarily happening, but let's just assume it might.
//
class Plugin {
    NOCOPY(Plugin)
public:
    Plugin() { }
    virtual ~Plugin() { }

    virtual void load() = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
	
	virtual CSSM_SPI_ModuleLoadFunction load = 0;
	virtual CSSM_SPI_ModuleUnloadFunction unload = 0;
	virtual CSSM_SPI_ModuleAttachFunction attach = 0;
	virtual CSSM_SPI_ModuleDetachFunction detach = 0;
};


//
// The supervisor class that manages searching and loading.
//
class ModuleLoader {
    NOCOPY(ModuleLoader)
public:
    ModuleLoader();
    
    Plugin *operator () (const string &path);
        
private:
    // the table of all loaded modules
    typedef map<string, Plugin *> PluginTable;
    PluginTable mPlugins;
};



} // end namespace Security


#endif //_H_MODLOADER
