#!/usr/bin/perl

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use MachO::Base;
use MachO::CPU;

use constant {
  FAT_MAGIC    => 0xCAFEBABE,
  FAT_MAGIC_32 => 0xCAFEBABE,
  FAT_MAGIC_64 => 0xCAFEBABF,
  FAT_CIGAM    => 0xBEBAFECA,
  FAT_CIGAM_32 => 0xBEBAFECA,
  FAT_CIGAM_64 => 0xBFBAFECA,
};

use constant FAT_MAGICS =>
qw(FAT_MAGIC FAT_MAGIC_64 FAT_CIGAM FAT_CIGAM_64);

###

package FatHeader;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $obj, $data, $offset) = @_;
  my ($self) = bless {}, $class;

  $self->depack($obj, $data, defined($offset) ? $offset : 0);

  return $self;
}

sub fields {
  return qw(magic nfat_arch);
}

sub fieldsTemplate {
  return "N2";
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "magic") {
    return "x";
  }
  return $self->SUPER::fieldType($field);
}

###

package FatArch;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $obj, $data, $offset) = @_;
  my ($self) = bless {}, $class;

  $self->depack($obj, $data, $offset);
  $self->{"_location"} = $offset;

  return $self;
}

sub fields {
  return qw(cputype cpusubtype offset size align);
}

sub fieldsTemplate {
  return "N5";
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "align") {
    return "s";
  }
  return $self->SUPER::fieldType($field);
}

sub fieldValue {
  my ($self, $field) = @_;
  if ($field eq "align") {
    my $value = $self->{$field};
    my $pow = 2 ** $value;
    return sprintf "2^${value} (${pow})";
  }
  return $self->SUPER::fieldValue($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  if ($field eq "cputype") {
    my $value = $self->{$field};
    return &MachO::cputypeName($value);
  }
  elsif ($field eq "cpusubtype") {
    my $cputype = $self->{"cputype"};
    my $value = $self->{$field};
    return &MachO::cpusubtypeName($cputype, $value);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

1;
