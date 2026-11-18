// CFObjCMessage.h - a typed objc_msgSend call that also works where the runtime has no objc_msgSend
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: Apache-2.0
#ifndef __COREFOUNDATION_CFOBJCMESSAGE__
#define __COREFOUNDATION_CFOBJCMESSAGE__

#include <objc/message.h>

/* CF_MSGSEND(type, receiver, selector, args...) is ((type)objc_msgSend)(receiver, selector, args...).
 * wasm checks the type of every indirect call, so a single objc_msgSend cannot stand in for every
 * method. There the method is looked up and called through a pointer of the right type, as clang
 * does for [receiver message] on the GNUstep ABI. */
#if defined(__EMSCRIPTEN__)
#define CF_MSGSEND(type, receiver, selector, ...) ({                                \
        id __cf_receiver = (id)(receiver);                                          \
        SEL __cf_selector = (selector);                                             \
        ((type)objc_msg_lookup(__cf_receiver, __cf_selector))(__cf_receiver, __cf_selector, ##__VA_ARGS__); })
#else
#define CF_MSGSEND(type, receiver, selector, ...) ((type)objc_msgSend)(receiver, selector, ##__VA_ARGS__)
#endif

/* CF_PERFORM0(receiver, selector) and CF_PERFORM1(receiver, selector, object) send an action-style message
 * whose method is not known until run time: a timer's, a notification observer's, performSelector:'s.
 * Elsewhere that is a void (id, SEL, id) call, as it always was. wasm traps a call through any type but the
 * method's own, so there the method's encoding picks the type and whatever it returns is dropped. */
#if defined(__EMSCRIPTEN__)
#include <objc/runtime.h>
#include <string.h>

static inline id _CFObjCPerform(id receiver, SEL selector, int count, id first, id second) {
    Method method = class_getInstanceMethod(object_getClass(receiver), selector);
    const char *types = method != NULL ? method_getTypeEncoding(method) : NULL;
    IMP imp = objc_msg_lookup(receiver, selector);

    while (types != NULL && *types != '\0' && strchr("rnNoORV", *types) != NULL)
        types++;
    // the method's own argument count, not the caller's: Darwin drops an object the method does not take
    if (method != NULL) {
        int taken = (int)method_getNumberOfArguments(method) - 2;
        if (taken >= 0 && taken < count)
            count = taken;
    }
    switch (types != NULL ? *types : '@') {
    case 'v':
        if (count == 0) ((void (*)(id, SEL))imp)(receiver, selector);
        else if (count == 1) ((void (*)(id, SEL, id))imp)(receiver, selector, first);
        else ((void (*)(id, SEL, id, id))imp)(receiver, selector, first, second);
        return nil;
    case 'f':
        if (count == 0) ((float (*)(id, SEL))imp)(receiver, selector);
        else if (count == 1) ((float (*)(id, SEL, id))imp)(receiver, selector, first);
        else ((float (*)(id, SEL, id, id))imp)(receiver, selector, first, second);
        return nil;
    case 'd':
        if (count == 0) ((double (*)(id, SEL))imp)(receiver, selector);
        else if (count == 1) ((double (*)(id, SEL, id))imp)(receiver, selector, first);
        else ((double (*)(id, SEL, id, id))imp)(receiver, selector, first, second);
        return nil;
    case 'q':
    case 'Q':
        if (count == 0) return (id)(intptr_t)((long long (*)(id, SEL))imp)(receiver, selector);
        if (count == 1) return (id)(intptr_t)((long long (*)(id, SEL, id))imp)(receiver, selector, first);
        return (id)(intptr_t)((long long (*)(id, SEL, id, id))imp)(receiver, selector, first, second);
    default:
        if (count == 0) return ((id (*)(id, SEL))imp)(receiver, selector);
        if (count == 1) return ((id (*)(id, SEL, id))imp)(receiver, selector, first);
        return ((id (*)(id, SEL, id, id))imp)(receiver, selector, first, second);
    }
}
#define CF_PERFORM0(receiver, selector) ((void)_CFObjCPerform((id)(receiver), (selector), 0, nil, nil))
#define CF_PERFORM1(receiver, selector, object) ((void)_CFObjCPerform((id)(receiver), (selector), 1, (id)(object), nil))
#define CF_PERFORM_RESULT(receiver, selector, count, first, second) _CFObjCPerform((id)(receiver), (selector), (count), (first), (second))
#else
#define CF_PERFORM0(receiver, selector) CF_MSGSEND(void (*)(id, SEL), receiver, selector)
#define CF_PERFORM1(receiver, selector, object) CF_MSGSEND(void (*)(id, SEL, id), receiver, selector, object)
#endif

#endif
