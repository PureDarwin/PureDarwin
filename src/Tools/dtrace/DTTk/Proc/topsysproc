#!/bin/sh
# #!/usr/bin/sh
#
# topsysproc - display top syscalls by process name.
#              Written using DTrace (Solaris 10 3/05).
#
# This program continually prints a report of the number of system calls
# by process name, and refreshes the display every 1 second or as specified
# at the command line. Similar data can be fetched with "prstat -m".
#
# 20-Apr-2006, ver 0.81
#
# USAGE:        topsysproc [interval]
#
# FIELDS:
#		load avg   load averages, see uptime(1)
#		syscalls   total number of syscalls in this interval
#		PROCESS    process name
#		COUNT      number of occurances in this interval
#
# NOTE: There may be several PIDs with the same process name.
#
# SEE ALSO:     prstat(1M)
#
# INSPIRATION:  top(1) by William LeFebvre
#
# Standard Disclaimer: This is freeware, use at your own risk.
#
# 13-Jun-2005  Brendan Gregg   Created this.
#

#
#  Check options
#
if [ "$1" = "-h" -o "$1" = "--help" ]; then
	cat <<-END
	USAGE: topsysproc [interval]
	   eg,
	       topsysproc            # default, 1 second updates
	       topsysproc 5          # 5 second updates
	END
	exit 1
fi
interval=1
if [ "$1" -gt 0 ]; then
	interval=$1
fi

#
#  Run DTrace
#
/usr/sbin/dtrace -n '
 #pragma D option quiet
 #pragma D option destructive

 /* constants */
 inline int INTERVAL = '$interval';
 inline int SCREEN   = 20;

 /* variables */
 dtrace:::BEGIN
 {
	secs = 0;
	printf("Tracing... Please wait.\n");
 }

 /* record syscall event */
 syscall:::entry
 {
	@Name[execname] = count();
	@Total = count();
 }

 /* update screen */
 profile:::tick-1sec
 /++secs >= INTERVAL/
 {
        /* fetch load averages */
	/* SOLARIS:
        this->load1a  = `hp_avenrun[0] / 65536;
        this->load5a  = `hp_avenrun[1] / 65536;
        this->load15a = `hp_avenrun[2] / 65536;
        this->load1b  = ((`hp_avenrun[0] % 65536) * 100) / 65536;
        this->load5b  = ((`hp_avenrun[1] % 65536) * 100) / 65536;
        this->load15b = ((`hp_avenrun[2] % 65536) * 100) / 65536;
	*/
	this->fscale = `averunnable.fscale;
	this->load1a  = `averunnable.ldavg[0] / this->fscale;
	this->load5a  = `averunnable.ldavg[1] / this->fscale;
	this->load15a = `averunnable.ldavg[2] / this->fscale;
	this->load1b  = ((`averunnable.ldavg[0] % this->fscale) * 100) / this->fscale;
	this->load5b  = ((`averunnable.ldavg[1] % this->fscale) * 100) / this->fscale;
	this->load15b = ((`averunnable.ldavg[2] % this->fscale) * 100) / this->fscale;

	/* clear screen */
	system("clear");

        /* print load average */
        printf("%Y, load average: %d.%02d, %d.%02d, %d.%02d",
            walltimestamp, this->load1a, this->load1b, this->load5a,
            this->load5b, this->load15a, this->load15b);

	/* print syscall count */
	printa("   syscalls: %@d\n",@Total);

	/* print report */
	trunc(@Name, SCREEN);
	printf("\n   %-25s %12s\n", "PROCESS", "COUNT");
	printa("   %-25s %@12d\n", @Name);

	/* reset variables */
	trunc(@Name);
	clear(@Total);
	secs = 0;
 }
'

