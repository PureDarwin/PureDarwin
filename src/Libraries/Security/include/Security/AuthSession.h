// AuthSession.h - security sessions, as apps ask whether they run with access to the screen
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// security sessions, as apps ask whether they run with access to the screen
#ifndef _SECURITY_AUTHSESSION_H_
#define _SECURITY_AUTHSESSION_H_

#include <Security/SecBase.h>

__BEGIN_DECLS

typedef UInt32 SecuritySessionId;

enum {
    noSecuritySession = 0,
    callerSecuritySession = (SecuritySessionId)-1,
};

typedef UInt32 SessionAttributeBits;

enum {
    sessionIsRoot = 0x0001,
    sessionHasGraphicAccess = 0x0010,
    sessionHasTTY = 0x0020,
    sessionIsRemote = 0x1000,
};

typedef UInt32 SessionCreationFlags;

enum {
    sessionKeepCurrentBootstrap = 0x8000,
};

enum {
    errSessionSuccess = 0,
    errSessionInvalidId = -60500,
    errSessionInvalidAttributes = -60501,
    errSessionAuthorizationDenied = -60502,
    errSessionValueNotSet = -60503,
    errSessionInternal = -60008,
    errSessionInvalidFlags = -60011,
};

OSStatus SessionGetInfo(SecuritySessionId session, SecuritySessionId *sessionId, SessionAttributeBits *attributes);
OSStatus SessionCreate(SessionCreationFlags flags, SessionAttributeBits attributes);

__END_DECLS

#endif
