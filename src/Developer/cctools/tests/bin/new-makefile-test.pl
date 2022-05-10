#!/usr/bin/perl -w
# new-test
# cctools
# Michael Trent <mtrent@apple.com>

use Cwd 'realpath';
use FileHandle;
use Getopt::Std;

exit &main();

##############################################################################
#
# tests_dir_path returns the tests repository or undef if the tests directory
# cannot be derived from the tool path.
#
sub tests_dir_path {
  my ($test_cases) = "test-cases";
  (my $dir = $0) =~ s|/[^/]*$||;
  while ($dir) {
    my $tests_dir = "${dir}/${test_cases}";
    return $tests_dir if ( -e $tests_dir );
    $dir =~ s|/?[^/]*$||;
  }
  return $test_cases;
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


sub main {
  # process args
  $opt_r = undef;
  $opt_h = undef;

  return &usage() unless (getopts('hr:'));

  if ($opt_h) {
    return &usage();
  }

  my ($tests_dir) = $opt_r;

  return &usage("no test name specified") unless (scalar(@ARGV));
  return &usage("multiple test names specified") unless scalar(@ARGV == 1);

  my ($test_name) = shift @ARGV;


  # set up the test directory
  $tests_dir = &tests_dir_path() unless(defined($tests_dir));
  $tests_dir = realpath $tests_dir;

  # read the tests from the test directory
  my (@tests) = &find_tests($tests_dir);
  my %tests = map { $_ => $_ } @tests;

  die "test '%s' already exists in $tests_dir\n"
    if (defined($tests{$test_name}));

  # create the test directory
  print "making:\n";
  print "\t$tests_dir/$test_name\n";
  system "mkdir $tests_dir/$test_name"
    and die "cannot make directory $tests_dir/$test_name\n";

  # create the makefile
  print "\t$tests_dir/$test_name/Makefile\n";
  my ($fh) = new FileHandle(">$tests_dir/$test_name/Makefile");
  print $fh <<MAKEFILE;
# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include \${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	echo PASS

clean:
	true

MAKEFILE

  return 0;
}

##############################################################################
#
# usage
#
sub usage {
  my ($err) = @_;
  (my $basename = $0) =~ s|.*/||;

  print STDERR "$err\n" if ($err);
  print <<USAGE;
usage: $basename [-r tests_dir] test_name
    -r <dir>  - create the test in the test repository <dir>. By default,
                the test repository is a directory named "test-cases" in the
	        same directory as $basename.
USAGE
  return 1;
}
