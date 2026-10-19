// AppleEvents.h - the Apple event classes, IDs and keywords apps compare against
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// the Apple event classes, IDs and keywords apps compare against
#ifndef __CORESERVICES_APPLEEVENTS__
#define __CORESERVICES_APPLEEVENTS__

enum {
    kCoreEventClass = 'aevt',
    kAEOpenApplication = 'oapp',
    kAEOpenDocuments = 'odoc',
    kAEPrintDocuments = 'pdoc',
    kAEOpenContents = 'ocon',
    kAEQuitApplication = 'quit',
    kAEAnswer = 'ansr',
    kAEApplicationDied = 'obit',
    kAEShowPreferences = 'pref',
    kAEReopenApplication = 'rapp',
};

enum {
    kInternetEventClass = 'GURL',
    kAEGetURL = 'GURL',
};

enum {
    keyDirectObject = '----',
    keyErrorNumber = 'errn',
    keyErrorString = 'errs',
    keyProcessSerialNumber = 'psn ',
    keyAEQuitReason = 'why?',
    kAEQuitReason = 'why?',
};

// why an app is being asked to quit
enum {
    kAELogOut = 'logo',
    kAEReallyLogOut = 'rlgo',
    kAEShowRestartDialog = 'rrst',
    kAEShowShutdownDialog = 'rsdn',
    kAERestart = 'rest',
    kAEShutDown = 'shut',
};

#endif
