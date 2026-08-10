#ifndef _SEC_SOSPLATFORM_H_
#define _SEC_SOSPLATFORM_H_

#include <TargetConditionals.h>

#if __has_include(<CrashReporterClient.h>)
#define HAVE_CRASHREPORTERCLIENT 1
#include <CrashReporterClient.h>
#else // __has_include(<CrashReporterClient.h>)
#define HAVE_CRASHREPORTERCLIENT 0
#endif // __has_include(<CrashReporterClient.h>)

#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#define HAVE_KEYBAG 1
#else // TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#define HAVE_KEYBAG 0
#endif // TARGET_OS_IOS && !TARGET_OS_SIMULATOR

#if HAVE_KEYBAG
#define CONFIG_ARM_AUTOACCEPT 1
#else // HAVE_KEYBAG
#define CONFIG_ARM_AUTOACCEPT 0
#endif // HAVE_KEYBAG

#endif /* _SEC_SOSPLATFORM_H_ */
