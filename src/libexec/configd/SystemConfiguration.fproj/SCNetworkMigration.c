/*
 * Copyright (c) 2014-2021 Apple Inc. All rights reserved.
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
//  SCNetworkMigration.c
//
//  Created by Ashish Kulkarni on 11/19/13.
//
//

#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesInternal.h"
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#if	!TARGET_OS_SIMULATOR
#include <IOKit/usb/USB.h>
#endif	// !TARGET_OS_SIMULATOR
#include <sys/stat.h>
#include <copyfile.h>
#include <sys/param.h>
#include <pthread.h>

#define NUM_MIGRATION_PATHS		2
#define PLUGIN_ID			CFSTR("System Migration")
#define PREFERENCES_PLIST_INDEX		0
#define NETWORK_INTERFACES_PLIST_INDEX	1

#define kProtocolType			CFSTR("Protocol Type")
#define kProtocolConfiguration		CFSTR("Protocol Configuration")
#define kProtocolEnabled		CFSTR("Protocol Enabled")


const CFStringRef kSCNetworkConfigurationMigrationActionKey = CFSTR("MigrationActionKey");
const CFStringRef kSCNetworkConfigurationRepair = CFSTR("ConfigurationRepair");
const CFStringRef kSCNetworkConfigurationRepairModel = CFSTR("ConfigurationRepairModel");


static void
logInterfaces(int level, const char *description, CFArrayRef interfaces, Boolean detailed)
{
	CFMutableArrayRef	interfaceNames	= NULL;

	if (detailed) {
		SC_log(level, "%s = ", description);
	} else {
		interfaceNames = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	for (CFIndex i = 0, n = CFArrayGetCount(interfaces); i < n; i++) {
		CFStringRef		bsdName;
		SCNetworkInterfaceRef	interface;
		CFStringRef		userDefinedName	= NULL;

		interface = CFArrayGetValueAtIndex(interfaces, i);
		if (isA_SCNetworkInterface(interface)) {
			bsdName = SCNetworkInterfaceGetBSDName(interface);
			if (bsdName == NULL) {
				continue;
			}
			userDefinedName = __SCNetworkInterfaceGetUserDefinedName(interface);
		} else if (isA_CFDictionary(interface)) {
			CFDictionaryRef	info;

			bsdName = CFDictionaryGetValue((CFDictionaryRef)interface, CFSTR(kSCNetworkInterfaceBSDName));
			if (bsdName == NULL) {
				continue;
			}

			info = CFDictionaryGetValue((CFDictionaryRef)interface, CFSTR(kSCNetworkInterfaceInfo));
			if (info != NULL) {
				userDefinedName = CFDictionaryGetValue(info, kSCPropUserDefinedName);
			}
		} else {
			bsdName = CFSTR("?");
		}

		if (interfaceNames == NULL) {
			SC_log(level, "  %@ (%@)",
			       bsdName,
			       userDefinedName != NULL ? userDefinedName : CFSTR("?"));
		} else {
			CFArrayAppendValue(interfaceNames, bsdName);
		}
	}

	if (interfaceNames != NULL) {
		CFStringRef	list;

		list = CFStringCreateByCombiningStrings(NULL, interfaceNames, CFSTR(", "));
		SC_log(level, "%s = %@", description, list);
		CFRelease(list);

		CFRelease(interfaceNames);
	}

	return;
}


typedef struct {
	int		level;
	const char	*name;
} logMappingContext;

/*
 * logMapping_one()
 *
 * Logs a single key/value of a mapping dictionary
 */
static void
logMapping_one(const void *key, const void *value, void *context)
{
#pragma unused(context)
	logMappingContext *	mapping_context		= (logMappingContext *)context;
	CFTypeRef		mapping_key		= NULL;
	CFTypeRef		mapping_value		= NULL;
	Boolean			mapping_value_retained	= FALSE;

	if (mapping_context->name != NULL) {
		SC_log(mapping_context->level, "%s =", mapping_context->name);
		mapping_context->name = NULL;
	}

	if (isA_SCNetworkService(key)) {
		mapping_key = SCNetworkServiceGetServiceID(key);
	} else if (isA_SCNetworkSet(key)) {
		mapping_key = SCNetworkSetGetSetID(key);
	} else if (isA_SCNetworkInterface(key)) {
		mapping_key = SCNetworkInterfaceGetBSDName(key);
	} else {
		mapping_key = key;
	}

	if (isA_SCNetworkService(value)) {
		mapping_value = SCNetworkServiceGetServiceID(value);
	} else if (isA_SCNetworkSet(value)) {
		mapping_value = SCNetworkSetGetSetID(value);
	} else if (isA_SCNetworkInterface(value)) {
		CFStringRef	bsdName;
		CFStringRef	userDefinedName;

		bsdName = SCNetworkInterfaceGetBSDName(value);
		userDefinedName = __SCNetworkInterfaceGetUserDefinedName(value);
		mapping_value = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%@)"),
							 bsdName,
							 userDefinedName != NULL ? userDefinedName : CFSTR("?"));
		mapping_value_retained = TRUE;
	} else if (isA_CFBoolean(value)) {
		mapping_value = CFSTR("None");
	} else if (isA_CFArray(value)) {
		CFIndex			n	= CFArrayGetCount(value);
		CFMutableStringRef	str	= CFStringCreateMutable(NULL, 0);

		CFStringAppendFormat(str, NULL, CFSTR("( "));
		for (CFIndex i = 0; i < n; i++) {
			CFTypeRef	val;

			val = CFArrayGetValueAtIndex(value, i);
			if (isA_SCNetworkSet(val)) {
				val = SCNetworkSetGetSetID(val);
			}
			CFStringAppendFormat(str, NULL, CFSTR("%s%@"),
					     i == 0 ? "" : ", ",
					     val);


		}
		CFStringAppendFormat(str, NULL, CFSTR(" )"));
		mapping_value = str;
		mapping_value_retained = TRUE;
	} else {
		mapping_value = value;
	}

	SC_log(mapping_context->level, "  %@ --> %@", mapping_key, mapping_value);

	if (mapping_value_retained) {
		CFRelease(mapping_value);
	}
	return;
}

/*
 * logMapping()
 *
 * Generates a log of the mappings between :
 *
 *   bsdNameServiceProtocolPreserveMapping
 *     SCNetworkService/SCNetworkInterface/bsdName
 *       --> array<info about protocols in the associated service>
 *
 *   mappingServiceBSDNameToInterface
 *     SCNetworkInterface/bsdName
 *       --> SCNetworkInterface
 *
 *   bsdNameMapping
 *     [source]interface BSD name
 *       --> [target]interface BSD name
 *
 *   externalMapping	(like bsdNameMapping but only for "external" interfaces)
 *
 *   serviceMapping	(matching on SCNetworkService/SCNetworkInterface/bsdName)
 *     [source]SCNetworkService
 *       --> [target]SCNetworkService
 *
 *   setMapping		(matching on the SCNetworkSet "name")
 *     [source]SCNetworkSet
 *       --> [target]SCNetworkSet
 *
 *   serviceSetMapping
 *     SCNetworkService
 *       --> array<SCNetworkSet's containing the service>
 */
static void
logMapping(int level, CFDictionaryRef mapping, const char *name)
{
	logMappingContext	mappingContext	= { .level = level, .name = name };

	CFDictionaryApplyFunction(mapping, logMapping_one, &mappingContext);
	return;
}

static SCPreferencesRef
__SCPreferencesCreateForMigration(CFAllocatorRef allocator, CFStringRef name, CFStringRef prefsID)
{
	CFStringRef		option_keys[]	= { kSCPreferencesOptionAllowModelConflict };
	CFPropertyListRef	option_vals[]	= { kCFBooleanTrue };
	CFDictionaryRef		options;
	SCPreferencesRef	prefs;

	options = CFDictionaryCreate(allocator,
				     (const void **)option_keys,
				     (const void **)option_vals,
				     sizeof(option_keys) / sizeof(option_keys[0]),
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	prefs = SCPreferencesCreateWithOptions(allocator,
					       name,
					       prefsID,
					       NULL,		// authorization
					       options);	// allow model conflicts
	CFRelease(options);

	return prefs;
}

#if	!TARGET_OS_IPHONE
static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToBridgeServices(SCPreferencesRef prefs);

static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToBondServices(SCPreferencesRef prefs);

static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToVLANServices(SCPreferencesRef prefs);
#endif	// !TARGET_OS_IPHONE

static Boolean
_SCNetworkConfigurationIsInterfaceNamerMappable(SCNetworkInterfaceRef interface1, SCNetworkInterfaceRef interface2, Boolean bypassActive);

static Boolean
_SCNetworkConfigurationMigrateConfiguration(CFURLRef sourceDir, CFURLRef targetDir);

static void
_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(CFURLRef baseURL, CFURLRef *prefs, CFURLRef *interfaces)
{
	if (baseURL != NULL) {
		CFRetain(baseURL);
	} else {
		baseURL = CFURLCreateFromFileSystemRepresentation(NULL,
								  (UInt8*)PREFS_DEFAULT_DIR_PATH,
								  sizeof(PREFS_DEFAULT_DIR_PATH) - 1,
								  TRUE);
	}

	*prefs = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
								       (UInt8*)PREFS_DEFAULT_CONFIG_PLIST,
								       sizeof(PREFS_DEFAULT_CONFIG_PLIST) - 1,
								       FALSE,
								       baseURL);

	*interfaces = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
									    (UInt8*)INTERFACES_DEFAULT_CONFIG_PLIST,
									    sizeof(INTERFACES_DEFAULT_CONFIG_PLIST) - 1,
									    FALSE,
									    baseURL);
	CFRelease(baseURL);
	return;
}

CFArrayRef
_SCNetworkConfigurationCopyMigrationPaths(CFDictionaryRef options)
{
#pragma unused(options)
	CFURLRef interfaces;
	CFMutableArrayRef migrationPaths = NULL;
	CFURLRef prefs;

	migrationPaths = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(NULL, &prefs, &interfaces);
	CFArrayAppendValue(migrationPaths, prefs);
	CFArrayAppendValue(migrationPaths, interfaces);
	CFRelease(prefs);
	CFRelease(interfaces);

	return migrationPaths;
}

static Boolean
_SCNetworkConfigurationRemoveConfigurationFiles(CFURLRef configDir)
{

	char configPathString[PATH_MAX];
	CFURLRef configPathURL = NULL;
	char configNetworkInterfacesPathString[PATH_MAX];
	CFURLRef configNetworkInterfacesPathURL = NULL;

	_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(configDir, &configPathURL, &configNetworkInterfacesPathURL);

	if(!CFURLGetFileSystemRepresentation(configPathURL,
					     TRUE,
					     (UInt8*)configPathString,
					     sizeof(configPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configPathURL);
	} else {
		if ((remove(configPathString) != 0) && (errno != ENOENT)) {
			SC_log(LOG_NOTICE, "remove(\"%s\") failed: %s", configPathString, strerror(errno));
		}
	}
	CFRelease(configPathURL);

	if(!CFURLGetFileSystemRepresentation(configNetworkInterfacesPathURL,
					     TRUE,
					     (UInt8*)configNetworkInterfacesPathString,
					     sizeof(configNetworkInterfacesPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configNetworkInterfacesPathURL);
	} else {
		if ((remove(configNetworkInterfacesPathString) != 0) && (errno != ENOENT)) {
			SC_log(LOG_NOTICE, "remove(\"%s\") failed: %s", configNetworkInterfacesPathString, strerror(errno));
		}
	}
	CFRelease(configNetworkInterfacesPathURL);

	return TRUE;
}

static Boolean
SCNetworkConfigurationCopyConfigurationFiles(CFURLRef	configDir,
					     CFURLRef	targetDir)      // TargetDir needs to exist
{
	errno_t error;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	char networkInterfacesPathString[PATH_MAX];
	CFURLRef networkInterfacesPathURL = NULL;
	copyfile_state_t networkInterfacesState;
	char preferencesPathString[PATH_MAX];
	CFURLRef preferencesPathURL = NULL;
	Boolean removeTargetFiles = FALSE;
	copyfile_state_t state;
	Boolean success = FALSE;
	char targetNetworkInterfacesPathString[PATH_MAX];
	CFURLRef targetNetworkInterfacesPathURL = NULL;
	char targetPathString[PATH_MAX];
	CFURLRef targetPathURL = NULL;

	_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(targetDir, &targetPathURL, &targetNetworkInterfacesPathURL);

	if (!CFURLGetFileSystemRepresentation(targetPathURL,
					      TRUE,
					      (UInt8*)targetPathString,
					      sizeof(targetPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", targetPathURL);
		goto done;
	}
	if (!CFURLGetFileSystemRepresentation(targetNetworkInterfacesPathURL,
					      TRUE,
					      (UInt8*)targetNetworkInterfacesPathString,
					      sizeof(targetNetworkInterfacesPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", targetNetworkInterfacesPathURL);
		goto done;
	}

	if (configDir == NULL) {
		removeTargetFiles = TRUE;
		success = TRUE;
		goto done;
	}

	_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(configDir, &preferencesPathURL, &networkInterfacesPathURL);

	if (!CFURLGetFileSystemRepresentation(preferencesPathURL,
					      TRUE,
					      (UInt8*)preferencesPathString,
					      sizeof(preferencesPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", preferencesPathURL);
		goto done;
	}
	if (!CFURLGetFileSystemRepresentation(networkInterfacesPathURL,
					      TRUE,
					      (UInt8*)networkInterfacesPathString,
					      sizeof(networkInterfacesPathString))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", networkInterfacesPathURL);
		goto done;
	}

	state = copyfile_state_alloc();
	error = copyfile(preferencesPathString, targetPathString, state, COPYFILE_ALL);
	if (error != 0) {
		SC_log(LOG_NOTICE, "copyFile(\"%s\", \"%s\", ...) failed: %s",
		       preferencesPathString,
		       targetPathString,
		       strerror(errno));
		copyfile_state_free(state);
		removeTargetFiles = TRUE;
		goto done;
	}
	copyfile_state_free(state);
	(void)chmod(targetPathString, mode);

	networkInterfacesState = copyfile_state_alloc();
	error = copyfile(networkInterfacesPathString, targetNetworkInterfacesPathString, networkInterfacesState, COPYFILE_ALL);
	if (error != 0) {
		SC_log(LOG_NOTICE, "copyFile(\"%s\", \"%s\", ...) failed: %s",
		       networkInterfacesPathString,
		       targetNetworkInterfacesPathString,
		       strerror(errno));
		copyfile_state_free(networkInterfacesState);
		removeTargetFiles = TRUE;
		goto done;
	}
	copyfile_state_free(networkInterfacesState);
	(void)chmod(targetNetworkInterfacesPathString, mode);

	success = TRUE;
done:
	if (removeTargetFiles) {
		_SCNetworkConfigurationRemoveConfigurationFiles(targetDir);
	}
	if (preferencesPathURL != NULL) {
		CFRelease(preferencesPathURL);
	}
	if (networkInterfacesPathURL != NULL) {
		CFRelease(networkInterfacesPathURL);
	}
	if (targetPathURL != NULL) {
		CFRelease(targetPathURL);
	}
	if (targetNetworkInterfacesPathURL != NULL) {
		CFRelease(targetNetworkInterfacesPathURL);
	}
	return success;
}


/* -----------------------------------------------------------------------------
 Create directories and intermediate directories as required.
 ----------------------------------------------------------------------------- */
static Boolean
_SCNetworkConfigurationMakePathIfNeeded(CFURLRef pathURL)
{
	char    *c;
	char	path[PATH_MAX];
	Boolean success		= FALSE;

	if (!CFURLGetFileSystemRepresentation(pathURL, TRUE, (UInt8 *)path, sizeof(path))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", pathURL);
		return success;
	}

	SC_log(LOG_INFO, "creating path: %s", path);

	c = path;
	if (*c == '/') {
		c++;	// skip leading /
	}
	for(; !success; c++) {
		if ((*c == '/') || (*c == '\0')){
			if (*c == '\0') {
				success = TRUE;
			} else {
				*c = '\0';
			}
			if (mkdir(path, (S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH)) != 0) {
				if ((errno != EEXIST) && (errno != EISDIR)) {
					SC_log(LOG_NOTICE, "mkdir(%s) failed: %s", path, strerror(errno));
					break;
				}
			}
			*c = '/';
		}
	}

	return success;
}

static void
__SCNetworkPopulateDefaultPrefs(SCPreferencesRef prefs)
{
	SCNetworkSetRef	currentSet;
	CFStringRef	model;
	CFNumberRef	version;

	SC_log(LOG_INFO,
	       "Populating preferences.plist"
	       "\n  %@",
	       prefs);

	currentSet = SCNetworkSetCopyCurrent(prefs);
	if (currentSet == NULL) {
		currentSet = _SCNetworkSetCreateDefault(prefs);
	}
	SCNetworkSetEstablishDefaultConfiguration(currentSet);
	CFRelease(currentSet);

	model = SCPreferencesGetValue(prefs, MODEL);
	if (model == NULL) {
		model = _SC_hw_model(FALSE);
		SCPreferencesSetValue(prefs, MODEL, model);
	}

	version = SCPreferencesGetValue(prefs, kSCPrefVersion);
	if (version == NULL) {
		const int	new_version	= NETWORK_CONFIGURATION_VERSION;

		version = CFNumberCreate(NULL, kCFNumberIntType, &new_version);
		SCPreferencesSetValue(prefs, kSCPrefVersion, version);
		CFRelease(version);
	}

	return;
}

static void
__SCNetworkPopulateDefaultNIPrefs(SCPreferencesRef ni_prefs)
{
	CFMutableArrayRef	interfaces	= NULL;
	CFStringRef		model;
	CFArrayRef		networkInterfaces;
	CFComparisonResult	res;
	CFNumberRef		version;

	interfaces = (CFMutableArrayRef)SCPreferencesGetValue(ni_prefs, INTERFACES);
	if (isA_CFArray(interfaces)) {
		// if already populated
		return;
	}

	SC_log(LOG_INFO,
	       "Populating NetworkInterfaces.plist"
	       "\n  %@",
	       ni_prefs);

	networkInterfaces = __SCNetworkInterfaceCopyAll_IONetworkInterface(TRUE);
	if (networkInterfaces == NULL) {
		SC_log(LOG_NOTICE, "Cannot populate NetworkInterfaces.plist, no network interfaces");
		return;
	}

	interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(networkInterfaces); idx++) {
		CFIndex idx2 = 0;
		CFNumberRef if_type;
		CFNumberRef if_unit;
		SCNetworkInterfaceRef interface = CFArrayGetValueAtIndex(networkInterfaces, idx);
		CFDictionaryRef interfaceEntity;

		interfaceEntity = __SCNetworkInterfaceCopyStorageEntity(interface);
		if (interfaceEntity == NULL) {
			continue;
		}

		if_type = _SCNetworkInterfaceGetIOInterfaceType(interface);
		if_unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
		if ((if_type == NULL) || (if_unit == NULL)) {
			CFRelease(interfaceEntity);
			continue;
		}

		for (idx2 = 0; idx2 < CFArrayGetCount(interfaces); idx2++) {
			CFNumberRef db_type;
			CFNumberRef db_unit;
			CFDictionaryRef dict = CFArrayGetValueAtIndex(interfaces, idx2);

			db_type = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceType));
			db_unit = CFDictionaryGetValue(dict, CFSTR(kIOInterfaceUnit));
			res = CFNumberCompare(if_type, db_type, NULL);
			if (res == kCFCompareLessThan
			|| (res == kCFCompareEqualTo
			&& (CFNumberCompare(if_unit, db_unit, NULL) == kCFCompareLessThan))) {
				break;
			}
		}

		CFArrayInsertValueAtIndex(interfaces, idx2, interfaceEntity);
		CFRelease(interfaceEntity);

	}

	SCPreferencesSetValue(ni_prefs, INTERFACES, interfaces);
	CFRelease(interfaces);

	model = SCPreferencesGetValue(ni_prefs, MODEL);
	if (model == NULL) {
		model = _SC_hw_model(FALSE);
		SCPreferencesSetValue(ni_prefs, MODEL, model);
	}

	version = SCPreferencesGetValue(ni_prefs, kSCPrefVersion);
	if (version == NULL) {
		const int	new_version	= NETWORK_CONFIGURATION_VERSION;

		version = CFNumberCreate(NULL, kCFNumberIntType, &new_version);
		SCPreferencesSetValue(ni_prefs, kSCPrefVersion, version);
		CFRelease(version);
	}

	CFRelease(networkInterfaces);

	return;
}


/*
 *  _SCNetworkConfigurationPerformMigration will migrate configuration between source and destination systems
 */
CF_RETURNS_RETAINED
CFArrayRef
_SCNetworkConfigurationPerformMigration(CFURLRef sourceDir, CFURLRef currentDir, CFURLRef targetDir, CFDictionaryRef options)
{
#pragma unused(options)
	CFURLRef	currentDirConfig	= NULL;
	CFURLRef	currentSystemPath	= NULL;
	Boolean		migrationComplete	= FALSE;
	CFArrayRef	paths			= NULL;
	Boolean		removeTargetOnFailure	= FALSE;
	CFURLRef	sourceDirConfig		= NULL;
	CFURLRef	targetDirConfig		= NULL;

	SC_log(LOG_INFO,
	       "Perform Migration%s"
	       "\n  sourceDir  = %@"
	       "\n  currentDir = %@"
	       "\n  targetDir  = %@"
	       "\n  options    = %@",
	       _SC_isInstallEnvironment() ? " (INSTALLER ENVIRONMENT)" : "",
	       sourceDir,
	       currentDir,
	       targetDir,
	       options != NULL ? options : (CFDictionaryRef)CFSTR("None"));

	if ((sourceDir != NULL) && !CFURLHasDirectoryPath(sourceDir)) {
		SC_log(LOG_NOTICE, "Migration source is not a directory: %@", sourceDir);
		goto done;
	}

	if ((currentDir != NULL) && !CFURLHasDirectoryPath(currentDir)) {
		SC_log(LOG_NOTICE, "Migration current is not a directory: %@", currentDir);
		goto done;
	}

	if ((targetDir != NULL) && !CFURLHasDirectoryPath(targetDir)) {
		SC_log(LOG_NOTICE, "Migration target is not a directory: %@", targetDir);
		goto done;
	}

	// Both sourceDir and currentDir cannot be NULL because NULL value indicates using current system
	if (sourceDir == NULL && currentDir == NULL) {
		SC_log(LOG_NOTICE, "Both migration source and current are NULL");
		goto done;
	}

	currentSystemPath = CFURLCreateWithString(NULL,
						  PREFS_DEFAULT_DIR,
						  NULL);

	// if either of the sourceDir or currentDir are NULL, then populate it with current system path
	if (sourceDir == NULL) {
		sourceDirConfig = CFRetain(currentSystemPath);
	} else {
		sourceDirConfig = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
									      PREFS_DEFAULT_DIR_RELATIVE,
									      kCFURLPOSIXPathStyle,
									      TRUE,
									      sourceDir);
	}

	if (currentDir != NULL) {
		currentDirConfig = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
									       PREFS_DEFAULT_DIR_RELATIVE,
									       kCFURLPOSIXPathStyle,
									       TRUE,
									       currentDir);
	}
	// If the targetDir is not provided then migration will take place in currentDir
	if (targetDir == NULL) {
		targetDirConfig = CFRetain(currentSystemPath);
	} else {
		targetDirConfig = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
									      PREFS_DEFAULT_DIR_RELATIVE,
									      kCFURLPOSIXPathStyle,
									      TRUE,
									      targetDir);
	}
	// Source directory cannot be the same as target directory
	if (CFEqual(sourceDirConfig, targetDirConfig)) {
		SC_log(LOG_NOTICE, "Source directory cannot be the same as target directory");
		goto done;
	}

	if ((currentDirConfig == NULL) || !CFEqual(currentDirConfig, targetDirConfig)) {
		if (!_SCNetworkConfigurationMakePathIfNeeded(targetDirConfig)) {
			SC_log(LOG_NOTICE, "Could not create target directory");
			goto done;
		}

		if (!SCNetworkConfigurationCopyConfigurationFiles(currentDirConfig, targetDirConfig)) {
			SC_log(LOG_NOTICE, "Could not copy configuration files from \"%@\" to \"%@\"",
			       currentDirConfig,
			       targetDirConfig);
		} else if (currentDirConfig != NULL) {
			removeTargetOnFailure = TRUE;	// Configuration files were copied over to target directory
							// If migration failed, then we should remove those configuration
							// files since current directory and target directory are not
							// the same
		}
	}

	// If both source and current configurations point to current system, then no migration needs to be done.
	if ((currentDirConfig != NULL) && CFEqual(sourceDirConfig, currentDirConfig)) {
		SC_log(LOG_NOTICE, "No migration needed, source and current configurations have the same path");
		migrationComplete = TRUE;
	} else {
		migrationComplete = _SCNetworkConfigurationMigrateConfiguration(sourceDirConfig, targetDirConfig);
	}
	if (migrationComplete) {
		SC_log(LOG_NOTICE, "Migration complete");
		paths = _SCNetworkConfigurationCopyMigrationPaths(NULL);
	} else {
		SC_log(LOG_NOTICE, "Migration failed: %s", SCErrorString(SCError()));

		// If migration fails, then remove configuration files from target config if they were
		// copied from the current directory
		if (removeTargetOnFailure) {
			_SCNetworkConfigurationRemoveConfigurationFiles(targetDirConfig);
		}
	}
done:
	if (currentDirConfig != NULL) {
		CFRelease(currentDirConfig);
	}
	if (currentSystemPath != NULL) {
		CFRelease(currentSystemPath);
	}
	if (sourceDirConfig != NULL) {
		CFRelease(sourceDirConfig);
	}
	if (targetDirConfig != NULL) {
		CFRelease(targetDirConfig);
	}

	return paths;
}

static Boolean
_SCNetworkConfigurationMigrateIsFilePresent(CFURLRef filePath)
{
	Boolean		fileExists	= false;
	char		filePathStr[PATH_MAX];
	int		statResult	= 0;
	struct stat	statStruct	= { 0, };

	if (filePath == NULL) {
		SC_log(LOG_NOTICE, "_SCNetworkConfigurationMigrateIsFilePresent: No path");
		goto done;
	}

	if (!CFURLGetFileSystemRepresentation(filePath, TRUE, (UInt8*) filePathStr, sizeof(filePathStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", filePath);
		goto done;
	}

	statResult = stat(filePathStr, &statStruct);
	if (statResult == 0) {
		SCPreferencesRef	prefs;
		CFStringRef		prefsID;

		if (statStruct.st_size == 0) {
			SC_log(LOG_INFO, "_SCNetworkConfigurationMigrateIsFilePresent: empty .plist: %@", filePath);	// REMOVE
			goto done;
		}

		prefsID = CFStringCreateWithCString(NULL, filePathStr, kCFStringEncodingUTF8);
		prefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, prefsID);
		CFRelease(prefsID);
		if (prefs == NULL) {
			SC_log(LOG_NOTICE, "_SCNetworkConfigurationMigrateIsFilePresent: bad .plist: %@", filePath);
			goto done;
		}

		if (!__SCPreferencesIsEmpty(prefs)) {
			// if non-empty .plist
			fileExists = TRUE;
		} else {
			SC_log(LOG_NOTICE, "_SCNetworkConfigurationMigrateIsFilePresent: effectively empty .plist: %@", filePath);
		}

		CFRelease(prefs);
	}
done:
	return fileExists;
}

static Boolean
__SCNetworkConfigurationMigrateConfigurationFilesPresent(CFURLRef baseURL, CFArrayRef* migrationPaths, Boolean expected)
{
	Boolean			configFilesPresent	= FALSE;
	CFIndex			count;
	CFURLRef		filePath		= NULL;
	CFURLRef		interfaces;
	CFMutableArrayRef	migrationPathsMutable	= NULL;
	CFURLRef		prefs;

	if (baseURL == NULL) {
		SC_log(LOG_NOTICE, "No base migration URL");
		goto done;
	}

	migrationPathsMutable = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	_SCNetworkConfigurationCopyMigrationPathsWithBaseURL(baseURL, &prefs, &interfaces);
	CFArrayAppendValue(migrationPathsMutable, prefs);
	CFArrayAppendValue(migrationPathsMutable, interfaces);
	CFRelease(prefs);
	CFRelease(interfaces);

	*migrationPaths = migrationPathsMutable;

	count = CFArrayGetCount(*migrationPaths);
	for (CFIndex idx = 0; idx < count; idx++) {
		Boolean		present;

		filePath = CFArrayGetValueAtIndex(*migrationPaths, idx);
		present = _SCNetworkConfigurationMigrateIsFilePresent(filePath);
		if (!present) {
			if (expected) {
				SC_log(LOG_INFO, "Expected migration file not present: %@", filePath);
			}
			goto done;
		}
	}

	configFilesPresent = TRUE;	// all necessary configuration files present
done:
	return configFilesPresent;
}


static CFMutableArrayRef
_SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(SCPreferencesRef ni_prefs, Boolean isBuiltin)
{
	CFIndex			count			= 0;
	SCNetworkInterfaceRef	interface;
	CFArrayRef		interfaceList		= NULL;
	SCPreferencesRef	prefs			= NULL;
	CFMutableArrayRef	resultInterfaceList	= NULL;

	interfaceList = __SCNetworkInterfaceCopyStoredWithPreferences(ni_prefs);
	if (interfaceList == NULL) {
		goto done;
	}

	count = CFArrayGetCount(interfaceList);
	if (count > 0) {
		resultInterfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		prefs = SCPreferencesCreateCompanion(ni_prefs, NULL);
	}

	for (CFIndex i = 0; i < count; i++) {
		interface = CFArrayGetValueAtIndex(interfaceList, i);
		if (_SCNetworkInterfaceIsBuiltin(interface) == isBuiltin) {
			SCNetworkInterfacePrivateRef	newInterface;

			newInterface = __SCNetworkInterfaceCreateCopy(NULL, interface, prefs, NULL);
			CFArrayAppendValue(resultInterfaceList, newInterface);
			CFRelease(newInterface);
		}
	}

done:
	if (interfaceList != NULL) {
		CFRelease(interfaceList);
	}
	if (prefs != NULL) {
		CFRelease(prefs);
	}
	return resultInterfaceList;
}

static CFMutableDictionaryRef
_SCNetworkInterfaceStorageCopyMaxUnitPerInterfaceType(SCPreferencesRef ni_prefs)
{
	CFNumberRef		cfMaxUnit;
	CFIndex			count				= 0;
	CFArrayRef		ifList				= NULL;
	SCNetworkInterfaceRef	interface;
	CFMutableDictionaryRef	interfaceTypeToMaxUnitMapping	= NULL;
	CFNumberRef		type;
	CFNumberRef		unit;

	ifList = __SCNetworkInterfaceCopyStoredWithPreferences(ni_prefs);
	if (ifList == NULL) {
		SC_log(LOG_INFO, "No interfaces");
		return NULL;
	}

	interfaceTypeToMaxUnitMapping = CFDictionaryCreateMutable(NULL, 0,
								  &kCFTypeDictionaryKeyCallBacks,
								  &kCFTypeDictionaryValueCallBacks);
	count = CFArrayGetCount(ifList);
	for (CFIndex idx = 0; idx < count; idx++) {
		cfMaxUnit = NULL;
		interface = CFArrayGetValueAtIndex(ifList, idx);

		if (!isA_SCNetworkInterface(interface)) {
			continue;
		}

		type  = _SCNetworkInterfaceGetIOInterfaceType(interface);
		if (!isA_CFNumber(type)) {
			SC_log(LOG_INFO, "No interface type");
			continue;
		}

		if (!CFDictionaryContainsKey(interfaceTypeToMaxUnitMapping, type)) {
			int temp = 0;
			cfMaxUnit = CFNumberCreate(NULL, kCFNumberIntType, &temp);
			CFDictionaryAddValue(interfaceTypeToMaxUnitMapping, type, cfMaxUnit);
			CFRelease(cfMaxUnit);
		}

		if (cfMaxUnit == NULL) {
			cfMaxUnit = CFDictionaryGetValue(interfaceTypeToMaxUnitMapping, type);
		}

		unit = _SCNetworkInterfaceGetIOInterfaceUnit(interface);
		if (!isA_CFNumber(unit)) {
			continue;
		}

		if (CFNumberCompare(unit, cfMaxUnit, NULL) == kCFCompareGreaterThan) {
			CFDictionarySetValue(interfaceTypeToMaxUnitMapping, type, unit);
		}
	}
	if (ifList != NULL) {
		CFRelease(ifList);
	}
	return interfaceTypeToMaxUnitMapping;
}

static CFMutableDictionaryRef
_SCNetworkConfigurationCopyBuiltinMapping (SCPreferencesRef sourcePrefs, SCPreferencesRef targetPrefs)
{
	CFMutableDictionaryRef	builtinMapping			= NULL;
	CFIndex			sourceBuiltinInterfaceCount	= 0;
	CFMutableArrayRef	sourceBuiltinInterfaces		= NULL;
	SCNetworkInterfaceRef	sourceInterface;
	CFIndex			targetBuiltinInterfaceCount	= 0;
	CFMutableArrayRef	targetBuiltinInterfaces		= NULL;
	SCNetworkInterfaceRef	targetInterface;

	sourceBuiltinInterfaces = _SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(sourcePrefs, TRUE);
	if (!isA_CFArray(sourceBuiltinInterfaces)) {
		SC_log(LOG_INFO, "No source built-in interfaces");
		goto done;
	}
	sourceBuiltinInterfaceCount = CFArrayGetCount(sourceBuiltinInterfaces);

	targetBuiltinInterfaces = _SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(targetPrefs, TRUE);
	if (!isA_CFArray(targetBuiltinInterfaces)) {
		SC_log(LOG_INFO, "No target built-in interfaces");
		goto done;
	}

	// Builtin Mapping will try to map all source interfaces into target interfaces
	for (CFIndex idx = 0; idx < sourceBuiltinInterfaceCount; idx++) {
		Boolean		matched	= FALSE;

		sourceInterface = CFArrayGetValueAtIndex(sourceBuiltinInterfaces, idx);
		targetBuiltinInterfaceCount = CFArrayGetCount(targetBuiltinInterfaces);

		for (CFIndex idx2 = 0; idx2 < targetBuiltinInterfaceCount; idx2++) {
			CFDataRef	sourceHardwareAddress;
			CFDataRef	targetHardwareAddress;

			targetInterface = CFArrayGetValueAtIndex(targetBuiltinInterfaces, idx2);
			sourceHardwareAddress = _SCNetworkInterfaceGetHardwareAddress(sourceInterface);
			targetHardwareAddress = _SCNetworkInterfaceGetHardwareAddress(targetInterface);
			if (_SC_CFEqual(sourceHardwareAddress, targetHardwareAddress)) {
				if (builtinMapping == NULL) {
					builtinMapping = CFDictionaryCreateMutable(NULL, 0,
										   &kCFTypeDictionaryKeyCallBacks,
										   &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionaryAddValue(builtinMapping, sourceInterface, targetInterface);
				CFArrayRemoveValueAtIndex(targetBuiltinInterfaces, idx2);
				matched = TRUE;
				break;
			}
		}
		if (matched) {
			// proceed to next source interface
			continue;
		}

		for (CFIndex idx2 = 0; idx2 < targetBuiltinInterfaceCount; idx2++) {
			targetInterface = CFArrayGetValueAtIndex(targetBuiltinInterfaces, idx2);

			if (_SCNetworkConfigurationIsInterfaceNamerMappable(sourceInterface, targetInterface, FALSE)) {
				if (builtinMapping == NULL) {
					builtinMapping = CFDictionaryCreateMutable(NULL, 0,
										   &kCFTypeDictionaryKeyCallBacks,
										   &kCFTypeDictionaryValueCallBacks);
				}
				CFDictionaryAddValue(builtinMapping, sourceInterface, targetInterface);
				CFArrayRemoveValueAtIndex(targetBuiltinInterfaces, idx2);
				break;
			}
		}
	}

done:
	if (sourceBuiltinInterfaces != NULL) {
		CFRelease(sourceBuiltinInterfaces);
	}
	if (targetBuiltinInterfaces != NULL) {
		CFRelease(targetBuiltinInterfaces);
	}
	return builtinMapping;
}

static Boolean
isExcludedExternalInterface(SCNetworkInterfaceRef interface)
{
	SCNetworkInterfacePrivateRef	interfacePrivate	= (SCNetworkInterfacePrivateRef)interface;
#if	!TARGET_OS_SIMULATOR
	int				vid;
#endif	// !TARGET_OS_SIMULATOR

	if (_SCNetworkInterfaceIsHiddenInterface(interface)) {
		return TRUE;
	}

#if	!TARGET_OS_SIMULATOR
	if (!isA_CFNumber(interfacePrivate->usb.vid) ||
	    !CFNumberGetValue(interfacePrivate->usb.vid, kCFNumberIntType, &vid) ||
	    (vid != kIOUSBAppleVendorID)) {
		// if not "Apple" interface
		return FALSE;
	}
#endif	// !TARGET_OS_SIMULATOR

	if (_SC_CFEqual(interfacePrivate->name, CFSTR("iBridge"))) {
		// exclude "Apple T2 Controller"
		return TRUE;
	}

	return FALSE;
}

static CFMutableDictionaryRef
_SCNetworkConfigurationCopyExternalInterfaceMapping (SCPreferencesRef sourceNIPrefs, SCPreferencesRef targetNIPrefs)
{
	CFNumberRef		cfMaxTargetUnit			= NULL;
	CFNumberRef		currentInterfaceUnit		= NULL;
	CFMutableDictionaryRef	externalMapping			= NULL;
	CFMutableDictionaryRef	interfaceTypeToMaxUnitMapping	= NULL;
	int			maxTargetUnit;
	int			newTargetUnit;
	CFIndex			sourceExternalInterfaceCount	= 0;
	CFMutableArrayRef	sourceExternalInterfaces	= NULL;
	SCNetworkInterfaceRef	sourceInterface			= NULL;
	CFIndex			targetExternalInterfaceCount	= 0;
	CFMutableArrayRef	targetExternalInterfaces	= NULL;
	SCNetworkInterfaceRef	targetInterface			= NULL;
	SCPreferencesRef	targetPrefs			= NULL;
	CFNumberRef		type;

	sourceExternalInterfaces = _SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(sourceNIPrefs, FALSE);
	if (!isA_CFArray(sourceExternalInterfaces)) {
		SC_log(LOG_INFO, "No source external interfaces");
		goto done;
	}
	sourceExternalInterfaceCount = CFArrayGetCount(sourceExternalInterfaces);
	if (sourceExternalInterfaceCount == 0) {
		SC_log(LOG_INFO, "No source external interfaces");
		goto done;
	}

	targetExternalInterfaces = _SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(targetNIPrefs, FALSE);
	if (!isA_CFArray(targetExternalInterfaces)) {
		SC_log(LOG_INFO, "No target external interfaces");
		goto done;
	}

	interfaceTypeToMaxUnitMapping = _SCNetworkInterfaceStorageCopyMaxUnitPerInterfaceType(targetNIPrefs);
	externalMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	targetPrefs = SCPreferencesCreateCompanion(targetNIPrefs, NULL);

	// Map all external interfaces which exist in both source and target
	for (CFIndex idx = 0; idx < sourceExternalInterfaceCount; idx++) {
		Boolean		matched	= FALSE;

		sourceInterface = CFArrayGetValueAtIndex(sourceExternalInterfaces, idx);
		targetExternalInterfaceCount = CFArrayGetCount(targetExternalInterfaces);
		currentInterfaceUnit = NULL;

		for (CFIndex idx2 = 0; idx2 < targetExternalInterfaceCount; idx2++) {
			CFDataRef	sourceHardwareAddress;
			CFDataRef	targetHardwareAddress;

			targetInterface = CFArrayGetValueAtIndex(targetExternalInterfaces, idx2);
			sourceHardwareAddress = _SCNetworkInterfaceGetHardwareAddress(sourceInterface);
			targetHardwareAddress = _SCNetworkInterfaceGetHardwareAddress(targetInterface);
			if (_SC_CFEqual(sourceHardwareAddress, targetHardwareAddress)) {
				CFDictionaryAddValue(externalMapping, sourceInterface, targetInterface);
				CFArrayRemoveValueAtIndex(targetExternalInterfaces, idx2);
				matched = TRUE;
				break;
			}
		}
		if (matched) {
			// proceed to next source interface
			continue;
		}

		for (CFIndex idx2 = 0; idx2 < targetExternalInterfaceCount; idx2++) {
			targetInterface = CFArrayGetValueAtIndex(targetExternalInterfaces, idx2);

			if (_SCNetworkConfigurationIsInterfaceNamerMappable(sourceInterface, targetInterface, TRUE)) {
				CFDictionaryAddValue(externalMapping, sourceInterface, targetInterface);
				CFArrayRemoveValueAtIndex(targetExternalInterfaces, idx2);
				matched = TRUE;
				break;
			}
		}
		if (matched) {
			// proceed to next source interface
			continue;
		}

		// Before we create mapping for external source interfaces that do not exist
		// in the target we need check for and handle some exclusions.
		if (isExcludedExternalInterface(sourceInterface)) {
			// if excluded
			continue;
		}

		// Create new mappings for external source interfaces which don't exist in the target
		type = _SCNetworkInterfaceGetIOInterfaceType(sourceInterface);

		cfMaxTargetUnit = CFDictionaryGetValue(interfaceTypeToMaxUnitMapping, type);
		if (cfMaxTargetUnit != NULL) {
			CFNumberGetValue(cfMaxTargetUnit, kCFNumberIntType, &maxTargetUnit);
			newTargetUnit = maxTargetUnit + 1;
		} else {
			newTargetUnit = 0;
		}

		cfMaxTargetUnit = CFNumberCreate(NULL, kCFNumberIntType, &newTargetUnit);
		CFDictionarySetValue(interfaceTypeToMaxUnitMapping, type, cfMaxTargetUnit);

		targetInterface = (SCNetworkInterfaceRef)__SCNetworkInterfaceCreateCopy(NULL,
											sourceInterface,
											targetPrefs,
											NULL);

		currentInterfaceUnit = _SCNetworkInterfaceGetIOInterfaceUnit(targetInterface);
		if (!isA_CFNumber(currentInterfaceUnit) ||
		    !CFEqual(currentInterfaceUnit, cfMaxTargetUnit)) {
			// Update the interface unit
			__SCNetworkInterfaceSetIOInterfaceUnit(targetInterface, cfMaxTargetUnit);
		}

		CFDictionaryAddValue(externalMapping, sourceInterface, targetInterface);

		CFRelease(targetInterface);
		targetInterface = NULL;
		CFRelease(cfMaxTargetUnit);
		cfMaxTargetUnit = NULL;

	}
done:
	if (sourceExternalInterfaces != NULL) {
		CFRelease(sourceExternalInterfaces);
	}
	if (targetExternalInterfaces != NULL) {
		CFRelease(targetExternalInterfaces);
	}
	if (interfaceTypeToMaxUnitMapping != NULL) {
		CFRelease(interfaceTypeToMaxUnitMapping);
	}
	if (targetPrefs != NULL) {
		CFRelease(targetPrefs);
	}
	return externalMapping;
}

static Boolean
__SCNetworkConfigurationInterfaceNameIsEquiv(CFStringRef interfaceName1, CFStringRef interfaceName2);

static Boolean
_SCNetworkConfigurationIsInterfaceNamerMappable(SCNetworkInterfaceRef interface1, SCNetworkInterfaceRef interface2, Boolean bypassActive)
{
#pragma unused(bypassActive)
	Boolean		interface1IsBuiltin;
	CFStringRef	interface1Prefix;
	CFStringRef	interface1Type;
	CFStringRef	interface1UserDefinedName;
	Boolean		interface2IsBuiltin;
	CFStringRef	interface2Prefix;
	CFStringRef	interface2Type;
	CFStringRef	interface2UserDefinedName;

	if (interface1 == interface2) {
		// No work needs to be done
		return TRUE;
	}

	// Check if have same builtin values.
	interface1IsBuiltin = _SCNetworkInterfaceIsBuiltin(interface1);
	interface2IsBuiltin = _SCNetworkInterfaceIsBuiltin(interface2);
	if (interface1IsBuiltin != interface2IsBuiltin) {
		return FALSE;
	}

	// Check if SCNetworkInterface Type matches
	interface1Type = SCNetworkInterfaceGetInterfaceType(interface1);
	interface2Type = SCNetworkInterfaceGetInterfaceType(interface2);
	if (!_SC_CFEqual(interface1Type, interface2Type)) {
		return FALSE;
	}

	// Check if SCNetworkInterface prefix matches
	interface1Prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface1);
	interface2Prefix = _SCNetworkInterfaceGetIOInterfaceNamePrefix(interface2);
	if (!_SC_CFEqual(interface1Prefix, interface2Prefix)) {
		return FALSE;
	}

	// Check if user defined name matches
	interface1UserDefinedName = SCNetworkInterfaceGetLocalizedDisplayName(interface1);
	interface2UserDefinedName = SCNetworkInterfaceGetLocalizedDisplayName(interface2);
	if (!_SC_CFEqual(interface1UserDefinedName, interface2UserDefinedName)) {
		// Checking if we have a mismatch because of the name Ethernet and Ethernet 1
		// Checking if we have a mismatch because of the name Airport and WiFi
		if (interface1IsBuiltin &&
		    interface2IsBuiltin &&
		    __SCNetworkConfigurationInterfaceNameIsEquiv(interface1UserDefinedName, interface2UserDefinedName)) {
			    return TRUE;
		}
		return FALSE;
	}

	return TRUE;
}

static CFStringRef
interfaceNameCreateWithoutPortSuffix(CFStringRef interfaceName)
{
	CFIndex		len;
	CFRange		range;
	CFStringRef	shortName	= NULL;

	range = CFStringFind(interfaceName, CFSTR(", Port "), kCFCompareBackwards);
	if (range.location == kCFNotFound) {
		return NULL;
	}

	len = CFStringGetLength(interfaceName);
	for (CFIndex i = range.location + range.length; i < len; i++) {
		UniChar	ch;

		ch = CFStringGetCharacterAtIndex(interfaceName, i);
		if (!isdigit(ch)) {
			// if not digit
			return NULL;
		}
	}

	shortName = CFStringCreateWithSubstring(NULL, interfaceName, CFRangeMake(0, range.location));
	return shortName;
}

static Boolean
__SCNetworkConfigurationInterfaceNameIsEquiv(CFStringRef interfaceName1, CFStringRef interfaceName2)
{
	CFStringRef	interfaceArray[]	= { CFSTR("iPhone"), CFSTR("X-iPhone"),
						    CFSTR("iPad"),   CFSTR("X-iPad"),
						    CFSTR("iPod"),
						    CFSTR("AppleTV") };
	const int	interfaceCount		= sizeof(interfaceArray) / sizeof(CFStringRef);
	Boolean		match			= FALSE;
	CFStringRef	shortName;

	if ((isA_CFString(interfaceName1)) && (isA_CFString(interfaceName2))) {
		if (!CFEqual(interfaceName1, interfaceName2)) {
			// Check if we are looking at the WiFi interface
			if ((CFEqual(interfaceName1, CFSTR("AirPort")) ||
			     (CFEqual(interfaceName1, CFSTR("Wi-Fi")))) &&
			    (CFEqual(interfaceName2, CFSTR("AirPort")) ||
			     (CFEqual(interfaceName2, CFSTR("Wi-Fi"))))) {
				return TRUE;
			}

			if (((CFEqual(interfaceName1, CFSTR("Ethernet"))) ||
			     (CFEqual(interfaceName1, CFSTR("Ethernet 1")))) &&
			    ((CFEqual(interfaceName2, CFSTR("Ethernet"))) ||
			     (CFEqual(interfaceName2, CFSTR("Ethernet 1"))))) {
				return TRUE;
			}

			shortName = interfaceNameCreateWithoutPortSuffix(interfaceName1);
			if (shortName != NULL) {
				// check for (and match) "Thunderbolt Ethernet Slot X" vs. "Thunderbolt Ethernet Slot X, Port Y"
				match = CFEqual(shortName, interfaceName2);
				CFRelease(shortName);
			} else {
				// check for (and match) "Thunderbolt Ethernet Slot X, Port Y" vs. "Thunderbolt Ethernet Slot X"
				shortName = interfaceNameCreateWithoutPortSuffix(interfaceName2);
				if (shortName != NULL) {
					match = CFEqual(shortName, interfaceName1);
					CFRelease(shortName);
				}
			}
			if (match) {
				return TRUE;
			}

			for (CFIndex idx = 0; idx < interfaceCount; idx++) {
				CFStringRef tempInterfaceName = interfaceArray[idx];
				if ((CFEqual(interfaceName1, tempInterfaceName) ||
				     __SCNetworkInterfaceMatchesName(interfaceName1, tempInterfaceName)) &&
				    (CFEqual(interfaceName2, tempInterfaceName) ||
				     __SCNetworkInterfaceMatchesName(interfaceName2, tempInterfaceName))) {
					return TRUE;
				}
			}
		} else {
			return TRUE;
		}
	}

	return FALSE;
}

typedef struct {
	CFDictionaryRef		interfaceMapping;
	CFMutableArrayRef	interfacesMissingServices;
} missingServiceContext;

typedef struct {
	CFDictionaryRef		bsdNameToBridgeServices;	// Mapping of BSD Name to SCBridgeInterfaceRef
	CFDictionaryRef		bsdNameToBondServices;		// Mapping of BSD Name to SCBondInterfaceRef
	CFDictionaryRef		bsdNameToVLANServices;		// Mapping of BSD Name to SCVLANInterfaceRef
	CFDictionaryRef		interfaceMapping;
	Boolean*		isValid;
	CFMutableArrayRef	interfacesToRemove;		// SCNetworkInterfaceRef. Services containing the interface will be removed
	CFMutableArrayRef	interfacesToReplace;		// SCNetworkInterfaceRef. Services containing the interface will be replaced with default service
	CFMutableArrayRef	interfacesToPreserveServiceInformation; // SCNetworkInterfaceRef. Services containing the interface will be replaced with new service which has same configuration as the current service with issue.
	CFMutableDictionaryRef	bsdNameServiceProtocolPreserveMapping;
	SCPreferencesRef	prefs;
	Boolean			repair;
} validityContext;

static void
_SCNetworkConfigurationValidateInterface(const void *key, const void *value, void *context)
{
	CFStringRef		bsdName					= (CFStringRef)key;
	validityContext		*ctx					= (validityContext*)context;
	CFDictionaryRef		bsdNameToBridgeServices			= ctx->bsdNameToBridgeServices;
	CFDictionaryRef		bsdNameToBondServices			= ctx->bsdNameToBondServices;
	CFDictionaryRef		bsdNameToVLANServices			= ctx->bsdNameToVLANServices;
	SCNetworkInterfaceRef	interface				= NULL;
	CFDictionaryRef		interfaceMapping			= ctx->interfaceMapping;
	CFStringRef		interfaceUserDefinedName		= NULL;
	Boolean			repair					= ctx->repair;
	SCNetworkInterfaceRef	serviceInterface			= (SCNetworkInterfaceRef)value;
	CFStringRef		serviceInterfaceUserDefinedName		= NULL;
	CFMutableArrayRef	interfacesToRemove			= ctx->interfacesToRemove;
	CFMutableArrayRef	interfacesToReplace			= ctx->interfacesToReplace;
	CFMutableArrayRef	interfacesToPreserveServiceInformation	= ctx->interfacesToPreserveServiceInformation;

	// No work needs to be done if we have already made determination that configuration somewhere is not valid,
	// or we don't intend to repair invalid configuration.
	if ((*ctx->isValid == FALSE) && !repair) {
		return;
	}

	// There is no interface present for the service
	interface = CFDictionaryGetValue(interfaceMapping, bsdName);
	if (interface == NULL) {
		if (((bsdNameToBridgeServices != NULL) && !CFDictionaryContainsKey(bsdNameToBridgeServices, bsdName)) &&
		    ((bsdNameToBondServices   != NULL) && !CFDictionaryContainsKey(bsdNameToBondServices  , bsdName)) &&
		    ((bsdNameToVLANServices   != NULL) && !CFDictionaryContainsKey(bsdNameToVLANServices  , bsdName))) {
			// Not a virtual interface
			SC_log(LOG_NOTICE,
			       "Configuration validity check: no interface with BSD name \"%@\" present for service",
			       bsdName);

			if (repair) {
				CFArrayAppendValue(interfacesToRemove, serviceInterface);
			}
			*ctx->isValid = FALSE;
		}
		return;
	}

	// Need to compare between both SCNetworkInterfaceRefs
	interfaceUserDefinedName = __SCNetworkInterfaceGetUserDefinedName(interface);
	serviceInterfaceUserDefinedName = __SCNetworkInterfaceGetUserDefinedName(serviceInterface);

	if (!__SCNetworkConfigurationInterfaceNameIsEquiv(interfaceUserDefinedName, serviceInterfaceUserDefinedName)) {
		SC_log(LOG_NOTICE,
		       "Configuration validity check: interface user defined name \"%@\" doesn't match service/interface user defined name \"%@\"",
		       interfaceUserDefinedName,
		       serviceInterfaceUserDefinedName);
		*ctx->isValid = FALSE;
		// Check if the service interface name is set to localized key
		if (isA_CFArray(interfacesToPreserveServiceInformation) != NULL &&
		    __SCNetworkInterfaceMatchesName(interfaceUserDefinedName, serviceInterfaceUserDefinedName)) {
			SC_log(LOG_NOTICE,
			       "Configuration validity check: serviceInterfaceUserDefinedName: \"%@\" is the localized key for interface name \"%@\"",
			       serviceInterfaceUserDefinedName,
			       interfaceUserDefinedName);
			CFArrayAppendValue(interfacesToPreserveServiceInformation, serviceInterface);
		}
		// Add service interface to the interfacesToReplace list
		if (isA_CFArray(interfacesToReplace) != NULL) {
			CFArrayAppendValue(interfacesToReplace, interface);
		}
		if (isA_CFArray(interfacesToRemove) != NULL) {
			CFArrayAppendValue(interfacesToRemove, serviceInterface);
		}
		return;
	}
}

static void
_SCNetworkConfigurationCollectMissingService(const void *key, const void *value, void *context)
{
	CFStringRef		bsdName				= (CFStringRef)key;
	missingServiceContext	*ctx				= (missingServiceContext*)context;
	SCNetworkInterfaceRef	interface			= (SCNetworkInterfaceRef)value;
	CFMutableArrayRef	interfacesMissingServices	= ctx->interfacesMissingServices;
	CFDictionaryRef		serviceInterfaceMapping		= ctx->interfaceMapping;

	if (!isA_SCNetworkInterface(interface) ||
	    !_SCNetworkInterfaceIsBuiltin(interface)) {
		return;
	}

	// Check if services have mapping for the BSD name of the interface
	if (!CFDictionaryContainsKey(serviceInterfaceMapping, bsdName)) {
		CFArrayAppendValue(interfacesMissingServices, interface); // Adding interface since the corresponding service seems to be missing
	}
}

static Boolean
_SCNetworkConfigurationCreateBuiltinInterfaceServices(SCPreferencesRef pref,
						      SCPreferencesRef ni_pref)
{
	missingServiceContext	context;
	SCNetworkInterfaceRef	interface				= NULL;
	CFArrayRef		interfaces				= NULL;
	CFMutableArrayRef	interfacesWithoutService		= NULL;
	CFDictionaryRef		mappingBSDNameToInterface		= NULL;
	CFDictionaryRef		mappingServiceBSDNameToInterface	= NULL;
	CFIndex			missingServiceCount			= 0;
	Boolean			success					= FALSE;

	interfaces = __SCNetworkInterfaceCopyStoredWithPreferences(ni_pref);
	if (interfaces == NULL) {
		SC_log(LOG_NOTICE, "No interfaces");
		goto done;
	}

	mappingBSDNameToInterface = __SCNetworkInterfaceCreateMappingUsingBSDName(interfaces);
	CFRelease(interfaces);
	if (!isA_CFDictionary(mappingBSDNameToInterface)) {
		goto done;
	}

	interfaces = __SCNetworkServiceCopyAllInterfaces(pref);
	if (interfaces == NULL) {
		SC_log(LOG_NOTICE, "No [service] interfaces");
		goto done;
	}
	mappingServiceBSDNameToInterface = __SCNetworkInterfaceCreateMappingUsingBSDName(interfaces);
	CFRelease(interfaces);
	if (!isA_CFDictionary(mappingServiceBSDNameToInterface)) {
		goto done;
	}

	interfacesWithoutService = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	context.interfaceMapping = mappingServiceBSDNameToInterface;
	context.interfacesMissingServices = interfacesWithoutService;

	CFDictionaryApplyFunction(mappingBSDNameToInterface, _SCNetworkConfigurationCollectMissingService, &context);
	missingServiceCount = CFArrayGetCount(interfacesWithoutService);

	success = TRUE;

	for (CFIndex idx = 0; idx < missingServiceCount; idx++) {
		interface = CFArrayGetValueAtIndex(interfacesWithoutService, idx);

		if (__SCNetworkInterfaceIsMember(pref, interface)) {
			// if the interface is a member of a bond or bridge
			continue;
		}

		if (!__SCNetworkServiceCreate(pref, interface, NULL)) {
			SC_log(LOG_NOTICE, "Could not create service for interface: %@", interface);
			success = FALSE;
		}
	}
done:
	if (mappingBSDNameToInterface != NULL) {
		CFRelease(mappingBSDNameToInterface);
	}
	if (mappingServiceBSDNameToInterface != NULL) {
		CFRelease(mappingServiceBSDNameToInterface);
	}
	if (interfacesWithoutService != NULL) {
		CFRelease(interfacesWithoutService);
	}

	return success;
}

static void
add_service(const void *value, void *context)
{
	validityContext		*ctx				= (validityContext *)context;
	SCNetworkSetRef		currentSet			= NULL;
	Boolean			enabled;
	SCNetworkInterfaceRef	interface			= (SCNetworkInterfaceRef)value;
	CFDictionaryRef		bsdNameServiceProtocolMapping	= ctx->bsdNameServiceProtocolPreserveMapping;
	SCPreferencesRef	prefs				= ctx->prefs;
	SCNetworkServiceRef	service;
	CFStringRef		serviceID;
	CFStringRef		serviceName;
	CFStringRef		bsdName				= SCNetworkInterfaceGetBSDName(interface);
	CFArrayRef		protocolArray			= NULL;

	if (isA_CFString(bsdName)) {
		protocolArray = CFDictionaryGetValue(bsdNameServiceProtocolMapping, bsdName);
	}
	service = SCNetworkServiceCreate(prefs, interface);
	if (service == NULL) {
		SC_log(LOG_NOTICE, "Could not create new service");
		goto done;
	}

	if (!SCNetworkServiceEstablishDefaultConfiguration(service)) {
		SC_log(LOG_NOTICE, "SCNetworkServiceEstablishDefaultConfiguration() failed");
		SCNetworkServiceRemove(service);
		goto done;
	}

	if (protocolArray != NULL) {
		CFIndex protocolArrayCount = CFArrayGetCount(protocolArray);

		for (CFIndex idx = 0; idx < protocolArrayCount; idx++) {
			CFDictionaryRef protocolInfo = CFArrayGetValueAtIndex(protocolArray, idx);
			CFDictionaryRef configuration = CFDictionaryGetValue(protocolInfo, kProtocolConfiguration);
			CFStringRef protocolType = CFDictionaryGetValue(protocolInfo, kProtocolType);
			CFBooleanRef cfEnabled = CFDictionaryGetValue(protocolInfo, kProtocolEnabled);
			if (cfEnabled) {
				enabled = CFBooleanGetValue(cfEnabled);
			} else {
				enabled = FALSE;
			}
			__SCNetworkServiceAddProtocolToService(service, protocolType, configuration, enabled);
		}
	}

	// Add service to current set
	currentSet = SCNetworkSetCopyCurrent(prefs);
	if (currentSet == NULL) {
		SC_log(LOG_NOTICE, "Could not find current set");
		SCNetworkServiceRemove(service);
		goto done;
	}

	if (!SCNetworkSetAddService(currentSet, service)) {
		SC_log(LOG_NOTICE,  "Could not add service to current set");
		SCNetworkServiceRemove(service);
		goto done;
	}

	serviceID   = SCNetworkServiceGetServiceID(service);
	serviceName = SCNetworkServiceGetName(service);
	if (serviceName == NULL) serviceName = CFSTR("");
	SC_log(LOG_INFO, "Adding service : %@ (%@%s%@)",
		       serviceID,
		       serviceName,
		       bsdName != NULL ? ", " : "",
		       bsdName != NULL ? bsdName : CFSTR(""));

    done:

	if (service != NULL) {
		CFRelease(service);
	}
	if (currentSet != NULL) {
		CFRelease(currentSet);
	}
}

static void
create_bsd_name_service_protocol_mapping(const void *value, void *context)
{
	validityContext		*ctx					= (validityContext *)context;
	CFArrayRef		interfacesToPreserveServiceInformation	= ctx->interfacesToPreserveServiceInformation;
	CFMutableDictionaryRef	bsdNameServiceProtocolMapping		= ctx->bsdNameServiceProtocolPreserveMapping;
	SCNetworkInterfaceRef	interface;
	SCNetworkServiceRef	service					= (SCNetworkServiceRef)value;

	interface = SCNetworkServiceGetInterface(service);

	if (CFArrayContainsValue(interfacesToPreserveServiceInformation, CFRangeMake(0, CFArrayGetCount(interfacesToPreserveServiceInformation)), interface)) {
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (isA_CFString(bsdName)) {

			CFArrayRef protocols = SCNetworkServiceCopyProtocols(service);
			if (protocols != NULL) {
				CFMutableArrayRef protocolArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				CFIndex protocolCount = CFArrayGetCount(protocols);

				for (CFIndex idx = 0; idx < protocolCount; idx++) {
					SCNetworkProtocolRef protocol = CFArrayGetValueAtIndex(protocols, idx);
					CFDictionaryRef configuration = SCNetworkProtocolGetConfiguration(protocol);
					CFStringRef protocolType = SCNetworkProtocolGetProtocolType(protocol);
					Boolean enabled = SCNetworkProtocolGetEnabled(protocol);

					if (configuration == NULL ||  protocolType == NULL) {
						continue;
					}
					CFMutableDictionaryRef protocolInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

					CFDictionaryAddValue(protocolInfo, kProtocolType, protocolType);
					CFDictionaryAddValue(protocolInfo, kProtocolConfiguration, configuration);
					CFDictionaryAddValue(protocolInfo, kProtocolEnabled, enabled ? kCFBooleanTrue : kCFBooleanFalse);
					CFArrayAppendValue(protocolArray, protocolInfo);
					CFRelease(protocolInfo);
				}
				CFDictionaryAddValue(bsdNameServiceProtocolMapping, bsdName, protocolArray);
				CFRelease(protocols);
				CFRelease(protocolArray);
			}

		}
	}
}

static void
remove_service(const void *value, void *context)
{
	validityContext		*ctx		= (validityContext *)context;
	SCNetworkInterfaceRef	interface;
	SCNetworkServiceRef	service		= (SCNetworkServiceRef)value;
	CFArrayRef		toBeRemoved	= ctx->interfacesToRemove;

	interface = SCNetworkServiceGetInterface(service);
	if (CFArrayContainsValue(toBeRemoved, CFRangeMake(0, CFArrayGetCount(toBeRemoved)), interface)) {
		CFStringRef	bsdName;
		CFStringRef	serviceID;
		CFStringRef	serviceName;

		serviceID   = SCNetworkServiceGetServiceID(service);
		serviceName = SCNetworkServiceGetName(service);
		if (serviceName == NULL) serviceName = CFSTR("");
		bsdName     = SCNetworkInterfaceGetBSDName(interface);
		SC_log(LOG_INFO, "Removing service : %@ (%@%s%@)",
			       serviceID,
			       serviceName,
			       bsdName != NULL ? ", " : "",
			       bsdName != NULL ? bsdName : CFSTR(""));

		SCNetworkServiceRemove(service);
	}
}

static Boolean
_SCNetworkConfigurationRepairUsingPreferences(SCPreferencesRef	prefs,
					      SCPreferencesRef	ni_prefs,
					      validityContext	*context)
{
	CFIndex		removeCount;
	CFIndex		replaceCount;
	CFArrayRef	serviceList;
	CFArrayRef	interfacesToRemove	= context->interfacesToRemove;
	CFArrayRef	interfacesToReplace	= context->interfacesToReplace;

	removeCount = CFArrayGetCount(interfacesToRemove);
	replaceCount = CFArrayGetCount(interfacesToReplace);
	if ((removeCount == 0) && (replaceCount == 0)) {
		// We don't have any information to repair
		return FALSE;
	}

	// Backup current preferences before making changes
	__SCNetworkConfigurationBackup(prefs, CFSTR("pre-repair"), prefs);
	__SCNetworkConfigurationBackup(ni_prefs, CFSTR("pre-repair"), prefs);

	__SCNetworkConfigurationReport(LOG_DEBUG, "pre-repair", prefs, ni_prefs);
	if (interfacesToRemove != NULL) {
		logInterfaces(LOG_DEBUG, "Interfaces to be removed", interfacesToRemove, FALSE);
	}
	if (interfacesToReplace != NULL) {
		logInterfaces(LOG_DEBUG, "Interfaces to be replaced", interfacesToReplace, FALSE);
	}

	serviceList = SCNetworkServiceCopyAll(prefs);
	CFArrayApplyFunction(serviceList, CFRangeMake(0, CFArrayGetCount(serviceList)), create_bsd_name_service_protocol_mapping, context);
	if (context->bsdNameServiceProtocolPreserveMapping != NULL) {
		logMapping(LOG_DEBUG, context->bsdNameServiceProtocolPreserveMapping, "BSD name / Service Protocol mapping");
	}

	CFArrayApplyFunction(serviceList, CFRangeMake(0, CFArrayGetCount(serviceList)), remove_service, (void*)context);
	CFArrayApplyFunction(interfacesToReplace, CFRangeMake(0, replaceCount), add_service, (void*)context);
	CFRelease(serviceList);

	__SCNetworkConfigurationReport(LOG_DEBUG, "post-repair", prefs, ni_prefs);

	return TRUE;
}

static void
validate_bridge(const void *value, void *context)
{
	SCBridgeInterfaceRef	bridge			= (SCBridgeInterfaceRef)value;
	CFArrayRef		memberInterfaces	= SCBridgeInterfaceGetMemberInterfaces(bridge);
	CFMutableArrayRef	memberInterfacesMutable	= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	SCPreferencesRef	ni_prefs		= (SCPreferencesRef)context;

	for (CFIndex idx = 0; idx < CFArrayGetCount(memberInterfaces); idx++) {
		CFStringRef		bsdName;
		SCNetworkInterfaceRef	interface	= (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(memberInterfaces, idx);
		SCNetworkInterfaceRef	memberInterface;

		bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (bsdName == NULL) {
			continue;
		}

		// Check if member interface is present
		memberInterface = __SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(NULL, ni_prefs, bsdName);
		if (memberInterface != NULL) {
			CFArrayAppendValue(memberInterfacesMutable, memberInterface);
			CFRelease(memberInterface);
		}
	}

	if (CFArrayGetCount(memberInterfacesMutable) == 0) {
		SC_log(LOG_NOTICE, "Removing bridge w/no member interfaces: %@", bridge);
		SCBridgeInterfaceRemove(bridge);
	} else {
		SCBridgeInterfaceSetMemberInterfaces(bridge, memberInterfacesMutable);
	}
	CFRelease(memberInterfacesMutable);
}

#if	!TARGET_OS_IPHONE
static void
validate_bond(const void *value, void *context)
{
	SCBondInterfaceRef	bond			= (SCBondInterfaceRef)value;
	CFArrayRef		memberInterfaces	= SCBondInterfaceGetMemberInterfaces(bond);
	CFMutableArrayRef	memberInterfacesMutable	= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	SCPreferencesRef	ni_prefs		= (SCPreferencesRef)context;

	for (CFIndex idx = 0; idx < CFArrayGetCount(memberInterfaces); idx++) {
		CFStringRef		bsdName;
		SCNetworkInterfaceRef	interface	= (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(memberInterfaces, idx);
		SCNetworkInterfaceRef	memberInterface;

		bsdName = SCNetworkInterfaceGetBSDName(interface);
		if (bsdName == NULL) {
			continue;
		}

		// Check if member interface is present
		memberInterface = __SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(NULL, ni_prefs, bsdName);
		if (memberInterface != NULL) {
			CFArrayAppendValue(memberInterfacesMutable, memberInterface);
			CFRelease(memberInterface);
		}
	}

	if (CFArrayGetCount(memberInterfacesMutable) == 0) {
		SC_log(LOG_NOTICE, "Removing bond w/no member interfaces: %@", bond);
		SCBondInterfaceRemove(bond);
	} else {
		SCBondInterfaceSetMemberInterfaces(bond, memberInterfacesMutable);
	}
	CFRelease(memberInterfacesMutable);
}
#endif	// !TARGET_OS_IPHONE

typedef struct {
	SCPreferencesRef	prefs;
	SCPreferencesRef	ni_prefs;
#if	!TARGET_OS_IPHONE
	CFArrayRef		bondInterfaces;
#endif	// !TARGET_OS_IPHONE
	CFArrayRef		bridgeInterfaces;
} validate_prefs_context;

static validate_prefs_context *
validate_prefs_context_create(SCPreferencesRef prefs, SCPreferencesRef ni_prefs)
{
	validate_prefs_context	*context;

	context = calloc(1, sizeof(validate_prefs_context));
	context->prefs = prefs;
	context->ni_prefs = ni_prefs;
#if	!TARGET_OS_IPHONE
	context->bondInterfaces = (CFArrayRef)kCFNull;
#endif	// !TARGET_OS_IPHONE
	context->bridgeInterfaces = (CFArrayRef)kCFNull;
	return context;
}

static void
validate_prefs_context_release(validate_prefs_context *context)
{
#if	!TARGET_OS_IPHONE
	if ((context->bondInterfaces != NULL) && (context->bondInterfaces != (CFArrayRef)kCFNull)) {
		CFRelease(context->bondInterfaces);
	}
#endif	// !TARGET_OS_IPHONE
	if ((context->bridgeInterfaces != NULL) && (context->bridgeInterfaces != (CFArrayRef)kCFNull)) {
		CFRelease(context->bridgeInterfaces);
	}
	free(context);
}

static Boolean
interfaceArrayContainsBSDName(CFArrayRef interfaces, CFStringRef bsdName)
{
	Boolean	match	= FALSE;
	CFIndex	n	= CFArrayGetCount(interfaces);

	for (CFIndex i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface;
		CFStringRef		matchName;

		interface = CFArrayGetValueAtIndex(interfaces, i);
		matchName = SCNetworkInterfaceGetBSDName(interface);
		if (_SC_CFEqual(bsdName, matchName)) {
			match = TRUE;
			break;
		}
	}

	return match;
}

static void
validate_vlan(const void *value, void *context)
{
	CFStringRef		bsdName;
	SCNetworkInterfaceRef	interface;
	Boolean			isValid			= FALSE;
	SCNetworkInterfaceRef	physicalInterface;
	validate_prefs_context	*prefs			= (validate_prefs_context *)context;
	SCVLANInterfaceRef	vlan			= (SCVLANInterfaceRef)value;

	physicalInterface = SCVLANInterfaceGetPhysicalInterface(vlan);
	bsdName = SCNetworkInterfaceGetBSDName(physicalInterface);
	if (bsdName == NULL) {
		goto done;
	}

	// Check if the physical interface is present
	interface = __SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(NULL, prefs->ni_prefs, bsdName);
	if (interface != NULL) {
		CFRelease(interface);
		isValid = TRUE;
		goto done;
	}

#if	!TARGET_OS_IPHONE
	if (prefs->bondInterfaces == (CFArrayRef)kCFNull) {
		prefs->bondInterfaces = SCBondInterfaceCopyAll(prefs->prefs);
	}
	if (prefs->bondInterfaces != NULL) {
		isValid = interfaceArrayContainsBSDName(prefs->bondInterfaces, bsdName);
		if (isValid) {
			goto done;
		}
	}
#endif	// !TARGET_OS_IPHONE

	if (prefs->bridgeInterfaces == (CFArrayRef)kCFNull) {
		prefs->bridgeInterfaces = SCBridgeInterfaceCopyAll(prefs->prefs);
	}
	if (prefs->bridgeInterfaces != NULL) {
		isValid = interfaceArrayContainsBSDName(prefs->bridgeInterfaces, bsdName);
		if (isValid) {
			goto done;
		}
	}

    done:
	if (!isValid) {
		SC_log(LOG_NOTICE, "Removing VLAN w/no physical interface: %@", vlan);
		SCVLANInterfaceRemove(vlan);
	}
}

Boolean
_SCNetworkConfigurationCheckValidityWithPreferences(SCPreferencesRef	prefs,
						     SCPreferencesRef	ni_prefs,
						     CFDictionaryRef	options)
{
	CFArrayRef		allServices				= NULL;
	CFArrayRef		allSets					= NULL;
	CFDictionaryRef		bsdNameToBridgeServices			= NULL;
	CFDictionaryRef		bsdNameToBondServices			= NULL;
	CFDictionaryRef		bsdNameToVLANServices			= NULL;
	validityContext		context;
	CFArrayRef		interfaces				= NULL;
	CFMutableArrayRef	interfacesToRemove			= NULL;
	CFMutableArrayRef	interfacesToReplace			= NULL;
	CFMutableArrayRef	interfacesToPreserveServiceInformation	= NULL;
	CFMutableDictionaryRef	bsdNameServiceProtocolPreserveMapping	= NULL;
	Boolean			isValid					= TRUE;
	CFDictionaryRef		mappingBSDNameToInterface		= NULL;
	CFDictionaryRef		mappingServiceBSDNameToInterface	= NULL;
	CFStringRef		model					= NULL;
	CFStringRef		ni_model				= NULL;
	Boolean			repairConfiguration			= FALSE;
	Boolean			repairModel				= FALSE;
	Boolean			revertBypassSystemInterfaces		= FALSE;
	CFArrayRef		setServiceOrder				= NULL;
	CFArrayRef		setServices				= NULL;

	if  ((isA_CFDictionary(options) != NULL)) {
		CFBooleanRef val;

		val = CFDictionaryGetValue(options, kSCNetworkConfigurationRepair);
		if (isA_CFBoolean(val) != NULL) {
			repairConfiguration = CFBooleanGetValue(val);
		}

		val = CFDictionaryGetValue(options, kSCNetworkConfigurationRepairModel);
		if (isA_CFBoolean(val) != NULL) {
			repairModel = CFBooleanGetValue(val);
		}
	}

	SC_log(LOG_INFO,
	       "Configuration validity check%s%s%s"
	       "\n  prefs    = %@"
	       "\n  ni_prefs = %@",
	       repairModel ? ", w/repair-model" : "",
	       repairConfiguration ? ", w/repair-configuration" : "",
	       _SCNetworkConfigurationBypassSystemInterfaces(prefs) ? ", bypass system interfaces" : "",
	       prefs,
	       ni_prefs);

	if (!_SCNetworkConfigurationBypassSystemInterfaces(prefs)) {
		_SCNetworkConfigurationSetBypassSystemInterfaces(prefs, TRUE);
		revertBypassSystemInterfaces = TRUE;
	}

	/*
	 Check the validity by:
	 - Comparing if the models are the same
	 */
	model    = SCPreferencesGetValue(prefs   , MODEL);
	ni_model = SCPreferencesGetValue(ni_prefs, MODEL);
	if (!isA_CFString(model)) {
		if (repairModel && isA_CFString(ni_model)) {
			SC_log(LOG_INFO, "  updating \"Model\" property in preferences.plist");
			SCPreferencesSetValue(prefs, MODEL, ni_model);	// have prefs model match ni_prefs
			model = ni_model;
		} else {
			SC_log(LOG_INFO, "  no \"Model\" property in preferences.plist");
		}
	}
	if (!isA_CFString(ni_model)) {
		if (repairModel && isA_CFString(model)) {
			SC_log(LOG_INFO, "  updating \"Model\" property in NetworkInterfaces.plist");
			SCPreferencesSetValue(ni_prefs, MODEL, model);	// have ni_prefs model match prefs
			ni_model = model;
		} else {
			SC_log(LOG_INFO, "  no \"Model\" property in NetworkInterfaces.plist");
		}
	}
	if (isA_CFString(model) && isA_CFString(ni_model) && !CFEqual(model, ni_model)) {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: model names do not match!"
		       "\n  %@"
		       "\n  %@",
		       prefs,
		       ni_prefs);
		goto done;
	}

	/*
	 * Comparing if the interfaces names mentioned in NetworkInterfaces.plist and preferences.plist match
	 */
	interfaces = __SCNetworkInterfaceCopyStoredWithPreferences(ni_prefs);
	if (interfaces == NULL) {
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no network interfaces!"
		       "\n  %@",
		       ni_prefs);
		isValid = FALSE;
		goto done;
	}
	mappingBSDNameToInterface = __SCNetworkInterfaceCreateMappingUsingBSDName(interfaces);
	CFRelease(interfaces);
	if (!isA_CFDictionary(mappingBSDNameToInterface)) {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no BSD name to network interface mapping!"
		       "\n  %@",
		       ni_prefs);
		goto done;
	}

	interfaces = __SCNetworkServiceCopyAllInterfaces(prefs);
	if (interfaces == NULL) {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no service interfaces!"
		       "\n  %@",
		       prefs);
		goto done;
	} else {
		logInterfaces(LOG_DEBUG, "interfaces", interfaces, TRUE);
	}

	mappingServiceBSDNameToInterface = __SCNetworkInterfaceCreateMappingUsingBSDName(interfaces);
	CFRelease(interfaces);
	if (mappingServiceBSDNameToInterface != NULL) {
		logMapping(LOG_DEBUG, mappingServiceBSDNameToInterface, "mappingServiceBSDNameToInterface");
	} else {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no BSD name to service interface mapping!"
		       "\n  %@",
		       prefs);
		goto done;
	}

	if (repairConfiguration) {
		interfacesToRemove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		interfacesToReplace = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		interfacesToPreserveServiceInformation = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		bsdNameServiceProtocolPreserveMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#if	!TARGET_OS_IPHONE
		bsdNameToBridgeServices = _SCNetworkMigrationCopyMappingBSDNameToBridgeServices(prefs);
		bsdNameToBondServices = _SCNetworkMigrationCopyMappingBSDNameToBondServices(prefs);
		bsdNameToVLANServices = _SCNetworkMigrationCopyMappingBSDNameToVLANServices(prefs);
#endif	// !TARGET_OS_IPHONE
	}

	context.interfaceMapping = mappingBSDNameToInterface;
	context.isValid = &isValid;
	context.interfacesToRemove = interfacesToRemove;
	context.interfacesToReplace = interfacesToReplace;
	context.interfacesToPreserveServiceInformation = interfacesToPreserveServiceInformation;
	context.bsdNameToBridgeServices = bsdNameToBridgeServices;
	context.bsdNameToBondServices = bsdNameToBondServices;
	context.bsdNameToVLANServices = bsdNameToVLANServices;
	context.repair = repairConfiguration;
	context.prefs = prefs;
	context.bsdNameServiceProtocolPreserveMapping = bsdNameServiceProtocolPreserveMapping;
	CFDictionaryApplyFunction(mappingServiceBSDNameToInterface, _SCNetworkConfigurationValidateInterface, &context);

	if (!isValid) {
		SC_log(LOG_NOTICE,
		       "Configuration validity check: mismatched interface names between NetworkInterfaces.plist and preferences.plist!"
		       "\n  %@"
		       "\n  %@",
		       prefs,
		       ni_prefs);
		if (repairConfiguration) {
			isValid = _SCNetworkConfigurationRepairUsingPreferences(prefs, ni_prefs, &context);
			if (!isValid) {
				goto done;
			}
			// Save the changes if repair fixed an invalid configuration
			if (!SCPreferencesCommitChanges(prefs)) {
				SC_log(LOG_NOTICE, "SCPreferencesCommitChanges() failed");
			}
		} else {
			goto done;
		}
	}

	/*
	 - Check if all the network services mentioned in the SCNetworkSet are actually present in the SCNetworkService array
	 */
	allServices = SCNetworkServiceCopyAll(prefs);
	if (!isA_CFArray(allServices)) {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no services!"
		       "\n  %@",
		       prefs);
		goto done;
	}

	allSets = SCNetworkSetCopyAll(prefs);
	if (!isA_CFArray(allSets)) {
		isValid = FALSE;
		SC_log(LOG_NOTICE,
		       "Configuration validity check: no sets!"
		       "\n  %@",
		       prefs);
		goto done;
	}

	for (CFIndex idx = 0; ((idx < CFArrayGetCount(allSets)) && isValid); idx++) {
		SCNetworkSetRef set = CFArrayGetValueAtIndex(allSets, idx);

		setServices = SCNetworkSetCopyServices(set);
		if (setServices == NULL) {
			SC_log(LOG_NOTICE,
			       "Configuration validity check: set w/no services!"
			       "\n  %@"
			       "\n  %@",
			       prefs,
			       set);
			continue;
		}
		for (CFIndex idx2 = 0; idx2 < CFArrayGetCount(setServices); idx2++) {
			SCNetworkServiceRef service = CFArrayGetValueAtIndex(setServices, idx2);

			if (!CFArrayContainsValue(allServices, CFRangeMake(0, CFArrayGetCount(allServices)), service)) {
				isValid = FALSE;
				SC_log(LOG_NOTICE,
				       "All network services in the network set are not present in SCNetworkService array");
				break;
			}
		}
		if (!isValid) {
			break;
		}

		/*
		 - Check if service IDs in service order do exist in the SET
		 */
		setServiceOrder = SCNetworkSetGetServiceOrder(set);
		if (setServiceOrder != NULL) {
			for (CFIndex idx2 = 0; idx2 < CFArrayGetCount(setServiceOrder); idx2++) {
				SCNetworkServiceRef service = CFArrayGetValueAtIndex(setServiceOrder, idx2);
				if (!CFArrayContainsValue(setServiceOrder, CFRangeMake(0, CFArrayGetCount(setServiceOrder)), service) &&
				    !CFArrayContainsValue(allServices, CFRangeMake(0, CFArrayGetCount(allServices)), service)) {
					SC_log(LOG_NOTICE,
					       "Service: %@ is not present in the service order for set %@",
					       service,
					       set);
					break;
				}
			}
		}
		if (setServices != NULL) {
			CFRelease(setServices);
			setServices = NULL;
		}
	}

	/*
	 - Check if the virtual network interfaces have valid member interfaces
	 */
	CFArrayRef bridges = SCBridgeInterfaceCopyAll(prefs);
	if (bridges != NULL) {
		CFArrayApplyFunction(bridges, CFRangeMake(0, CFArrayGetCount(bridges)), validate_bridge, (void*)ni_prefs);
		CFRelease(bridges);
	}
#if	!TARGET_OS_IPHONE
	CFArrayRef bonds = SCBondInterfaceCopyAll(prefs);
	if (bonds != NULL) {
		CFArrayApplyFunction(bonds, CFRangeMake(0, CFArrayGetCount(bonds)), validate_bond, (void*)ni_prefs);
		CFRelease(bonds);
	}
#endif	// !TARGET_OS_IPHONE
	CFArrayRef vlans = SCVLANInterfaceCopyAll(prefs);
	if (vlans != NULL) {
		validate_prefs_context	*validate_prefs;

		validate_prefs = validate_prefs_context_create(prefs, ni_prefs);
		CFArrayApplyFunction(vlans, CFRangeMake(0, CFArrayGetCount(vlans)), validate_vlan, (void*)validate_prefs);
		validate_prefs_context_release(validate_prefs);
		CFRelease(vlans);
	}


done:
	if (mappingBSDNameToInterface != NULL) {
		CFRelease(mappingBSDNameToInterface);
	}
	if (mappingServiceBSDNameToInterface != NULL) {
		CFRelease(mappingServiceBSDNameToInterface);
	}
	if (allServices != NULL) {
		CFRelease(allServices);
	}
	if (allSets != NULL) {
		CFRelease(allSets);
	}
#if	!TARGET_OS_IPHONE
	if (bsdNameToBridgeServices != NULL) {
		CFRelease(bsdNameToBridgeServices);
	}
	if (bsdNameToBondServices != NULL) {
		CFRelease(bsdNameToBondServices);
	}
	if (bsdNameToVLANServices != NULL) {
		CFRelease(bsdNameToVLANServices);
	}
#endif	// !TARGET_OS_IPHONE
	if (setServices != NULL) {
		CFRelease(setServices);
	}
	if (interfacesToRemove != NULL) {
		CFRelease(interfacesToRemove);
	}
	if (interfacesToReplace != NULL) {
		CFRelease(interfacesToReplace);
	}
	if (interfacesToPreserveServiceInformation != NULL) {
		CFRelease(interfacesToPreserveServiceInformation);
	}
	if (bsdNameServiceProtocolPreserveMapping != NULL) {
		CFRelease(bsdNameServiceProtocolPreserveMapping);
	}
	if (revertBypassSystemInterfaces) {
		_SCNetworkConfigurationSetBypassSystemInterfaces(prefs, FALSE);
	}

	SC_log(LOG_INFO, "  configuration is %svalid", isValid ? "" : "not ");
	return isValid;
}

Boolean
_SCNetworkConfigurationCheckValidity(CFURLRef configDir, CFDictionaryRef options)
{
	CFURLRef		baseURL					= NULL;
	CFURLRef		configNetworkInterfaceFile		= NULL;
	CFStringRef		configNetworkInterfaceFileString	= NULL;
	SCPreferencesRef	configNetworkInterfacePref		= NULL;
	SCPreferencesRef	configPref				= NULL;
	CFURLRef		configPreferenceFile			= NULL;
	CFStringRef		configPreferencesFileString		= NULL;
	Boolean			isValid					= FALSE;
	char			networkInterfaceStr[PATH_MAX];
	char			prefsStr[PATH_MAX];

	if (configDir == NULL) {
		SC_log(LOG_NOTICE, "Migration files not found in directory: %@",
		       (configDir == NULL) ? CFSTR("NULL") : CFURLGetString(configDir));
		goto done;
	}
	baseURL = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
							      PREFS_DEFAULT_DIR_RELATIVE,
							      kCFURLPOSIXPathStyle,
							      TRUE,
							      configDir);

	configPreferenceFile = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
										     (const UInt8*)PREFS_DEFAULT_CONFIG_PLIST,
										     sizeof(PREFS_DEFAULT_CONFIG_PLIST),
										     FALSE,
										     baseURL);
	configNetworkInterfaceFile = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
											   (const UInt8*)INTERFACES_DEFAULT_CONFIG_PLIST,
											   sizeof(INTERFACES_DEFAULT_CONFIG_PLIST),
											   FALSE,
											   baseURL);

	if (!CFURLGetFileSystemRepresentation(configPreferenceFile, TRUE, (UInt8*)prefsStr, sizeof(prefsStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configPreferenceFile);
		goto done;
	}
	if (!CFURLGetFileSystemRepresentation(configNetworkInterfaceFile, TRUE, (UInt8*)networkInterfaceStr, sizeof(networkInterfaceStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configNetworkInterfaceFile);
		goto done;
	}

	configPreferencesFileString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), prefsStr);
	configNetworkInterfaceFileString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), networkInterfaceStr);

	configPref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, configPreferencesFileString);
	configNetworkInterfacePref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, configNetworkInterfaceFileString);
	if ((configPref == NULL) || (configNetworkInterfacePref == NULL)) {
		goto done;
	}

	// This function compares preferences.plist and NetworkInterfaces.plist and verifies if the values are correct
	// Checking interface mismatch for validity
	isValid = _SCNetworkConfigurationCheckValidityWithPreferences(configPref, configNetworkInterfacePref, options);

done:
	if (baseURL != NULL) {
		CFRelease(baseURL);
	}
	if (configPreferencesFileString != NULL) {
		CFRelease(configPreferencesFileString);
	}
	if (configNetworkInterfaceFileString != NULL) {
		CFRelease(configNetworkInterfaceFileString);
	}
	if (configPref != NULL) {
		CFRelease(configPref);
	}
	if (configNetworkInterfacePref != NULL) {
		CFRelease(configNetworkInterfacePref);
	}
	if (configPreferenceFile != NULL) {
		CFRelease(configPreferenceFile);
	}
	if (configNetworkInterfaceFile != NULL) {
		CFRelease(configNetworkInterfaceFile);
	}
	return isValid;
}


typedef struct {
	CFMutableArrayRef	externalInterfaceList;
	CFMutableArrayRef	networkInterfaceList;
	Boolean			foundNewInterfaces;
} externalMappingContext;

static void
_SCNetworkConfigurationCollectInterfaceStorageEntity(const void *key, const void *value, void *context)
{
#pragma unused(key)
	externalMappingContext	*ctx			= context;
	CFDictionaryRef		interface_entity;
	CFIndex			matchIndex;
	SCNetworkInterfaceRef	targetInterface		= (SCNetworkInterfaceRef)value;

	matchIndex = CFArrayGetFirstIndexOfValue(ctx->externalInterfaceList,
						 CFRangeMake(0, CFArrayGetCount(ctx->externalInterfaceList)),
						 targetInterface);
	if (matchIndex != kCFNotFound) {
		SCNetworkInterfaceRef	matchInterface;
		CFStringRef		matchName;
		CFStringRef		targetName;

		matchInterface = CFArrayGetValueAtIndex(ctx->externalInterfaceList, matchIndex);
		matchName  = __SCNetworkInterfaceGetUserDefinedName(matchInterface);
		targetName = __SCNetworkInterfaceGetUserDefinedName(targetInterface);
		if (_SC_CFEqual(matchName, targetName)) {
			// the target interface already exists; do not add it to NetworkInterfaces.plist
			SC_log(LOG_DEBUG,
			       "Target interface already exists, not updating NetworkInterfaces.plist"
			       "\n   %@",
			       targetInterface);
			return;
		} else {
			// the target interface differs from the NetworkInterfaces.plist interface; replace it
			SC_log(LOG_DEBUG,
			       "Current interface morphed, replacing"
			       "\n   %@",
			       matchInterface);
			CFArrayRemoveValueAtIndex(ctx->externalInterfaceList, matchIndex);
		}
	}

	ctx->foundNewInterfaces = TRUE;
	interface_entity = __SCNetworkInterfaceCopyStorageEntity(targetInterface);
	if (interface_entity != NULL) {
		SC_log(LOG_DEBUG,
		       "adding network interface entity"
		       "\n%@",
		       interface_entity);
		CFArrayAppendValue(ctx->networkInterfaceList, interface_entity);
		CFRelease(interface_entity);
	}
}

static CFArrayRef   // CFDictionaryRef
_SCNetworkMigrationCreateNetworkInterfaceArray(SCPreferencesRef ni_prefs, CFDictionaryRef externalMapping, Boolean *hasNewInterface)
{
	externalMappingContext	context;
	CFIndex			count			= 0;
	CFMutableArrayRef	externalInterfaceList	= NULL;
	CFArrayRef		if_list			= NULL;
	CFDictionaryRef		interface_entity	= NULL;
	CFMutableArrayRef	networkInterfaceList	= NULL;

	if (ni_prefs == NULL) {
		SC_log(LOG_NOTICE, "No NetworkInterfaces.plist");
		return NULL;
	}

	if_list = SCPreferencesGetValue(ni_prefs, INTERFACES);
	if (!isA_CFArray(if_list) ||
	    ((count = CFArrayGetCount(if_list)) == 0)) {
		SC_log(LOG_NOTICE, "No interfaces");
		return NULL;
	}

	networkInterfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// Keep the same builtin and external interfaces
	for (CFIndex idx = 0; idx < count; idx++) {
		interface_entity = CFArrayGetValueAtIndex(if_list, idx);
		if (!isA_CFDictionary(interface_entity)) {
			continue;
		}
		CFArrayAppendValue(networkInterfaceList, interface_entity);
	}

	if (!isA_CFDictionary(externalMapping)) {
		// if no external mapping
		goto done;
	}

	// Add any new external interfaces found
	externalInterfaceList = _SCNetworkInterfaceCopyInterfacesFilteredByBuiltinWithPreferences(ni_prefs, FALSE);
	context.externalInterfaceList = externalInterfaceList;
	context.networkInterfaceList = networkInterfaceList;
	context.foundNewInterfaces = FALSE;

	SC_log(LOG_DEBUG, "Updating network interface list");
	logInterfaces(LOG_DEBUG, "  externalInterfaceList", externalInterfaceList, FALSE);
	logInterfaces(LOG_DEBUG, "  networkInterfaceList" , networkInterfaceList,  FALSE);

	CFDictionaryApplyFunction(externalMapping, _SCNetworkConfigurationCollectInterfaceStorageEntity, &context);

	if (hasNewInterface != NULL) {
		*hasNewInterface = context.foundNewInterfaces;
	}
done:
	if (externalInterfaceList != NULL) {
		CFRelease(externalInterfaceList);
	}
	return networkInterfaceList;
}

static void
SCNetworkMigrationMapSourceToTargetName(const void *key, const void *value, void *context)
{
	SCNetworkInterfaceRef	interfaceKey	= (SCNetworkInterfaceRef)key;
	SCNetworkInterfaceRef	interfaceValue	= (SCNetworkInterfaceRef)value;
	CFMutableDictionaryRef	mapping		= (CFMutableDictionaryRef)context;
	CFStringRef		sourceBSDName	= NULL;
	CFStringRef		targetBSDName	= NULL;

	sourceBSDName = SCNetworkInterfaceGetBSDName(interfaceKey);
	if (!isA_CFString(sourceBSDName)) {
		return;
	}

	targetBSDName = SCNetworkInterfaceGetBSDName(interfaceValue);
	if (!isA_CFString(targetBSDName)) {
		return;
	}

	if (!CFDictionaryContainsKey(mapping, sourceBSDName)) {
		CFDictionaryAddValue(mapping, sourceBSDName, targetBSDName);
	}
	return;
}

static CFDictionaryRef
_SCNetworkMigrationCreateBSDNameMapping(CFDictionaryRef internalMapping, CFDictionaryRef externalMapping)
{
	CFMutableDictionaryRef bsdNameMapping = CFDictionaryCreateMutable(NULL, 0,
									  &kCFTypeDictionaryKeyCallBacks,
									  &kCFTypeDictionaryValueCallBacks);

	if ((internalMapping == NULL) && (externalMapping == NULL)) {
		goto done;
	}

	if (internalMapping != NULL) {
		CFDictionaryApplyFunction(internalMapping, SCNetworkMigrationMapSourceToTargetName, bsdNameMapping);
	}

	if (externalMapping != NULL) {
		CFDictionaryApplyFunction(externalMapping, SCNetworkMigrationMapSourceToTargetName, bsdNameMapping);
	}

done:
	return bsdNameMapping;
}

static CFDictionaryRef
_SCNetworkMigrationCreateServiceSetMapping(SCPreferencesRef prefs)
{
	SCNetworkServiceRef	service;
	CFMutableDictionaryRef	serviceSetMapping;
	CFArrayRef		services	= NULL;
	CFMutableArrayRef	setList		= NULL;
	CFArrayRef		sets		= NULL;

	serviceSetMapping = CFDictionaryCreateMutable(NULL, 0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
	services = SCNetworkServiceCopyAll(prefs);
	if (services == NULL) {
		goto done;
	}
	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		service = CFArrayGetValueAtIndex(services, idx);
		setList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFDictionaryAddValue(serviceSetMapping, service, setList);
		CFRelease(setList);
	}
	CFRelease(services);

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		goto done;
	}

	for (CFIndex idx = 0; idx < CFArrayGetCount(sets); idx++) {
		SCNetworkSetRef	set;

		set = CFArrayGetValueAtIndex(sets, idx);
		services = SCNetworkSetCopyServices(set);
		for (CFIndex idx2 = 0; idx2 < CFArrayGetCount(services); idx2++) {
			service = CFArrayGetValueAtIndex(services, idx2);
			setList = (CFMutableArrayRef)CFDictionaryGetValue(serviceSetMapping, service);
			if (setList != NULL) {
				CFArrayAppendValue(setList, set);
			}
		}
		CFRelease(services);
	}

done:
	if (sets != NULL) {
		CFRelease(sets);
	}
	return serviceSetMapping;
}

static CFDictionaryRef
_SCNetworkMigrationCreateSetMapping(SCPreferencesRef sourcePrefs,
				    SCPreferencesRef targetPrefs)
{
	SCNetworkSetRef		currentSourceSet	= NULL;
	CFMutableDictionaryRef	setMapping		= NULL;
	CFStringRef		setName;
	CFArrayRef		sourceSets		= NULL;
	CFIndex			targetCount;
	SCNetworkSetRef		targetSet;
	CFArrayRef		targetSets		= NULL;
	CFMutableArrayRef	targetSetsMutable	= NULL;

	sourceSets = SCNetworkSetCopyAll(sourcePrefs);
	if (sourceSets == NULL) {
		goto done;
	}

	targetSets = SCNetworkSetCopyAll(targetPrefs);
	if (targetSets == NULL) {
		goto done;
	}
	targetSetsMutable = CFArrayCreateMutableCopy(NULL, 0, targetSets);
	targetCount = CFArrayGetCount(targetSetsMutable);

	setMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	currentSourceSet = SCNetworkSetCopyCurrent(sourcePrefs);

	// Mapping the current source set to the first target set, and setting it as current set
	if (currentSourceSet != NULL) {
		if (targetCount > 0) {
			targetSet = CFArrayGetValueAtIndex(targetSetsMutable, 0);
			CFRetain(targetSet);
			CFArrayRemoveValueAtIndex(targetSetsMutable, 0);

			setName = SCNetworkSetGetName(currentSourceSet);
			SCNetworkSetSetName(targetSet, setName);
			CFDictionaryAddValue(setMapping, currentSourceSet, targetSet);
			SCNetworkSetSetCurrent(targetSet);
			CFRelease(targetSet);
		}
		else {
			SC_log(LOG_NOTICE, "Number of sets in the target should be at least 1, but is found to be %ld", targetCount);
			goto done;
		}
	}

	for (CFIndex idx = 0; idx < CFArrayGetCount(sourceSets); idx++) {
		SCNetworkSetRef sourceSet = CFArrayGetValueAtIndex(sourceSets, idx);

		if ((currentSourceSet != NULL) && CFEqual(sourceSet, currentSourceSet)) {
			continue;
		}

		targetCount = CFArrayGetCount(targetSetsMutable);
		setName = SCNetworkSetGetName(sourceSet);

		if (targetCount > 0) {
			targetSet = CFArrayGetValueAtIndex(targetSetsMutable, 0);
			CFRetain(targetSet);
			CFArrayRemoveValueAtIndex(targetSetsMutable, 0);
		} else {
			targetSet = SCNetworkSetCreate(targetPrefs);
		}
		SCNetworkSetSetName(targetSet, setName);
		CFDictionaryAddValue(setMapping, sourceSet, targetSet);

		CFRelease(targetSet);
	}

    done:

	if (sourceSets != NULL) {
		CFRelease(sourceSets);
	}
	if (targetSets != NULL) {
		CFRelease(targetSets);
	}
	if (targetSetsMutable != NULL) {
		CFRelease(targetSetsMutable);
	}
	if (currentSourceSet != NULL) {
		CFRelease(currentSourceSet);
	}

	return setMapping;
}

// This function finds the mapping between source and target preferences (SCNetworkServicesRef -> SCNetworkServicesRef)
// If there is no mapping found between source and target preferences, then the CFBooleanRef value indicating no value is found is stored (SCNetworkServicesRef -> kCFBooleanFalse)
static CFDictionaryRef
_SCNetworkMigrationCreateServiceMappingUsingBSDNameMapping(SCPreferencesRef	sourcePrefs,
							   SCPreferencesRef	targetPrefs,
							   CFDictionaryRef	bsdNameMapping)
{
	CFMutableDictionaryRef	serviceMapping			= NULL;	// Mapping of services between source and target configurations
	CFStringRef		sourceBSDName			= NULL;
	CFIndex			sourceCount			= 0;
	SCNetworkInterfaceRef	sourceInterface			= NULL;
	CFStringRef		sourceInterfaceSubType		= NULL;	// Check interface type and subtype to be able to transfer VPN
	CFStringRef		sourceInterfaceType		= NULL;
	CFArrayRef		sourceSCNetworkServices		= NULL;
	CFMutableArrayRef	sourceSCNetworkServicesMutable	= NULL;	// Source SCNetworkServiceRef mutable array
	SCNetworkServiceRef	sourceService			= NULL;
	CFStringRef		targetBSDName			= NULL;
	CFStringRef		targetBSDNameMapped		= NULL;
	CFIndex			targetCount			= 0;	// Count of Source and Target Services
	SCNetworkInterfaceRef	targetInterface			= NULL;
	CFStringRef		targetInterfaceSubType		= NULL;	// services during migration
	CFStringRef		targetInterfaceType		= NULL;
	CFArrayRef		targetSCNetworkServices		= NULL;
	CFMutableArrayRef	targetSCNetworkServicesMutable	= NULL;	// Target SCNetworkServiceRef mutable array
	SCNetworkServiceRef	targetService			= NULL;

	// We need BSD Mapping to successfully create service mapping
	if (bsdNameMapping == NULL) {
		SC_log(LOG_NOTICE, "No BSD name mappings");
		goto done;
	}
	sourceSCNetworkServices = SCNetworkServiceCopyAll(sourcePrefs);
	if (!isA_CFArray(sourceSCNetworkServices)) {
		SC_log(LOG_NOTICE, "No source network services");
		goto done;
	}
	targetSCNetworkServices = SCNetworkServiceCopyAll(targetPrefs);
	if (!isA_CFArray(targetSCNetworkServices)) {
		SC_log(LOG_NOTICE, "No target network services");
		goto done;
	}

	sourceCount = CFArrayGetCount(sourceSCNetworkServices);

	sourceSCNetworkServicesMutable = CFArrayCreateMutableCopy(NULL, 0, sourceSCNetworkServices);
	targetSCNetworkServicesMutable = CFArrayCreateMutableCopy(NULL, 0, targetSCNetworkServices);

	serviceMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (CFIndex idx = 0;  idx < sourceCount; idx++) {
		sourceBSDName = NULL;
		sourceService = NULL;
		sourceInterface = NULL;
		sourceInterfaceType = NULL;
		sourceInterfaceSubType = NULL;
		targetBSDNameMapped = NULL;

		targetCount = CFArrayGetCount(targetSCNetworkServicesMutable);
		sourceService = (SCNetworkServiceRef) CFArrayGetValueAtIndex(sourceSCNetworkServicesMutable, idx);

		sourceInterface = SCNetworkServiceGetInterface(sourceService);
		if (sourceInterface == NULL) {
			SC_log(LOG_NOTICE, "source service with no interface, should never happen");
			continue;
		}

		sourceInterfaceType = __SCNetworkInterfaceGetEntityType(sourceInterface);
		if (isA_CFString(sourceInterfaceType) &&
		    (CFEqual(sourceInterfaceType, kSCValNetInterfaceTypeVPN) ||
		     CFEqual(sourceInterfaceType, kSCValNetInterfaceTypePPP))) {
			sourceInterfaceSubType = __SCNetworkInterfaceGetEntitySubType(sourceInterface);
			if (!isA_CFString(sourceInterfaceSubType)) {
				SC_log(LOG_NOTICE, "source service with VPN/PPP interface missing \"SubType\"");
				continue;
			}
		} else if (isA_SCBondInterface(sourceInterface) ||
			   isA_SCBridgeInterface(sourceInterface) ||
			   isA_SCVLANInterface(sourceInterface)) {
			sourceBSDName = SCNetworkInterfaceGetBSDName(sourceInterface);
			if (!isA_CFString(sourceBSDName)) {
				// virtual interface w/o BSD interface
				continue;
			}
			targetBSDNameMapped = sourceBSDName;
		} else if (isA_CFString(sourceInterfaceType) &&
			   (CFEqual(sourceInterfaceType, kSCValNetInterfaceTypeIPSec) ||
			    CFEqual(sourceInterfaceType, kSCValNetInterfaceType6to4) ||
			    CFEqual(sourceInterfaceType, kSCValNetInterfaceTypeLoopback))) {
			// no migration
			continue;
		} else {
			sourceBSDName = SCNetworkInterfaceGetBSDName(sourceInterface);
			if (!isA_CFString(sourceBSDName)) {
				// source interface w/o BSD interface
				continue;
			}

			if (!CFDictionaryGetValueIfPresent(bsdNameMapping,
							   sourceBSDName,
							   (const void **)&targetBSDNameMapped) ||
			    !isA_CFString(targetBSDNameMapped)) {
				SC_log(LOG_INFO, "No BSD name mapping for %@", sourceBSDName);
				continue;
			}
		}

		// Find the bsd name in target service
		for (CFIndex idx2 = 0; idx2 < targetCount; idx2++) {
			targetService = NULL;
			targetInterface = NULL;
			targetBSDName = NULL;
			targetInterfaceType = NULL;
			targetInterfaceSubType = NULL;

			targetService = (SCNetworkServiceRef) CFArrayGetValueAtIndex(targetSCNetworkServicesMutable, idx2);

			targetInterface = SCNetworkServiceGetInterface(targetService);
			if (targetInterface == NULL) {
				SC_log(LOG_NOTICE, "target service with no interface, should never happen");
				continue;
			}
			if (sourceBSDName != NULL) {
				targetBSDName = SCNetworkInterfaceGetBSDName(targetInterface);
				if (!isA_CFString(targetBSDName)) {
					// source interface w/BSD interface, target w/o BSD interface
					continue;
				}

				if (CFEqual(targetBSDName, targetBSDNameMapped)) {
					SC_log(LOG_NOTICE, "Removing target service: %@", targetBSDName);
					CFDictionaryAddValue(serviceMapping, sourceService, targetService);
					CFArrayRemoveValueAtIndex(targetSCNetworkServicesMutable, idx2);
					break;
				}
			} else {
				// Source interface type should be VPN
				targetInterfaceType = __SCNetworkInterfaceGetEntityType(targetInterface);
				if ((!isA_CFString(targetInterfaceType)) ||
				    (!CFEqual(targetInterfaceType, kSCValNetInterfaceTypeVPN) &&
				     !CFEqual(targetInterfaceType, kSCValNetInterfaceTypePPP))) {
					SC_log(LOG_NOTICE, "Unexpected target interface \"Type\": %@", targetInterface);
					continue;
				}

				targetInterfaceSubType = __SCNetworkInterfaceGetEntitySubType(targetInterface);
				if (!isA_CFString(targetInterfaceSubType)) {
					SC_log(LOG_NOTICE, "No target interface \"SubType\": %@", targetInterface);
					continue;
				}

				// Check if the target interface type and the target interface sub type match
				if (CFEqual(targetInterfaceType, sourceInterfaceType) &&
				    CFEqual(targetInterfaceSubType, sourceInterfaceSubType)) {
					SC_log(LOG_NOTICE, "Removing target service: %@ for VPN", targetBSDName);
					CFDictionaryAddValue(serviceMapping, sourceService, targetService);
					CFArrayRemoveValueAtIndex(targetSCNetworkServicesMutable, idx2);
					break;
				}
			}
		}
		// Check if sourceService has found a mapping or not, if not the create a NULL mapping to indicate
		// the this service needs to be added and not replaced
		if (!CFDictionaryContainsKey(serviceMapping, sourceService)) {
			CFDictionaryAddValue(serviceMapping, sourceService, kCFBooleanFalse);
		}
	}

    done :

	if (sourceSCNetworkServices != NULL) {
		CFRelease(sourceSCNetworkServices);
	}
	if (targetSCNetworkServices != NULL) {
		CFRelease(targetSCNetworkServices);
	}
	if (sourceSCNetworkServicesMutable != NULL) {
		CFRelease(sourceSCNetworkServicesMutable);
	}
	if (targetSCNetworkServicesMutable != NULL) {
		CFRelease(targetSCNetworkServicesMutable);
	}

	return serviceMapping;
}

typedef struct {
	SCPreferencesRef	targetPrefs;
	CFDictionaryRef		bsdNameMapping;
	CFDictionaryRef		setMapping;
	CFDictionaryRef		serviceSetMapping;
} serviceMigrationContext;

// value can be:
//	SCNetworkServiceRef: if target service needs replacement
//	CFBooleanRef: if target service is not present
static void
ServiceMigrationAddOrReplace(const void *key, const void *value, void *context)
{
	CFDictionaryRef		bsdNameMapping		= NULL;
	serviceMigrationContext	*ctx			= (serviceMigrationContext*)context;
	CFDictionaryRef		setMapping;
	CFDictionaryRef		sourceServiceSetMapping;
	SCNetworkServiceRef	sourceService		= (SCNetworkServiceRef)key;
	SCPreferencesRef	targetPrefs		= NULL;
	SCNetworkServiceRef	targetService		= (SCNetworkServiceRef)value;

	targetPrefs = ctx->targetPrefs;
	bsdNameMapping = ctx->bsdNameMapping;
	setMapping = ctx->setMapping;
	sourceServiceSetMapping = ctx->serviceSetMapping;

	if ((setMapping != NULL) || (sourceServiceSetMapping != NULL)) {
		if (isA_SCNetworkService(targetService)) {
			(void)SCNetworkServiceGetName(targetService);	// ensures that the service name will be logged
			SC_log(LOG_INFO, "Removing [target] service: %@", targetService);
			SCNetworkServiceRemove(targetService);
		}
	}

	(void)SCNetworkServiceGetName(sourceService);	// ensures that the service name will be logged
	SC_log(LOG_INFO, "Adding [source] service: %@", sourceService);

	if (!__SCNetworkServiceMigrateNew(targetPrefs, sourceService, bsdNameMapping, setMapping, sourceServiceSetMapping)) {
		(void)SCNetworkServiceGetName(sourceService);	// ensures that the service name will be logged
		SC_log(LOG_INFO, "*** adding [source] service failed: %@", sourceService);
	}
}

static Boolean
_SCNetworkMigrationDoServiceMigration(SCPreferencesRef	sourcePrefs,
				      SCPreferencesRef	targetPrefs,
				      CFDictionaryRef	serviceMapping,
				      CFDictionaryRef	bsdNameMapping,
				      CFDictionaryRef	setMapping,
				      CFDictionaryRef	serviceSetMapping)
{
	serviceMigrationContext	context;
	Boolean			success	= FALSE;

	if ((sourcePrefs == NULL) ||
	    (targetPrefs == NULL) ||
	    !isA_CFDictionary(serviceMapping) ||
	    !isA_CFDictionary(bsdNameMapping)) {
		SC_log(LOG_INFO, "No sourcePrefs, targetPrefs, serviceMapping, or bsdNameMapping");
		goto done;
	}

	if (bsdNameMapping != NULL) {
		logMapping(LOG_INFO, bsdNameMapping, "BSD name mapping");
	} else {
		SC_log(LOG_NOTICE, "No BSD name mapping");
	}

	if (serviceMapping != NULL) {
		logMapping(LOG_INFO, serviceMapping, "SCNetworkService mapping");
	} else {
		SC_log(LOG_NOTICE, "No SCNetworkService mapping");
	}

	if (setMapping != NULL) {
		logMapping(LOG_INFO, setMapping, "SCNetworkSet mapping");
	} else {
		SC_log(LOG_NOTICE, "No SCNetworkSet mapping");
	}

	if (serviceSetMapping != NULL) {
		logMapping(LOG_INFO, serviceSetMapping, "SCNetworkService/SCNetworkSet mapping");
	} else {
		SC_log(LOG_NOTICE, "No SCNetworkService/SCNetworkSet mapping");
	}

	context.targetPrefs = targetPrefs;
	context.bsdNameMapping = bsdNameMapping;
	context.setMapping = setMapping;
	context.serviceSetMapping = serviceSetMapping;
	CFDictionaryApplyFunction(serviceMapping, ServiceMigrationAddOrReplace, &context);

	success = TRUE;
done:
	return success;
}

static Boolean
_SCNetworkMigrationDoSystemMigration(SCPreferencesRef sourcePrefs, SCPreferencesRef targetPrefs)
{
	CFStringRef		computerName;
	CFStringRef		hostname;
	CFStringRef		localHostname;
	CFStringEncoding	nameEncoding;

	SC_log(LOG_NOTICE, "Copying \"system\" settings");

	if ((sourcePrefs == NULL) ||
	    (targetPrefs == NULL)) {
		return FALSE;
	}

	hostname = SCPreferencesGetHostName(sourcePrefs);
	if (hostname != NULL) {
		SCPreferencesSetHostName(targetPrefs, hostname);
		SC_log(LOG_NOTICE, "  copied HostName");
	}

	localHostname = _SCPreferencesCopyLocalHostName(sourcePrefs);
	if (localHostname != NULL) {
		SCPreferencesSetLocalHostName(targetPrefs, localHostname);
		CFRelease(localHostname);
		SC_log(LOG_NOTICE, "  copied LocalHostName");
	}

	computerName = _SCPreferencesCopyComputerName(sourcePrefs, &nameEncoding);
	if (computerName != NULL) {
		SCPreferencesSetComputerName(targetPrefs, computerName, nameEncoding);
		CFRelease(computerName);
		SC_log(LOG_NOTICE, "  copied ComputerName");
	}

	return TRUE;
}

#if	!TARGET_OS_IPHONE

typedef struct {
	CFMutableArrayRef	interfaceList;
	SCPreferencesRef	ni_prefs;
	CFDictionaryRef		bsdNameMapping;
} virtualInterfaceMemberListContext;

typedef struct {
	SCPreferencesRef	prefs;
	SCPreferencesRef	ni_prefs;
	CFDictionaryRef		bsdNameMapping;
	CFDictionaryRef		virtualBSDNameMapping;
	CFDictionaryRef		mappingBSDNameToService;
	CFDictionaryRef		setMapping;
	CFDictionaryRef		serviceSetMapping;
} virtualInterfaceContext;

static void
add_virtual_interface(const void *value, void *context)
{
	virtualInterfaceMemberListContext	*ctx			= (virtualInterfaceMemberListContext*)context;
	CFDictionaryRef				bsdNameMapping		= ctx->bsdNameMapping;
	CFMutableArrayRef			interfaceList		= ctx->interfaceList;
	CFStringRef				oldInterfaceBSDName	= (CFStringRef)value;
	SCNetworkInterfaceRef			newInterface;
	CFStringRef				newInterfaceBSDName;

	SC_log(LOG_INFO, "old BSD interface name: %@", oldInterfaceBSDName);

	newInterfaceBSDName = CFDictionaryGetValue(bsdNameMapping, oldInterfaceBSDName);
	if (newInterfaceBSDName == NULL) {
		return;
	}
	SC_log(LOG_INFO, "new BSD interface name: %@", newInterfaceBSDName);

	newInterface = __SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(NULL, ctx->ni_prefs, newInterfaceBSDName);
	if (newInterface != NULL) {
		SC_log(LOG_INFO, "adding interface to interfaceList: %@", newInterface);
		CFArrayAppendValue(interfaceList, newInterface);
		CFRelease(newInterface);
	}
	return;
}

static void
add_target_bridge(const void *key, const void *value, void *context)
{
	CFStringRef				bridgeName;
	CFDictionaryRef				bridgeOptions;
	virtualInterfaceContext			*ctx			= (virtualInterfaceContext*)context;
	CFDictionaryRef				bridgeBSDNameMapping	= ctx->virtualBSDNameMapping;
	CFDictionaryRef				bsdNameToServiceMapping	= ctx->mappingBSDNameToService;
	virtualInterfaceMemberListContext	memberListContext;
	CFMutableArrayRef			newInterfaceList;
	SCBridgeInterfaceRef			newBridge;
	SCBridgeInterfaceRef			oldBridge		= (SCBridgeInterfaceRef)key;
	CFStringRef				oldBSDName;
	CFArrayRef				oldInterfaceList	= (CFArrayRef)value;
	CFArrayRef				oldServiceList;
	SCPreferencesRef			prefs			= ctx->prefs;
	CFDictionaryRef				serviceSetMapping	= ctx->serviceSetMapping;
	CFDictionaryRef				setMapping		= ctx->setMapping;

	newInterfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	memberListContext.bsdNameMapping = ctx->bsdNameMapping;
	memberListContext.interfaceList = newInterfaceList;
	memberListContext.ni_prefs = ctx->ni_prefs;

	CFArrayApplyFunction(oldInterfaceList, CFRangeMake(0, CFArrayGetCount(oldInterfaceList)), add_virtual_interface, &memberListContext);

	newBridge = SCBridgeInterfaceCreate(prefs);

	if (!__SCBridgeInterfaceSetMemberInterfaces(newBridge, newInterfaceList)) {
		SC_log(LOG_NOTICE, "__SCBridgeInterfaceSetMemberInterfaces() failed");
	}
	CFRelease(newInterfaceList);

	bridgeOptions = SCBridgeInterfaceGetOptions(oldBridge);
	if (bridgeOptions != NULL) {
		SCBridgeInterfaceSetOptions(newBridge, bridgeOptions);
	}

	bridgeName = SCNetworkInterfaceGetLocalizedDisplayName(oldBridge);

	if (bridgeName != NULL) {
		SCBridgeInterfaceSetLocalizedDisplayName(newBridge, bridgeName);
	}

	oldBSDName = SCNetworkInterfaceGetBSDName(oldBridge);
	if (oldBSDName == NULL) {
		goto done;
	}

	oldServiceList = CFDictionaryGetValue(bsdNameToServiceMapping, oldBSDName);
	if (oldServiceList == NULL) {
		goto done;
	}

	for (CFIndex idx = 0; idx < CFArrayGetCount(oldServiceList); idx++) {
		SCNetworkServiceRef oldService = CFArrayGetValueAtIndex(oldServiceList, idx);
		if (!__SCNetworkServiceMigrateNew(prefs, oldService, bridgeBSDNameMapping, setMapping, serviceSetMapping)) {
			SC_log(LOG_NOTICE, "could not migrate bridge service: %@", oldService);
		}
	}
done:
	CFRelease(newBridge);
}

static void
_SCNetworkMigrationRemoveBridgeServices(SCPreferencesRef prefs)
{
	CFArrayRef	services	= SCNetworkServiceCopyAll(prefs);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		    (SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeBridge)) {
			SC_log(LOG_INFO, "removing service: %@", service);
			SCNetworkServiceRemove(service);
		}
	}
	CFRelease(services);
}


static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToBridgeServices(SCPreferencesRef prefs)
{
	CFArrayRef		services	= SCNetworkServiceCopyAll(prefs);
	CFMutableDictionaryRef	bridgeServices	= CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeBridge) {
			CFMutableArrayRef serviceList;
			if (!CFDictionaryContainsKey(bridgeServices, bsdName)) {
				serviceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				CFDictionaryAddValue(bridgeServices, bsdName, serviceList);
				CFRelease(serviceList);
			}
			serviceList = (CFMutableArrayRef)CFDictionaryGetValue(bridgeServices, bsdName);
			CFArrayAppendValue(serviceList, service);
		}
	}
	CFRelease(services);
	return bridgeServices;
}


static Boolean
_SCNetworkMigrationDoBridgeMigration(SCPreferencesRef	sourcePrefs,
				     SCPreferencesRef	sourceNIPrefs,
				     SCPreferencesRef	targetPrefs,
				     SCPreferencesRef	targetNIPrefs,
				     CFDictionaryRef	bsdNameMapping,
				     CFDictionaryRef	setMapping,
				     CFDictionaryRef	serviceSetMapping)
{
#pragma unused(sourceNIPrefs)
	CFArrayRef		allSourceBridges;
	CFArrayRef		allTargetBridges;
	SCBridgeInterfaceRef	bridge;
	CFMutableDictionaryRef	bridgeInterfaceMapping	= NULL;
	CFMutableDictionaryRef	bridgeMapping;
	CFDictionaryRef		bsdNameToBridgeServices;
	virtualInterfaceContext	context;
	CFIndex			count			= 0;
	Boolean			success			= FALSE;

	allSourceBridges = SCBridgeInterfaceCopyAll(sourcePrefs);
	allTargetBridges = SCBridgeInterfaceCopyAll(targetPrefs);

	bsdNameToBridgeServices = _SCNetworkMigrationCopyMappingBSDNameToBridgeServices(sourcePrefs);

	bridgeInterfaceMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	bridgeMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// Create Bridge Interface Mapping
	for (CFIndex idx = 0; idx < CFArrayGetCount(allSourceBridges); idx++) {
		bridge = CFArrayGetValueAtIndex(allSourceBridges, idx);
		CFArrayRef bridgeMembers = SCBridgeInterfaceGetMemberInterfaces(bridge);
		CFMutableArrayRef interfaceList;

		interfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (CFIndex idx2 = 0; idx2 < CFArrayGetCount(bridgeMembers); idx2++) {
			CFStringRef interfaceName = NULL;
			SCNetworkInterfaceRef interface = NULL;

			interface = CFArrayGetValueAtIndex(bridgeMembers, idx2);
			interfaceName = SCNetworkInterfaceGetBSDName(interface);

			if (CFDictionaryContainsKey(bsdNameMapping, interfaceName)) {
				CFStringRef bridgeNewName = CFStringCreateWithFormat(NULL, NULL, CFSTR("bridge%ld"), count);
				CFDictionaryAddValue(bridgeMapping, interfaceName, bridgeNewName);
				CFArrayAppendValue(interfaceList, interfaceName);
				CFRelease(bridgeNewName);
				count++;
			}
		}
		if (CFArrayGetCount(interfaceList) > 0) {
			CFDictionaryAddValue(bridgeInterfaceMapping, bridge, interfaceList);
		}
		CFRelease(interfaceList);
	}
	// Remove bridge services from target
	_SCNetworkMigrationRemoveBridgeServices(targetPrefs);

	// Remove Target Bridges
	for (CFIndex idx = 0; idx < CFArrayGetCount(allTargetBridges); idx++) {
		bridge = CFArrayGetValueAtIndex(allTargetBridges, idx);
		if (!SCBridgeInterfaceRemove(bridge)) {
			SC_log(LOG_NOTICE, "SCBridgeInterfaceRemove() failed: %@", bridge);
			goto done;
		}
	}

	context.prefs = targetPrefs;
	context.ni_prefs = targetNIPrefs;
	context.bsdNameMapping = bsdNameMapping;
	context.virtualBSDNameMapping = bridgeMapping;
	context.mappingBSDNameToService = bsdNameToBridgeServices;
	context.setMapping = setMapping;
	context.serviceSetMapping = serviceSetMapping;

	// Add Bridge configurations at the target using mapping
	CFDictionaryApplyFunction(bridgeInterfaceMapping, add_target_bridge, &context);

	success = TRUE;
done:
	CFRelease(allSourceBridges);
	CFRelease(allTargetBridges);
	CFRelease(bridgeInterfaceMapping);
	CFRelease(bridgeMapping);
	CFRelease(bsdNameToBridgeServices);
	return success;
}


static void
add_target_bond(const void *key, const void *value, void *context)
{
	CFNumberRef				bondMode;
	CFStringRef				bondName;
	CFDictionaryRef				bondOptions;
	virtualInterfaceContext			*ctx			= (virtualInterfaceContext*)context;
	CFDictionaryRef				bondBSDNameMapping	= ctx->virtualBSDNameMapping;
	CFDictionaryRef				bsdNameToServiceMapping	= ctx->mappingBSDNameToService;
	virtualInterfaceMemberListContext	memberListContext;
	CFMutableArrayRef			newInterfaceList;
	SCBondInterfaceRef			newBond;
	SCBondInterfaceRef			oldBond			= (SCBondInterfaceRef)key;
	CFStringRef				oldBSDName;
	CFArrayRef				oldInterfaceList	= (CFArrayRef)value;
	CFArrayRef				oldServiceList;
	SCPreferencesRef			prefs			= ctx->prefs;
	CFDictionaryRef				serviceSetMapping	= ctx->serviceSetMapping;
	CFDictionaryRef				setMapping		= ctx->setMapping;

	newInterfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	memberListContext.bsdNameMapping = ctx->bsdNameMapping;
	memberListContext.interfaceList = newInterfaceList;
	memberListContext.ni_prefs = ctx->ni_prefs;

	CFArrayApplyFunction(oldInterfaceList, CFRangeMake(0, CFArrayGetCount(oldInterfaceList)), add_virtual_interface, &memberListContext);

	newBond = SCBondInterfaceCreate(prefs);
	if (!__SCBondInterfaceSetMemberInterfaces(newBond, newInterfaceList)) {
		SC_log(LOG_NOTICE, "__SCBondInterfaceSetMemberInterfaces() failed");
	}
	CFRelease(newInterfaceList);

	bondOptions = SCBondInterfaceGetOptions(oldBond);
	if (bondOptions != NULL) {
		SCBondInterfaceSetOptions(newBond, bondOptions);
	}

	bondName = SCNetworkInterfaceGetLocalizedDisplayName(oldBond);
	if (bondName != NULL) {
		SCBondInterfaceSetLocalizedDisplayName(newBond, bondName);
	}

	bondMode = SCBondInterfaceGetMode(oldBond);
	if (bondMode != NULL) {
		SCBondInterfaceSetMode(newBond, bondMode);
	}
	oldBSDName = SCNetworkInterfaceGetBSDName(oldBond);
	if (oldBSDName == NULL) {
		goto done;
	}

	oldServiceList = CFDictionaryGetValue(bsdNameToServiceMapping, oldBSDName);
	if (oldServiceList == NULL) {
		goto done;
	}

	for (CFIndex idx = 0; idx < CFArrayGetCount(oldServiceList); idx++) {
		SCNetworkServiceRef oldService = CFArrayGetValueAtIndex(oldServiceList, idx);
		if (!__SCNetworkServiceMigrateNew(prefs, oldService, bondBSDNameMapping, setMapping, serviceSetMapping)) {
			SC_log(LOG_NOTICE, "could not migrate bond service: %@", oldService);
		}
	}
done:
	CFRelease(newBond);
}

static void
_SCNetworkMigrationRemoveBondServices(SCPreferencesRef prefs)
{
	CFArrayRef services = SCNetworkServiceCopyAll(prefs);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeBond) {
			SCNetworkServiceRemove(service);
		}
	}
	CFRelease(services);
}


static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToBondServices(SCPreferencesRef prefs)
{
	CFArrayRef		services	= SCNetworkServiceCopyAll(prefs);
	CFMutableDictionaryRef	bondServices	= CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeBond) {
			CFMutableArrayRef serviceList;
			if (!CFDictionaryContainsKey(bondServices, bsdName)) {
				serviceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				CFDictionaryAddValue(bondServices, bsdName, serviceList);
				CFRelease(serviceList);
			}
			serviceList = (CFMutableArrayRef)CFDictionaryGetValue(bondServices, bsdName);
			CFArrayAppendValue(serviceList, service);
		}
	}
	CFRelease(services);
	return bondServices;
}


static Boolean
_SCNetworkMigrationDoBondMigration(SCPreferencesRef	sourcePrefs,
				   SCPreferencesRef	sourceNIPrefs,
				   SCPreferencesRef	targetPrefs,
				   SCPreferencesRef	targetNIPrefs,
				   CFDictionaryRef	bsdNameMapping,
				   CFDictionaryRef	setMapping,
				   CFDictionaryRef	serviceSetMapping)
{
#pragma unused(sourceNIPrefs)
	CFArrayRef		allSourceBonds;
	CFArrayRef		allTargetBonds;
	SCBondInterfaceRef	bond;
	CFMutableDictionaryRef	bondInterfaceMapping	= NULL;
	CFMutableDictionaryRef	bondMapping;
	CFDictionaryRef		bsdNameToBondServices;
	virtualInterfaceContext	context;
	CFIndex			count			= 0;
	Boolean			success			= FALSE;

	allSourceBonds = SCBondInterfaceCopyAll(sourcePrefs);
	allTargetBonds = SCBondInterfaceCopyAll(targetPrefs);

	bsdNameToBondServices = _SCNetworkMigrationCopyMappingBSDNameToBondServices(sourcePrefs);

	bondInterfaceMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	bondMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	// Create Bond Interface mapping
	for (CFIndex idx = 0; idx < CFArrayGetCount(allSourceBonds); idx++) {
		bond = CFArrayGetValueAtIndex(allSourceBonds, idx);
		CFArrayRef bondMembers = SCBondInterfaceGetMemberInterfaces(bond);
		CFMutableArrayRef interfaceList;

		interfaceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (CFIndex idx2 = 0; idx2 < CFArrayGetCount(bondMembers); idx2++) {
			CFStringRef interfaceName;
			SCNetworkInterfaceRef interface;

			interface = CFArrayGetValueAtIndex(bondMembers, idx2);
			interfaceName = SCNetworkInterfaceGetBSDName(interface);

			if (CFDictionaryContainsKey(bsdNameMapping, interfaceName)) {
				CFStringRef bondNewName = CFStringCreateWithFormat(NULL, NULL, CFSTR("bond%ld"), count);
				CFDictionaryAddValue(bondMapping, interfaceName, bondNewName);
				CFArrayAppendValue(interfaceList, interfaceName);
				CFRelease(bondNewName);
				count++;
			}
		}
		if (CFArrayGetCount(interfaceList) > 0) {
			CFDictionaryAddValue(bondInterfaceMapping, bond, interfaceList);
		}
		CFRelease(interfaceList);
	}
	// Remove bond services from target
	_SCNetworkMigrationRemoveBondServices(targetPrefs);

	// Remove Target Bonds
	for (CFIndex idx = 0; idx < CFArrayGetCount(allTargetBonds); idx++) {
		bond = CFArrayGetValueAtIndex(allTargetBonds, idx);
		if (!SCBondInterfaceRemove(bond)) {
			SC_log(LOG_NOTICE, "SCBondInterfaceRemove() failed: %@", bond);
			goto done;
		}
	}

	context.prefs = targetPrefs;
	context.ni_prefs = targetNIPrefs;
	context.bsdNameMapping = bsdNameMapping;
	context.virtualBSDNameMapping = bondMapping;
	context.mappingBSDNameToService = bsdNameToBondServices;
	context.setMapping = setMapping;
	context.serviceSetMapping = serviceSetMapping;

	// Add Bond configurations at the target using mapping
	CFDictionaryApplyFunction(bondInterfaceMapping, add_target_bond, &context);

	success = TRUE;
done:
	CFRelease(allSourceBonds);
	CFRelease(allTargetBonds);
	CFRelease(bondInterfaceMapping);
	CFRelease(bondMapping);
	CFRelease(bsdNameToBondServices);
	return success;
}

static void
add_target_vlan(const void *value, void *context)
{
	CFDictionaryRef		bsdNameMapping;
	virtualInterfaceContext	*ctx			= (virtualInterfaceContext*)context;
	CFDictionaryRef		bsdNameToServiceMapping	= ctx->mappingBSDNameToService;
	SCPreferencesRef	prefs			= ctx->prefs;
	SCVLANInterfaceRef	newVLAN			= NULL;
	SCNetworkInterfaceRef	newPhysicalInterface	= NULL;
	CFStringRef		newPhysicalInterfaceName;
	SCVLANInterfaceRef	oldVLAN			= (SCVLANInterfaceRef)value;
	CFStringRef		oldBSDName;
	SCNetworkInterfaceRef	oldPhysicalInterface;
	CFStringRef		oldPhysicalInterfaceName;
	SCNetworkServiceRef	oldService;
	CFArrayRef		oldServiceList;
	CFDictionaryRef		serviceSetMapping	= ctx->serviceSetMapping;
	CFDictionaryRef		setMapping		= ctx->setMapping;
	CFDictionaryRef		vlanBSDNameMapping	= ctx->virtualBSDNameMapping;
	CFNumberRef		vlanTag;
	CFStringRef		vlanName;
	CFDictionaryRef		vlanOptions;

	bsdNameMapping = ctx->bsdNameMapping;

	oldPhysicalInterface = SCVLANInterfaceGetPhysicalInterface(oldVLAN);
	if (oldPhysicalInterface == NULL) {
		SC_log(LOG_NOTICE, "No old VLAN physical interface");
		goto done;
	}

	oldPhysicalInterfaceName = SCNetworkInterfaceGetBSDName(oldPhysicalInterface);
	if (oldPhysicalInterfaceName == NULL) {
		SC_log(LOG_NOTICE, "No old VLAN physical interface name");
		goto done;
	}

	newPhysicalInterfaceName = CFDictionaryGetValue(bsdNameMapping, oldPhysicalInterfaceName);
	if (newPhysicalInterfaceName == NULL) {
		SC_log(LOG_NOTICE, "No new VLAN physical interface name");
		goto done;
	}
	newPhysicalInterface = __SCNetworkInterfaceCreateWithNIPreferencesUsingBSDName(NULL, ctx->ni_prefs, newPhysicalInterfaceName);
	if (newPhysicalInterface == NULL) {
		SC_log(LOG_NOTICE, "Could not create new VLAN physical interface");
		goto done;
	}

	vlanTag = SCVLANInterfaceGetTag(oldVLAN);
	if (vlanTag == NULL) {
		SC_log(LOG_NOTICE, "No old VLAN interface tag");
		goto done;
	}

	newVLAN = SCVLANInterfaceCreate(prefs, newPhysicalInterface, vlanTag);
	if (newVLAN == NULL) {
		SC_log(LOG_NOTICE, "Could not create new VLAN interface");
		goto done;
	}

	vlanName = SCNetworkInterfaceGetLocalizedDisplayName(oldVLAN);
	if (vlanName != NULL) {
		SCVLANInterfaceSetLocalizedDisplayName(newVLAN, vlanName);
	}

	vlanOptions = SCVLANInterfaceGetOptions(oldVLAN);
	if (vlanOptions != NULL) {
		SCVLANInterfaceSetOptions(newVLAN, vlanOptions);
	}
	oldBSDName = SCNetworkInterfaceGetBSDName(oldVLAN);

	if (oldBSDName == NULL) {
		goto done;
	}

	oldServiceList = CFDictionaryGetValue(bsdNameToServiceMapping, oldBSDName);
	if (oldServiceList == NULL) {
		goto done;
	}

	for (CFIndex idx = 0; idx < CFArrayGetCount(oldServiceList); idx++) {
		oldService = CFArrayGetValueAtIndex(oldServiceList, idx);
		if (!__SCNetworkServiceMigrateNew(prefs, oldService, vlanBSDNameMapping, setMapping, serviceSetMapping)) {
			SC_log(LOG_NOTICE, "Could not migrate VLAN service: %@", oldService);
		}
	}

done:
	if (newPhysicalInterface != NULL) {
		CFRelease(newPhysicalInterface);
	}
	if (newVLAN != NULL) {
		CFRelease(newVLAN);
	}
}

static void
_SCNetworkMigrationRemoveVLANServices(SCPreferencesRef prefs)
{
	CFArrayRef services = SCNetworkServiceCopyAll(prefs);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		    SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeVLAN) {
			SCNetworkServiceRemove(service);
		}
	}

	CFRelease(services);
}


static CFDictionaryRef
_SCNetworkMigrationCopyMappingBSDNameToVLANServices(SCPreferencesRef prefs)
{
	CFArrayRef services = SCNetworkServiceCopyAll(prefs);
	CFMutableDictionaryRef vlanServices = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(services); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, idx);
		SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(interface);

		if ((bsdName != NULL) &&
		    SCNetworkInterfaceGetInterfaceType(interface) == kSCNetworkInterfaceTypeVLAN) {
			CFMutableArrayRef serviceList;
			if (!CFDictionaryContainsKey(vlanServices, bsdName)) {
				serviceList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				CFDictionaryAddValue(vlanServices, bsdName, serviceList);
				CFRelease(serviceList);
			}
			serviceList = (CFMutableArrayRef)CFDictionaryGetValue(vlanServices, bsdName);
			CFArrayAppendValue(serviceList, service);
		}
	}
	CFRelease(services);
	return vlanServices;
}

static Boolean
_SCNetworkMigrationDoVLANMigration(SCPreferencesRef	sourcePrefs,
				   SCPreferencesRef	sourceNIPrefs,
				   SCPreferencesRef	targetPrefs,
				   SCPreferencesRef	targetNIPrefs,
				   CFDictionaryRef	bsdNameMapping,
				   CFDictionaryRef	setMapping,
				   CFDictionaryRef	serviceSetMapping)
{
#pragma unused(sourceNIPrefs)
	CFArrayRef		allSourceVLAN;
	CFArrayRef		allTargetVLAN;
	virtualInterfaceContext	context;
	CFIndex			count		= 0;
	Boolean			success		= FALSE;
	SCVLANInterfaceRef	vlan;
	CFMutableArrayRef	vlanList;
	CFMutableDictionaryRef	vlanMapping;
	CFDictionaryRef		bsdNameToVLANServices;

	allSourceVLAN = SCVLANInterfaceCopyAll(sourcePrefs);
	allTargetVLAN = SCVLANInterfaceCopyAll(targetPrefs);

	bsdNameToVLANServices = _SCNetworkMigrationCopyMappingBSDNameToVLANServices(sourcePrefs);

	vlanList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	vlanMapping = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(allSourceVLAN); idx++) {
		vlan = CFArrayGetValueAtIndex(allSourceVLAN, idx);
		CFStringRef vlanBSDName = SCNetworkInterfaceGetBSDName(vlan);
		SCNetworkInterfaceRef physicalInterface = SCVLANInterfaceGetPhysicalInterface(vlan);
		CFStringRef physicalInterfaceName = SCNetworkInterfaceGetBSDName(physicalInterface);

		// Add VLAN to be migrated if the mapping between interfaces exists
		if (CFDictionaryContainsKey(bsdNameMapping, physicalInterfaceName)) {
			CFStringRef vlanNewName = CFStringCreateWithFormat(NULL, NULL, CFSTR("vlan%ld"), count);
			CFDictionaryAddValue(vlanMapping, vlanBSDName, vlanNewName);
			CFArrayAppendValue(vlanList, vlan);
			CFRelease(vlanNewName);
			count++;
		}
	}
	// Remove vlan services from target
	_SCNetworkMigrationRemoveVLANServices(targetPrefs);

	// Remove Target VLANs
	for (CFIndex idx = 0; idx < CFArrayGetCount(allTargetVLAN); idx++) {
		vlan = CFArrayGetValueAtIndex(allTargetVLAN, idx);
		if (!SCVLANInterfaceRemove(vlan)) {
			SC_log(LOG_NOTICE, "SCVLANInterfaceRemove() failed: %@", vlan);
			goto done;
		}
	}

	context.prefs = targetPrefs;
	context.ni_prefs = targetNIPrefs;
	context.bsdNameMapping = bsdNameMapping;
	context.virtualBSDNameMapping = vlanMapping;
	context.mappingBSDNameToService = bsdNameToVLANServices;
	context.setMapping = setMapping;
	context.serviceSetMapping = serviceSetMapping;

	// Add VLAN configurations at the target using vlanList
	CFArrayApplyFunction(vlanList, CFRangeMake(0, CFArrayGetCount(vlanList)), add_target_vlan, &context);

	success = TRUE;
done:
	CFRelease(allSourceVLAN);
	CFRelease(allTargetVLAN);
	CFRelease(vlanList);
	CFRelease(vlanMapping);
	CFRelease(bsdNameToVLANServices);
	return success;
}

static Boolean
_SCNetworkMigrationDoVirtualNetworkInterfaceMigration(SCPreferencesRef sourcePrefs,
						      SCPreferencesRef sourceNIPrefs,
						      SCPreferencesRef targetPrefs,
						      SCPreferencesRef targetNIPrefs,
						      CFDictionaryRef bsdNameMapping,
						      CFDictionaryRef setMapping,
						      CFDictionaryRef serviceSetMapping)
{
	// Handle Bridges
	if (!_SCNetworkMigrationDoBridgeMigration(sourcePrefs, sourceNIPrefs,
						 targetPrefs, targetNIPrefs,
						 bsdNameMapping, setMapping, serviceSetMapping)) {
		SC_log(LOG_NOTICE, "Bridge migration failed");
	}

	// Handle Bonds
	if (!_SCNetworkMigrationDoBondMigration(sourcePrefs, sourceNIPrefs,
					       targetPrefs, targetNIPrefs,
					       bsdNameMapping, setMapping, serviceSetMapping)) {
		SC_log(LOG_NOTICE, "Bond migration failed");
	}

	// Handle VLANs
	if (!_SCNetworkMigrationDoVLANMigration(sourcePrefs, sourceNIPrefs,
					       targetPrefs, targetNIPrefs,
					       bsdNameMapping, setMapping, serviceSetMapping)) {
		SC_log(LOG_NOTICE, "VLAN migration failed");
	}
	return TRUE;
}
#endif	// !TARGET_OS_IPHONE

typedef struct {
	SCPreferencesRef	prefs;
	CFArrayRef		serviceOrder;
	CFMutableArrayRef	serviceOrderMutable;
	Boolean			*success;
} migrated_service_context;

static void
create_migrated_order(const void *value, void *context)
{
	migrated_service_context	*ctx			= (migrated_service_context*)context;
	CFMutableArrayRef		migratedServiceOrder	= ctx->serviceOrderMutable;
	CFArrayRef			targetServiceOrder	= ctx->serviceOrder;
	CFStringRef			migratedServiceID	= (CFStringRef)value;
	Boolean				*success		= ctx->success;

	if (*success == FALSE) {
		return;
	}

	// Preserving the service order in the source configuration for the services
	// which were migrated into the target configuration
	for (CFIndex idx = 0; idx < CFArrayGetCount(targetServiceOrder); idx++) {
		CFStringRef targetServiceID = CFArrayGetValueAtIndex(targetServiceOrder, idx);
		if (CFEqual(migratedServiceID, targetServiceID)) {
			CFArrayAppendValue(migratedServiceOrder, migratedServiceID);
			return;
		}
	}
}

typedef struct {
	SCPreferencesRef	prefs;
	CFMutableArrayRef	serviceListMutable;
	CFArrayRef		serviceOrder;
	Boolean			*success;
} non_migrated_service_context;

static void
create_non_migrated_service_list(const void *value, void *context)
{
	non_migrated_service_context	*ctx			= (non_migrated_service_context*)context;
	CFArrayRef			migratedServiceOrder	= ctx->serviceOrder;
	CFMutableArrayRef		nonMigratedService	= ctx->serviceListMutable;
	SCPreferencesRef		prefs			= ctx->prefs;
	SCNetworkServiceRef		service;
	Boolean				*success		= ctx->success;
	CFStringRef			targetServiceID		= (CFStringRef)value;

	if (*success == FALSE) {
		return;
	}

	// Adding all services not present in migratedServiceOrder into nonMigrated service
	if (CFArrayGetFirstIndexOfValue(migratedServiceOrder,
					CFRangeMake(0, CFArrayGetCount(migratedServiceOrder)),
					targetServiceID) != kCFNotFound) {
		// if service already present
		return;
	}

	service = SCNetworkServiceCopy(prefs, targetServiceID);
	if (service == NULL) {
		*success = FALSE;
		return;
	}

	CFArrayAppendValue(nonMigratedService, service);
	CFRelease(service);
}

static void
preserve_service_order(const void *key, const void *value, void *context)
{
	migrated_service_context	migrated_context;
	CFMutableArrayRef		migratedServiceOrder;
	non_migrated_service_context	non_migrated_context;
	CFMutableArrayRef		nonMigratedServices;
	SCNetworkSetRef			sourceSet		= (SCNetworkSetRef)key;
	CFArrayRef			sourceServiceOrder	= NULL;
	Boolean				*success		= (Boolean*)context;
	SCNetworkSetRef			targetSet		= (SCNetworkSetRef)value;
	SCNetworkSetPrivateRef		targetPrivate		= (SCNetworkSetPrivateRef)targetSet;
	CFArrayRef			targetServiceOrder	= NULL;

	if (*success == FALSE) {
		return;
	}

	migratedServiceOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	nonMigratedServices = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	sourceServiceOrder = SCNetworkSetGetServiceOrder(sourceSet);
	if (sourceServiceOrder == NULL) {
		goto done;
	}
	targetServiceOrder = SCNetworkSetGetServiceOrder(targetSet);
	if (targetServiceOrder == NULL) {
		goto done;
	}

	migrated_context.prefs = NULL;
	migrated_context.serviceOrder = targetServiceOrder;
	migrated_context.serviceOrderMutable = migratedServiceOrder;
	migrated_context.success = success;

	// Creating a list of service IDs which were migrated in the target set
	// while maintaining the service order of the source set
	CFArrayApplyFunction(sourceServiceOrder, CFRangeMake(0, CFArrayGetCount(sourceServiceOrder)), create_migrated_order, &migrated_context);
	if (*success == FALSE) {
		goto done;
	}

	non_migrated_context.prefs = targetPrivate->prefs;
	non_migrated_context.serviceOrder = migratedServiceOrder;
	non_migrated_context.serviceListMutable = nonMigratedServices;
	non_migrated_context.success = success;

	// Creating a list of all the services which were not migrated from the
	// source set to the target set
	CFArrayApplyFunction(targetServiceOrder, CFRangeMake(0, CFArrayGetCount(targetServiceOrder)), create_non_migrated_service_list, &non_migrated_context);

	// Remove non migrated service
	for (CFIndex idx = 0; idx < CFArrayGetCount(nonMigratedServices); idx++) {
		SCNetworkServiceRef service = CFArrayGetValueAtIndex(nonMigratedServices, idx);
		SCNetworkSetRemoveService(targetSet, service);
	}

	// Set migrated service order
	SCNetworkSetSetServiceOrder(targetSet, migratedServiceOrder);

	// Add non migrated services
	for (CFIndex idx = 0; idx < CFArrayGetCount(nonMigratedServices); idx++) {
		Boolean			ok;
		SCNetworkServiceRef	service;

		service = CFArrayGetValueAtIndex(nonMigratedServices, idx);
		ok = SCNetworkSetAddService(targetSet, service);
		if (!ok) {
			CFStringRef		bsdName;
			SCNetworkInterfaceRef	interface;

			interface = SCNetworkServiceGetInterface(service);
			bsdName = SCNetworkInterfaceGetBSDName(interface);
			SC_log(LOG_NOTICE, "preserve_service_order():  could not add service: %s"
					   "\n  serviceID = %@, interface = %@",
			       SCErrorString(SCError()),
			       SCNetworkServiceGetServiceID(service),
			       bsdName != NULL ? bsdName : CFSTR("?"));
		}
	}

done:
	CFRelease(migratedServiceOrder);
	CFRelease(nonMigratedServices);
	return;

}

static Boolean
_SCNetworkMigrationDoServiceOrderMigration(SCPreferencesRef	sourcePrefs,
					   SCPreferencesRef	targetPrefs,
					   CFDictionaryRef	setMapping)
{
#pragma unused(sourcePrefs)
#pragma unused(targetPrefs)
	Boolean success = TRUE;

	if (!isA_CFDictionary(setMapping)) {
		return FALSE;
	}

	CFDictionaryApplyFunction(setMapping, preserve_service_order, &success);
	return success;
}


static Boolean
hasSameModel(SCPreferencesRef source, SCPreferencesRef target)
{
	Boolean		match;
	CFStringRef	sourceModel;
	CFStringRef	targetModel;

	sourceModel = SCPreferencesGetValue(source, MODEL);
	targetModel = SCPreferencesGetValue(target, MODEL);
	match = (isA_CFString(sourceModel) && isA_CFString(targetModel) && CFEqual(sourceModel, targetModel));
	return match;
}


static Boolean
hasInterfaceConflict(SCPreferencesRef sourceNIPrefs, SCPreferencesRef targetNIPrefs)
{
	Boolean		hasConflict		= FALSE;
	CFDictionaryRef	mappedInterfaces	= NULL;
	CFArrayRef	sourceInterfaces;
	CFArrayRef	targetInterfaces;

	// create a mapping dictionary from interface name (CFStringRef) to corresponding SCNetworkInterfaceRef

	sourceInterfaces = __SCNetworkInterfaceCopyStoredWithPreferences(sourceNIPrefs);
	if (sourceInterfaces != NULL) {
		mappedInterfaces = __SCNetworkInterfaceCreateMappingUsingBSDName(sourceInterfaces);
		CFRelease(sourceInterfaces);
	}

	// for upgrades, every interface in the target configuration must be unchanged from the source confguration

	targetInterfaces = __SCNetworkInterfaceCopyStoredWithPreferences(targetNIPrefs);
	if (targetInterfaces != NULL) {
		for (CFIndex i = 0, n = CFArrayGetCount(targetInterfaces); i < n; i++) {
			SCNetworkInterfaceRef	interface;
			CFStringRef		interfaceName;
			SCNetworkInterfaceRef	mappedInterface	= NULL;

			interface = CFArrayGetValueAtIndex(targetInterfaces, i);
			interfaceName = SCNetworkInterfaceGetBSDName(interface);
			if (interfaceName == NULL) {
				continue;
			}
			if ((mappedInterfaces == NULL) ||
			    !CFDictionaryGetValueIfPresent(mappedInterfaces,
							   interfaceName,
							   (const void **)&mappedInterface) ||
			    !CFEqual(interface, mappedInterface)) {
				// if target interface does not match source interface
				hasConflict = TRUE;
				break;
			}
		}

		CFRelease(targetInterfaces);
	}

	if (mappedInterfaces != NULL) CFRelease(mappedInterfaces);

	return hasConflict;
}


// This is a function that looks at source and target network configuration
// and determines what network configurations can be transferred from source to
// target
static Boolean
_SCNetworkConfigurationMigrateConfiguration(CFURLRef sourceDir, CFURLRef targetDir)
{
	CFDictionaryRef		bsdNameMapping				= NULL;	// Mapping between BSD name and SCNetworkInterfaceRef to help with mapping services
	CFMutableDictionaryRef	builtinMapping				= NULL;	// Mapping between builtin interfaces between source and target configurations: (SCNetworkInterfaceRef -> SCNetworkInterfaceRef)
	CFMutableDictionaryRef	externalMapping				= NULL;	// Mapping between external interfaces between source and target configurations: (SCNetworkInterfaceRef -> SCNetworkInterfaceRef)
	Boolean			migrationSuccess			= FALSE;
	CFArrayRef		newTargetNetworkInterfaceEntities	= NULL;	// Array of Interface Entity which used to create new target interfaces created during migration
	CFDictionaryRef		serviceMapping				= NULL;	// Mapping between services of source to target. (SCNetworkServicesRef -> SCNetworkServicesRef)
	CFDictionaryRef		setMapping				= NULL;
	CFDictionaryRef		sourceServiceSetMapping			= NULL;
	CFArrayRef		sourceConfigurationFiles		= NULL;	// Path to the source configuration files which need to be migrated
	CFURLRef		sourceNetworkInterfaceFile		= NULL;	// Source CFURLRef for preferences.plist and NetworkInterfaces.plist
	char			sourceNetworkInterfaceFileStr[PATH_MAX];
	CFStringRef		sourceNetworkInterfaceFileString	= NULL;	// Source CFStringRef for preferences.plist and NetworkInterfaces.plist
	SCPreferencesRef	sourceNetworkInterfacePrefs		= NULL;	// Source SCPreferencesRef for preferences.plist and NetworkInterfaces.plist
	CFURLRef		sourcePreferencesFile			= NULL;
	char			sourcePreferencesFileStr[PATH_MAX];
	CFStringRef		sourcePreferencesFileString		= NULL;
	SCPreferencesRef	sourcePrefs				= NULL;
	CFStringRef		suffix;
	CFArrayRef		targetConfigurationFiles		= NULL;	// Path to the target configuration files where migration will take place to
	Boolean			targetConfigurationFilesPresent;
	CFURLRef		targetNetworkInterfaceFile		= NULL;	// Target CFURLRef for preferences.plist and NetworkInterfaces.plist
	char			targetNetworkInterfaceFileStr[PATH_MAX];
	CFStringRef		targetNetworkInterfaceFileString	= NULL;	// Target CFStringRef for preferences.plist and NetworkInterfaces.plist
	SCPreferencesRef	targetNetworkInterfacePrefs		= NULL;	// Target SCPreferencesRef for preferences.plist and NetworkInterfaces.plist
	CFURLRef		targetPreferencesFile			= NULL;
	char			targetPreferencesFileStr[PATH_MAX];
	CFStringRef		targetPreferencesFileString		= NULL;
	SCPreferencesRef	targetPrefs				= NULL;
	Boolean			isUpgradeScenario			= FALSE;
	CFMutableDictionaryRef	validityOptions				= NULL;
	const char		*migrationScenario			= "?";

	// Check if configuration files exist in sourceDir
	if (!__SCNetworkConfigurationMigrateConfigurationFilesPresent(sourceDir, &sourceConfigurationFiles, TRUE)) {
		SC_log(LOG_NOTICE, "sourceDir: (%@) missing configuration files", sourceDir);
		goto done;
	}

	sourcePreferencesFile = CFArrayGetValueAtIndex(sourceConfigurationFiles, PREFERENCES_PLIST_INDEX);
	if (!CFURLGetFileSystemRepresentation(sourcePreferencesFile, TRUE, (UInt8*)sourcePreferencesFileStr, sizeof(sourcePreferencesFileStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", sourcePreferencesFile);
		goto done;
	}

	sourceNetworkInterfaceFile = CFArrayGetValueAtIndex(sourceConfigurationFiles, NETWORK_INTERFACES_PLIST_INDEX);
	if (!CFURLGetFileSystemRepresentation(sourceNetworkInterfaceFile, TRUE, (UInt8*)sourceNetworkInterfaceFileStr, sizeof(sourceNetworkInterfaceFileStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", sourceNetworkInterfaceFile);
		goto done;
	}

	sourcePreferencesFileString = CFStringCreateWithCString(NULL, sourcePreferencesFileStr, kCFStringEncodingUTF8);
	sourceNetworkInterfaceFileString = CFStringCreateWithCString(NULL, sourceNetworkInterfaceFileStr, kCFStringEncodingUTF8);

	sourcePrefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, sourcePreferencesFileString);
	sourceNetworkInterfacePrefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, sourceNetworkInterfaceFileString);
	if ((sourcePrefs == NULL) || (sourceNetworkInterfacePrefs == NULL)) {
		goto done;
	}

	targetConfigurationFilesPresent = __SCNetworkConfigurationMigrateConfigurationFilesPresent(targetDir, &targetConfigurationFiles, FALSE);
	if (!targetConfigurationFilesPresent) {
		// if the expected configuration files are not present in the target directory
		if (targetConfigurationFiles == NULL) {
			// but we don't know what files are needed (no target URL)
			SC_log(LOG_NOTICE, "targetConfigurationFiles is NULL");
			goto done;
		}
	}

	targetPreferencesFile = CFArrayGetValueAtIndex(targetConfigurationFiles, PREFERENCES_PLIST_INDEX);
	if (!CFURLGetFileSystemRepresentation(targetPreferencesFile, TRUE, (UInt8*)targetPreferencesFileStr, sizeof(targetPreferencesFileStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", targetPreferencesFile);
		goto done;
	}
	targetNetworkInterfaceFile = CFArrayGetValueAtIndex(targetConfigurationFiles, NETWORK_INTERFACES_PLIST_INDEX);
	if (!CFURLGetFileSystemRepresentation(targetNetworkInterfaceFile, TRUE, (UInt8*)targetNetworkInterfaceFileStr, sizeof(targetNetworkInterfaceFileStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", targetNetworkInterfaceFile);
		goto done;
	}

	targetPreferencesFileString = CFStringCreateWithCString(NULL, targetPreferencesFileStr, kCFStringEncodingUTF8);
	targetNetworkInterfaceFileString = CFStringCreateWithCString(NULL, targetNetworkInterfaceFileStr, kCFStringEncodingUTF8);

	if (targetConfigurationFilesPresent) {
		targetPrefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, targetPreferencesFileString);
		targetNetworkInterfacePrefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, targetNetworkInterfaceFileString);
		if ((targetPrefs == NULL) || (targetNetworkInterfacePrefs == NULL)) {
			SC_log(LOG_NOTICE, "Could not open target prefs/ni_prefs");
			goto done;
		}
	} else {
		// create and populate a new preferences.plist
		targetPrefs = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, targetPreferencesFileString);
		__SCNetworkPopulateDefaultPrefs(targetPrefs);

		// create and populate a new NetworkInterfaces.plist
		targetNetworkInterfacePrefs = SCPreferencesCreateCompanion(targetPrefs, INTERFACES_DEFAULT_CONFIG);
		__SCNetworkPopulateDefaultNIPrefs(targetNetworkInterfacePrefs);
	}
	validityOptions = CFDictionaryCreateMutable(NULL, 0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(validityOptions, kSCNetworkConfigurationRepair, kCFBooleanTrue);

	// Setting Bypass Interface to avoid looking at system interfaces
	_SCNetworkConfigurationSetBypassSystemInterfaces(sourcePrefs, TRUE);
	_SCNetworkConfigurationSetBypassSystemInterfaces(targetPrefs, TRUE);

	if (!hasSameModel(sourcePrefs, targetPrefs)) {
		// if [preferences] sourceModel != targetModel
		migrationScenario = "model change (preferences)";
	} else if (!hasSameModel(sourceNetworkInterfacePrefs, targetNetworkInterfacePrefs)) {
		// if [NetworkInterfaces] sourceModel != targetModel
		migrationScenario = "model change (interfaces)";
	} else if (hasInterfaceConflict(sourceNetworkInterfacePrefs, targetNetworkInterfacePrefs)) {
		// if there is a conflict between the target interfaces and those in the source
		migrationScenario = "interface conflicts";
	} else {
		isUpgradeScenario = TRUE;
	}
	if (isUpgradeScenario) {
		SC_log(LOG_NOTICE, "Upgrading network configuration");
	} else {
		SC_log(LOG_NOTICE, "Migrating network configuration: %s", migrationScenario);
	}

	SC_log(LOG_INFO,
		 "  sourcePrefs                 = %@"
	       "\n  sourceNetworkInterfacePrefs = %@"
	       "\n  targetPrefs                 = %@%s"
	       "\n  targetNetworkInterfacePrefs = %@%s",
	       sourcePrefs,
	       sourceNetworkInterfacePrefs,
	       targetPrefs,
	       targetConfigurationFilesPresent ? "" : ", new",
	       targetNetworkInterfacePrefs,
	       targetConfigurationFilesPresent ? "" : ", new");

	// create backup of migration source
	suffix = CFStringCreateWithFormat(NULL, NULL,
					  CFSTR("pre-%s-source"),
					  isUpgradeScenario ? "upgrade" : "migration");
	__SCNetworkConfigurationBackup(sourcePrefs, suffix, targetPrefs);
	__SCNetworkConfigurationBackup(sourceNetworkInterfacePrefs, suffix, targetPrefs);
	CFRelease(suffix);

	// create backup of migration target
	suffix = CFStringCreateWithFormat(NULL, NULL,
					  CFSTR("pre-%s-%starget"),
					  isUpgradeScenario ? "upgrade" : "migration",
					  targetConfigurationFilesPresent ? "" : "new-");
	__SCNetworkConfigurationBackup(targetPrefs, suffix, targetPrefs);
	__SCNetworkConfigurationBackup(targetNetworkInterfacePrefs, suffix, targetPrefs);
	CFRelease(suffix);

	// log what we're starting with
	__SCNetworkConfigurationReport(LOG_NOTICE, "Source", sourcePrefs, sourceNetworkInterfacePrefs);
	__SCNetworkConfigurationReport(LOG_NOTICE, "Target", targetPrefs, targetNetworkInterfacePrefs);

	// Create services for builtin interfaces at source if they don't exist
	(void)_SCNetworkConfigurationCreateBuiltinInterfaceServices(sourcePrefs, sourceNetworkInterfacePrefs);
	// Checking validity of the source and destination preferences before continuing
	if (!_SCNetworkConfigurationCheckValidityWithPreferences(sourcePrefs,
								 sourceNetworkInterfacePrefs,
								 validityOptions)) {
		SC_log(LOG_NOTICE, "Source configuration not valid");
		goto skipServiceMigration;
	}
	// Only call this function if configuration files were not created by default
	if (targetConfigurationFilesPresent) {
		// Create services for builtin interfaces at target if they don't exist
		(void)_SCNetworkConfigurationCreateBuiltinInterfaceServices(targetPrefs, targetNetworkInterfacePrefs);
		if (!_SCNetworkConfigurationCheckValidityWithPreferences(targetPrefs,
									 targetNetworkInterfacePrefs,
									 validityOptions)) {
			SC_log(LOG_NOTICE, "Target configuration not valid");
			goto skipServiceMigration;
		}
	}
	// Upgrade scenario, source and target models match
	if (isUpgradeScenario) {
		Boolean foundNewInterfaces = FALSE;

		// Create SCPreferences to copy the target prefs
		SCPreferencesRef upgradeSourcePrefs	= __SCPreferencesCreateForMigration(NULL, CFSTR("Upgrade Source Prefs"), NULL);
		SCPreferencesRef upgradeSourceNIPrefs	= __SCPreferencesCreateForMigration(NULL, CFSTR("Upgrade Source NI Prefs"), INTERFACES_DEFAULT_CONFIG);

		// Setting Bypass Interface to avoid looking at system interfaces
		_SCNetworkConfigurationSetBypassSystemInterfaces(upgradeSourcePrefs, TRUE);

		SC_log(LOG_INFO,
		       "Upgrading network configuration:"
		       "\n  upgradeSourcePrefs [temp]   = %@"
		       "\n  upgradeSourceNIPrefs [temp] = %@"
		       "\n  Copying target --> upgrade, source --> target",
		       upgradeSourcePrefs,
		       upgradeSourceNIPrefs);

		// Content of target prefs
		CFDictionaryRef targetPrefsContent = SCPreferencesPathGetValue(targetPrefs, CFSTR("/"));
		CFDictionaryRef targetNIPrefsContent  = SCPreferencesPathGetValue(targetNetworkInterfacePrefs, CFSTR("/"));

		// Backing up the target prefs into source prefs
		SCPreferencesPathSetValue(upgradeSourcePrefs, CFSTR("/"), targetPrefsContent);
		SCPreferencesPathSetValue(upgradeSourceNIPrefs, CFSTR("/"), targetNIPrefsContent);

		// Copying content from the source prefs
		CFDictionaryRef sourcePrefsContent = SCPreferencesPathGetValue(sourcePrefs, CFSTR("/"));
		CFDictionaryRef sourceNIPreferencesContent = SCPreferencesPathGetValue(sourceNetworkInterfacePrefs, CFSTR("/"));

		// Setting the contents of the source prefs into the target prefs
		SCPreferencesPathSetValue(targetPrefs, CFSTR("/"), sourcePrefsContent);
		SCPreferencesPathSetValue(targetNetworkInterfacePrefs, CFSTR("/"), sourceNIPreferencesContent);

		// Getting the mapping of the non builtin interfaces between source and target
		externalMapping = _SCNetworkConfigurationCopyExternalInterfaceMapping(upgradeSourceNIPrefs, targetNetworkInterfacePrefs);
		if (externalMapping != NULL) {
			logMapping(LOG_INFO, externalMapping, "Upgrading, external interface mapping");
		}

		newTargetNetworkInterfaceEntities = _SCNetworkMigrationCreateNetworkInterfaceArray(targetNetworkInterfacePrefs, externalMapping, &foundNewInterfaces);
		SC_log(LOG_INFO, "Upgrading, %s new interfaces", foundNewInterfaces ? "found" : "no");

		if (foundNewInterfaces) {
			if (newTargetNetworkInterfaceEntities == NULL) {
				SC_log(LOG_NOTICE, "Upgrading, failed w/no new interface list");
				CFRelease(upgradeSourcePrefs);
				CFRelease(upgradeSourceNIPrefs);
				goto done;
			}

			// add new interface mapping to NetworkInterfaces.plist
			if (!__SCNetworkInterfaceSaveStoredWithPreferences(targetNetworkInterfacePrefs, newTargetNetworkInterfaceEntities)) {
				SC_log(LOG_NOTICE, "Upgrading, failed to update NetworkInterfaces.plist");
				CFRelease(upgradeSourcePrefs);
				CFRelease(upgradeSourceNIPrefs);
				goto done;
			}

			// create BSD name mapping to facilitate mapping of services
			bsdNameMapping = _SCNetworkMigrationCreateBSDNameMapping(NULL, externalMapping);

			serviceMapping = _SCNetworkMigrationCreateServiceMappingUsingBSDNameMapping(upgradeSourcePrefs, targetPrefs, bsdNameMapping);

			setMapping = _SCNetworkMigrationCreateSetMapping(upgradeSourcePrefs, targetPrefs);

			sourceServiceSetMapping = _SCNetworkMigrationCreateServiceSetMapping(upgradeSourcePrefs);

			_SCNetworkMigrationDoServiceMigration(upgradeSourcePrefs,
							      targetPrefs,
							      serviceMapping,
							      bsdNameMapping,
							      setMapping,
							      sourceServiceSetMapping);
		}
		CFRelease(upgradeSourcePrefs);
		CFRelease(upgradeSourceNIPrefs);
	} else {
		builtinMapping = _SCNetworkConfigurationCopyBuiltinMapping(sourceNetworkInterfacePrefs, targetNetworkInterfacePrefs);

		externalMapping = _SCNetworkConfigurationCopyExternalInterfaceMapping(sourceNetworkInterfacePrefs, targetNetworkInterfacePrefs);

		newTargetNetworkInterfaceEntities = _SCNetworkMigrationCreateNetworkInterfaceArray(targetNetworkInterfacePrefs, externalMapping, NULL);
		if (newTargetNetworkInterfaceEntities == NULL) {
			SC_log(LOG_NOTICE, "Migrating, failed w/no new interface list");
			goto done;
		}

		// Write new interface mapping to NetworkInterfaces.plist
		if (!__SCNetworkInterfaceSaveStoredWithPreferences(targetNetworkInterfacePrefs, newTargetNetworkInterfaceEntities)) {
			SC_log(LOG_NOTICE, "Migrating, failed to update NetworkInterfaces.plist");
			goto done;
		}

		// create BSD name mapping to facilitate mapping of services
		bsdNameMapping = _SCNetworkMigrationCreateBSDNameMapping(builtinMapping, externalMapping);

		serviceMapping = _SCNetworkMigrationCreateServiceMappingUsingBSDNameMapping(sourcePrefs, targetPrefs, bsdNameMapping);
		if (serviceMapping == NULL) {
			goto done;
		}

		setMapping = _SCNetworkMigrationCreateSetMapping(sourcePrefs, targetPrefs);

		sourceServiceSetMapping = _SCNetworkMigrationCreateServiceSetMapping(sourcePrefs);

		if (!_SCNetworkMigrationDoServiceMigration(sourcePrefs,
							   targetPrefs,
							   serviceMapping,
							   bsdNameMapping,
							   setMapping,
							   sourceServiceSetMapping)) {
			SC_log(LOG_NOTICE, "SCNetworkMigrationDoServiceMigration(): service migration failed");
			goto done;
		}
#if	!TARGET_OS_IPHONE
		// Migrating Virtual Network Interfaces
		if (!_SCNetworkMigrationDoVirtualNetworkInterfaceMigration(sourcePrefs, sourceNetworkInterfacePrefs,
									  targetPrefs, targetNetworkInterfacePrefs,
									  bsdNameMapping, setMapping, sourceServiceSetMapping)) {
			SC_log(LOG_NOTICE, "SCNetworkMigrationDoServiceMigration(): virtual interface migration failed");
		}
#endif	// !TARGET_OS_IPHONE
		// Migrate Service Order
		if (!_SCNetworkMigrationDoServiceOrderMigration(sourcePrefs, targetPrefs, setMapping)) {
			SC_log(LOG_NOTICE, "SCNetworkMigrationDoServiceMigration(): service order migration failed");
		}
	}

skipServiceMigration:
	// Migrating System Information
	if (!isUpgradeScenario) {
		if (!_SCNetworkMigrationDoSystemMigration(sourcePrefs, targetPrefs)) {
			SC_log(LOG_NOTICE, "SCNetworkMigrationDoServiceMigration(): system setting migration failed");
		}
	}
	CFDictionaryAddValue(validityOptions, kSCNetworkConfigurationRepairModel, kCFBooleanTrue);
	if (!_SCNetworkConfigurationCheckValidityWithPreferences(targetPrefs, targetNetworkInterfacePrefs, validityOptions)) {
		SC_log(LOG_NOTICE,
		       "%s configuration not valid",
		       isUpgradeScenario ? "Upgraded" : "Migrated");
		goto done;
	}
	if (!SCPreferencesCommitChanges(targetPrefs)) {
		SC_log(LOG_NOTICE, "SCPreferencesCommitChanges(target preferences.plist) failed: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCPreferencesCommitChanges(targetNetworkInterfacePrefs)) {
		SC_log(LOG_NOTICE, "SCPreferencesCommitChanges(target NetworkInterfaces.plist) failed: %s", SCErrorString(SCError()));
		goto done;
	}
	migrationSuccess = TRUE;

	__SCNetworkConfigurationReport(LOG_NOTICE, isUpgradeScenario ? "Upgraded" : "Migrated", targetPrefs, targetNetworkInterfacePrefs);

done:
	if (setMapping != NULL) {
		CFRelease(setMapping);
	}
	if (sourceServiceSetMapping != NULL) {
		CFRelease(sourceServiceSetMapping);
	}
	if (sourceConfigurationFiles != NULL) {
		CFRelease(sourceConfigurationFiles);
	}
	if (targetConfigurationFiles != NULL) {
		CFRelease(targetConfigurationFiles);
	}
	if (sourcePreferencesFileString != NULL) {
		CFRelease(sourcePreferencesFileString);
	}
	if (sourceNetworkInterfaceFileString != NULL) {
		CFRelease(sourceNetworkInterfaceFileString);
	}
	if (targetPreferencesFileString != NULL) {
		CFRelease(targetPreferencesFileString);
	}
	if (targetNetworkInterfaceFileString != NULL) {
		CFRelease(targetNetworkInterfaceFileString);
	}
	if (newTargetNetworkInterfaceEntities != NULL) {
		CFRelease(newTargetNetworkInterfaceEntities);
	}
	if (builtinMapping != NULL) {
		CFRelease(builtinMapping);
	}
	if (externalMapping != NULL) {
		CFRelease(externalMapping);
	}
	if (bsdNameMapping != NULL) {
		CFRelease(bsdNameMapping);
	}
	if (serviceMapping != NULL) {
		CFRelease(serviceMapping);
	}
	if (targetPrefs != NULL) {
		CFRelease(targetPrefs);
	}
	if (sourcePrefs != NULL) {
		CFRelease(sourcePrefs);
	}
	if (sourceNetworkInterfacePrefs != NULL) {
		CFRelease(sourceNetworkInterfacePrefs);
	}
	if (targetNetworkInterfacePrefs != NULL) {
		CFRelease(targetNetworkInterfacePrefs);
	}
	if (validityOptions != NULL) {
		CFRelease(validityOptions);
	}
	return migrationSuccess;
}

#define N_QUICK 64

static Boolean
_SCNetworkMigrationAreServicesIdentical(SCPreferencesRef configPref, SCPreferencesRef expectedConfigPref)
{
	const void * expected_vals_q[N_QUICK];
	const void ** expected_vals = expected_vals_q;
	CFMutableArrayRef expectedServiceArray = NULL;
	CFIndex expectedServiceArrayCount = 0;
	CFDictionaryRef expectedServiceDict = NULL;
	size_t expectedServiceDictCount = 0;
	CFDictionaryRef expectedServiceEntity = 0;
	Boolean foundMatch = FALSE;
	CFMutableArrayRef serviceArray = NULL;
	CFIndex serviceArrayCount = 0;
	CFDictionaryRef serviceDict = NULL;
	size_t serviceDictCount = 0;
	CFDictionaryRef serviceEntity = NULL;
	Boolean success = FALSE;
	const void * vals_q[N_QUICK];
	const void ** vals = vals_q;

	serviceDict = SCPreferencesGetValue(configPref, kSCPrefNetworkServices);
	if (!isA_CFDictionary(serviceDict)) {
		goto done;
	}
	serviceDictCount = CFDictionaryGetCount(serviceDict);

	expectedServiceDict = SCPreferencesGetValue(expectedConfigPref, kSCPrefNetworkServices);
	if (!isA_CFDictionary(expectedServiceDict)) {
		goto done;
	}
	expectedServiceDictCount = CFDictionaryGetCount(expectedServiceDict);

	if (serviceDictCount != expectedServiceDictCount) {
		goto done;
	}

	if (serviceDictCount > (sizeof(vals_q) / sizeof(CFTypeRef))) {
		vals = CFAllocatorAllocate(NULL, serviceDictCount * sizeof(CFPropertyListRef), 0);
	}

	CFDictionaryGetKeysAndValues(serviceDict, NULL, vals);
	serviceArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (size_t idx=0; idx < serviceDictCount; idx++) {
		serviceEntity = vals[idx];
		if (!isA_CFDictionary(serviceEntity)) {
			continue;
		}
		CFArrayAppendValue(serviceArray, serviceEntity);
	}

	serviceArrayCount = CFArrayGetCount(serviceArray);

	if (expectedServiceDictCount > (sizeof(expected_vals_q) / sizeof(CFTypeRef))) {
		expected_vals = CFAllocatorAllocate(NULL, expectedServiceDictCount, 0);
	}

	CFDictionaryGetKeysAndValues(expectedServiceDict, NULL, expected_vals);
	expectedServiceArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (size_t idx = 0; idx < expectedServiceDictCount; idx++) {
		serviceEntity = expected_vals[idx];
		if (!isA_CFDictionary(serviceEntity)) {
			continue;
		}
		CFArrayAppendValue(expectedServiceArray, serviceEntity);
	}
	expectedServiceArrayCount = CFArrayGetCount(expectedServiceArray);

	if (serviceArrayCount != expectedServiceArrayCount) {
		goto done;
	}

	for (CFIndex idx = 0; idx < expectedServiceArrayCount; idx++) {
		foundMatch = FALSE;
		expectedServiceEntity = CFArrayGetValueAtIndex(expectedServiceArray, idx);
		serviceArrayCount = CFArrayGetCount(serviceArray);

		for (CFIndex idx2 = 0; idx2 < serviceArrayCount; idx2++) {
			serviceEntity = CFArrayGetValueAtIndex(serviceArray, idx2);

			if (CFEqual(expectedServiceEntity, serviceEntity)) {
				foundMatch = TRUE;
				break;
			}
		}

		if (!foundMatch) {
			break;
		}
	}

	success = foundMatch;
done:
	if (vals != vals_q) {
		CFAllocatorDeallocate(NULL, vals);
	}
	if (expected_vals != expected_vals_q) {
		CFAllocatorDeallocate(NULL, expected_vals);
	}
	return success;
}

static Boolean
_SCNetworkMigrationAreNetworkInterfaceConfigurationsIdentical (SCPreferencesRef configNetworkInterfacePref, SCPreferencesRef expectedNetworkInterfacePref)
{
	CFDictionaryRef expectedInterfaceEntity = NULL;
	CFArrayRef expectedInterfaceList = NULL;
	CFIndex expectedInterfaceListCount;
	Boolean foundMatch = FALSE;
	CFDictionaryRef interfaceEntity = NULL;
	CFArrayRef interfaceList = NULL;
	CFIndex interfaceListCount;
	CFMutableArrayRef interfaceListMutable = NULL;
	Boolean success = FALSE;

	interfaceList = SCPreferencesGetValue(configNetworkInterfacePref, INTERFACES);
	if (!isA_CFArray(interfaceList)) {
		goto done;
	}
	interfaceListMutable = CFArrayCreateMutableCopy(NULL, 0, interfaceList);
	interfaceListCount = CFArrayGetCount(interfaceListMutable);

	expectedInterfaceList = SCPreferencesGetValue(expectedNetworkInterfacePref, INTERFACES);
	if (!isA_CFArray(expectedInterfaceList)) {
		goto done;
	}
	expectedInterfaceListCount = CFArrayGetCount(expectedInterfaceList);

	if (interfaceListCount != expectedInterfaceListCount) {
		goto done;
	}

	for (CFIndex idx = 0; idx < expectedInterfaceListCount; idx++) {
		foundMatch = FALSE;
		expectedInterfaceEntity = CFArrayGetValueAtIndex(expectedInterfaceList, idx);
		interfaceListCount = CFArrayGetCount(interfaceListMutable);

		for (CFIndex idx2 = 0; idx2 < interfaceListCount; idx2++) {
			interfaceEntity = CFArrayGetValueAtIndex(interfaceList, idx2);
			if (CFEqual(expectedInterfaceEntity, interfaceEntity)) {
				foundMatch = TRUE;
				break;
			}
		}
		if (!foundMatch) {
			break;
		}
	}
	success = foundMatch;

done:
	if (interfaceListMutable != NULL) {
		CFRelease(interfaceListMutable);
	}

	return success;
}

Boolean
_SCNetworkMigrationAreConfigurationsIdentical (CFURLRef configurationURL,
					       CFURLRef expectedConfigurationURL)
{
	CFURLRef baseConfigURL = NULL;
	CFURLRef baseExpectedConfigURL = NULL;
	CFURLRef configPreferencesURL = NULL;
	CFURLRef configNetworkInterfacesURL = NULL;
	SCPreferencesRef configPref = NULL;
	SCPreferencesRef configNetworkInterfacePref = NULL;
	SCPreferencesRef expectedConfigPref = NULL;
	SCPreferencesRef expectedNetworkInterfacePref = NULL;
	CFURLRef expectedNetworkInterfaceURL = NULL;
	CFURLRef expectedPreferencesURL = NULL;
	Boolean isIdentical = FALSE;
	CFStringRef networkInterfaceConfigString = NULL;
	CFStringRef networkInterfaceExpectedString = NULL;
	CFStringRef prefsConfigString = NULL;
	CFStringRef prefsExpectedString = NULL;
	char networkInterfaceConfigStr[PATH_MAX];
	char networkInterfaceExpectedStr[PATH_MAX];
	char prefsConfigStr[PATH_MAX];
	char prefsExpectedStr[PATH_MAX];

	if (configurationURL == NULL ||
	    expectedConfigurationURL == NULL) {
		return FALSE;
	}
	baseConfigURL = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
								    PREFS_DEFAULT_DIR_RELATIVE,
								    kCFURLPOSIXPathStyle,
								    TRUE,
								    configurationURL);
	configPreferencesURL = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
										     (const UInt8*)PREFS_DEFAULT_CONFIG_PLIST,
										     sizeof(PREFS_DEFAULT_CONFIG_PLIST),
										     FALSE,
										     baseConfigURL);
	if (!CFURLResourceIsReachable(configPreferencesURL, NULL)) {
		SC_log(LOG_NOTICE, "No preferences.plist file: %@", configPreferencesURL);
		goto done;
	}

	configNetworkInterfacesURL = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
											   (const UInt8*)INTERFACES_DEFAULT_CONFIG_PLIST,
											   sizeof(INTERFACES_DEFAULT_CONFIG_PLIST),
											   FALSE,
											   baseConfigURL);
	if (!CFURLResourceIsReachable(configNetworkInterfacesURL, NULL)) {
		SC_log(LOG_NOTICE, "No NetworkInterfaces.plist file: %@", configNetworkInterfacesURL);
		goto done;
	}

	if (!CFURLGetFileSystemRepresentation(configPreferencesURL, TRUE, (UInt8*)prefsConfigStr, sizeof(prefsConfigStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configPreferencesURL);
		goto done;
	}
	if (!CFURLGetFileSystemRepresentation(configNetworkInterfacesURL, TRUE, (UInt8*)networkInterfaceConfigStr, sizeof(networkInterfaceConfigStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", configNetworkInterfacesURL);
		goto done;
	}

	baseExpectedConfigURL = CFURLCreateWithFileSystemPathRelativeToBase(NULL,
									    PREFS_DEFAULT_DIR_RELATIVE,
									    kCFURLPOSIXPathStyle,
									    TRUE,
									    expectedConfigurationURL);
	expectedPreferencesURL = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
										       (const UInt8*)PREFS_DEFAULT_CONFIG_PLIST,
										       sizeof(PREFS_DEFAULT_CONFIG_PLIST),
										       FALSE,
										       baseExpectedConfigURL);

	if (!CFURLResourceIsReachable(expectedPreferencesURL, NULL)) {
		SC_log(LOG_NOTICE, "No expected preferences.plist file");
		goto done;
	}

	expectedNetworkInterfaceURL = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
											    (const UInt8*)INTERFACES_DEFAULT_CONFIG_PLIST,
											    sizeof(INTERFACES_DEFAULT_CONFIG_PLIST),
											    FALSE,
											    baseExpectedConfigURL);

	if (!CFURLResourceIsReachable(expectedNetworkInterfaceURL, NULL)) {
		SC_log(LOG_NOTICE, "No expected NetworkInterfaces.plist file");
		goto done;
	}

	if (!CFURLGetFileSystemRepresentation(expectedPreferencesURL, TRUE, (UInt8*)prefsExpectedStr, sizeof(prefsExpectedStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", expectedPreferencesURL);
		goto done;
	}
	if (!CFURLGetFileSystemRepresentation(expectedNetworkInterfaceURL, TRUE, (UInt8*)networkInterfaceExpectedStr, sizeof(networkInterfaceExpectedStr))) {
		SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", expectedNetworkInterfaceURL);
		goto done;
	}

	prefsConfigString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), prefsConfigStr);
	networkInterfaceConfigString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), networkInterfaceConfigStr);
	prefsExpectedString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), prefsExpectedStr);
	networkInterfaceExpectedString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), networkInterfaceExpectedStr);

	configPref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, prefsConfigString);
	expectedConfigPref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, prefsExpectedString);
	configNetworkInterfacePref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, networkInterfaceConfigString);
	expectedNetworkInterfacePref = __SCPreferencesCreateForMigration(NULL, PLUGIN_ID, networkInterfaceExpectedString);
done:
	if (configPref == NULL ||
	    expectedConfigPref == NULL ||
	    configNetworkInterfacePref == NULL ||
	    expectedNetworkInterfacePref == NULL) {
		SC_log(LOG_NOTICE, "One of the preferences is NULL");
		isIdentical = FALSE;
	} else {
		isIdentical = (_SCNetworkMigrationAreServicesIdentical(configPref, expectedConfigPref) &&
			       _SCNetworkMigrationAreNetworkInterfaceConfigurationsIdentical(configNetworkInterfacePref, expectedNetworkInterfacePref));
	}
	if (baseConfigURL != NULL) {
		CFRelease(baseConfigURL);
	}
	if (configPreferencesURL != NULL) {
		CFRelease(configPreferencesURL);
	}
	if (configNetworkInterfacesURL != NULL) {
		CFRelease(configNetworkInterfacesURL);
	}
	if (baseExpectedConfigURL != NULL) {
		CFRelease(baseExpectedConfigURL);
	}
	if (expectedPreferencesURL != NULL) {
		CFRelease(expectedPreferencesURL);
	}
	if (expectedNetworkInterfaceURL != NULL) {
		CFRelease(expectedNetworkInterfaceURL);
	}
	if (prefsConfigString != NULL) {
		CFRelease(prefsConfigString);
	}
	if (networkInterfaceConfigString != NULL) {
		CFRelease(networkInterfaceConfigString);
	}
	if (prefsExpectedString != NULL) {
		CFRelease(prefsExpectedString);
	}
	if (networkInterfaceExpectedString != NULL) {
		CFRelease(networkInterfaceExpectedString);
	}
	if (configPref != NULL) {
		CFRelease(configPref);
	}
	if (expectedConfigPref != NULL) {
		CFRelease(expectedConfigPref);
	}
	if (configNetworkInterfacePref != NULL) {
		CFRelease(configNetworkInterfacePref);
	}
	if (expectedNetworkInterfacePref != NULL) {
		CFRelease(expectedNetworkInterfacePref);
	}
	return isIdentical;
}

CFArrayRef
_SCNetworkConfigurationCopyMigrationRemovePaths	(CFArrayRef	targetPaths,
						 CFURLRef	targetDir)
{
	CFURLRef affectedURL;
	char filePath[PATH_MAX];
	CFURLRef targetFile;
	CFMutableArrayRef toBeRemoved = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	for (CFIndex idx = 0; idx < CFArrayGetCount(targetPaths); idx++) {
		affectedURL = CFArrayGetValueAtIndex(targetPaths, idx);

		if (!CFURLGetFileSystemRepresentation(affectedURL, TRUE, (UInt8*)filePath, sizeof(filePath))) {
			SC_log(LOG_NOTICE, "Cannot get file system representation for url: %@", affectedURL);
			continue;
		}
		targetFile = CFURLCreateFromFileSystemRepresentationRelativeToBase(NULL,
										   (const UInt8*)filePath,
										   strnlen(filePath, sizeof(filePath)),
										   FALSE,
										   targetDir);

		if (!CFURLResourceIsReachable(targetFile, NULL)) {
			CFArrayAppendValue(toBeRemoved, affectedURL);
		}
		CFRelease(targetFile);
	}
	// If number of files to be removed is 0, return NULL
	if (CFArrayGetCount(toBeRemoved) == 0) {
		CFRelease(toBeRemoved);
		toBeRemoved = NULL;
	}
	return toBeRemoved;
}
