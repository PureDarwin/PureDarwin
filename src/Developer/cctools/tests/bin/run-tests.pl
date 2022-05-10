#!/usr/bin/perl -w
#
# run-tests
# cctools
# Michael Trent <mtrent@apple.com>

use Cwd 'realpath';
use FileHandle;
use Getopt::Std;

##############################################################################
#
# The following variables only apply when running tests asynchronously:
#
#   $MAXCHILDREN specifies the maximum number of child processes allowed
#
#   $CURCHILDREN describes the number of children currently in use.
#
#   $PASS counts the number of asynchronous tests that have passed.
#
$MAXCHILDREN = 4;
$CURCHILDREN = 0;
$PASS = 0;

##############################################################################
#
# @PLATFORMS holds the list of valid platforms. This list is hashed into
# %PLATFORMS for ease of access.
#
@PLATFORMS = (
  "MACOS",
  "IOS",
  "WATCHOS",
  "TVOS",
);
%PLATFORMS = map { $_ => $_ } @PLATFORMS;

exit &main();

##############################################################################
#
# get_ro_testroot returns the path to the main testroot, relative to the
# test driver. the test driver is likely one of these two paths:
#
#     .../tests/driver
#     .../tests/bin/driver
#
sub get_ro_testroot {
  my $path = realpath $0;
  $path =~ s|/[^/]*$||;
  $path =~ s|/[^/]*$|| if ($path =~ m|/bin|);
  return $path;
}

##############################################################################
#
# get_rw_testroot returns the path to a location in temp where the testroot
# should be copied into. The file may or may not exist.
#
sub get_rw_testroot
{
  my $name = "cctools";

  # see if the word "cctools" appears in any of the preceding path components,
  # and if it does, use that path component.
  my ($ro) = &get_ro_testroot();
  for my $dir (reverse split "/", $ro) {
    if ($dir =~ /cctools/) {
      $name = $dir;
    }
  }

  $name .= ".testroot";
  return "/tmp/$name";
}

##############################################################################
#
# find_tests returns an array of tests for a given test repository. tests are
# really just subdirectories.
#
sub find_tests {
  my ($path) = @_;
  opendir TESTSDIR, "$path" or die "can't read $path: $!\n";
  my (@tests) = grep !/^\./, readdir TESTSDIR;
  closedir TESTSDIR;
  return sort @tests;
}

##############################################################################
#
# shellcmd is a brutal little wrapper around system()
#
sub shellcmd {
  my (@args) = @_;
  my $rc = 0xffff & system @args;

  if ($rc == 0xff00) {
    warn "system failed: $!\n";
    $rc = -1;
  }
  elsif ($rc > 0x80) {
    $rc >>= 8;
    #warn "command '@args' exited with status $rrc\n";
  }
  elsif ($rc) {
    my $w = "ran with ";
    if ($rc & 0x80) {
      $rc &= ~0x80;
      $w .= "coredump from ";
    }
    $w .= "signal $rc";
    warn "$w\n";
  }

  return $rc;
}

##############################################################################
#
# print_test prints a formatted line of text including the test name and a
# brief note meant to fit on one line.
#
sub print_test {
  my ($test, $note) = @_;

  printf "%-32s %s\n", $test, $note;

}

##############################################################################
#
# indent indents a string by line.
#
sub indent {
  my ($message) = @_;
  my ($result);

  foreach my $line (split '\n', $message) {
    $result .= "\t$line\n";
  }

  return $result;
}


##############################################################################
#
# run_test runs the makefile in the current directory once for each platform
# specified by @$platformRef. The name of the test is supplied in $test. If
# $verbose is true, run_test will print a line for each subphase. It returns
# 1 if all tests pass. Sorry about that.
#
# run_test can be called serially or asynchronously. In either case, test
# results will be printed to STDOUT.
#
sub run_test {
  my ($test, $platformRef, $cctools_root, $verbose, $src_testroot,
      $dst_testroot) = @_;

  my ($test_failed);
  my ($logmsg);
  my ($pass) = 0;

  print "$test starting...\n" if ($verbose);

#   print "\tcleaning\n" if ($verbose);
#   &shellcmd("make clean > /dev/null");

  if ($opt_C) {
    chdir "$src_testroot/test-cases/$test" or
      die "can't cd to $src_testroot/test-cases/$test: $!\n";

    print "\tcleaning\n" if ($verbose);
    &shellcmd("make clean > /dev/null");

    &print_test($test, "cleaned");
    return 1;
  }

  unless ( -e "$dst_testroot/test-cases/$test" ) {
    print "\tcopying\n" if ($verbose);
    &shellcmd("ditto $src_testroot/test-cases/$test " .
              "$dst_testroot/test-cases/$test");
  }

  chdir "$dst_testroot/test-cases/$test" or
    die "can't cd to $dst_testroot/test-cases/$test: $!\n";

  # run the test once for each platform
  foreach $platform (@$platformRef) {

    print "\ttesting $platform\n" if ($verbose);

    # run the makefile, redirecting to file.
    my $outfile = "/tmp/stdout.$$";
    my $errfile = "/tmp/stderr.$$";
    my $makearg = "PLATFORM=$platform";
    $makearg .= " CCTOOLS_ROOT=$cctools_root" if (defined($cctools_root));

    #print "make $makearg 1>$outfile 2>$errfile\n" if ($verbose);
    my $rc = &shellcmd("make $makearg 1>$outfile 2>$errfile");

    # interpret the return code and make output:
    #
    # 1) see if we got a FAIL marker.
    # 2) check the return code of make
    # 3) check for spurious STDERR
    # 4) check we got a PASS marker
    chomp(my $errstr = `cat $errfile`);
    chomp(my $outstr = `cat $outfile`);
    if ($outstr =~ /^FAIL/m) {
      $logmsg = "FAIL $platform";
      $logmsg .= "\nSTDOUT:\n" . &indent($outstr) if ($outstr);
      $logmsg .= "\nSTDERR:\n" . &indent($errstr) if ($errstr);
      $test_failed = 1;
    }
    elsif ($outstr =~ /^XFAIL/m) {
      $logmsg = "XFAIL" unless(defined($logmsg));
    }
    elsif ($rc) {
      $logmsg = "FAIL $platform Makefile failure";
      $logmsg .= "\nSTDOUT:\n" . &indent($outstr) if ($outstr);
      $logmsg .= "\nSTDERR:\n" . &indent($errstr) if ($errstr);
      $test_failed = 1;
    }
    elsif ($errstr ne "") {
      $logmsg = "FAIL $platform spurious stderr failure";
      $logmsg .= "\nSTDOUT:\n" . &indent($outstr) if ($outstr);
      $logmsg .= "\nSTDERR:\n" . &indent($errstr) if ($errstr);
      $test_failed = 1;
    }
    elsif (!($outstr =~ /^(PASS|XPASS)/m)) {
      $logmsg = "AMBIGIOUS $platform missing [X]PASS/[X]FAIL";
      $logmsg .= "\nSTDOUT:\n" . &indent($outstr) if ($outstr);
      $logmsg .= "\nSTDERR:\n" . &indent($errstr) if ($errstr);
      $test_failed = 1;
    }
    else {
      $logmsg = "PASS" unless(defined($logmsg));
#       $logmsg .= " $platform";
    }

    # remove the temporary files
    unlink $outfile;
    unlink $errfile;

    # if the test failed, bail on remaining platforms
    next if ($test_failed);
  }

#   print "\tcleaning\n" if ($verbose);
#   &shellcmd("make clean > /dev/null");

  &print_test($test, $logmsg) if ($logmsg);

  # this test passes only if none of its platforms failed.
  $pass += 1 unless ($test_failed);

  return $pass;
}

##############################################################################
#
# collect_test returns results from a single asynchronous test. It should be
# called once for each call to schedule_test. Tests will be collected in the
# order in which they return, which is not strictly specified. Be aware that
# collect_test will block until test results are available. Test results are
# printed to STDOUT. collect_test has no meaningful return result.
#
sub collect_test {

  if ($CURCHILDREN) {
    # block and wait for a test to complete
    my $pid = wait;
    my $status = $?;
    die "no children: $!\n" if ($pid == -1);

    $CURCHILDREN -= 1;

    # print the test results
    $path = "/tmp/result.$pid";
    my $fh = new FileHandle($path) or
      die "can't open $path: $!\n";

    while (<$fh>) {
      print;
    }

    # record success or failure
    $PASS += 1 if ($status == 0);

    unlink $path;
  }
}

##############################################################################
#
# schedule_test schedules a test to run asynchronously. If resources are
# available, schedule_test will return right away; it not, schedule_test will
# block until resources are available. schedule_test has no meaningful return
# result.
#
sub schedule_test {
  my ($test, $platformRef, $cctools_root, $verbose, $src_testroot,
      $dst_testroot) = @_;

  my $pid = fork;
  die "fork: $!\n" unless defined ($pid);

  if ($pid == 0) {
    # child
    open(STDOUT, ">/tmp/result.$$") || die "can't redirect stdout: $!";
    open(STDERR, ">&STDOUT") || die "can't dup stderr to stdout: $!";
    exit (&run_test($test, $platformRef, $cctools_root, $verbose,
                    $src_testroot, $dst_testroot) == 0);
  } else {
    # parent
    $CURCHILDREN += 1;
    if ($CURCHILDREN >= $MAXCHILDREN) {
      &collect_test();
    }
  }
}

##############################################################################
#
# main
#
sub main {

  $opt_a = 1;
  $opt_C = undef;
  $opt_c = undef;
  $opt_h = undef;
  $opt_t = undef;
  $opt_v = undef;

  return &usage() unless (getopts('aCc:t:hv'));

  my ($async) = $opt_a;
  my ($cctools_root) = $opt_c;
  my ($test_filter) = $opt_t;
  my ($verbose) = $opt_v;

  if ($opt_h) {
    return &usage();
  }

  # set the number of child processes to the number of cpus
  if ($async) {
    my ($ncpu) = int(`sysctl -n hw.ncpu`);
    if ($ncpu > $MAXCHILDREN) {
      $MAXCHILDREN = $ncpu;
    }
  }

  # verify the $cctools_root if necessary
  if (defined($cctools_root)) {
    die "can't find $cctools_root\n" unless ( -e $cctools_root );
    die "not a directory: $cctools_root\n" unless ( -d $cctools_root );
    die "does not appear to be a cctools root: $cctools_root\n"
      unless ( -e "$cctools_root/usr/bin/lipo" );
  }

  # construct a temporary testroot path
  my $src_testroot = &get_ro_testroot();
  my $dst_testroot = &get_rw_testroot();

  # remove the temporary testroot contents if any
  if (-e $dst_testroot) {
    print "removing $dst_testroot ...\n" if ($verbose);
    system "rm -rf $dst_testroot" and
      die "can't remove $dst_testroot: $!";
  }

  # copy the source testroot into the temporary location
  #
  # if we are running all the tests, just copy everything; if we are
  # filtering tests, just copy the infrastructure and trust the tests
  # to copy themselves.
  if ($test_filter) {
    print "copying\n" if ($verbose);
    for my $sub ("bin", "include", "src") {
      print "\t$sub ...\n" if ($verbose);
      system "ditto $src_testroot/$sub $dst_testroot/$sub" and
        die "can't copy $src_testroot/$sub into $dst_testroot/$sub: $!\n";
    }
    print "\tcreating test-cases ...\n" if ($verbose);
    system "mkdir $dst_testroot/test-cases" and
      die "can't mkdir $dst_testroot/test-cases: $!\n";
  }
  else {
    print "copying $src_testroot into $dst_testroot ...\n" if ($verbose);
    system "ditto $src_testroot $dst_testroot" and
      die "can't copy $src_testroot into $dst_testroot: $!\n";
  }

  # read the tests from the source test directory, as they may not have
  # been copied yet.
  my $tests_dir = "$src_testroot/test-cases";
  my (@tests) = &find_tests($tests_dir);

  # apply the test_filter. first look up a test by requested name.
  if ($test_filter) {
    my %tests = map { $_ => $_ } @tests;
    if ($tests{$test_filter}) {
      @tests = ( $test_filter );
    } else {
      my @matches;
      for my $test (@tests) {
        if ($test =~ /$test_filter/) {
          push @matches, $test;
        }
      }
      die "tests matching $test_filter not found in $tests_dir\n"
        unless scalar @matches;
      @tests = @matches;
    }
  }

  my (%platform_map);
  my ($total, $pass);

  # run the selected tests
  my $verbing = $opt_C ? "cleaning" : "running";
  printf "### $verbing %s from $tests_dir in $dst_testroot\n",
         defined($test_filter) ? $test_filter : "all tests";

  foreach my $test (@tests)
  {
    $total += 1;

    # find the makefile
    my ($makefile) = "$tests_dir/$test/Makefile";
    unless ( -e "$makefile" ) {
      &print_test($test, "FAIL missing Makefile");
      next;
    }

    # read the PLATFORM string from the makefile. The line must be in one of
    # the following forms:
    #
    #   "# PLATFORM:  <platform 1> ... "
    #   "# PLATFORMS: <platform 1> ... "
    #
    # Just for fun, more than one platform string can be specified.
    my (@platform_entries) = `grep -E '^#\\s*PLATFORMS?:' '$makefile'`;
    unless (@platform_entries) {
      &print_test($test, "FAIL Makefile contains no platforms");
      next;
    }

    # extract the platform names from the PLATFORM line
    my (@known_platforms, @unknown_platforms);

    foreach my $entry (@platform_entries) {
      $entry =~ s/.*PLATFORMS?://;
      foreach my $platform (split ' ', $entry) {
        if ($PLATFORMS{$platform}) {
          push @known_platforms, $platform;
        } else {
          push @unknown_platforms, $platform;
        }
      }
    }

    # warn on unknown platforms
    if (@unknown_platforms) {
      my ($unknown) = join ' ', @unknown_platforms;
      &print_test($test, "FAIL Makefile contains unknown platforms: $unknown");
      next;
    }

    # warn on empty platforms
    if (@known_platforms == 0) {
      &print_test($test, "FAIL Makefile contains no known platforms");
      next;
    }

    # change into the proper directory
    if ($opt_C) {
      chdir "$src_testroot/test-cases" or
        die "can't cd to $src_testroot/test-cases: $!\n";
    }
    else {
      chdir "$dst_testroot/test-cases" or
        die "can't cd to $dst_testroot/test-cases: $!\n";
    }

    # run / schedule the test
    if ($async) {
      &schedule_test($test, \@known_platforms, $cctools_root, $verbose,
                     $src_testroot, $dst_testroot);
    } else {
      $pass += &run_test($test, \@known_platforms, $cctools_root, $verbose,
                         $src_testroot, $dst_testroot);
    }
  } # foreach my $test (@tests)

  # if necessary, wait for any scheduled tests to complete
  if ($async) {
    while ($CURCHILDREN) {
      &collect_test();
    }
    $pass = $PASS;
  }

  # print summary
  if ($opt_C) {
    printf "### %d of %d tests cleaned\n", $pass, $total;
  } else {
    printf "### %d of %d tests passed (%.1f percent)\n",
           $pass, $total, ($pass / $total) * 100;
  }

  return $pass == $total ? 0 : 1;
}

##############################################################################
#
# usage
#
sub usage {
  (my $basename = $0) =~ s|.*/||;
  print <<USAGE;
usage: $basename [-a] [-C] [-c cctools_root] [-t test] [-v]
  -a        - run tests asynchronously, potentially boosting performance.
              This value is currently on by default.
  -C        - clean all of the local test directories.
  -c <dir>  - use cctools installed in <dir>. The directory must be a root
              filesystem; i.e., one containing ./usr/bin. By default,
              cctools will all run through "xcrun", although individual
              tests have the final say.
  -t <test> - run only the test named <test> from the test repository. Test
              may be the name of a test, or a regex pattern matching one or
              more tests.
  -v        - verbose, print a line for each phase of the test.
USAGE
  return 1;
}
