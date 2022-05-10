//
//  print_objc2_util.c
//  otool-classic
//
//  Created by Michael Trent on 9/20/19.
//

#include "print_objc2_util.h"

#include <stdarg.h>
#include <stdio.h>

void
indent_reset(
struct indent* indent)
{
    indent->level = 0;
    indent->widths[indent->level] = 0;
}

void
indent_push(
struct indent* indent,
uint32_t width)
{
    indent->level += 1;
    if (indent->level < MAXINDENT)
        indent->widths[indent->level] = width;
}

void
indent_pop(
struct indent* indent)
{
    if (indent->level)
        indent->level -= 1;
}

/*
 * print_field_scalar() prints a label followed by a formatted value. the label
 * is idented to fit within the info's indent state.
 */
void
print_field_scalar(
struct indent* indent,
const char* label,
const char* fmt,
...)
{
    /* print the label */
    print_field_label(indent, label);

    /* print the data, if any */
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
}

/*
 * print_field_label() prints a formatted label. the label is indented to fit
 * within the info's indent state. A single space character will follow the
 * label so that the next value can simply be printed.
 */
void
print_field_label(
struct indent* indent,
const char* label,
...)
{
    va_list ap;
    int width = 0;
    uint32_t label_indent;
    uint32_t label_width;

    /* get the current label field width from the indent state */
    label_indent = indent->level * 4;
#if 1
    /*
     * use the curent indent width. if the indent level is too deep, just print
     * the value immediately after the label.
     */
    label_width = (indent->level < MAXINDENT ?
                   indent->widths[indent->level] : 0);
#else
    /*
     * use the current indent width unless that would cause the value at this
     * level to print to the left of the previous value. In practice, we need
     * to loop over all the indent widths, compute the right edge of the label
     * field, and use the largest such value.
     */
    uint32_t right = 0;
    for (uint32_t i = 0; i < MAXINDENT; ++i) {
        if (i > indent->level)
            break;

        uint32_t r = i * 4 + indent->widths[i];
        if (r > right)
            right = r;
    }
    label_width = right - label_indent;
#endif

    /* measure the width of the string data */
    va_start(ap, label);
    if (label) {
        width = vsnprintf(NULL, 0, label, ap);
    }
    va_end(ap);

    /* adjust the width to represent the space following the label */
    width = width < label_width ? label_width - width : 0;

    /* print the indent spaces */
    printf("%*s", label_indent, "");

    /* print the label */
    if (label) {
        va_start(ap, label);
        vprintf(label, ap);
        va_end(ap);
    }

    /* print right padding */
    if (label)
        printf("%*s", width + 1, "");
}

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
enum bool newline)
{
    if (indentFlag)
        indent_push(indent, 0);
    print_field_label(indent,
                      "(%s contents stored in zerofill (%.16s,%.16s) section)",
                      typename, segname, sectname);
    if (newline)
        printf("\n");
    if (indentFlag)
        indent_pop(indent);
}
