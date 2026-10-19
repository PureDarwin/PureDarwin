// pd_authsession.c - security sessions, one per login
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// PD has one session per login. A process has graphic access when the desktop session handed it a
// display to connect to, the same test winemac and other GUI tools make before starting AppKit
#include <Security/AuthSession.h>
#include <stdlib.h>
#include <unistd.h>

OSStatus SessionGetInfo(SecuritySessionId session, SecuritySessionId *sessionId, SessionAttributeBits *attributes)
{
    SessionAttributeBits bits = 0;

    if (session != callerSecuritySession && session != (SecuritySessionId)getsid(0))
        return errSessionInvalidId;
    if (getenv("WAYLAND_DISPLAY") != NULL || getenv("DISPLAY") != NULL)
        bits |= sessionHasGraphicAccess;
    if (isatty(STDIN_FILENO))
        bits |= sessionHasTTY;
    if (getuid() == 0)
        bits |= sessionIsRoot;
    if (getenv("SSH_CONNECTION") != NULL)
        bits |= sessionIsRemote;
    if (sessionId)
        *sessionId = (SecuritySessionId)getsid(0);
    if (attributes)
        *attributes = bits;
    return errSessionSuccess;
}

OSStatus SessionCreate(SessionCreationFlags flags, SessionAttributeBits attributes)
{
    return errSessionAuthorizationDenied;
}
