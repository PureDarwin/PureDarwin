# MachHeader.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use MachO::Base;
use MachO::CPU;

# magic
use constant {
  MH_MAGIC    => 0xFEEDFACE,
  MH_MAGIC_32 => 0xFEEDFACE,
  MH_MAGIC_64 => 0xFEEDFACF,
  MH_CIGAM    => 0xCEFAEDFE,
  MH_CIGAM_32 => 0xCEFAEDFE,
  MH_CIGAM_64 => 0xCFFAEDFE,
};

use constant MH_MAGICS =>
qw(MH_MAGIC MH_MAGIC_64 MH_CIGAM MH_CIGAM_64);

# file type
use constant {
  MH_OBJECT      => 0x1,
  MH_EXECUTE     => 0x2,
  MH_FVMLIB      => 0x3,
  MH_CORE        => 0x4,
  MH_PRELOAD     => 0x5,
  MH_DYLIB       => 0x6,
  MH_DYLINKER    => 0x7,
  MH_BUNDLE      => 0x8,
  MH_DYLIB_STUB  => 0x9,
  MH_DSYM        => 0xa,
  MH_KEXT_BUNDLE => 0xb,
};

use constant FILETYPES =>
qw(MH_OBJECT MH_EXECUTE MH_FVMLIB MH_CORE MH_PRELOAD MH_DYLIB MH_DYLINKER
   MH_BUNDLE MH_DYLIB_STUB MH_DSYM MH_KEXT_BUNDLE);

# flags
use constant {
  MH_NOUNDEFS                   => 0x1,
  MH_INCRLINK                   => 0x2,
  MH_DYLDLINK                   => 0x4,
  MH_BINDATLOAD                 => 0x8,
  MH_PREBOUND                   => 0x10,
  MH_SPLIT_SEGS                 => 0x20,
  MH_LAZY_INIT                  => 0x40,
  MH_TWOLEVEL                   => 0x80,
  MH_FORCE_FLAT                 => 0x100,
  MH_NOMULTIDEFS                => 0x200,
  MH_NOFIXPREBINDING            => 0x400,
  MH_PREBINDABLE                => 0x800,
  MH_ALLMODSBOUND               => 0x1000,
  MH_SUBSECTIONS_VIA_SYMBOLS    => 0x2000,
  MH_CANONICAL                  => 0x4000,
  MH_WEAK_DEFINES               => 0x8000,
  MH_BINDS_TO_WEAK              => 0x10000,
  MH_ALLOW_STACK_EXECUTION      => 0x20000,
  MH_ROOT_SAFE                  => 0x40000,
  MH_SETUID_SAFE                => 0x80000,
  MH_NO_REEXPORTED_DYLIBS       => 0x100000,
  MH_PIE                        => 0x200000,
  MH_DEAD_STRIPPABLE_DYLIB      => 0x400000,
  MH_HAS_TLV_DESCRIPTORS        => 0x800000,
  MH_NO_HEAP_EXECUTION          => 0x1000000,
  MH_APP_EXTENSION_SAFE         => 0x02000000,
  MH_NLIST_OUTOFSYNC_WITH_DYLDINFO => 0x04000000,
  MH_SIM_SUPPORT                => 0x08000000,
  MH_DYLIB_IN_CACHE             => 0x80000000,
};

use constant FLAGS =>
qw(MH_NOUNDEFS MH_INCRLINK MH_DYLDLINK MH_BINDATLOAD MH_PREBOUND
   MH_SPLIT_SEGS MH_LAZY_INIT MH_TWOLEVEL MH_FORCE_FLAT MH_NOMULTIDEFS
   MH_NOFIXPREBINDING MH_PREBINDABLE MH_ALLMODSBOUND
   MH_SUBSECTIONS_VIA_SYMBOLS MH_CANONICAL MH_WEAK_DEFINES MH_BINDS_TO_WEAK
   MH_ALLOW_STACK_EXECUTION MH_ROOT_SAFE MH_SETUID_SAFE
   MH_NO_REEXPORTED_DYLIBS MH_PIE MH_DEAD_STRIPPABLE_DYLIB
   MH_HAS_TLV_DESCRIPTORS MH_NO_HEAP_EXECUTION MH_APP_EXTENSION_SAFE
   MH_NLIST_OUTOFSYNC_WITH_DYLDINFO MH_SIM_SUPPORT MH_DYLIB_IN_CACHE);

sub mhMagicName {
  my ($value) = @_;
  return &nameForValue($value, MH_MAGICS);
}

sub filetypeName {
  my ($value) = @_;
  return &nameForValue($value, FILETYPES);
}

sub flagName {
  my ($value) = @_;
  return &nameForValue($value, FLAGS);
}

# MachHeader is an object describing a struct mach_header or
# struct mach_header64 structure.
#
# The mach-header fields are:
#
#   magic       - one of 0xFEEDFACE, 0xFEEDFACF, etc.
#   cputype     - numeric cputype_t value.
#   cpusubtype  - numeric cpusubtype_t value.
#   filetype    - Mach-O file type
#   ncmds       - number of load commands
#   sizeofcmds  - size of the load commands in bytes
#   flags       - Mach-O file flags
#   reserved    - 0, only available for 64-bit mach headers

package MachHeader;

use parent -norequire, MachO::Base;

# todo rename newFromData
sub new {
  my ($class, $mo, $data, $offset) = @_;
  my ($self) = bless {}, $class;

  $self->{'bits'} = $mo->{'bits'};
  if (defined($data)) {
    $self->depack($mo, $data, defined($offset) ? $offset : 0);
  }
  else {
    $self->{'_location'} = 0;
  }

  return $self;
}

sub fields {
  my ($self) = @_;
  my (@fields) = ("magic", "cputype", "cpusubtype", "filetype",
                  "ncmds", "sizeofcmds", "flags");
  if ($self->{'bits'} == 64) {
    push @fields, "reserved";
  }
  return @fields;
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  MachO::abort "missing mach object" unless defined $mo;
  my ($swap) = $mo->{"swap"};
  my ($count) = $mo->{"bits"} == 64 ? 8 : 7;

  return "L${swap}${count}";
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "magic" or
      $field eq "flags") {
    return "x";
  }
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq "magic") {
    return &MachO::mhMagicName($value);
  }
  elsif ($field eq "filetype") {
    return &MachO::filetypeName($value);
  }
  if ($field eq "cputype") {
    return &MachO::cputypeName($value);
  }
  elsif ($field eq "cpusubtype") {
    my $cputype = $self->{"cputype"};
    return &MachO::cpusubtypeName($cputype, $value);
  }
  elsif ($field eq "flags") {
    my $string;
    for (my $bit = 0; $bit < 32; ++$bit) {
      my $flag = (1 << $bit);
      if ($value & $flag) {
        my $name = &MachO::flagName($flag);
        if (defined($name) and defined($string)) {
          $string .= " $name";
        } else {
          $string = $name;
        }
      }
    }
    return $string;
  }
  return $self->SUPER::fieldVerboseValue($field);
}

1;
