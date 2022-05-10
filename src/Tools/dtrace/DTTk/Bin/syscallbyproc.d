#!/usr/sbin/dtrace -s
/*
 * syscallbyproc.d - report on syscalls by process name . DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

syscall:::entry { @num[execname] = count(); }
