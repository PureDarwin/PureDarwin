/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef Diagnostics_h
#define Diagnostics_h

#include <stdint.h>

#if BUILDING_CACHE_BUILDER
#include <set>
#include <string>
#include <vector>
#include <dispatch/dispatch.h>
#endif

#include "Logging.h"


class VIS_HIDDEN Diagnostics
{
public:
                    Diagnostics(bool verbose=false);
                    ~Diagnostics();

    void            error(const char* format, ...)  __attribute__((format(printf, 2, 3)));
    void            error(const char* format, va_list list);
#if BUILDING_CACHE_BUILDER
                    Diagnostics(const std::string& prefix, bool verbose=false);

    void            warning(const char* format, ...)  __attribute__((format(printf, 2, 3)));
    void            verbose(const char* format, ...)  __attribute__((format(printf, 2, 3)));
    void            copy(const Diagnostics&);
#endif

    bool            hasError() const;
    bool            noError() const;
    void            clearError();
    void            assertNoError() const;
    bool            errorMessageContains(const char* subString) const;

#if !BUILDING_CACHE_BUILDER
    const char*                     errorMessage() const;
#else
    const std::string               prefix() const;
    std::string                     errorMessage() const;
    const std::set<std::string>     warnings() const;
    void                            clearWarnings();
#endif

private:

    void*                    _buffer = nullptr;
#if BUILDING_CACHE_BUILDER
    std::string              _prefix;
    std::set<std::string>    _warnings;
    bool                     _verbose = false;
#endif
};

#if BUILDING_CACHE_BUILDER

class VIS_HIDDEN TimeRecorder
{
public:
    // Call pushTimedSection(), then mark events with recordTime. Call popTimedSection() to stop the current timing session.
    // This is stack-based, so you can start a sub-timer with pushTimedSection() / recordTime / recordTime... / popTimedSection()
    // inside a first timed section.
    // Call logTimings() to print everything.

    // Start a new timed section.
    void pushTimedSection();

    // Records the time taken since the last pushTimedSection() / recordTime() at the current level
    void recordTime(const char* format, ...);

    // Stop the current timed section and pop back one level.
    void popTimedSection();

    void logTimings();
private:
    struct TimingEntry {
        uint64_t time;
        std::string logMessage;
        int depth;
    };

    std::vector<uint64_t> openTimings;
    std::vector<TimingEntry> timings;
};

#endif /* BUILDING_CACHE_BUILDER */

#endif // Diagnostics_h
