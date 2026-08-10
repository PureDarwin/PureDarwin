#ifndef SecDbInternal_h
#define SecDbInternal_h

#include "SecDb.h"

static const size_t kSecDbMaxReaders = 5;

// Do not increase this without changing lock types in SecDb
static const size_t kSecDbMaxWriters = 1;

// maxreaders + maxwriters
static const size_t kSecDbMaxIdleHandles = 6;

#endif /* SecDbInternal_h */
