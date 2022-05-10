#!/usr/sbin/dtrace -s
/*
 * loads.d - print load averages. Written using DTrace (Solaris 10 3/05).
 *
 * These are the same load averages that the "uptime" command prints.
 * The purpose of this script is to demonstrate fetching these values
 * from the DTrace language.
 *
 * 10-Jun-2005, ver 0.90
 *
 * USAGE:	loads.d
 *
 * SEE ALSO:	uptime(1)
 *
 * The first field is the 1 minute average, the second is the 5 minute,
 * and the third is the 15 minute average. The value represents the average
 * number of runnable threads in the system, a value higher than your
 * CPU (core/hwthread) count may be a sign of CPU saturation.
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 10-Jun-2005	Brendan Gregg	Created this.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	/* fetch load averages */
	this->fscale = `averunnable.fscale;
	this->load1a  = `averunnable.ldavg[0] / this->fscale;
	this->load5a  = `averunnable.ldavg[1] / this->fscale;
	this->load15a = `averunnable.ldavg[2] / this->fscale;
	this->load1b  = ((`averunnable.ldavg[0] % this->fscale) * 100) / this->fscale;
	this->load5b  = ((`averunnable.ldavg[1] % this->fscale) * 100) / this->fscale;
	this->load15b = ((`averunnable.ldavg[2] % this->fscale) * 100) / this->fscale;

	/* print load average */
	printf("%Y,  load average: %d.%02d, %d.%02d, %d.%02d\n",
	    walltimestamp, this->load1a, this->load1b, this->load5a,
	    this->load5b, this->load15a, this->load15b);

	exit(0);
}
