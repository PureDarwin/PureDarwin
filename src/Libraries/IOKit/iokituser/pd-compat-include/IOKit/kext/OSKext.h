/*
 * The one OSKext entry point IOCFPlugIn.c uses. Declared here so the plugin
 * loader does not drag in kext.subproj, which PureDarwin does not have.
 */

#ifndef _PD_IOKIT_KEXT_OSKEXT_H
#define _PD_IOKIT_KEXT_OSKEXT_H

#include <CoreFoundation/CoreFoundation.h>

CFArrayRef OSKextGetSystemExtensionsFolderURLs(void);

#endif /* _PD_IOKIT_KEXT_OSKEXT_H */
