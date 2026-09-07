/*
 * These legacy constant names are declared CF_EXPORT in the public
 * CFLocale.h/CFDateFormatter.h headers in this same vendored source tree,
 * but are never actually defined anywhere in the vendored .c files -
 * apparently dropped in this swift-corelibs-foundation port while the
 * "...Key" suffixed replacements they alias were kept. Call sites elsewhere
 * in CF (CFDateIntervalFormatter.c, CFCalendar_Enumerate.c, CFDateFormatter.c)
 * confirm the intended values by usage: dictionary-key roles matching the
 * "...Key" constants, and a calendar-identifier role for kCFGregorianCalendar
 * matching kCFCalendarIdentifierGregorian.
 *
 * A plain "const CFStringRef kCFLocaleCalendar = kCFLocaleCalendarKey;"
 * isn't a valid C static initializer here: kCFLocaleCalendarKey is itself
 * an extern global, and copying another global's *value* (as opposed to
 * taking its address) isn't a constant expression, so this has to run as
 * load-time initialization instead.
 *
 * An uninitialized "const CFLocaleKey kCFLocaleCalendar;" tentative
 * definition lands in __TEXT,__const (read-only, r-x segment permissions -
 * confirmed via objdump) rather than __DATA/bss, so the constructor's write
 * through a cast pointer faulted (SIGSEGV, write to a read-only page) the
 * first time this actually ran. Force real writable storage with an
 * explicit section attribute instead of relying on how the linker happens
 * to place a const tentative definition.
 */

#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFDateFormatter.h>
#include <CoreFoundation/CFCalendar.h>
#include "CFLocaleInternal.h"

#define PD_WRITABLE_CONST __attribute__((section("__DATA,__data")))

PD_WRITABLE_CONST const CFLocaleKey kCFLocaleCalendar;
PD_WRITABLE_CONST const CFLocaleKey kCFLocaleLanguageCode;
PD_WRITABLE_CONST const CFLocaleKey kCFLocaleCountryCode;
PD_WRITABLE_CONST const CFLocaleKey kCFLocaleScriptCode;
PD_WRITABLE_CONST const CFDateFormatterKey kCFDateFormatterCalendar;
PD_WRITABLE_CONST const CFDateFormatterKey kCFDateFormatterTimeZone;
PD_WRITABLE_CONST const CFCalendarIdentifier kCFGregorianCalendar;

__attribute__((constructor))
static void __CFLocalePureDarwinLegacyAliasesInit(void) {
    *(CFStringRef *)&kCFLocaleCalendar = kCFLocaleCalendarKey;
    *(CFStringRef *)&kCFLocaleLanguageCode = kCFLocaleLanguageCodeKey;
    *(CFStringRef *)&kCFLocaleCountryCode = kCFLocaleCountryCodeKey;
    *(CFStringRef *)&kCFLocaleScriptCode = kCFLocaleScriptCodeKey;
    *(CFStringRef *)&kCFDateFormatterCalendar = kCFDateFormatterCalendarKey;
    *(CFStringRef *)&kCFDateFormatterTimeZone = kCFDateFormatterTimeZoneKey;
    *(CFStringRef *)&kCFGregorianCalendar = kCFCalendarIdentifierGregorian;
}
