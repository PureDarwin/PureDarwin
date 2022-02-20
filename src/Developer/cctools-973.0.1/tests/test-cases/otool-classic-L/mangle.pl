#!/usr/bin/perl -w

BEGIN {
  # preferred location for cctools perl modules
  unshift @INC, "../../include";
}

use FileHandle;
use MachO;

exit &main();

sub main {
  # parse args
  my $input = shift @ARGV;
  my $output = shift @ARGV;
  return &usage("missing input") unless ($input);
  return &usage("missing output") unless ($output);
  die "file not found: $input\n" unless ( -e $input );

  # open the mach-o
  my ($obj) = MachO->newWithPath($input);
  my ($perm) = (stat $input)[2] & 07777;
  return 1 unless ($obj);

  # find the load_dylib load commands
  my @cmds;
  foreach my $lc (@{$obj->{"loadcmds"}}) {
    if ($lc->{"cmd"} == MachO::LC_LOAD_DYLIB and
        $lc->{"name"} =~ /lazy\.dylib/)
    {
      push @cmds, $lc;
      print "modifying load command:\n" . $lc->description();
    }
  }

  # modify the file data
  my $data = $obj->{"data"};
  foreach my $lc (@cmds) {
    my $loc = $lc->location();
    my $swap = $obj->{"swap"} ? "<" : "";

    my $raw = pack ("L${swap}", MachO::LC_LAZY_LOAD_DYLIB);
    substr ($data, $loc, 4) = $raw;
  }

  # write out the file to a new location
  my $fh = FileHandle->new(">$output");

  my $wrote = syswrite($fh, $data, length($data));
  if ($wrote != length($data)) {
    print STDERR "can't write to $output: $!\n";
    return 1;
  }
  chmod $perm, $output;

  return 0;
}

sub usage {
  my ($msg) = @_;
  (my $basename = $0) =~ s|.*/||;
  print "$0: $msg\n" if ($msg);
  print "usage: $basename <input> <output>\n";
  return 1;
}
