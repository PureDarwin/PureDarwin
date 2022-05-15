/*
 * Copyright (c) 2004-2008, 2010, 2011, 2014, 2016, 2017, 2020, 2021 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * August 5, 2004			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "scutil.h"
#include "commands.h"
#include "prefs.h"
#include "net.h"
#include "net_interface.h"
#include "net_protocol.h"
#include "net_service.h"
#include "net_set.h"

#include "SCNetworkConfigurationInternal.h"
#include "SCPreferencesInternal.h"

#include <unistd.h>


__private_extern__ CFMutableArrayRef		new_interfaces	= NULL;

__private_extern__ CFArrayRef			interfaces	= NULL;
__private_extern__ CFArrayRef			services	= NULL;
__private_extern__ CFArrayRef			protocols	= NULL;
__private_extern__ CFArrayRef			sets		= NULL;

__private_extern__ SCNetworkInterfaceRef	net_interface	= NULL;
__private_extern__ SCNetworkServiceRef		net_service	= NULL;
__private_extern__ SCNetworkProtocolRef		net_protocol	= NULL;
__private_extern__ SCNetworkSetRef		net_set		= NULL;

__private_extern__ CFNumberRef			CFNumberRef_0	= NULL;
__private_extern__ CFNumberRef			CFNumberRef_1	= NULL;


/* -------------------- */


__private_extern__
CF_RETURNS_RETAINED CFNumberRef
_copy_number(const char *arg)
{
	int	val;

	if (sscanf(arg, "%d", &val) != 1) {
		return NULL;
	}

	return CFNumberCreate(NULL, kCFNumberIntType, &val);
}


/* -------------------- */


__private_extern__
CFIndex
_find_option(const char *option, optionsRef options, const int nOptions)
{
	CFIndex	i;

	for (i = 0; i < nOptions; i++) {
		if (strcasecmp(option, options[i].option) == 0) {
			return i;
		}
	}

	return kCFNotFound;
}


__private_extern__
CFIndex
_find_selection(CFStringRef choice, selections choices[], unsigned int *flags)
{
	CFIndex	i;

	i = 0;
	while (choices[i].selection != NULL) {
		if (CFStringCompare(choice,
				    choices[i].selection,
				    kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			if (flags != NULL) {
				*flags = choices[i].flags;
			}
			return i;
		}
		i++;
	}

	return kCFNotFound;
}


__private_extern__
Boolean
_process_options(optionsRef options, int nOptions, int argc, char * const argv[], CFMutableDictionaryRef newConfiguration)
{
	while (argc > 0) {
		CFIndex	optionIndex	= kCFNotFound;

		optionIndex = _find_option(argv[0], options, nOptions);
		if (optionIndex == kCFNotFound) {
			SCPrint(TRUE, stdout, CFSTR("set what?\n"));
			return FALSE;
		}
		argv++;
		argc--;

		switch (options[optionIndex].type) {
			case isOther :
				// all option processing is managed by the "handler"
				break;
			case isHelp :
				SCPrint(TRUE, stdout, CFSTR("%s\n"), (char *)options[optionIndex].info);
				return FALSE;
			case isChooseOne : {
				CFStringRef	choice;
				selections	*choices	= (selections *)options[optionIndex].info;
				unsigned int	flags;
				CFIndex		i;

				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
					return FALSE;
				}

				choice = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				i = _find_selection(choice, choices, &flags);
				CFRelease(choice);

				if (i != kCFNotFound) {
					if (choices[i].flags & selectionNotAvailable) {
						SCPrint(TRUE, stdout,
							CFSTR("cannot select %s\n"),
							options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
							return FALSE;
					}

					if (choices[i].key != NULL) {
						CFDictionarySetValue(newConfiguration,
								     *(options[optionIndex].key),
								     *(choices[i].key));
					} else {
						CFDictionaryRemoveValue(newConfiguration,
									*(options[optionIndex].key));
					}
				} else {
					SCPrint(TRUE, stdout,
						CFSTR("invalid %s\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
					return FALSE;
				}

				argv++;
				argc--;
				break;
			}
			case isChooseMultiple :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s(s) not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
					return FALSE;
				}

				if (strlen(argv[0]) > 0) {
					CFIndex			i;
					CFIndex			n;
					CFMutableArrayRef	chosen;
					CFStringRef		str;
					CFArrayRef		str_array;

					str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
					str_array = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR(","));
					CFRelease(str);

					chosen = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

					n = CFArrayGetCount(str_array);
					for (i = 0; i < n; i++) {
						CFStringRef	choice;
						selections	*choices	= (selections *)options[optionIndex].info;
						unsigned int	flags;
						CFIndex		j;

						choice = CFArrayGetValueAtIndex(str_array, i);
						j = _find_selection(choice, choices, &flags);

						if (j != kCFNotFound) {
							if (choices[j].flags & selectionNotAvailable) {
								SCPrint(TRUE, stdout,
									CFSTR("cannot select %s\n"),
									options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
								CFArrayRemoveAllValues(chosen);
								break;
							}

							CFArrayAppendValue(chosen, *(choices[j].key));
						} else {
							SCPrint(TRUE, stdout,
								CFSTR("invalid %s\n"),
								options[optionIndex].description != NULL ? options[optionIndex].description : "selection");
							CFArrayRemoveAllValues(chosen);
							break;
						}
					}
					CFRelease(str_array);

					if (CFArrayGetCount(chosen) > 0) {
						CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), chosen);
					} else {
						CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
					}
					CFRelease(chosen);
				} else {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				}

				argv++;
				argc--;
				break;
			case isBool :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "enable/disable");
					return FALSE;
				}

				if        ((strcasecmp(argv[0], "disable") == 0) ||
					   (strcasecmp(argv[0], "no"     ) == 0) ||
					   (strcasecmp(argv[0], "off"    ) == 0) ||
					   (strcasecmp(argv[0], "0"      ) == 0)) {
					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), kCFBooleanFalse);
				} else if ((strcasecmp(argv[0], "enable") == 0) ||
					   (strcasecmp(argv[0], "yes"   ) == 0) ||
					   (strcasecmp(argv[0], "on"   ) == 0) ||
					   (strcasecmp(argv[0], "1"     ) == 0)) {
					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), kCFBooleanTrue);
				} else if (strcmp(argv[0], "") == 0) {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				} else {
					SCPrint(TRUE, stdout, CFSTR("invalid value\n"));
					return FALSE;
				}

				argv++;
				argc--;
				break;
			case isBoolean :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "enable/disable");
					return FALSE;
				}

				if        ((strcasecmp(argv[0], "disable") == 0) ||
					   (strcasecmp(argv[0], "no"     ) == 0) ||
					   (strcasecmp(argv[0], "off"    ) == 0) ||
					   (strcasecmp(argv[0], "0"      ) == 0)) {
					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), CFNumberRef_0);
				} else if ((strcasecmp(argv[0], "enable") == 0) ||
					   (strcasecmp(argv[0], "yes"   ) == 0) ||
					   (strcasecmp(argv[0], "on"   ) == 0) ||
					   (strcasecmp(argv[0], "1"     ) == 0)) {
					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), CFNumberRef_1);
				} else if (strcmp(argv[0], "") == 0) {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				} else {
					SCPrint(TRUE, stdout, CFSTR("invalid value\n"));
					return FALSE;
				}

				argv++;
				argc--;
				break;
			case isNumber :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "value");
					return FALSE;
				}

				if (strlen(argv[0]) > 0) {
					CFNumberRef	num;

					num = _copy_number(argv[0]);
					if (num != NULL) {
						CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), num);
						CFRelease(num);
					} else {
						SCPrint(TRUE, stdout, CFSTR("invalid value\n"));
						return FALSE;
					}
				} else {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				}

				argv++;
				argc--;
				break;
			case isString :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "value");
					return FALSE;
				}

				if (strlen(argv[0]) > 0) {
					CFStringRef	str;

					str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), str);
					CFRelease(str);
				} else {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				}

				argv++;
				argc--;
				break;
			case isStringArray :
				if (argc < 1) {
					SCPrint(TRUE, stdout,
						CFSTR("%s(s) not specified\n"),
						options[optionIndex].description != NULL ? options[optionIndex].description : "value");
					return FALSE;
				}

				if (strlen(argv[0]) > 0) {
					CFStringRef	str;
					CFArrayRef	str_array;

					str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
					str_array = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR(","));
					CFRelease(str);

					CFDictionarySetValue(newConfiguration, *(options[optionIndex].key), str_array);
					CFRelease(str_array);
				} else {
					CFDictionaryRemoveValue(newConfiguration, *(options[optionIndex].key));
				}

				argv++;
				argc--;
				break;
		}

		if (options[optionIndex].handler != NULL) {
			CFStringRef	key;
			int		nArgs;

			key = options[optionIndex].key != NULL ? *(options[optionIndex].key) : NULL;
			nArgs = (*options[optionIndex].handler)(key,
								options[optionIndex].description,
								options[optionIndex].info,
								argc,
								argv,
								newConfiguration);
			if (nArgs < 0) {
				return FALSE;
			}

			argv += nArgs;
			argc -= nArgs;
		}
	}

	return TRUE;
}


/* -------------------- */


#define	N_QUICK	32

static CFComparisonResult
compare_CFString(const void *val1, const void *val2, void *context)
{
#pragma unused(context)
	CFStringRef		str1	= (CFStringRef)val1;
	CFStringRef		str2	= (CFStringRef)val2;

	return CFStringCompare(str1, str2, 0);
}


__private_extern__
void
_show_entity(CFDictionaryRef entity, CFStringRef prefix)
{
	CFArrayRef		array;
	const void *		keys_q[N_QUICK];
	const void **		keys	= keys_q;
	CFIndex			i;
	CFIndex			n;
	CFMutableArrayRef	sorted;

	n = CFDictionaryGetCount(entity);
	if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
	}
	CFDictionaryGetKeysAndValues(entity, keys, NULL);

	array  = CFArrayCreate(NULL, keys, n, &kCFTypeArrayCallBacks);
	sorted = CFArrayCreateMutableCopy(NULL, n, array);
	if (n > 1) {
		CFArraySortValues(sorted,
				  CFRangeMake(0, n),
				  compare_CFString,
				  NULL);
	}

	for (i = 0; i < n; i++) {
		CFStringRef	key;
		CFTypeRef	value;

		key   = CFArrayGetValueAtIndex(sorted, i);
		value = CFDictionaryGetValue(entity, key);
		if (isA_CFArray(value)) {
			CFIndex		i;
			CFIndex		n	= CFArrayGetCount(value);

			SCPrint(TRUE, stdout, CFSTR("%@    %@ = ("), prefix, key);
			for (i = 0; i < n; i++) {
				CFTypeRef	val;

				val = CFArrayGetValueAtIndex(value, i);
				SCPrint(TRUE, stdout,
					CFSTR("%s%@"),
					(i > 0) ? ", " : "",
					val);
			}
			SCPrint(TRUE, stdout, CFSTR(")\n"));
		} else {
			SCPrint(TRUE, stdout, CFSTR("%@    %@ = %@\n"), prefix, key, value);
		}
	}

	CFRelease(sorted);
	CFRelease(array);
	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
	}

	return;
}


/* -------------------- */


static void
_net_close()
{
	if (net_interface != NULL) {
		CFRelease(net_interface);
		net_interface = NULL;
	}

	if (net_service != NULL) {
		CFRelease(net_service);
		net_service = NULL;
	}

	if (net_protocol != NULL) {
		CFRelease(net_protocol);
		net_protocol = NULL;
	}

	if (net_set != NULL) {
		CFRelease(net_set);
		net_set = NULL;
	}

	if (interfaces != NULL) {
		CFRelease(interfaces);
		interfaces = NULL;
	}

	if (services != NULL) {
		CFRelease(services);
		services = NULL;
	}

	if (protocols != NULL) {
		CFRelease(protocols);
		protocols = NULL;
	}

	if (sets != NULL) {
		CFRelease(sets);
		sets = NULL;
	}

	if (new_interfaces != NULL) {
		CFRelease(new_interfaces);
		new_interfaces = NULL;
	}

	return;
}


__private_extern__
void
do_net_init()
{
	int	one	= 1;
	int	zero	= 0;

	CFNumberRef_0 = CFNumberCreate(NULL, kCFNumberIntType, &zero);
	CFNumberRef_1 = CFNumberCreate(NULL, kCFNumberIntType, &one);

	return;
}


__private_extern__
void
do_net_open(int argc, char * const argv[])
{
	Boolean		ok;

	if (prefs != NULL) {
		if (_prefs_commitRequired(argc, argv, "close")) {
			return;
		}

		_net_close();
		_prefs_close();
	}

	ok = _prefs_open(CFSTR("scutil --net"), (argc > 0) ? argv[0] : NULL);
	if (!ok) {
		SCPrint(TRUE,
			stdout,
			CFSTR("Could not open prefs: %s\n"),
			SCErrorString(SCError()));
		return;
	}

	net_set = SCNetworkSetCopyCurrent(prefs);
	if (net_set != NULL) {
		CFStringRef	setName;

		setName = SCNetworkSetGetName(net_set);
		if (setName != NULL) {
			SCPrint(TRUE, stdout, CFSTR("set \"%@\" selected\n"), setName);
		} else {
			SCPrint(TRUE, stdout,
				CFSTR("set ID \"%@\" selected\n"),
				SCNetworkSetGetSetID(net_set));
		}
	}

	return;
}


__private_extern__
void
do_net_commit(int argc, char * const argv[])
{
#pragma unused(argc)
#pragma unused(argv)
	if (!SCPreferencesCommitChanges(prefs)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		return;
	}

	_prefs_changed = FALSE;
	return;
}


__private_extern__
void
do_net_apply(int argc, char * const argv[])
{
#pragma unused(argc)
#pragma unused(argv)
	if (!SCPreferencesApplyChanges(prefs)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
	}
	return;
}


__private_extern__
void
do_net_close(int argc, char * const argv[])
{
	if (_prefs_commitRequired(argc, argv, "close")) {
		return;
	}

	_net_close();
	_prefs_close();

	return;
}


__private_extern__
void
do_net_quit(int argc, char * const argv[])
{
	if (_prefs_commitRequired(argc, argv, "quit")) {
		return;
	}

	_net_close();
	_prefs_close();

	termRequested = TRUE;
	return;
}


/* -------------------- */


typedef void (*net_func) (int argc, char * const argv[]);

static const struct {
	char		*key;
	net_func	create;
	net_func	disable;
	net_func	enable;
	net_func	select;
	net_func	set;
	net_func	show;
	net_func	remove;
} net_keys[] = {

	{ "interfaces", NULL            , NULL            , NULL            ,
			NULL            , NULL            , show_interfaces ,
			NULL                                                },

	{ "interface",  create_interface, NULL            , NULL            ,
			select_interface, set_interface   , show_interface  ,
			NULL                                                },

	{ "services",   NULL            , NULL            , NULL            ,
			NULL            , NULL            , show_services   ,
			NULL                                                },

	{ "service",    create_service  , disable_service , enable_service  ,
			select_service  , set_service     , show_service    ,
			remove_service                                      },

	{ "protocols",  NULL            , NULL            , NULL            ,
			NULL            , NULL            , show_protocols  ,
			NULL                                                },

	{ "protocol",   create_protocol , disable_protocol, enable_protocol ,
			select_protocol , set_protocol    , show_protocol   ,
			remove_protocol                                     },

	{ "sets",       NULL            , NULL            , NULL            ,
			NULL            , NULL            , show_sets       ,
			NULL                                                },

	{ "set",        create_set      , NULL            , NULL            ,
			select_set      , set_set         , show_set        ,
			remove_set                                          }

};
#define	N_NET_KEYS	(sizeof(net_keys) / sizeof(net_keys[0]))


static int
findNetKey(char *key)
{
	int	i;

	for (i = 0; i < (int)N_NET_KEYS; i++) {
		if (strcmp(key, net_keys[i].key) == 0) {
			return i;
		}
	}

	return -1;
}


/* -------------------- */


__private_extern__
void
do_net_create(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("create what?\n"));
		return;
	}

	if (net_keys[i].create == NULL) {
		SCPrint(TRUE, stderr, CFSTR("create what?\n"));
		return;
	}

	(*net_keys[i].create)(argc, argv);
	return;
}


__private_extern__
void
do_net_disable(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("disable what?\n"));
		return;
	}

	if (net_keys[i].disable == NULL) {
		SCPrint(TRUE, stderr, CFSTR("disable what?\n"));
		return;
	}

	(*net_keys[i].disable)(argc, argv);
	return;
}


__private_extern__
void
do_net_enable(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("enable what?\n"));
		return;
	}

	if (net_keys[i].enable == NULL) {
		SCPrint(TRUE, stderr, CFSTR("enable what?\n"));
		return;
	}

	(*net_keys[i].enable)(argc, argv);
	return;
}


static void
do_net_migrate_perform(int argc, char * const argv[])
{
	char * sourceConfiguration = NULL;
	char * targetConfiguration = NULL;
	char * currentConfiguration = NULL;
	CFStringRef str = NULL;
	CFURLRef sourceConfigurationURL = NULL;
	CFURLRef targetConfigurationURL = NULL;
	CFURLRef currentConfigurationURL = NULL;
	CFArrayRef migrationFiles = NULL;

	sourceConfiguration = argv[0];
	targetConfiguration = argv[1];

	if (argc == 3) {
		currentConfiguration = argv[2];
	}

	SCPrint(_sc_debug, stdout,
		CFSTR("sourceConfiguration: %s\n"
		      "targetConfiguration: %s\n"
		      "currentConfiguration: %s\n"),
		sourceConfiguration,
		targetConfiguration,
		(currentConfiguration != NULL) ? currentConfiguration : "<current system>" );

	str = CFStringCreateWithCString(NULL, sourceConfiguration, kCFStringEncodingUTF8);
	sourceConfigurationURL = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, TRUE);
	CFRelease(str);

	str = CFStringCreateWithCString(NULL, targetConfiguration, kCFStringEncodingUTF8);
	targetConfigurationURL = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, TRUE);
	CFRelease(str);

	if (currentConfiguration != NULL) {
		str = CFStringCreateWithCString(NULL, currentConfiguration, kCFStringEncodingUTF8);
		currentConfigurationURL = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, TRUE);
		CFRelease(str);
	}

	SCPrint(TRUE, stdout, CFSTR("Start migration\n"));
	migrationFiles = _SCNetworkConfigurationPerformMigration(sourceConfigurationURL, currentConfigurationURL, targetConfigurationURL, NULL);
	if (migrationFiles != NULL) {
		SCPrint(TRUE, stdout, CFSTR("Migration complete\n"));
		SCPrint(_sc_debug, stdout, CFSTR("updated files: %@\n"), migrationFiles);
	} else {
		SCPrint(TRUE, stdout, CFSTR("Migration failed\n"));
	}

	if (sourceConfigurationURL != NULL) {
		CFRelease(sourceConfigurationURL);
	}
	if (targetConfigurationURL != NULL) {
		CFRelease(targetConfigurationURL);
	}
	if (currentConfigurationURL != NULL) {
		CFRelease(currentConfigurationURL);
	}
	if (migrationFiles != NULL) {
		CFRelease(migrationFiles);
	}
}


static void
do_net_migrate_validate(int argc, char * const argv[])
{
#pragma unused(argc)
	char *configuration = NULL;
	CFURLRef configurationURL = NULL;
	char *expectedConfiguration = NULL;
	CFURLRef expectedConfigurationURL = NULL;
	Boolean isValid = FALSE;
	CFStringRef str = NULL;

	configuration = argv[0];
	str = CFStringCreateWithCString(NULL, configuration, kCFStringEncodingUTF8);
	configurationURL = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, TRUE);
	CFRelease(str);

	expectedConfiguration = argv[1];
	str = CFStringCreateWithCString(NULL, expectedConfiguration, kCFStringEncodingUTF8);
	expectedConfigurationURL = CFURLCreateWithFileSystemPath(NULL, str, kCFURLPOSIXPathStyle, TRUE);
	CFRelease(str);

	isValid = _SCNetworkMigrationAreConfigurationsIdentical(configurationURL, expectedConfigurationURL);

	SCPrint(TRUE, stdout, CFSTR("Configuration at location %s %s\n"), configuration, isValid ? "is valid" : "is NOT valid");

	if (configurationURL != NULL) {
		CFRelease(configurationURL);
	}
	if (expectedConfigurationURL != NULL) {
		CFRelease(expectedConfigurationURL);
	}
}


__private_extern__
void
do_net_migrate(int argc, char * const argv[])
{
	char	*key;

	key = argv[0];
	argv++;
	argc--;

	if (strcmp(key, "perform") == 0) {
		do_net_migrate_perform(argc, argv);
	} else if (strcmp(key, "validate") == 0) {
		do_net_migrate_validate(argc, argv);
	} else {
		SCPrint(TRUE, stderr, CFSTR("migrate what?\n"));
		return;
	}

}


__private_extern__
void
do_net_remove(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("remove what?\n"));
		return;
	}

	if (net_keys[i].remove == NULL) {
		SCPrint(TRUE, stderr, CFSTR("remove what?\n"));
		return;
	}

	(*net_keys[i].remove)(argc, argv);
	return;
}


__private_extern__
void
do_net_select(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("select what?\n"));
		return;
	}

	if (*net_keys[i].select == NULL) {
		SCPrint(TRUE, stderr, CFSTR("select what?\n"));
		return;
	}

	(*net_keys[i].select)(argc, argv);
	return;
}


__private_extern__
void
do_net_set(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("set what?\n"));
		return;
	}

	(*net_keys[i].set)(argc, argv);
	return;
}


__private_extern__
void
do_net_show(int argc, char * const argv[])
{
	char	*key;
	int	i;

	key = argv[0];
	argv++;
	argc--;

	i = findNetKey(key);
	if (i < 0) {
		SCPrint(TRUE, stderr, CFSTR("show what?\n"));
		return;
	}

	(*net_keys[i].show)(argc, argv);
	return;
}


__private_extern__
void
do_net_clean(int argc, char * const argv[])
{
#pragma unused(argc)
#pragma unused(argv)
	Boolean		updated;

	if (prefs == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network configuration not open\n"));
		return;
	}

	if (ni_prefs == NULL) {
		ni_prefs = SCPreferencesCreateCompanion(prefs, INTERFACES_DEFAULT_CONFIG);
		if (ni_prefs == NULL) {
			SC_log(LOG_NOTICE, "SCPreferencesCreate( <NetworkInterfaces.plist> ) failed: %s", SCErrorString(SCError()));
		}
	}

	updated = __SCNetworkConfigurationClean(prefs, ni_prefs);

	if (updated) {
		SCPrint(TRUE, stdout,
			CFSTR("network configuration updated, use \"commit\" to save\n"));
		if ((prefsPath == NULL) || (strcmp(prefsPath, PREFS_DEFAULT_CONFIG_PLIST) == 0)) {
			SCPrint(TRUE, stdout,
				CFSTR( "\n"
				      "NOTE: because you have modified the system's \"live\" network configuration,\n"
				      "      a <reboot> is also REQUIRED.\n"
				      "\n"));
		}
		_prefs_changed = TRUE;
	} else {
		SCPrint(TRUE, stdout, CFSTR("network configuration not updated\n"));
	}

	return;
}


__private_extern__
void
do_net_update(int argc, char * const argv[])
{
#pragma unused(argc)
#pragma unused(argv)
	SCNetworkSetRef	set;
	Boolean		setCreated	= FALSE;
	Boolean		setUpdated	= FALSE;

	if (prefs == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network configuration not open\n"));
		return;
	}

	if (net_set != NULL) {
		set = CFRetain(net_set);
	} else {
		set = SCNetworkSetCopyCurrent(prefs);
		if (set == NULL) {
			// if no "current" set, create a new/default ("Automatic") set
			set = _SCNetworkSetCreateDefault(prefs);
			if (set == NULL) {
				SCPrint(TRUE, stdout,
					CFSTR("could not initialize \"Automatic\" set: %s\n"),
					SCErrorString(SCError()));
				return;
			}

			if (net_set != NULL) CFRelease(net_set);
			net_set = set;
			CFRetain(set);

			setCreated = TRUE;

			if (sets != NULL) {
				CFRelease(sets);
				sets = NULL;
			}
		}
	}

	setUpdated = SCNetworkSetEstablishDefaultConfiguration(set);
	if (setUpdated) {
		CFStringRef	setName;

		_prefs_changed = TRUE;

		setName = SCNetworkSetGetName(set);
		if (setName != NULL) {
			SCPrint(TRUE, stdout,
				CFSTR("set \"%@\" (%@) %supdated\n"),
				setName,
				SCNetworkSetGetSetID(set),
				setCreated ? "created, selected, and " : "");
		} else {
			SCPrint(TRUE, stdout,
				CFSTR("set ID \"%@\" %supdated\n"),
				SCNetworkSetGetSetID(set),
				setCreated ? "created, selected, and " : "");
		}
		_prefs_changed = TRUE;
	}

	CFRelease(set);
	return;
}


__private_extern__
void
do_net_upgrade(int argc, char * const argv[])
{
	Boolean		do_commit	= FALSE;
	Boolean		upgraded;

	if (prefs == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network configuration not open\n"));
		return;
	}

	if (prefsPath != NULL) {
		const char	*prefs_plist;

		prefs_plist = strrchr(prefsPath, '/');
		if (prefs_plist != NULL) {
			prefs_plist++;
		} else {
			prefs_plist = prefsPath;
		}

		if (strcmp(prefs_plist, PREFS_DEFAULT_CONFIG_PLIST) != 0) {
			SCPrint(TRUE, stdout, CFSTR("not updating a \"preferences.plist\" file\n"));
			return;
		}
	}

	if (argc > 0) {
		if (strcmp(argv[0], PREFS_DEFAULT_CONFIG_PLIST) == 0) {
			upgraded = __SCNetworkConfigurationUpgrade(&prefs, NULL, do_commit);
		} else if (strcmp(argv[0], INTERFACES_DEFAULT_CONFIG_PLIST) == 0) {
			if (ni_prefs == NULL) {
				ni_prefs = SCPreferencesCreateCompanion(prefs, INTERFACES_DEFAULT_CONFIG);
				if (ni_prefs == NULL) {
					SC_log(LOG_NOTICE, "SCPreferencesCreate( <NetworkInterfaces.plist> ) failed: %s", SCErrorString(SCError()));
				}
			}

			do_commit = TRUE;	// using alternate, created on-the-fly preferences.plist

			upgraded = __SCNetworkConfigurationUpgrade(NULL, &ni_prefs, do_commit);
		} else {
			SCPrint(TRUE, stdout, CFSTR("invalid .plist (\"preferences.plist\", \"NetworkInterfaces.plist\"\n"));
			return;
		}
	} else {
		upgraded = __SCNetworkConfigurationUpgrade(&prefs, &ni_prefs, do_commit);
	}

	if (upgraded) {
		SCPrint(TRUE, stdout,
			do_commit ? CFSTR("network configuration upgraded/saved\n")
				  : CFSTR("network configuration upgraded, use \"commit\" to save\n"));
		if ((prefsPath == NULL) || (strcmp(prefsPath, PREFS_DEFAULT_CONFIG_PLIST) == 0)) {
			SCPrint(TRUE, stdout,
				do_commit ? CFSTR("\n"
						  "NOTE: because you have modified the system's \"live\" network configuration,\n"
						  "      a <reboot> is also REQUIRED.\n"
						  "\n")

					  : CFSTR("\n"
						  "NOTE: because you are modifying the system's \"live\" network configuration,\n"
						  "      a <reboot> will also be REQUIRED.\n"
						  "\n")
				);
		}

		if (!do_commit) {
			_prefs_changed = TRUE;
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("network configuration upgrade not needed\n"));
	}

	return;
}


#include "SCPreferencesInternal.h"
#include <fcntl.h>
#include <unistd.h>
__private_extern__
void
do_net_snapshot(int argc, char * const argv[])
{
#pragma unused(argc)
#pragma unused(argv)
	if (prefs == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network configuration not open\n"));
		return;
	}

	if (prefs != NULL) {
		SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

		if (prefsPrivate->prefs != NULL) {
			int		fd;
			static int	n_snapshot	= 0;
			char		*path;
			CFDataRef	xmlData;

			asprintf(&path, "/tmp/prefs_snapshot_%d", n_snapshot++);
			(void)unlink(path);
			fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, 0644);
			free(path);
			if (fd == -1) {
				SCPrint(TRUE, stdout, CFSTR("could not write snapshot: open() failed : %s\n"), strerror(errno));
				return;
			}

			xmlData = CFPropertyListCreateData(NULL, prefsPrivate->prefs, kCFPropertyListXMLFormat_v1_0, 0, NULL);
			if (xmlData != NULL) {
				(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
				CFRelease(xmlData);
			} else {
				SCPrint(TRUE, stdout, CFSTR("could not write snapshot: CFPropertyListCreateData() failed\n"));
			}

			(void) close(fd);
		} else {
			SCPrint(TRUE, stdout, CFSTR("prefs have not been accessed\n"));
		}
	}

	return;
}

__private_extern__
void
do_configuration(int argc, char **argv)
{
	const char		*description	= NULL;
	SCPreferencesRef	ni_prefs;
	CFStringRef		path		= NULL;
	SCPreferencesRef	prefs;
	int			_sc_log_save;

	// scutil --configuration
	// scutil --configuration <path-to-preferences.plist>
	// scutil --configuraiton <description> <path-to-preferences.plist>

	if (argc > 0) {
		if (argc > 1) {
			description = argv[0];
		}
		path = CFStringCreateWithCString(NULL,
						 argc > 1 ? argv[1] : argv[0],
						 kCFStringEncodingUTF8);
	}
	prefs = SCPreferencesCreate(NULL, CFSTR("scutil --configuration"), path);
	ni_prefs = SCPreferencesCreateCompanion(prefs, INTERFACES_DEFAULT_CONFIG);

	_sc_log_save = _sc_log;
	_sc_log = kSCLogDestinationFile;
	__SCNetworkConfigurationReport(LOG_NOTICE, description, prefs, ni_prefs);
	_sc_log = _sc_log_save;

	if (path != NULL) CFRelease(path);
	CFRelease(prefs);
	CFRelease(ni_prefs);

	return;
}
