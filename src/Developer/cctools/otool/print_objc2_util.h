//
//  print_objc2_util.h
//  otool-classic
//
//  Created by Michael Trent on 9/20/19.
//

#ifndef print_objc2_util_h
#define print_objc2_util_h

#include <stdint.h>

#include "stuff/bool.h"

#define MAXINDENT 10
struct indent {
    uint32_t level;
    uint32_t widths[MAXINDENT];
};

/*
 * indent_reset() sets struct indent to stable initial values.
 */
void indent_reset(
    struct indent* indent);

/*
 * indent_push() adjust the indent's state by the width value. Widths are stored
 * in a stack so that indent levels can easily be popped off.
 */
void indent_push(
    struct indent* indent,
    uint32_t width);

/*
 * indent_pop() pops an indent level and width pushed onto the stack by
 * indent_push().
 */
void indent_pop(
    struct indent* indent);

/*
 * print_field_label() prints a formatted label. the label is indented to fit
 * within the current indent state. A single space character will follow the
 * label so that the next value can simply be printed.
 */
void print_field_label(
    struct indent* indent,
    const char* label,
    ...);

/*
 * print_field_scalar() prints a label followed by a formatted value. the label
 * is idented to fit within the current indent state.
 */
void print_field_scalar(
    struct indent* indent,
    const char* label,
    const char* fmt,...);

/*
* warn_about_zerofill prints a warning message when a pointer to Objective-C
* data resides in a zerofill section. If indentFlag is true the warning is
* properly aligned to the indent level, otherwise, it draws in place.
*/
void warn_about_zerofill(
    const char* segname,
    const char* sectname,
    const char* typename,
    struct indent* indent,
    enum bool indentFlag,
    enum bool newline);

#endif /* print_objc2_util_h */
