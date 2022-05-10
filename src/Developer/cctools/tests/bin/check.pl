#!/usr/bin/perl -w
#
# check.pl
# cctools
# Michael Trent <mtrent@apple.com>
#
# A little perl script with FileCheck-like ability.
#
# check.pl reads a file of check rules from the command line, and then uses
# those rules against a list of program output from STDIN (or from file).
# Rule lines take the format of:
#
#   <a # comment> <label>[-<suffix]:<regex pattern>
#
# Because each rule is a comment, they can be embedded directly into a shell
# script or makefile, and can follow immediately after the command that
# generates the output under test. E.g.,:
#
#   otool -lv /bin/ls | check.pl $0
#   #CHECK: cmd LC_BUILD_VERSION
#
# The default label is "CHECK", but this can be overridden using "-p" so one
# can have more than one test of checkrules in a single file.
#
# Rules are applied in order, as they are found in the checkfile.
#
# Supported rules are:
#
#   CHECK:       Find the pattern in a line of input. Fails if the pattern is
#                not found in the input under test.
#   CHECK-NEXT:  Apply the pattern to the line immediately following a CHECK
#                rule. A NEXT rule cannot be the first rule in the list. Fails
#                if the applied pattern does not immediately follow the next
#                line in the input under test.
#   CHECK-NOT:   Verify the pattern does not appear before/after a CHECK rule.
#                NOT rules can be the first and/or last rules in the list.
#                Because they only fail if the pattern matches, they assert
#                that a pattern does not appear between two CHECK rules and/or
#                the boundaries the input under test. To verify a token was
#                completely replaced by an operation, you might use:
#
#                  CHECK-NOT: old
#                  CHECK:     new
#                  CHECK-NOT: old
#   CHECK-EMPTY: Verify the next line after a CHECK rule is completely blank,
#                with no whitespace.
#
# check.pl has a self-test mode (check.pl -t) that demonstrates these rules
# in practive (as well as verifying the test engine works properly.)

use FileHandle;
use Getopt::Std;
use POSIX ":sys_wait_h";

my $gPrefix;
my $gVerbose;

my $gSpecials = "^()[]{}\$\@";
my $gEscapes = "";
my %gSpecmap;

exit(&main());

##############################################################################
#
# escape
sub escape {
  my ($line) = @_;

  # initialize our escapes and specmap
  if (length($gSpecials) and !length($gEscapes)) {
    for (split //, $gSpecials) {
      my ($spc) = $_;
      my ($esc) = "\\$spc";
      $gSpecmap{$esc} = $spc;
      $gSpecmap{$spc} = $esc;
      $gEscapes .= $esc;
    }
  }

  $line =~ s/ (
                (\\[$gEscapes]) # escaped specials
              |
                ([$gEscapes])   # un-escaped specials
              )
            /$gSpecmap{$1}/gx;  # replace the entire match

  return $line;
}

##############################################################################
#
# read_checkfile reads through the checkfile and returns a ref to a list
# checks. Each check is composed of a rule and its argument separated by a
# colon character.
sub read_checkfile {
  my ($path, $prefix) = @_;
  my @checks;

  my $file = FileHandle->new($path, "r");
  foreach my $line (<$file>) {
    chomp $line;
    if ($line =~ /\s*\#\s*(${prefix}(-(NOT|NEXT|EMPTY))?)\:(.*)/) {
      my $rule = defined($3) ? $3 : "IS";
      my $pred = defined($4) ? $4 : "";

      $pred = "" if ($rule eq "EMPTY");
      $pred =~ s/^\s*//;
      $pred =~ s/\s*$//;

#       # escape ( ) [ ] { } $ @
#       my $specials = "()[]{}\$\@";
#       my $escapes = "";
#       my %specmap;
#
#       for (split //, $specials) {
#         $spc = $_;
#         $esc = "\\$spc";
#         $specmap{$esc} = $spc;
#         $specmap{$spc} = $esc;
#         $escapes .= $esc;
#       }
#
#       # replace un-escaped specials with escaped specials and vice versa
#       $pred =~ s/ (
#                     (\\[$escapes]) # escaped specials
#                   |
#                     ([$escapes])   # un-escaped specials
#                   )
#                 /$specmap{$1}/gx;  # replace the entire match
      $pred = &escape($pred);

      if ($gVerbose) {
        printf "  RULE : '$rule':'$pred'\n";
      }

      push @checks, "$rule:$pred";
    }
  }

  return \@checks;
}

##############################################################################
#
# apply_pattern_to_line returns 1 if a pattern matches line, and if $gVerbose
# is set, it will print the matching rule to STDOUT.
sub apply_pattern_to_line {
  my ($rule, $pred, $line) = @_;
  my $res;
  if ($rule eq "EMPTY") {
    $res = $line eq "";
    if ($gVerbose and $res) {
      print "  MATCH: '$rule'\n";
    }
  }
  else {
    $res = $line =~ /^\s*${pred}\s*$/;
    if ($gVerbose and $res) {
      print "  MATCH: '$rule':'$pred'\n";
    }
  }

  return $res;
}

##############################################################################
#
# apply_fail prints an error message to STDERR and returns 1.
sub apply_fail {
  my ($rule, $pred, $line, $linesRef, $iline) = @_;
  my $pattern = $rule eq "EMPTY" ? "" : " '$pred'";
  my $label = ($rule eq "IS") ? "$gPrefix" : "$gPrefix-$rule";
  print STDERR "error: failed to match ${label}:${pattern}\n";
  if ($line) {
    print STDERR "       expected: '$pred'\n"
      unless ($rule eq "NOT" or $rule eq "EMPTY");

  }
  if (defined($linesRef)) {
    my ($nline) = scalar @$linesRef;
    while (++$iline < $nline) {
      my ($line) = $linesRef->[$iline];
      print STDERR "  LINES: '$line'\n";
    }
  }
  return 1;
}


##############################################################################
#
# apply_checks_to_lines tests the input lines against the rules found in the
# checkfile.
sub apply_checks_to_lines {
  my ($checksRef, $linesRef) = @_;

  my $ncheck = scalar @$checksRef;
  my $icheck = 0;
  my $nline = scalar @$linesRef;
  my $iline = 0;
  my $ilast = -1;

  while ($iline < $nline)
  {
    # get the line
    my $line = $linesRef->[$iline];
    if ($gVerbose) {
      print "  LINE : '$line'\n";
    }

    # apply each rule, in order.
    for (my $jcheck = $icheck; $jcheck < $ncheck; ++$jcheck)
    {
      my $check = $checksRef->[$jcheck];
      $check =~ /([^:]*):(.*)/;
      my $rule = $1;
      my $pred = $2;

      if ($rule eq "IS") {
        # if this rule matches,
        #   advance the icheck index to the next command
        # in any case, reset and advance to the next line
        if (&apply_pattern_to_line($rule, $pred, $line)) {
          $icheck = $jcheck + 1;
          $ilast = $iline;
        }
        last;
      }
      elsif ($rule eq "NEXT") {
        # if this rule matches,
        #   advance the icheck index to the next command
        #   reset and advance to the next line
        # if this rule doesn't match, fail
        if (&apply_pattern_to_line($rule, $pred, $line)) {
          $icheck = $jcheck + 1;
          $ilast = $iline;
          last;
        }
        else {
          return &apply_fail($rule, $pred, $line, $linesRef, $ilast);
        }
      }
      elsif ($rule eq "NOT") {
        # if this rule matches, fail
        if (&apply_pattern_to_line($rule, $pred, $line)) {
          return &apply_fail($rule, $pred, $line, $linesRef, $ilast);
        }
      }
      elsif ($rule eq "EMPTY") {
        # if this rule does not match, fail
        # otherwise,
        #   advance the icheck index to the next command
        #   reset and advance to the next line
        unless (&apply_pattern_to_line($rule, $pred, $line)) {
          return &apply_fail($rule, $pred, $line, $linesRef, $ilast);
        }
        $icheck = $jcheck + 1;
        last;
      }
      else {
        printf STDERR "unkown rule type: $rule\n";
        return -1;
      }
    }

    # if we have reached the end of the list of checks, break.
    if ($icheck >= $ncheck) {
      last
    }

    # advance to the next line of input
    #$ilast = $iline;
    $iline += 1;
  }

  # if any rules remain, make sure they're optional (NOT)
  for (my $jcheck = $icheck; $jcheck < $ncheck; ++$jcheck)
  {
    my $check = $checksRef->[$jcheck];
    $check =~ /([^:]*):(.*)/;
    my $rule = $1;
    my $pred = $2;
    unless ($rule eq "NOT") {
      return &apply_fail($rule, $pred, undef, $linesRef, $ilast);
    }
  }

  return 0;
}

##############################################################################
#
# self_test_fork forks a copy of check.pl to run the specified test. it will
# block until the child process terminates. test output (STDERR, STDOUT) will
# be printed to STDOUT, and the exit-status of the test will be returned.
sub self_test_fork {
  my ($test) = @_;

  print "self_test: $test\n";

  # set up a common file handle
  my $testInput = new FileHandle or die "can't create write handle";
  my $testOutput = new FileHandle or die "can't create read handle";
  pipe my $childInput, $testInput or die "can't create read handle";
  pipe $testOutput, my $childOutput or die "can't create write handle";

  # fork and exec a child process to run our test
  my $pid = fork();
  unless ($pid) {
    close $testInput;
    close $testOutput;
    open(STDIN, "<&=" . fileno($childInput)) or die "can't redirect stdin";
    open(STDOUT, "<&=" . fileno($childOutput)) or die "can't redirect stdout";
    open(STDERR, "<&=" . fileno($childOutput)) or die "can't redirect stderr";
    my $v = $gVerbose ? "-v" : "";
    exec "$0 $v -T -p $test $0";
    die "unreachable";
  }

  close $childInput;
  close $childOutput;

  # send the test input to the test tool
  print $testInput "A\n";
  print $testInput "B\n";
  print $testInput "C\n";
  print $testInput "D\n";
  print $testInput "\n";
  print $testInput "E\n";
  close $testInput;

# Here be the test rules! Yarr!
# MATCH: B
# MATCH: D
# MATCHERR: A
# MATCHERR: F
# NEXT: A
# NEXT-NEXT: B
# NEXTERR: A
# NEXTERR-NEXT: C
# NOT-NOT: B
# NOT: A
# NOT-NEXT: B
# NOT-NOT: D
# NOT: C
# NOT2-NOT: F
# NOT2: C
# NOT2-NOT: F
# NOTERR-NOT: B
# NOTERR: A
# NOTERR-NOT: B
# NOTERR: C
# EMPTY: D
# EMPTY-EMPTY:
# EMPTYERR: E
# EMPTYERR-EMPTY:

  # wait for the test tool to exit
  wait;
  my $status = $?;
  $status = $status == -1 ? $status : $status >> 8;

  # print test output
  while (<$testOutput>) {
    print;
  }
  close $testOutput;

  # swallow expected errors. is there a better way to do this?
  if ($status and $test =~ /ERR/) {
    $status = 0;
    printf "error is expected, passing.\n";
  }

  return $status;
}

##############################################################################
#
# self_test tests check.pl by feeding a set of test data to a fork/execed
# copy of itself. In this way, self_test fully tests the check.pl driver and
# implementation.
sub self_test {
  my ($unused) = @_;
  for my $test ( "MATCH", "MATCHERR", "NEXT", "NEXTERR", "NOT", "NOT2",
                 "NOTERR", "EMPTY", "EMPTYERR" ) {
    if (&self_test_fork($test)) {
      print STDERR "FAIL\n";
      return 1;
    }
  }

  return 0;
}

##############################################################################
#
sub escape_file {
  my ($fh) = @_;
  my ($hex) = "[[:xdigit:]]";
  my ($ehex) = &escape($hex);
  my ($next) = "";
  my ($specials) = "*+?";

  for my $line (<$fh>) {
    # escape pattern specials
    $line =~ s/([$specials])/\\$1/g;

    # replace hex-looking numbers with regex
    if ($opt_x) {
      $line =~ s/${hex}{4,}/$ehex\+/g;
    }

    print "# ${opt_p}${next}: $line";
    if ($opt_n and !$next) {
      $next = "-NEXT";
    }
  }
}

##############################################################################
#
sub main {
  # read the options
  #
  # because getopts stops when it encounters a non-flag file, we'll pull out
  # non-options first, check for options, and then look at those files again.
  return &usage() unless (@ARGV);

  # read the options and set globals
  $opt_p = "CHECK";
  $opt_x = undef;
  $opt_n = undef;
  my (@files);
  while (scalar @ARGV) {
    return &usage() unless getopts('ei:np:tTvx');
    push @files, shift @ARGV if (scalar @ARGV);
  }

  $gPrefix = $opt_p;
  $gVerbose = $opt_v;
  @ARGV = @files;

  # -e and -t mode cannot be specified together
  if ($opt_t and $opt_e) {
    print STDERR "error: -e and -t may not be combined\n";
    return 1;
  }

  # handle self test mode
  if ($opt_t) {
    &usage() if ($opt_t or $opt_p or scalar @ARGV);
    return &self_test($self_test);
  }

  # if this is check mode, verify we have a checkfile
  unless ($opt_e) {
    return &usage() unless (scalar @ARGV == 1);
  }

  # turn off file handle buffering so that the self_test output mingles results
  # and errors correctly.
  if (defined($opt_T)) {
    my $old_fh = select(STDOUT);
    $| = 1;
    select(STDERR);
    $| = 1;
    select($old_fh);
  }

  # verify our input files
  my $file = shift @ARGV;
  unless ($opt_e) {
    unless ( -f "$file" ) {
      print STDERR "error: file not found: $file\n";
      return 1;
    }
  }
  if ($opt_i) {
    unless ( -f "$opt_i" ) {
      print STDERR "error: file not found: $opt_i\n";
      return 1;
    }
  }

  my $input_fh = $opt_i ? FileHandle->new($opt_i, "r") :
                          FileHandle->new_from_fd(0, "r");
  unless ($input_fh) {
    if ($opt_i) {
      print STDERR "error: cannae read $opt_i\n";
      return 1;
    } else {
      print STDERR "error: cannae read STDIN?\n";
      return 1;
    }
  }

  # handle escape mode
  if ($opt_e) {
    return &escape_file($input_fh);
  }

  # handle check mode
  $gPrefix = $opt_p;
  $gVerbose = $opt_v ? $opt_v : "";

  # read the checkfile
  my $checksRef = &read_checkfile($file, $opt_p);
  die "error: no ${opt_p} lines found in checkfile\n"
    unless(scalar @$checksRef);

  # verify the first check is not a "NEXT" rule.
  # should this move somewhere else?
  my $firstCheck = @$checksRef[0];
  $firstCheck =~ /([^:]*):(.*)/;
  my $rule = $1;
  die "error: first rule cannot be '-NEXT'\n"
    if ($rule eq "NEXT");

  # read the test input from STDIN or from -i
  my @input;
  while (<$input_fh>) {
    chomp;
    push @input, $_;
  }

  # do the work!
  my $res = &apply_checks_to_lines($checksRef, \@input);
  if ($res and !defined($opt_T)) {
    print STDERR "FAIL\n";
  }

  return $res ? 1 : 0;
}

##############################################################################
#
sub usage {
  (my $basename = $0) =~ s|.*/||;
  print <<USAGE;
usage: $basename [-i <input>] [-p <prefix>] [-v] checkfile
       $basename [-i <input>] [-p <prefix>] -e [-x]
       $basename -t [-v]
    -e          - write out input with a test prefix appended to each line.
    -i <input>  - read the input under test from <input> rather than STDIN.
    -n          - used with -e. Append "-NEXT" to the text prefix.
    -p <prefix> - use <prefix> instead of CHECK for rule labels.
    -t          - run $basename in "self-test" mode.
    -v          - print verbose output from the rules matching engine.
    -x          - used with -e. escape large hexadecimal numbers.
USAGE
  return 1;
}
