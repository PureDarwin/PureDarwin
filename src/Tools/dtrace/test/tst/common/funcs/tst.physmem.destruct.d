/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2020 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * ASSERTION:
 * This test assers that physmem_read can read and write memory
 */

#pragma D option destructive

BEGIN
{
	virt = `dtrace_nprobes_default;
	phys_addr = kvtophys(&`dtrace_nprobes_default);
	size = sizeof(`dtrace_nprobes_default);
	phys = physmem_read(phys_addr, size);
}

BEGIN
/ phys != virt /
{
	printf("reading failed: phys: %d, virt: %d", phys, virt);
	exit(2);
}

BEGIN
/ virt == phys /
{
	physmem_write(phys_addr, 42, size);
	new_virt = `dtrace_nprobes_default;
	new_phys = physmem_read(phys_addr, size);
	physmem_write(phys_addr, virt, size);
}

BEGIN
/ new_virt != new_phys /
{
	printf("writing failed: phys: %d, virt: %d", new_phys, new_virt);
	exit(3);
}

BEGIN
/ new_virt == new_phys /
{
	exit(0);
}

tick-5s
{
	exit(1);
}

