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

#import "Usage.h"

@implementation Usage

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
// Display version + info
///////////////////////////////////////////////////////////////////////////////

- showVersion
{
	const char *lines[] = { "",
		""PROGRAM" 0.3",
		"Copyright (c) 2009-2010 The PureDarwin Project",
		"",
		"A fake scrambled xnu kernel panic, appeared a 1st April 2009.",
		"aladin@puredarwin.org",
		"",
		"http://puredarwin.org",
		"http://puredarwin.googlecode.com",
		"",
	};
	int lines_count=sizeof(lines) / sizeof(lines[0]);

	id scrambler = [[Scrambler alloc] init];
	[scrambler draw : lines_count : lines : 0 ]; // 0 <- no randomization if '0' found
	[scrambler free];

	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Display usage
///////////////////////////////////////////////////////////////////////////////

- showUsage
{
	const char *lines[] = { "",
		"Usage: "PROGRAM" [OPTION]",
		"A fake scrambled xnu kernel panic",
		"  -v,  --version\t\tprint the version",
		"  -h,  --help   \t\tprint this help message",
		"",
	};
	int lines_count=sizeof(lines) / sizeof(lines[0]);

	id scrambler = [[Scrambler alloc] init];
	[scrambler draw : lines_count : lines : 0 ]; // 0 <- no randomization if '0' found
	[scrambler free];

	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Wrapper of argc/argc from main() given to getopt 
///////////////////////////////////////////////////////////////////////////////

- interceptor : (int) argc : (char **)argv
{
        static struct option long_options[] = {
		{ "version",	no_argument, 	0, 'v' },
		{ "help",	no_argument, 	0, 'h' },
		{ 0, 		0, 		0,  0  }
	};

        int option_index = 0;

		int c = getopt_long(argc, argv, ":vh", long_options, &option_index);
		switch(c) {
             		case 'v':
        			[self showVersion];
            			break;
           		default:
				[self showUsage];
        	}
	return self;
}

@end
