/*
# Copyright (c) 2007-2009 The PureDarwin Project.
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

#import "Scrambler.h"

@implementation Scrambler : Object

///////////////////////////////////////////////////////////////////////////////
// Default constructor
///////////////////////////////////////////////////////////////////////////////

- init
{
	[super init];
 	[self clear];
 	return self;
}

///////////////////////////////////////////////////////////////////////////////
// Clear
///////////////////////////////////////////////////////////////////////////////

- clear { return self; }

///////////////////////////////////////////////////////////////////////////////
// Return a random integer between 0 and "max_dec"
///////////////////////////////////////////////////////////////////////////////

- (int) hexarandomize : (int) max_dec 
{
	return ( rand() / (float)RAND_MAX) * max_dec ;
}

///////////////////////////////////////////////////////////////////////////////
// Initialize "linecd**" (a clone of "lines**" with space and random chars)
///////////////////////////////////////////////////////////////////////////////

- (void) init_scramble : (int) A_LINES : (const char **) lines 
{

	linecd = malloc(sizeof(int *) * A_LINES);

	int seed = 10000;
	srand(seed);	
	sranddev();
	int 	i;
	size_t j;

	for (i = 0; i < A_LINES; i++) {

		linecd[i] = malloc(strlen(lines[i]) * sizeof(int));

		for (j = 0; j < strlen(lines[i]); j++) {

			if ( (lines[i][j] == (char)' ') || (lines[i][j] == (char)'\t')) {
				linecd[i][j] = lines[i][j];
			} else	{
				linecd[i][j] = ((float)rand() / (float)RAND_MAX) * (j+40) + 41;
			}
			// no randomization
			//linecd[i][j] = (lines[i][j]);
			// debug
			//printf("%c",linecd[i][j]);
		}
		// debug
		//printf("\n");
	}
}

///////////////////////////////////////////////////////////////////////////////
// Free "linecd**"
///////////////////////////////////////////////////////////////////////////////

- (void) free_scramble: (int) A_LINES
{
	int i;
	for (i = 0; i < A_LINES; i++) 
		free(linecd[i]);
}

///////////////////////////////////////////////////////////////////////////////
// 1 pass. Randomize and display "linecd**" (which tends to "lines**")
///////////////////////////////////////////////////////////////////////////////
- (int) draw_lines : (int) A_LINES : (const char **) lines : (int) canbe_randomized {
	int i;
	size_t j;
	char	buf[0x100];
	int scrambledchar=0;


	for (i = 0; i < A_LINES; i++) {

		for (j = 0; j < strlen(lines[i]); j++)  {

			if (linecd[i][j] > 61) {

				buf[j] = ' ';
				linecd[i][j]--;

			} else if ((linecd[i][j] != (char)'\t') && (linecd[i][j] != (char)' ') ) {

				buf[j] = (rand() / (float)RAND_MAX) * 35 + 41;
				linecd[i][j]--;
				scrambledchar++;

			} else {

				if (lines[i][j] == (char)'0' && (canbe_randomized !=0 )) {

					if (lines[i][j+1] == (char)'x') {
						canbe_randomized=1;
					} else if (	(lines[i][j+1] == (char)'0') ||
							(lines[i][j-1] == (char)'0') ||
							(lines[i][j-1] == (char)'x')	) {
						canbe_randomized=16;
					}

					char * hex = malloc(sizeof(char) * 1);
					snprintf(hex, sizeof(hex), "%x", [self hexarandomize : canbe_randomized]); // 0 or 16
					buf[j] = hex[0];
					free(hex);

					canbe_randomized=1;

				} else	{
					buf[j] = lines[i][j];
				}
			}	
		}

		buf[j] = '\0';
		printf("%s\n\r",buf);
	}
	return scrambledchar;
}	

///////////////////////////////////////////////////////////////////////////////
// n pass. Loop while "linecd**" != "lines**"
///////////////////////////////////////////////////////////////////////////////

- (void) draw : (int) A_LINES : (const char *[]) lines : (int) canbe_randomized
{
	
	[ self init_scramble : A_LINES : lines];
	
	while (1) {
		//system ("clear");
		printf("\033[2J\033[0;0f");
		if ([self draw_lines : A_LINES : lines : canbe_randomized] == 0)
			break;
		usleep(5180);
	}
}

@end

