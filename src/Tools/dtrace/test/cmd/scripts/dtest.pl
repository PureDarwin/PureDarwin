#!/usr/bin/perl
##!/usr/perl5/bin/perl
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)dtest.pl	1.1	06/08/28 SMI"

require 5.6.1;

use File::Find;
use File::Copy;
use File::Basename;
use Getopt::Std;
use Cwd;
use Time::HiRes qw(time);

$PNAME = $0;
$PNAME =~ s:.*/::;
$USAGE = "Usage: $PNAME [-hlqsx] [-d dir] [-f file]"
    . "[-x opt[=arg]] [file | dir ...]\n";
($MACH = `uname -p`) =~ s/\W*\n//;

$IS_WATCH = `sw_vers -productName` =~m/^Watch OS/ ? 1 : 0;
$TIMEOUT = $IS_WATCH ? 480 : 240;

# Destructive actions are enabled when SIP is not enabled
$DESTRUCT_ON = not(-e '/usr/bin/csrutil' && (`csrutil status` =~/enabled/));
print($DESTRUCT_ON);
$dtrace_path = '/usr/sbin/dtrace';
@dtrace_argv = ();

$DEFAULT_TEST_LIST = $MACH eq 'arm' ? 'common/NoSafetyTests.arm' : 'common/NoSafetyTests';

$dash_path = '/bin/dash';

@files = ();
$errs = 0;
$bypassed = 0;

my $ts = time;

sub dirname {
	my($s) = @_;
	my($i);

	$s = substr($s, 0, $i) if (($i = rindex($s, '/')) != -1);
	return $i == -1 ? '.' : $i == 0 ? '/' : $s;
}

sub usage
{
	print $USAGE;
	print "\t -d  specify directory for test results files and cores\n";
	print "\t -f  specify test list file\n";
	print "\t -h  display verbose usage message\n";
	print "\t -l  save log file of results and PIDs used by tests\n";
	print "\t -q  set quiet mode (only report errors and summary)\n";
	print "\t -s  save results files even for tests that pass\n";
	print "\t -x  pass corresponding -x argument to dtrace(1M)\n";
	exit(2);
}

sub errmsg
{
	my($msg) = @_;

	print STDOUT $msg;
	print LOG $msg if ($opt_l);
	$errs++;
}

sub fail
{
	my(@parms) = @_;
	my($msg) = $parms[0];
	my($errfile) = $parms[1];
	my($n) = 0;
	my($dest) = basename($file);

	while (-d "$opt_d/failure.$n") {
		$n++;
	}

	unless (mkdir "$opt_d/failure.$n") {
		warn "[FAIL] failed to make directory $opt_d/failure.$n: $!";
		exit(125);
	}

	open(README, ">$opt_d/failure.$n/README");
	print README "[FAIL] " . $file . " " . $msg;
	
	if (scalar @parms > 1) {
		print README "; see $errfile\n";
	} else {
		if (-f "$opt_d/$pid.core") {
			print README "; see $pid.core\n";
		} else {
			print README "\n";
		}
	}

	close(README);

	if (-f "$opt_d/$pid.out") {
		rename("$opt_d/$pid.out", "$opt_d/failure.$n/$pid.out");
		copy("$file.out", "$opt_d/failure.$n/$dest.out");
	}

	if (-f "$opt_d/$pid.err") {
		rename("$opt_d/$pid.err", "$opt_d/failure.$n/$pid.err");
		copy("$file.err", "$opt_d/failure.$n/$dest.err");
	}

	if (-f "$opt_d/$pid.core") {
		rename("$opt_d/$pid.core", "$opt_d/failure.$n/$pid.core");
	}

	copy("$file", "$opt_d/failure.$n/$dest");


	if (scalar @parms > 1) {
		$msg = $msg . "; see $errfile in failure.$n";
	} else {
		$msg = $msg . "; details in failure.$n";
	}
	$msg = $msg . "\n[FAIL] " . $file . "\n";
	errmsg($msg);
}

sub logmsg
{
	my($msg) = @_;

	print STDOUT $msg unless ($opt_q);
	print LOG $msg if ($opt_l);
}

sub readtestlist
{
	my ($file) = @_;
	open(my $fd, '<', $file) or die "Could not open $file";
	while (my $test = <$fd>) {
		if (not($test =~ /^#/) and length $test > 1) {
			chomp($test);
			if (not($test =~ /Destruct|destruct|chill/) or $DESTRUCT_ON) {
				push(@files, $test);
			}
		}
	}
	chdir dirname($file) or die "Could not change directory";
}

die $USAGE unless (getopts('d:f:hi:lqsux:'));
usage() if ($opt_h);

readtestlist($opt_f) if ($opt_f);

foreach $arg (@ARGV) {
	if (-f $arg) {
		push(@files, $arg);
	} elsif (-d $arg) {
		find(\&wanted, $arg);
	} else {
		die "$PNAME: $arg is not a valid file or directory\n";
	}
}

$defdir = '.';
$bindir = '.';

readtestlist($DEFAULT_TEST_LIST) if (scalar(@files) == 0);
$files_count = scalar(@files);
die $USAGE if ($files_count == 0);

if (!$opt_d) {
	$opt_d = "/tmp/dtest.$$";
}

die "$PNAME: -d arg must be absolute path\n" unless ($opt_d =~ /^\//);
mkdir $opt_d unless (-e "$opt_d");
die "$PNAME: -d arg $opt_d is not a directory\n" unless (-d "$opt_d");

if ($opt_x) {
	push(@dtrace_argv, '-x');
	push(@dtrace_argv, $opt_x);
}

die "$PNAME: failed to open $PNAME.$$.log: $!\n"
    unless (!$opt_l || open(LOG, ">$PNAME.$$.log"));

#
# Ensure that $PATH contains a cc(1) so that we can execute the
# test programs that require compilation of C code.
#
$ENV{'PATH'} = $ENV{'PATH'} . ':/usr/bin';
$ENV{'TZ'} = 'Etc/UTC';

logmsg "[TEST] dtrace\n";
logmsg "Results in $opt_d\n";
logmsg "Total tests $files_count\n";

#
# Iterate over the set of test files specified on the command-line or located
# by a find on "." and execute each one.  If the test file is executable, we
# assume it is a #! script and run it.  Otherwise we run dtrace -s on it.
# If the file is named tst.* we assume it should return exit status 0.
# If the file is named err.* we assume it should return exit status 1.
# If the file is named err.D_[A-Z0-9]+[.*].d we use dtrace -xerrtags and
# examine stderr to ensure that a matching error tag was produced.
# If the file is named drp.[A-Z0-9]+[.*].d we use dtrace -xdroptags and
# examine stderr to ensure that a matching drop tag was produced.
# If any *.out or *.err files are found we perform output comparisons.
#
foreach $file (@files) {
	$file =~ m:.*/((.*)\.(\w+)):;
	$name = $1;
	$base = $2;
	$ext = $3;
	
	$dir = dirname($file);
	$isksh = 0;
	$isbinary = 0;
	$tag = 0;
	$droptag = 0;
	$perftest = 0;

	logmsg("[BEGIN] $file\n");
	my $ts_before = time;

	if ($name =~ /^tst\./) {
		$isksh = ($ext eq 'ksh');
		$status = 0;
	} elsif ($name =~ /^perf\./) {
		$isksh = ($ext eq 'ksh');
		$isbinary = ($ext eq 'exe');
		$status = 0;
		$perftest = 1;
	} elsif ($name =~ /^err\.(D_[A-Z0-9_]+)\./) {
		$status = 1;
		$tag = $1;
	} elsif ($name =~ /^err\./) {
		$status = 1;
	} elsif ($name =~ /^drp\.([A-Z0-9_]+)\./) {
		$status = 0;
		$droptag = $1;
	} else {
		errmsg("[FAIL] $file is not a valid test file name\n");
		next;
	}

	$fullname = "$dir/$name";
	$exe = "$dir/$base.exe";
	$exe_pid = -1;

	if (!($isksh || $isbinary) && -x $exe) {
		if (($exe_pid = fork()) == -1) {
			errmsg("[FAIL] failed to fork to run $exe: $!\n");
			next;
		}

		if ($exe_pid == 0) {
			open(STDIN, '</dev/null');

			exec($exe);

			warn "[FAIL] failed to exec $exe: $!\n";
		}
	}

	if (($pid = fork()) == -1) {
		errmsg("[FAIL] failed to fork to run test $file: $!\n");
		next;
	}

	if ($pid == 0) {
		open(STDIN, '</dev/null');
		exit(125) unless open(STDOUT, ">$opt_d/$$.out");
		exit(125) unless open(STDERR, ">$opt_d/$$.err");

		unless (chdir($dir)) {
			warn "[FAIL] failed to chdir for $file: $!\n";
			exit(126);
		}
		if ($perftest == 1) {
			$ENV{'PERFDATA_FILE'} = "$opt_d/$$.perfdata";
		}
		push(@dtrace_argv, '-xerrtags') if ($tag);
		push(@dtrace_argv, '-xdroptags') if ($droptag);
##		push(@dtrace_argv, $exe_pid) if ($exe_pid != -1);

		if ($isbinary and -x $name) {
			exec('./' . $name);
		}
		elsif ($isksh) {
			exit(123) unless open(STDIN, "<$name");
			exec($dash_path);
		}
		else {
			if ($tag == 0 && $status == $0 && $opt_a) {
				push(@dtrace_argv, '-A');
			}
			if ($file =~ /preprocessor/) {
				push(@dtrace_argv, '-C');
			}

			push(@dtrace_argv, '-s');
			push(@dtrace_argv, $name);
## Following moved here from above. Puts the pid number in the right place on the "command line"
			push(@dtrace_argv, $exe_pid) if ($exe_pid != -1);
			exec($dtrace_path, @dtrace_argv);
		}

		warn "[FAIL] failed to exec for $file: $!\n";
		exit(127);
	}

	eval {
		local $SIG{ALRM} = sub { die "alarm clock restart" };
		alarm($TIMEOUT);
		if (waitpid($pid, 0) == -1) {
			alarm(0);
			die "waitpid returned -1";
		}
		alarm(0);
	};

	if ($@) {
		my $timespent = time - $ts_before;
		logmsg("PID: $pid - TIME: $timespent\n");
		fail("timed out waiting for $file");
		kill(9, $exe_pid) if ($exe_pid != -1);
		kill(9, $pid);
		next;

	}
	kill(9, $exe_pid) if ($exe_pid != -1);

	my $timespent = time - $ts_before;
	logmsg("PID: $pid - TIME: $timespent\n");

	$wstat = $?;
	$wifexited = ($wstat & 0xFF) == 0;
	$wexitstat = ($wstat >> 8) & 0xFF;
	$wtermsig = ($wstat & 0x7F);

	if (!$wifexited) {
		fail("died from signal $wtermsig");
		next;
	}

	if ($wexitstat == 125) {
		die "$PNAME: failed to create output file in $opt_d " .
		    "(cd elsewhere or use -d)\n";
	}

	if ($wexitstat != $status) {
		fail("returned $wexitstat instead of $status");
		next;
	}

	if (-f "$file.out" && system("cmp -s $file.out $opt_d/$pid.out") != 0) {
		fail("stdout mismatch", "$pid.out");
		next;
	}

	if (-f "$file.err" && system("cmp -s $file.err $opt_d/$pid.err") != 0) {
		fail("stderr mismatch: see $pid.err");
		next;
	}

	if ($tag) {
		local $/ = undef; # Remove the input record separator to read the whole file at once
		open(TSTERR, "<$opt_d/$pid.err");
		$tsterr = <TSTERR>;
		close(TSTERR);

		unless ($tsterr =~ /: \[$tag\] line \d+:/m) {
			fail("errtag mismatch: see $pid.err");
			next;
		}
	}

	if ($droptag) {
		$found = 0;
		open(TSTERR, "<$opt_d/$pid.err");

		while (<TSTERR>) {
			if (/\[$droptag\] /) {
				$found = 1;
				last;
			}
		}

		close (TSTERR);

		unless ($found) {
			fail("droptag mismatch: see $pid.err");
			next;
		}
	}
	logmsg("[PASS] $file\n");

	unless ($opt_s) {
		unlink($pid . '.out');
		unlink($pid . '.err');
	}
}

if ($files_count > 1) {
	$opt_q = 0; # force final summary to appear regardless of -q option
	my $timespent = time - $ts;

	logmsg("[SUMMARY]\n");
	logmsg("Passed: " . ($files_count - $errs - $bypassed) . "\n");

	if ($bypassed) {
		logmsg("Bypassed: " . $bypassed . "\n");
	}

	logmsg("Failed: " . $errs . "\n");
	logmsg("Total: " . $files_count . "\n");
	logmsg("Time: "  . $timespent . "\n");
}

exit($errs != 0);
