// libcups.c - the CUPS client calls print dialogs make, on a system with no print server
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// there are no destinations, connections fail and requests get no response
// options and IPP attribute lists are real, since callers build them before asking the server

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

typedef struct cups_option_s {
    char *name;
    char *value;
} cups_option_t;

typedef struct cups_dest_s {
    char *name, *instance;
    int is_default;
    int num_options;
    cups_option_t *options;
} cups_dest_t;

typedef struct _http_s http_t;
typedef union _http_addr_u http_addr_t;
typedef struct _cups_dinfo_s cups_dinfo_t;
typedef struct cups_size_s cups_size_t;
typedef int (*cups_dest_cb_t)(void *user_data, unsigned flags, cups_dest_t *dest);
typedef int ipp_op_t, ipp_tag_t;

typedef struct ipp_attribute_s {
    struct ipp_attribute_s *next;
    ipp_tag_t group_tag, value_tag;
    char *name;
    int num_values;
    char **values;
} ipp_attribute_t;

typedef struct _ipp_s {
    ipp_op_t op;
    int request_id;
    ipp_attribute_t *attrs, *last;
} ipp_t;

enum { IPP_STATUS_ERROR_NOT_FOUND = 0x0406, IPP_STATUS_ERROR_SERVICE_UNAVAILABLE = 0x0503 };

static int sLastError;

int cupsLastError(void)
{
    return sLastError;
}

const char *cupsServer(void)
{
    const char *server = getenv("CUPS_SERVER");
    return server && *server ? server : "/private/var/run/cupsd";
}

int ippPort(void)
{
    return 631;
}

// no destinations exist
int cupsGetDests(cups_dest_t **dests)
{
    if (dests)
        *dests = NULL;
    return 0;
}

int cupsGetDests2(http_t *http, cups_dest_t **dests)
{
    (void)http;
    return cupsGetDests(dests);
}

cups_dest_t *cupsGetNamedDest(http_t *http, const char *name, const char *instance)
{
    (void)http;
    (void)name;
    (void)instance;
    sLastError = IPP_STATUS_ERROR_NOT_FOUND;
    return NULL;
}

// enumeration completes successfully with nothing to report
int cupsEnumDests(unsigned flags, int msec, int *cancel, unsigned type, unsigned mask, cups_dest_cb_t cb, void *user_data)
{
    (void)flags;
    (void)msec;
    (void)cancel;
    (void)type;
    (void)mask;
    (void)cb;
    (void)user_data;
    return 1;
}

static char *copyString(const char *s)
{
    return s ? strdup(s) : NULL;
}

void cupsFreeOptions(int num_options, cups_option_t *options)
{
    for (int i = 0; options && i < num_options; i++) {
        free(options[i].name);
        free(options[i].value);
    }
    free(options);
}

void cupsFreeDests(int num_dests, cups_dest_t *dests)
{
    for (int i = 0; dests && i < num_dests; i++) {
        free(dests[i].name);
        free(dests[i].instance);
        cupsFreeOptions(dests[i].num_options, dests[i].options);
    }
    free(dests);
}

const char *cupsGetOption(const char *name, int num_options, cups_option_t *options)
{
    for (int i = 0; name && options && i < num_options; i++) {
        if (strcasecmp(options[i].name, name) == 0)
            return options[i].value;
    }
    return NULL;
}

int cupsCopyDest(cups_dest_t *dest, int num_dests, cups_dest_t **dests)
{
    if (dest == NULL || dests == NULL)
        return num_dests;
    cups_dest_t *list = realloc(*dests, (size_t)(num_dests + 1) * sizeof(cups_dest_t));
    if (list == NULL)
        return num_dests;
    cups_dest_t *d = &list[num_dests];
    d->name = copyString(dest->name);
    d->instance = copyString(dest->instance);
    d->is_default = dest->is_default;
    d->num_options = 0;
    d->options = dest->num_options ? calloc((size_t)dest->num_options, sizeof(cups_option_t)) : NULL;
    for (int i = 0; d->options && i < dest->num_options; i++) {
        d->options[i].name = copyString(dest->options[i].name);
        d->options[i].value = copyString(dest->options[i].value);
        d->num_options++;
    }
    *dests = list;
    return num_dests + 1;
}

http_t *cupsConnectDest(cups_dest_t *dest, unsigned flags, int msec, int *cancel, char *resource, size_t resourcesize,
                        cups_dest_cb_t cb, void *user_data)
{
    (void)dest;
    (void)flags;
    (void)msec;
    (void)cancel;
    (void)cb;
    (void)user_data;
    if (resource && resourcesize)
        resource[0] = 0;
    sLastError = IPP_STATUS_ERROR_SERVICE_UNAVAILABLE;
    return NULL;
}

cups_dinfo_t *cupsCopyDestInfo(http_t *http, cups_dest_t *dest)
{
    (void)http;
    (void)dest;
    sLastError = IPP_STATUS_ERROR_SERVICE_UNAVAILABLE;
    return NULL;
}

void cupsFreeDestInfo(cups_dinfo_t *dinfo)
{
    free(dinfo);
}

int cupsCheckDestSupported(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, const char *option, const char *value)
{
    (void)http;
    (void)dest;
    (void)dinfo;
    (void)option;
    (void)value;
    return 0;
}

int cupsGetDestMediaCount(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, unsigned flags)
{
    (void)http;
    (void)dest;
    (void)dinfo;
    (void)flags;
    return 0;
}

int cupsGetDestMediaByIndex(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, int n, unsigned flags, cups_size_t *size)
{
    (void)http;
    (void)dest;
    (void)dinfo;
    (void)n;
    (void)flags;
    (void)size;
    return 0;
}

int cupsGetDestMediaDefault(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, unsigned flags, cups_size_t *size)
{
    return cupsGetDestMediaByIndex(http, dest, dinfo, 0, flags, size);
}

const char *cupsLocalizeDestMedia(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, unsigned flags, cups_size_t *size)
{
    (void)http;
    (void)dest;
    (void)dinfo;
    (void)flags;
    (void)size;
    return NULL;
}

// connections are never made, so any http_t here is NULL or the caller's mistake
void httpClose(http_t *http)
{
    (void)http;
}

http_addr_t *httpGetAddress(http_t *http)
{
    (void)http;
    return NULL;
}

int httpAddrPort(http_addr_t *addr)
{
    return addr ? -1 : ippPort();
}

const char *httpGetHostname(http_t *http, char *s, int slen)
{
    (void)http;
    if (s == NULL || slen <= 0)
        return NULL;
    if (gethostname(s, (size_t)slen) != 0)
        return NULL;
    s[slen - 1] = 0;
    return s;
}

// IPP messages: attributes kept in order, each a name and its string values
ipp_t *ippNewRequest(ipp_op_t op)
{
    static int requestID;
    ipp_t *ipp = calloc(1, sizeof(*ipp));
    if (ipp) {
        ipp->op = op;
        ipp->request_id = ++requestID;
    }
    return ipp;
}

void ippDelete(ipp_t *ipp)
{
    if (ipp == NULL)
        return;
    for (ipp_attribute_t *a = ipp->attrs, *next; a; a = next) {
        next = a->next;
        for (int i = 0; i < a->num_values; i++)
            free(a->values[i]);
        free(a->values);
        free(a->name);
        free(a);
    }
    free(ipp);
}

ipp_attribute_t *ippAddStrings(ipp_t *ipp, ipp_tag_t group, ipp_tag_t value_tag, const char *name, int num_values,
                               const char *language, const char *const *values)
{
    (void)language;
    if (ipp == NULL || name == NULL || num_values < 1)
        return NULL;
    ipp_attribute_t *a = calloc(1, sizeof(*a));
    if (a == NULL)
        return NULL;
    a->group_tag = group;
    a->value_tag = value_tag;
    a->name = strdup(name);
    a->values = calloc((size_t)num_values, sizeof(char *));
    for (int i = 0; a->values && i < num_values; i++)
        a->values[i] = copyString(values ? values[i] : NULL);
    a->num_values = a->values ? num_values : 0;
    if (ipp->last)
        ipp->last->next = a;
    else
        ipp->attrs = a;
    ipp->last = a;
    return a;
}

ipp_attribute_t *ippAddString(ipp_t *ipp, ipp_tag_t group, ipp_tag_t value_tag, const char *name, const char *language,
                              const char *value)
{
    return ippAddStrings(ipp, group, value_tag, name, 1, language, &value);
}

ipp_attribute_t *ippFindAttribute(ipp_t *ipp, const char *name, ipp_tag_t type)
{
    (void)type;
    for (ipp_attribute_t *a = ipp && name ? ipp->attrs : NULL; a; a = a->next) {
        if (strcmp(a->name, name) == 0)
            return a;
    }
    return NULL;
}

int ippGetCount(ipp_attribute_t *attr)
{
    return attr ? attr->num_values : 0;
}

const char *ippGetString(ipp_attribute_t *attr, int element, const char **language)
{
    if (language)
        *language = NULL;
    return attr && element >= 0 && element < attr->num_values ? attr->values[element] : NULL;
}

// there is no server to send a request to, the request is consumed as cupsDoRequest does
ipp_t *cupsDoRequest(http_t *http, ipp_t *request, const char *resource)
{
    (void)http;
    (void)resource;
    ippDelete(request);
    sLastError = IPP_STATUS_ERROR_SERVICE_UNAVAILABLE;
    return NULL;
}
