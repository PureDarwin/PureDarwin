#!/usr/sbin/dtrace -s
/*
 * lockbyproc.d - lock time by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

lockstat:::adaptive-block { @time[execname] = sum(arg1); }
