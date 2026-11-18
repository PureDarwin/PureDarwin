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

#endif
