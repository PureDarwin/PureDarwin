/*
 * PureDarwin: real SDKs install libdispatch/private/private.h at
 * usr/local/include/dispatch/private.h; this build doesn't replicate that
 * installed layout. Forward to the real file instead.
 */
#include "../../../../libSystem/libdispatch/private/private.h"
