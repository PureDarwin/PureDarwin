#!/usr/bin/perl -w

use Cwd;

BEGIN {
  # silence perl editor warnings
  $dir = getcwd();
  push @INC, "$dir/../include";

  # preferred location for cctools perl modules
  (my $dir = $0) =~ s|/?[^/]*$||;
  unshift @INC, "$dir/../include";
}

use Getopt::Std;
use MachO;

exit &main();

##############################################################################
#
# main
#

sub main {
  # parse args
  $opt_h = undef;
  $opt_a = undef;
  $opt_c = undef;
  $opt_e = undef;
  $opt_o = undef;
  $opt_v = undef;

  my $input;

  return &usage() unless (@ARGV);
  while (@ARGV) {
    return &usage() unless (getopts('ha:c:e:o:v'));
    return &usage() if ($opt_h);

    if (@ARGV and $ARGV[0] !~ /^-/) {
      return &usage("only one input file can be specified") if defined ($input);
      $input = shift @ARGV;
    }
  }

  my $archFlag = $opt_a;
  my $cpuFlag = $opt_c;
  my $endianFlag = $opt_e;
  my $output = $opt_o;
  my $verbose = $opt_v;
  return &usage("missing input file") unless (defined($input));
  return &usage("missing output file") unless (defined($output));

  # open the mach-o
  die "file not found: $input\n" unless ( -e $input );
  my ($perm) = (stat $input)[2] & 07777;
  my ($obj) = MachO->newWithPath($input);
  die "unable to read $input\n" unless ($obj);

  # get the default state from the mach-o
  my $mh = $obj->{'header'};
  my $swap = $obj->{'swap'};
  my $cputype = $mh->{'cputype'};
  my $cpusubtype = $mh->{'cpusubtype'};

  # finish processing arguments in the context of the target file
  if (defined($archFlag)) {
    my $arch = &MachO::archForFlag($archFlag);
    die "unknown arch flag: $archFlag\n" unless defined ($arch);
    $swap = $arch->{'swap'};
    $cputype = $arch->{'cputype'};
    $cpusubtype = $arch->{'cpusubtype'};
  }
  if (defined($endianFlag)) {
    my $swapMap = {
      "big"    => ">",
      "little" => "<",
      "host"   => "",
      "<"      => "<",
      ">"      => ">",
    };
    $swap = $swapMap->{$endianFlag};
    die "unknown endian flag: $endianFlag\n" unless (defined($swap));
  }
  if (defined($cpuFlag)) {
    my ($cputypeFlag, $cpusubtypeFlag) = split ':', $cpuFlag;
    if (defined($cputypeFlag) and length($cputypeFlag)) {
      $cputype = &MachO::cputypeFromName($cputypeFlag);
      $cputype = $cputypeFlag unless defined($cputype);
    }
    if (defined($cpusubtypeFlag) and length($cpusubtypeFlag)) {
      $cpusubtype = &MachO::cpusubtypeFromName($cputype, $cpusubtypeFlag);
      $cpusubtype = $cpusubtypeFlag unless defined($cpusubtype);
    }
  }

  # reconfigure the mach-o
  if ($verbose) {
    my $swapNameMap = {
      "<"      => "little",
      ">"      => "big",
      ""       => "host",
    };
    my $oldSwapName = $swapNameMap->{$obj->{'swap'}};
    my $newSwapName = $swapNameMap->{$swap};

    my $oldCpuName = &MachO::cputypeName($mh->{'cputype'});
    $oldCpuName = $mh->{'cputype'} unless defined ($oldCpuName);
    my $newCpuName = &MachO::cputypeName($cputype);
    $newCpuName = $cputype unless defined ($newCpuName);

    my $oldSubtypeName = &MachO::cpusubtypeName($mh->{'cputype'},
                                             $mh->{'cpusubtype'});
    $oldSubtypeName = $mh->{'cpusubtype'} unless defined ($oldSubtypeName);
    my $newSubtypeName = &MachO::cpusubtypeName($cputype, $cpusubtype);
    $newSubtypeName = $cpusubtype unless defined ($newSubtypeName);

    print "writing $output:\n";
    print "    swap:       '$oldSwapName' => '$newSwapName'\n";
    print "    cputype:    '$oldCpuName' => '$newCpuName'\n";
    print "    cpusubtype: '$oldSubtypeName' => '$newSubtypeName'\n";
  }

  $obj->{'path'} = $output;
  $obj->{'swap'} = $swap;
  $mh->{'cputype'} = $cputype;
  $mh->{'cpusubtype'} = $cpusubtype;

  # rewrite the mach-o data
  $obj->repack();

  # write out the file to a new location
  my $temp = "$output.$$";
  my $fh = FileHandle->new(">$temp");

  my $data = $obj->{"data"};
  my $wrote = syswrite($fh, $data, length($data));
  if ($wrote != length($data)) {
    print STDERR "can't write to $temp: $!\n";
    unlink $temp;
    return 1;
  }
  chmod $perm, $temp;
  rename $temp, $output;

  return 0;
}

##############################################################################
#
# usage
#
sub usage {
  my ($msg) = @_;
  (my $basename = $0) =~ s|.*/||;
  print "$0: $msg\n" if ($msg);
  print <<USAGE;
usage: $basename [-hv] [-a <arch>] [-c <cputype>:<cpusubtype>] [-e <order>]
                 -o <output> <file>
  -h          - help, print usage and exit.
  -a <arch>   - change <file>'s cpu type and subtype to match those specified
                by <arch>. the output will be in the proper byte order for
                that arch, unless overridden by the -e flag.
  -c <cputype>:<cpusubtype>
              - change <file>'s cpu type and subtype to match those specified
                by <cputype> and <cpusubtype>, separated by a ':'. Either value
                may be missing, in which case the value will not be changed.
                Values may be either symbolic names or decimal numbers.
  -e <order>  - write <file's> mach_header and load_command values in <end>
                byte order. must be one of "big", "little", or "host".
  -o <output> - output file. required.
  -v          - print verbose output.
USAGE
  return 1;
}
