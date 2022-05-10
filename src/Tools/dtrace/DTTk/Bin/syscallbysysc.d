#!/usr/sbin/dtrace -s
/*
 * syscallbysysc.d - report on syscalls by syscall. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

syscall:::entry { @num[probefunc] = count(); }
