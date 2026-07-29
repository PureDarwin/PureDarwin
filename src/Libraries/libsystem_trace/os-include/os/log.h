/*
 * <os/log.h> - forwards to libsystem_trace's log.h.
 *
 * Callers (libsystem_asl in particular) include <os/log.h>; the implementation
 * header lives one level up beside log.c. A forwarding header is used rather
 * than a symlink so the file survives being copied into the nix build sandbox.
 */
#ifndef _PUREDARWIN_OS_LOG_FORWARD_H_
#define _PUREDARWIN_OS_LOG_FORWARD_H_

#include "../../log.h"

#endif
