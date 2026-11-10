// weak-compat.m - objc4's zeroing weak references for Argon on Linux
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// libobjc2 stores a reference record in a weak location, so Argon keeps a table from objects to locations
// and NSObject's -dealloc zeroes them as objc4 does

#include <objc/runtime.h>
#include <objc/objc-arc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

struct argon_weak_loc { id *location; struct argon_weak_loc *next; };
struct argon_weak_obj { id object; struct argon_weak_loc *locations; struct argon_weak_obj *next; };

#define ARGON_WEAK_BUCKETS 1024
static struct argon_weak_obj *buckets[ARGON_WEAK_BUCKETS];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static size_t tracked;

static unsigned bucket(id object)
{
    return (unsigned)(((uintptr_t)object >> 4) % ARGON_WEAK_BUCKETS);
}

static struct argon_weak_obj **find(id object)
{
    struct argon_weak_obj **at = &buckets[bucket(object)];

    while (*at && (*at)->object != object)
        at = &(*at)->next;
    return at;
}

// drop one location from the object it currently names
static void forget(id *location)
{
    id old = *location;
    struct argon_weak_obj **entry;

    if (old == nil || ((uintptr_t)old & 7))
        return;
    entry = find(old);
    if (*entry == NULL)
        return;
    for (struct argon_weak_loc **at = &(*entry)->locations; *at; at = &(*at)->next) {
        if ((*at)->location == location) {
            struct argon_weak_loc *gone = *at;
            *at = gone->next;
            free(gone);
            break;
        }
    }
    if ((*entry)->locations == NULL) {
        struct argon_weak_obj *gone = *entry;
        *entry = gone->next;
        free(gone);
        tracked--;
    }
}

id argon_storeWeak(id *location, id object)
{
    pthread_mutex_lock(&lock);
    forget(location);
    *location = object;
    // tagged pointers are never deallocated, there is nothing to zero later
    if (object != nil && !((uintptr_t)object & 7)) {
        struct argon_weak_obj **entry = find(object);
        struct argon_weak_loc *loc = malloc(sizeof(*loc));

        if (*entry == NULL) {
            *entry = calloc(1, sizeof(**entry));
            (*entry)->object = object;
            tracked++;
        }
        loc->location = location;
        loc->next = (*entry)->locations;
        (*entry)->locations = loc;
    }
    pthread_mutex_unlock(&lock);
    return object;
}

id argon_loadWeakRetained(id *location)
{
    id object;

    pthread_mutex_lock(&lock);
    object = *location;
    if (object != nil)
        object = objc_retain(object);
    pthread_mutex_unlock(&lock);
    return object;
}

id argon_loadWeak(id *location)
{
    return objc_autorelease(argon_loadWeakRetained(location));
}

// zeroes every weak location naming a deallocating object
void argon_clearWeak(id object)
{
    if (tracked == 0)
        return;
    pthread_mutex_lock(&lock);
    struct argon_weak_obj **entry = find(object);
    if (*entry) {
        struct argon_weak_obj *gone = *entry;
        *entry = gone->next;
        for (struct argon_weak_loc *loc = gone->locations, *next; loc; loc = next) {
            next = loc->next;
            *loc->location = nil;
            free(loc);
        }
        free(gone);
        tracked--;
    }
    pthread_mutex_unlock(&lock);
}
