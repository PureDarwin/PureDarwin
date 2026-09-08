/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef WINDOWSERVER_RPC_H
#define WINDOWSERVER_RPC_H

#include <WindowServer/message.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * reply/replyLen may be NULL for a message that expects nothing back.
 */
kern_return_t _windowServerRPC(void *data, size_t len, void *reply, int *replyLen);

/* Establishes the compositor connection and discovers display geometry before
 * AppKit constructs NSDisplay. */
int _windowServerConnect(void);

/*
 * Returns 1 when msg was filled, 0 if the connection is gone.
 */
int _windowServerReceiveMessage(PortMessage *msg);

#ifdef __cplusplus
}
#endif

#endif /* WINDOWSERVER_RPC_H */
