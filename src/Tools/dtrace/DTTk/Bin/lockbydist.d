#!/usr/sbin/dtrace -s
/*
 * lockbydist.d - lock distrib. by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

lockstat:::adaptive-block { @time[execname] = quantize(arg1); }
