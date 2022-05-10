# Base.pm
#
# Base.p implements some common utilities used by a number of MachO modules.
# It also provides an abstract superclass for programmatically displaying
# object contents: MachO::Base.

use Devel::StackTrace;

package MachO;

use subs 'MachO::abort';

# nameForValue searches a list of constants and returns the first one which
# represents the specified value. In other words, it does a reverse mapping
# between constant and value.
#
# Arguments are:
#   value     - scalar. the value to find.
#   constants - array ref. The constants to search. Typically this list is
#               defined as a constant list:
#                 use constant Monday => 1;
#                 use constant Tuesday => 2;
#                 use constant LIST => qw(Monday Tuesday);
sub nameForValue {
  my ($value, @constants) = @_;

  foreach my $key (@constants) {
    return $key if ($value eq &$key());
  }
  return undef;
}

sub valueForName {
  my ($name, @constants) = @_;
  foreach my $key (@constants) {
    return &$key() if ($key eq $name);
  }
  return undef;
}

sub abort ($) {
  my ($msg) = @_;
  chomp($msg);
  my $trace = Devel::StackTrace->new;
  my $s = $trace->as_string; # like carp
  die "$msg\n$s\n";
}


# versionStringABCDE takes a 64-bit wide value and converts it to a human
# readable string using the rules laid out in mach-o/loder.h
sub versionStringABCDE {
  my ($version) = @_;
  my $a = ($version >> 40) & 0xffffff;
  my $b = ($version >> 30) & 0x3ff;
  my $c = ($version >> 20) & 0x3ff;
  my $d = ($version >> 10) & 0x3ff;
  my $e = $version & 0x3ff;
  return "$a.$b.$c.$d.$e" if ($e);
  return "$a.$b.$c.$d" if ($d);
  return "$a.$b.$c" if ($c);
  return "$a.$b";
}

# versionStringABCDE takes a 32-bit wide value and converts it to a human
# readable string using the rules laid out in mach-o/loder.h
sub versionStringXYZ {
  my ($version) = @_;
  my $x = ($version >> 16) & 0xffff;
  my $y = ($version >> 8) & 0xff;
  my $z = $version & 0xff;
  return "$x.$y.$z";
}

# readBytes returns a number of bytes from a file handle. it will print a
# message to stderr and return undef on error.
#
# Arguments are:
#
#   $fh   - a file handle
#   $size - the amount of data to read
sub readBytes {
  my ($fh, $size) = @_;

  my ($raw);
  my $readed = sysread($fh, $raw, $size);
  if ($readed != $size) {
    print STDERR "sysread read $readed of $size\n";
    return undef;
  }
  return $raw;
}

# error prints a consistently formatted error message and returns undef.
#
# Arguments are:
#
#   $path - the path or name of file where the error occurred.
#   $fmt  - the message to display, may contain printf format characters.
#   @args - any additional arguments needed by format charaters in $fmt.
sub error {
  my ($path, $fmt, @args) = @_;
  $fmt = "error: $path: " . $fmt . "\n";
  printf STDERR $fmt, @args;
  return undef;
}

sub hashKeysSet {
  my ($a, $b, @keys) = @_;
  foreach my $key (@keys) {
    $a->{$key} = $b->{$key};
  }
}

sub hashKeysMatch {
  my ($a, $b, @keys) = @_;
  foreach my $key (@keys) {
    return 0 if ($a->{$key} ne $b->{$key});
  }
  return 1;
}

sub hashKeysDiffer {
  my ($a, $b, @keys) = @_;
  abort "a is undefined" unless defined $a;
  abort "b is undefined" unless defined $b;
  my @diffs;
  foreach my $key (@keys) {
    next if (!defined($a->{$key}) and !defined($b->{$key}));
    push @diffs, $key if ($a->{$key} ne $b->{$key});
  }
  return @diffs;
}


# MachO::Base is the base class for all Mach-O components. It provides common
# API including a description system.

package MachO::Base;

sub fields {
  my ($self) = @_;
  my @fields;
  return @fields;
}

sub fieldsForPacking {
  my ($self, $mo) = @_;
  return $self->fields();
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  return undef;
}

sub fieldType {
  return "d";
}

sub fieldValue {
  my ($self, $field) = @_;
  return $self->{$field};
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  return undef;
}

sub description {
  my ($self, $verbose) = @_;
  my ($desc) = "";
  foreach my $field ($self->fields())
  {
    my $value;
    if ($verbose) {
      $value = $self->fieldVerboseValue($field);
    }
    if (defined($value)) {
      $desc .= sprintf("%16s %s\n", "$field:", $value);
    } else {
      $value = $self->fieldValue($field);
      MachO::abort "undefined field $field\n" unless defined ($value);
      my $type = $self->fieldType($field);
      if ($type eq "x") {
        $desc .= sprintf("%16s 0x%x\n", "$field:", $value);
      } else {
        $desc .= sprintf("%16s %${type}\n", "$field:", $value);
      }
    }
  }
  return $desc;
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  MachO::abort "missing mach object" unless defined $mo;

  $self->{'_data'} = $data if (defined($data));
  $self->{"_location"} = $offset if defined($offset);

  my @values;
  my $template = $self->fieldsTemplate($mo);
  if (defined($template) and defined ($data)) {
    @values = unpack $template, $data;
  }

  foreach my $field ($self->fieldsForPacking($mo)) {
    my $value = shift @values;
    MachO::abort "$self undefined value for $field\n" unless defined $value;
    $self->{$field} = $value;
  }
}

sub repack {
  my ($self, $mo) = @_;
  MachO::abort "missing mach object" unless defined $mo;

  my (@values);
  foreach my $field ($self->fieldsForPacking($mo)) {
    my $value = $self->{$field};
    MachO::abort "$self undefined value for $field\n" unless defined $value;
    push @values, $self->{$field};
  }

  my $template = $self->fieldsTemplate($mo);
  $self->{'_data'} = pack $template, @values;
}

sub packsize {
  my ($self, $mo) = @_;
  MachO::abort "missing mach object" unless defined $mo;
  my $template = $self->fieldsTemplate($mo);
  my $raw = pack $template;
  return length($raw);
}

sub data {
  my ($self) = @_;
  return ($self->{'_data'}, $self->{'_location'}, length($self->{'_data'}));
}

sub locate {
  my ($self, $mo, $location) = @_;
  $self->{'_location'} = $location;
}

# shallow copy
sub copy {
  my ($self) = @_;
  my $copy = bless {}, ref $self;
  %{$copy} = %{$self};
  return $copy;
}

1;
