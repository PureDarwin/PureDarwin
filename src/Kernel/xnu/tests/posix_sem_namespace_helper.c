/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/posix_sem.h>
#include <string.h>
#include <stdio.h>

/* spawned helper binary, so we don't have darwintest here */
/* usage: posix_sem_namespace_helper_teamN <semaphore_name> <operation> */
int
main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "error: wrong number of arguments (%d)\n", argc);
		return -1;
	}

	assert(argv[0] != NULL && strlen(argv[0]) > 0);
	int team_id = argv[0][strlen(argv[0]) - 1] - '0';
	if (team_id != 0 && team_id != 1) {
		fprintf(stderr, "error: invalid team_id %d\n", team_id);
		return -1;
	}

	char *sem_name = argv[1];
	char *op = argv[2];

	printf("running %s (%s)\n", op, sem_name);
	fflush(stdout);

	if (!strcmp(op, "open_excl")) {
		if (sem_open(sem_name, O_CREAT | O_EXCL, 0755, 0) == SEM_FAILED) {
			fprintf(stderr, "%s: ", sem_name);
			perror("sem_open (create exclusive)");
			return -1;
		}
	} else if (!strcmp(op, "check_access")) {
		if (sem_open(sem_name, 0) == SEM_FAILED) {
			fprintf(stderr, "%s: ", sem_name);
			perror("sem_open (check_access)");
			return -1;
		}
	} else if (!strcmp(op, "check_no_access")) {
		if (sem_open(sem_name, 0) != SEM_FAILED) {
			fprintf(stderr, "%s: sem_open unexpectedly succeeded\n", sem_name);
			return -1;
		}
	} else if (!strcmp(op, "unlink")) {
		if (sem_unlink(sem_name) != 0) {
			fprintf(stderr, "%s: ", sem_name);
			perror("sem_unlink");
			return -1;
		}
	} else if (!strcmp(op, "unlink_force")) {
		sem_unlink(sem_name);
	}
}
