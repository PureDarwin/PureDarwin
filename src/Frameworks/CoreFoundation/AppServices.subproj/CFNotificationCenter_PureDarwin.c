/*
 * CFNotificationCenter_PureDarwin.c - local, Darwin and distributed notification centers
 *
 * The local center delivers synchronously on the posting thread, as Apple's does. The Darwin
 * center rides notify(3), so names cross processes but objects and user info do not. There
 * is no distnoted yet, so the distributed center delivers within the process and also posts
 * the name through notify(3) for observers elsewhere.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include "CFNotificationCenter.h"
#include <dispatch/dispatch.h>

// notify(3) from libsystem_notify, its header is not on CoreFoundation's include path
extern uint32_t notify_post(const char *name);
extern uint32_t notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, void (^handler)(int token));
extern uint32_t notify_cancel(int token);
#include <os/lock.h>
#include <stdlib.h>

typedef enum { CenterLocal, CenterDarwin, CenterDistributed } CenterKind;

typedef struct {
    const void *observer;
    CFNotificationCallback callback;
    CFStringRef name;
    const void *object;
    int token;
    bool live;
} Observation;

struct __CFNotificationCenter {
    CFRuntimeBase base;
    CenterKind kind;
    os_unfair_lock lock;
    Observation *observations;
    CFIndex count;
    CFIndex capacity;
};

static const CFRuntimeClass centerClass = {
    0, "CFNotificationCenter", NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

CFTypeID CFNotificationCenterGetTypeID(void) {
    static CFTypeID typeID = _kCFRuntimeNotATypeID;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        typeID = _CFRuntimeRegisterClass(&centerClass);
    });
    return typeID;
}

static CFNotificationCenterRef createCenter(CenterKind kind) {
    struct __CFNotificationCenter *c = (struct __CFNotificationCenter *)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault,
        CFNotificationCenterGetTypeID(), sizeof(struct __CFNotificationCenter) - sizeof(CFRuntimeBase), NULL);

    c->kind = kind;
    c->lock = OS_UNFAIR_LOCK_INIT;
    return c;
}

CFNotificationCenterRef CFNotificationCenterGetLocalCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ center = createCenter(CenterLocal); });
    return center;
}

CFNotificationCenterRef CFNotificationCenterGetDarwinNotifyCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ center = createCenter(CenterDarwin); });
    return center;
}

CFNotificationCenterRef CFNotificationCenterGetDistributedCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ center = createCenter(CenterDistributed); });
    return center;
}

static char *copyCName(CFStringRef name, const char *prefix) {
    CFIndex max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + strlen(prefix) + 1;
    char *s = malloc(max);

    strcpy(s, prefix);
    if (!CFStringGetCString(name, s + strlen(prefix), max - strlen(prefix), kCFStringEncodingUTF8)) {
        free(s);
        return NULL;
    }
    return s;
}

// cross-process names, Darwin ones verbatim, distributed ones in their own namespace
static const char *notifyPrefix(CenterKind kind) {
    return kind == CenterDistributed ? "org.puredarwin.distnote." : "";
}

void CFNotificationCenterAddObserver(CFNotificationCenterRef center, const void *observer, CFNotificationCallback callBack, CFStringRef name, const void *object, CFNotificationSuspensionBehavior suspensionBehavior) {
    struct __CFNotificationCenter *c = (struct __CFNotificationCenter *)center;
    Observation o = { observer, callBack, name ? CFStringCreateCopy(NULL, name) : NULL, object, -1, true };

    if (c == NULL || callBack == NULL) {
        if (o.name) CFRelease(o.name);
        return;
    }
    if (c->kind == CenterDarwin && name == NULL) {
        // Apple requires a name on the Darwin center
        return;
    }
    if (c->kind != CenterLocal && name != NULL) {
        char *cname = copyCName(name, notifyPrefix(c->kind));
        CFStringRef retained = (CFStringRef)CFRetain(o.name);

        if (cname) {
            notify_register_dispatch(cname, &o.token, dispatch_get_main_queue(), ^(int token) {
                callBack(center, (void *)observer, retained, NULL, NULL);
            });
            free(cname);
        }
    }
    os_unfair_lock_lock(&c->lock);
    if (c->count == c->capacity) {
        c->capacity = c->capacity ? c->capacity * 2 : 16;
        c->observations = realloc(c->observations, c->capacity * sizeof(Observation));
    }
    c->observations[c->count++] = o;
    os_unfair_lock_unlock(&c->lock);
}

static void removeMatching(CFNotificationCenterRef center, const void *observer, CFStringRef name, const void *object, bool every) {
    struct __CFNotificationCenter *c = (struct __CFNotificationCenter *)center;
    CFIndex kept = 0;

    if (c == NULL) return;
    os_unfair_lock_lock(&c->lock);
    for (CFIndex i = 0; i < c->count; i++) {
        Observation *o = &c->observations[i];
        bool match = o->observer == observer && (every || ((name == NULL || (o->name && CFEqual(o->name, name))) && (object == NULL || o->object == object)));

        if (match) {
            if (o->token >= 0) notify_cancel(o->token);
            if (o->name) CFRelease(o->name);
        } else
            c->observations[kept++] = *o;
    }
    c->count = kept;
    os_unfair_lock_unlock(&c->lock);
}

void CFNotificationCenterRemoveObserver(CFNotificationCenterRef center, const void *observer, CFNotificationName name, const void *object) {
    removeMatching(center, observer, name, object, false);
}

void CFNotificationCenterRemoveEveryObserver(CFNotificationCenterRef center, const void *observer) {
    removeMatching(center, observer, NULL, NULL, true);
}

// snapshot the matching observers, then call them without the lock held
static void deliver(CFNotificationCenterRef center, CFStringRef name, const void *object, CFDictionaryRef userInfo, bool remote) {
    struct __CFNotificationCenter *c = (struct __CFNotificationCenter *)center;
    Observation *matches;
    CFIndex n = 0;

    os_unfair_lock_lock(&c->lock);
    matches = malloc(sizeof(Observation) * (c->count ? c->count : 1));
    for (CFIndex i = 0; i < c->count; i++) {
        Observation *o = &c->observations[i];

        // remote deliveries arrive once per registration through its own token
        if (remote && o->token >= 0) continue;
        if (o->name && !CFEqual(o->name, name)) continue;
        if (o->object && o->object != object && !(c->kind == CenterDistributed && object && CFEqual(o->object, object))) continue;
        matches[n] = *o;
        if (matches[n].name) CFRetain(matches[n].name);
        n++;
    }
    os_unfair_lock_unlock(&c->lock);
    for (CFIndex i = 0; i < n; i++) {
        matches[i].callback(center, (void *)matches[i].observer, name, object, userInfo);
        if (matches[i].name) CFRelease(matches[i].name);
    }
    free(matches);
}

static void postNotification(CFNotificationCenterRef center, CFStringRef name, const void *object, CFDictionaryRef userInfo) {
    struct __CFNotificationCenter *c = (struct __CFNotificationCenter *)center;

    if (c == NULL || name == NULL) return;
    if (c->kind == CenterLocal) {
        deliver(center, name, object, userInfo, false);
        return;
    }
    // the Darwin and distributed centers reach in-process observers through notify(3) as well
    {
        char *cname = copyCName(name, notifyPrefix(c->kind));
        bool anyWildcard = false;

        os_unfair_lock_lock(&c->lock);
        for (CFIndex i = 0; i < c->count; i++)
            if (c->observations[i].name == NULL) anyWildcard = true;
        os_unfair_lock_unlock(&c->lock);
        if (anyWildcard) deliver(center, name, c->kind == CenterDarwin ? NULL : object, c->kind == CenterDarwin ? NULL : userInfo, true);
        if (cname) {
            notify_post(cname);
            free(cname);
        }
    }
}

void CFNotificationCenterPostNotification(CFNotificationCenterRef center, CFNotificationName name, const void *object, CFDictionaryRef userInfo, Boolean deliverImmediately) {
    postNotification(center, name, object, userInfo);
}

void CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterRef center, CFNotificationName name, const void *object, CFDictionaryRef userInfo, CFOptionFlags options) {
    postNotification(center, name, object, userInfo);
}
