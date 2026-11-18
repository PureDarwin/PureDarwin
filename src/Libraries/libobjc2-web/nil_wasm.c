// nil_wasm.c - messages to nil for libobjc2 on wasm, answered by a zero method of the call's exact type
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MIT

// wasm traps on any indirect call whose type differs from the function's, so one nil method cannot
// answer every message. The selector's type encoding gives the call's wasm signature, the method for it
// is made once with emscripten's addFunction and kept.
#include "objc/runtime.h"
#include "objc/encoding.h"
#include "objc/slot.h"
#include "method.h"
#include "selector.h"

#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

// a zero of the signature's return type, clearing a returned struct when the first parameter points at one
EM_JS(int, nil_wasm_make, (const char *signature, int sret_size), {
    var sig = UTF8ToString(signature);
    var big = sig[0] == 'j';
    return addFunction(function(first) {
        if (sret_size)
            HEAPU8.fill(0, first, first + sret_size);
        return big ? BigInt(0) : 0;
    }, sig);
});

static const char *skip_qualifiers(const char *t)
{
    while (*t && strchr("rnNoORVA", *t))
        t++;
    return t;
}

static const char *skip_offset(const char *t)
{
    while (*t >= '0' && *t <= '9')
        t++;
    return t;
}

// the wasm type of a scalar encoding, 0 for an aggregate
static char scalar(const char *t)
{
    switch (*skip_qualifiers(t)) {
    case 'q': case 'Q': return 'j';
    case 'f': return 'f';
    case 'd': return 'd';
    case '{': case '(': case 'D': return 0;
    default: return 'i';
    }
}

// clang's wasm32 ABI passes and returns a struct of one scalar as that scalar, everything else in memory
static char single_element(const char *t)
{
    t = skip_qualifiers(t);
    if (*t != '{')
        return scalar(t);
    const char *fields = strchr(t, '=');
    if (fields == NULL)
        return 0;
    fields++;
    if (*fields == '}')
        return 0;
    const char *next = objc_skip_typespec(fields);
    if (*next != '}')
        return 0;
    return single_element(fields);
}

// one per signature, never freed: callers cache the slots they are handed
struct nil_entry {
    struct nil_entry *next;
    char signature[64];
    int sret;
    struct objc_slot slot;
    struct objc_method method;
};

static struct nil_entry *entries;

static struct nil_entry *nil_entry_for(SEL selector)
{
    const char *types = sel_getType_np(selector);
    char sig[64];
    int n = 0, sret = 0;

    char untyped[40];
    if (types == NULL) {
        // an untyped selector says how many arguments it takes in its colons, each one taken as a word
        const char *name = sel_getName(selector);
        int n = 0;
        untyped[n++] = '@';
        untyped[n++] = '@';
        untyped[n++] = ':';
        for (const char *c = name; c != NULL && *c != '\0' && n < (int)sizeof(untyped) - 1; c++)
            if (*c == ':')
                untyped[n++] = '@';
        untyped[n] = '\0';
        types = untyped;
    }
    const char *ret = skip_qualifiers(types);
    char r = *ret == 'v' ? 'v' : single_element(ret);
    if (r == 0) {
        // returned in memory: the caller passes where, and the function returns nothing
        sret = (int)objc_sizeof_type(ret);
        sig[n++] = 'v';
        sig[n++] = 'i';
    } else {
        sig[n++] = r;
    }
    for (const char *t = skip_offset(objc_skip_typespec(ret)); *t && n < (int)sizeof(sig) - 3;
         t = skip_offset(objc_skip_typespec(t))) {
        t = skip_qualifiers(t);
        if (*t == 'D') {
            // a long double argument is two 64-bit halves
            sig[n++] = 'j';
            sig[n++] = 'j';
            continue;
        }
        char a = single_element(t);
        sig[n++] = a ? a : 'i';
    }
    sig[n] = 0;

    for (struct nil_entry *e = entries; e; e = e->next)
        if (e->sret == sret && strcmp(e->signature, sig) == 0)
            return e;

    struct nil_entry *e = calloc(1, sizeof(*e));
    if (e == NULL)
        abort();
    strcpy(e->signature, sig);
    e->sret = sret;
    IMP imp = (IMP)(intptr_t)nil_wasm_make(sig, sret);
    e->slot.method = imp;
    e->slot.version = 1;
    e->method.imp = imp;
    e->next = entries;
    entries = e;
    return e;
}

struct objc_slot *__objc_wasm_nil_slot(SEL selector)
{
    return &nil_entry_for(selector)->slot;
}

struct objc_method *__objc_wasm_nil_method(SEL selector)
{
    return &nil_entry_for(selector)->method;
}

#include <stdio.h>

// a message the receiver does not implement, with no forwarding installed: logged once per selector and answered
// with zero of the call's own type, where Darwin would raise
struct objc_slot *__objc_wasm_unknown_slot(id receiver, SEL selector)
{
    struct nil_entry *e = nil_entry_for(selector);
    static const char *seen[256];
    static int nseen;
    const char *name = sel_getName(selector);
    int logged = 0;

    for (int i = 0; i < nseen; i++)
        if (seen[i] == name)
            logged = 1;
    if (!logged) {
        if (nseen < (int)(sizeof(seen) / sizeof(seen[0])))
            seen[nseen++] = name;
        fprintf(stderr, "objc: %c[%s %s] is not implemented, answering zero\n",
                class_isMetaClass(object_getClass(receiver)) ? '+' : '-', object_getClassName(receiver), name);
    }
    return &e->slot;
}

IMP __objc_wasm_nil_imp(SEL selector)
{
    return nil_entry_for(selector)->method.imp;
}
