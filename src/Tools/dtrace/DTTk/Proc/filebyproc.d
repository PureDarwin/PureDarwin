#!/usr/sbin/dtrace -s
/*
 * filebyproc.d - snoop files opened by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

syscall::open*:entry { printf("%s %s", execname, copyinstr(arg0)); }
