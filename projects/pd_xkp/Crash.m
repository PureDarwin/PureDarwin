/*
# Copyright (c) 2007-2010 The PureDarwin Project.
# All rights reserved.
#
# @LICENSE_HEADER_START@
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# @LICENSE_HEADER_END@
#
*/

#import "Crash.h"

@implementation Crash

///////////////////////////////////////////////////////////////////////////////
// Default constructor
///////////////////////////////////////////////////////////////////////////////

- (id) init
{
	[super init];
	[self clear];
	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Clear
///////////////////////////////////////////////////////////////////////////////

- clear
{
	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Crash in dereferencing a null pointer (BONUS: crash report is created in ~/Library/Logs/CrashReporter/ )
///////////////////////////////////////////////////////////////////////////////

- dereference
{
	char **__crashreporter_info__ = NULL;
	__crashreporter_info__ = dlsym(RTLD_DEFAULT, "__crashreporter_info__");

	if (__crashreporter_info__)
		*__crashreporter_info__ = "A fake scrambled xnu kernel panic.";

	// must be < 8000 ( 32768) to fail
	char * null = (char *) 4919; // 0x1337 == 4919
	* null = 0;

	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Crash "malloc: *** error for object 0xc0de: Non-aligned pointer being freed"
///////////////////////////////////////////////////////////////////////////////

- freed
{
	char * null = (char *) 12648430; // C0FFEE == 12648430
	free(null);

	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Crash "malloc: *** error for object 0xc0ffee: pointer being reallocated was not allocated"
///////////////////////////////////////////////////////////////////////////////

- reallocate
{
	char * null = (char *)49374; // 0xC0DE == 49374
	realloc(null,(size_t)1);

	return self;
}

@end
