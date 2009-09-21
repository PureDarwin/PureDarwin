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
#import "Crash.h"
#import "Usage.h"


int main(int argc, char **argv) {

	if (argc < 2) {

		int seed = 10000;
		srand(seed);
		sranddev();

		const char *lines_panic[] = { "",
			"Debugger called: <panic>",
			"Panic(CPU 0): Kernel trap at 0x00000000, type 00=unknown, registers:",
			"CR0: 0x00000000, CR2: 0x00000000, CR3: 0x00000000, CR4: 0x00000000",
			"EAX: 0x00000000, EBX: 0x00000000, ECX: 0x00000000, EDX: 0x00000000",
			"CR2: 0x00000000, EBP: 0x00000000, ESI: 0x00000000, EDI: 0x00000000",
			"EFL: 0x00000000, EIP: 0x00000000, CS:  0x00000000, DS:  0x00000000",
			"Backtrace (CPU 0), Frame : Return Address (4 potential args on stack)",
			"0x00000000 : 0x000000 (0x000000 0x00000000 0x0000000 0x0)",
			"0x00000000 : 0x000000 (0x000000 0x00000000 0x0000000 0x0)",
			"0x00000000 : 0x000000 (0x00000000 0x0 0x0 0x0)",
			"No mapping exists for frame pointer",
			"Backtrace terminated-invalid frame pointer 0x00000000",
			"       Kernel loadable modules in backtrace (with dependencies):",
			"               org.puredarwin."PROGRAM"(0.0.0)@0x00000000->0x00000000",
			"",
			"BSD process name corresponding to current thread: kernel_task",
			"",
		};

		/*
		const char *lines_panic[] = { "",
			"Welcome to PureDarwin nano",
			"                          ",
			"This is a proof-of-concept",
			"of a minimal Darwin system",
			"that can be built from the",
			"DarwinBuild project.      ",
			"If you would like to help,",
			"please join the PureDarwin",
			"project.",
			"http://www.puredarwin.org/",
			"",
		};
		*/

		int lines_count=sizeof(lines_panic) / sizeof(lines_panic[0]);

		id scrambler = [[Scrambler alloc] init];
		[scrambler draw : lines_count : lines_panic : 1 ]; // 1 <- randomization if '0' found
		[scrambler free];

		//id crash = [[Crash alloc] init];
		//[crash dereference];
		//[crash freed];
		//[crash reallocate];
		//[crash free];
		
		
	} else {

		id usage = [[Usage alloc] init];
		[usage interceptor : argc : argv];
		[usage free];

	}
	return 0;
}
