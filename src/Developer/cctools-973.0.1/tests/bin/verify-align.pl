#!/usr/bin/perl -w

# if (defined($ENV{"CAULDRON"})) {
#   @ARGV = (
#     "-a",
#     "4096",
#     "/bin/ls",
#   );
# }

use bigint qw/hex/;
use Getopt::Std;
use POSIX qw(sysconf _SC_PAGESIZE);

exit(&main());

sub load_commands {
  my ($path) = @_;
  my ($out);
  my ($cmd) = "xcrun otool-classic -lv '$path'";

  $out = `$cmd`;

  if ($?) {
    die "can't run command: $cmd\n" if ($? == -1);

    my ($status) = $? >> 8;
    die "command failed with status $status: $cmd\n";
  }

  return $out;
}

sub escape {
  my ($x) = @_;
  $x =~ s#([\.\^\*\+\?\(\)\[\{\|\$])#\\$1#g;
  return $x;
}

sub check_alignment {
  my ($pagealign, $load_commands, $inpath) = @_;
  my ($result) = 0;

  # split the otool command into lines
  my (@lines) = split '\n', $load_commands;

  # state for the state machine, data for the data throne.
  my ($cur_path);
  my ($cur_lc);
  my ($cur_cmd);
  my ($cur_segment_name);
  my ($cur_dataoff);
  my ($cur_datasize);

  # walk the otool output, pull out state, and check the segment offsets.
  # all segment offsets (except __LINKEDIT sizes) must be page aligned.
  foreach my $line (@lines) {
    my ($pattern) = &escape($inpath);
    if ($line =~ /^(${pattern}).*:/) {
      ($cur_path = $line) =~ s/://;
      $cur_lc = undef;
      $cur_segment = undef;
    }
    elsif ($line =~ /^Load command/) {
      $cur_lc = $line;
      $cur_segment = undef;
    }
    elsif ($line =~ /^\s+cmd LC/) {
      die "no load command?" unless defined $cur_lc;
      ($cur_cmd = $line) =~ s/.*cmd //;
    }
    elsif ($line =~ /segname/) {
      ($cur_segment_name = $line) =~ s/.* segname //;
    }
    elsif ($line =~ /(vmaddr|vmsize|fileoff|filesize)/)
    {
      # verify the segment alignment
      my ($field) = $1;
      unless (($field eq "filesize" or $field eq "vmsize") and
	      $cur_segment_name eq "__LINKEDIT")
      {
	(my $vstr = $line) =~ s/.*$field\s//;
	my ($value) = $vstr =~ /^0x/ ?
		      hex($vstr) :
		      int($vstr);
	my ($align) = $value ?
		      $value + ($pagealign - (($value - 1) % $pagealign) - 1) :
		      0;
	if ($value != $align) {
	  printf "$cur_path segment $cur_segment_name $field not aligned: " .
		"0x%016llx should be 0x%016llx\n", $value, $align;
	  $result = 1;
	}
      }
    }
    elsif ($line =~ /(dataoff|datasize)/) {
      # verify nothing follows after the code signature
      my ($field) = $1;
      (my $vstr = $line) =~ s/.*$field\s//;
      my ($value) = $vstr =~ /^0x/ ?
                    hex($vstr) :
                    int($vstr);
      if ($field eq "dataoff") {
        $cur_dataoff = $value;
      }
      elsif ($field eq "datasize") {
        $cur_datasize = $value;

        if ($cur_cmd eq "LC_CODE_SIGNATURE") {
          die "no dataoff?" unless defined $cur_dataoff;

          my ($filesize) = -s $inpath;
          my ($cslen) = $cur_dataoff + $cur_datasize;
          if ($filesize < $cslen) {
            print "$cur_path has been truncated: " .
                  "length $filesize should be $cslen\n";
            $result = 1;
          }
          elsif ($filesize > $cslen) {
            my $delta = $filesize - $cslen;
            print "$cur_path has $delta extra bytes after the code " .
                  "signature: length $filesize should be $cslen\n";
            $result = 1;
          }
        }
      }
    }
  }

  return $result;
}

sub main {

  return &usage() unless (@ARGV);
  return &usage() unless getopts('a:');

  my $segalgn = 0;
  my $result = 0;

  if ($opt_a) {
    $segalign = $opt_a =~ /0(x|X)/ ? hex($opt_a) : int($opt_a);
  }
  unless ($segalign) {
    $segalign = sysconf(_SC_PAGESIZE);
  }

  foreach my $path (@ARGV) {
    my $lc = &load_commands($path);
    if (&check_alignment($segalign, $lc, $path)) {
      $result = 1;
    }
  }

  return $result;
}

sub usage {
  (my $progname = $0) =~ s|.*/||;
  print "usage: $progname [-a=align] file...\n";
  return 1;
}
