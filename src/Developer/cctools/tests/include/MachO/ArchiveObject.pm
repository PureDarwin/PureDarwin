#!/usr/bin/perl

# package ArchiveObject
#
# ArchiveObject provides API for reading and interpreting archive files.
#
# Currently support for ArchiveFiles limited to what a linker might need in
# order to resolve symbols and retrieve object files. It lacks API for creating
# or modifying ArchiveObjects, and it has no code for serializing objects.
#
# Values
#
#   namefmt - archive name format: 'bsd', 'bsd44', or 'sysv'.
#   offsets - hash reference mapping from file offsets to MachObjects.
#   symbols - hash reference mapping symbols to file offsets.
#
# Methods
#
#   dataIsArchive   - Returns true if data appears to be an archive.
#   newWithData     - Create an ArchiveObject from data.
#   newWithPath     - Create an ArchiveObject from a file on disk.
#   objectForSymbol - Look up a MachObject for a given symbol.
#   pathIsArchive   - Returns true if file appears to be an archive.
#

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package ArchiveObject;

use FileHandle;
use MachO::Base;
use MachO::CPU;
use MachO::MachObject;

# PACKAGE->dataIsArchive($class, $data)
# Returns 1 if $data appears to be an archive, otherwise returns 0.
sub dataIsArchive {
  my ($class, $data) = @_;
  return (substr $data, 0, 8) eq "!<arch>\n";
}

# PACKAGE->pathIsArchive($class, $path)
# Returns 1 if the file at $path appears to be an archive. Returns 0 if the
# file does not appear to be an archive, or if any other error occurs, such
# as file not found, permission denied, i/o error, etc.
sub pathIsArchive {
  my ($class, $path) = @_;

  my $file = FileHandle->new($path);
  return 0 unless defined $file;
  my $data;
  sysread($file, $data, 8);
  return 0 unless defined $data;
  return $class->dataIsArchive($data);
}

# PACKAGE->newWithData($class, $data, $path)
# Reads interprets the contents of $data as an archive and returns an
# ArchiveObject reference. File contents will not be read from $path, it is
# largely provided only for verbose error reporting.
sub newWithData {
  my ($class, $data, $path) = @_;
  my $self = bless {}, $class;

  $self->{'namefmt'} = undef;
  $self->{'offsets'} = {};
  $self->{'symbols'} = {};

  my $datasize = length($data);
  if ($datasize < 8) {
    return &MachO::error($path, "too small for magic");
  }

  if (!$class->dataIsArchive) {
    return &MachO::error($path, "file is not archive");
  }

  my $namefmt;
  my $sysv_strtab;

  my $offset = 8; # size of magic, start of first file.
  my $i = 0;
  while ($offset < $datasize) {
    $i++;

    # read the file header
    my $fileoff = $offset;
    if ($datasize < $offset + 60) {
      return &MachO::error($path,
                           "header for member #$i extends beyond length " .
                           "of file");
    }

    my $raw = substr $data, $offset, 60;
    $offset += 60;

    my ($mname, $mtime, $muid, $mgid, $mmode, $msize, $mend) =
    unpack "A16 A12 A6 A6 a8 A10 A2", $raw;

    # check the filename ...
    #
    # the first filename suggests the format of entries that follow: if the
    # symbol index is named "/" this file is in sysv format. If the symbol
    # index begins with "#1/" this file is an extended bsd format. If the
    # symbol index begins with "__.SYMDEF" it is in bsd format.
    if (!defined($namefmt)) {
      if ($mname =~ m|^/|) {
        $self->{'namefmt'} = $namefmt = "sysv";
      } elsif ($mname =~ m|\#1/|) {
        $self->{'namefmt'} = $namefmt = "bsd44";
      } else {
        $self->{'namefmt'} = $namefmt = "bsd";
      }
    }

    my $filetype;

    # ... sysv filename strtab
    if ($mname =~ m|^// |) {
      $mname = "//";
      $filetype = "sysv string table";
    }
    # ... sysv index
    elsif ($mname =~ m|^/ |) {
      $mname = "/";
      $filetype = "sysv symbol table";
    }
    # ... sysv entry
    elsif ($mname =~ m|^/|) {
      my $begin = $1;
      my $end = index $sysv_strtab, "/", $begin;
      $end = length($sysv_strtab) if ($end eq -1);
      $mname = substr $sysv_strtab, $begin, $end - $begin;
    }
    # ... bsd 4.4 entry
    elsif ($mname =~ m|^\#1/(\d+)|) {
      my $namesize = $1;
      if ($datasize < $offset + $namesize) {
        return &MachO::error($path,
                             "extended name for member #$i extends beyond " .
                             "length of file");
      }
      if ($msize < $namesize) {
        return &MachO::error($path,
                             "extended name for member #$i extends beyond " .
                             "length of member data");
      }
      $raw = substr $data, $offset, $namesize;
      $offset += $namesize;

      $msize -= $namesize;
      $mname = unpack "A*", $raw;
      $mname =~ s|\s*$||;
    }
    # name ...
    else {
      $mname =~ s|\s*$||;
      $mname =~ s|/$|| if ($mname ne "//" and $mname ne "/");
    }

    # process the file data
    $filetype = "data for $mname" unless defined ($filetype);
    if ($datasize < $offset + $msize) {
      return &MachO::error($path, "$filetype extends beyond length of file");
    }
#     $raw = &MachO::readBytes($file, $msize);
#     $read += $msize;
    $raw = substr $data, $offset, $msize;
    $offset += $msize;
    unless (defined ($raw)) {
      return &MachO::error($path, "can't read $filetype");
    }

    # ... sysv strtab
    if ($mname eq "//") {
      $sysv_strtab = $raw;
    }
    elsif ($mname eq "/") {
      return undef unless $self->_readSymdex($raw);
    }
    elsif ($mname eq "__.SYMDEF SORTED" or
           $mname eq "__.SYMDEF") {
      return undef unless $self->_readSymdex($path, $raw);
    }
    else {
      return undef unless $self->_readMember($path, $raw, $fileoff, $mname);
    }
  }

  return $self;
}

# PACKAGE->newWithPath($class, $data)
# Reads the archive file at $path and returns an ArchiveObject reference.
sub newWithPath{
  my ($class, $path) = @_;

  # read the file into memory
  my ($size) = (stat $path)[7];
  my ($fh) = FileHandle->new($path);
  my ($data) = &MachO::readBytes($fh, $size);
  return undef unless $data;

  # finish initialization
  return $class->newWithData($data, $path);
}

sub _guessByteOrder {
  my ($self, $data) = @_;
  my ($size) = length($data);

  foreach my $swap (qw(< >)) {
    my $rlsize = unpack "L${swap}", $data;
    next if ($size - 4 < $rlsize);
    my $stsize = unpack "L${swap}", (substr $data, 4 + $rlsize);
    next if ($size - 8 < $rlsize + $stsize);
    my $d = $size - 8 - $rlsize - $stsize;
    return $swap if ($d eq 0);
  }

  return undef;
}

sub _readSymdex {
  my ($self, $path, $data) = @_;

  # guess byte order based on the structure of the index
  my ($swap) = $self->_guessByteOrder($data);
  unless (defined($swap)) {
    return &MachO::error($path, "can't determine symbol index byte order");
  }

  # isolate the ranlib table and the string table
  my $rlsize = unpack "L${swap}", $data;
  if ($rlsize + 4 > length($data)) {
    return &MachO::error($path,
                         "ranlib table extends beyond length of symbol index");
  }
  if ($rlsize % 8) {
    return &MachO::error($path, "invalid ranlib table size");
  }
  my $rldata = substr $data, 4, $rlsize;

  $data = substr $data, 4 + $rlsize;
  my $stsize = unpack "L${swap}", $data;
  if ($stsize + 4 > length($data)) {
    return &MachO::error($path,
                         "string table extends beyond length of symbol index");
  }

  # it's ok but unusual if the string table size does not include padding,
  # so long as the padding is accounted for in the ar member header. but
  # _guessByteOrder will require the padding be accounted for. So either it
  # has already been checked, or it's not important ...
  my $stdata = substr $data, 4, $stsize;

  # parse the ranlib table
  my @ranlib = unpack "L${swap}*", $rldata;
  my $zero = pack "x";
  while (@ranlib) {
    my $begin = shift @ranlib;
    my $offset = shift @ranlib;
    my $end = index $stdata, $zero, $begin;
    $end = length($stdata) - $begin if ($end eq -1);
    my $symname = substr $stdata, $begin, $end - $begin;
    $self->{'symbols'}->{$symname} = $offset;
  }

  return $self;
}

sub _readMember {
  my ($self, $path, $data, $offset, $mname) = @_;
  my $mo = MachObject->newWithData($offset, 0, $data, $mname);
  return undef unless $mo;
  $self->{'offsets'}->{$offset} = $mo;
  return $self;
}

# OBJ->objectForSymbol($self, $symbol)
# Returns a MachObject from the archive that implements $symbol, or undef if no
# so such MachObject exists.
sub objectForSymbol {
  my ($self, $symname) = @_;
  my $offset = $self->{'symbols'}->{$symname};
  return undef unless defined $offset;
  my $object = $self->{'offsets'}->{$offset};
  return $object;
}

1;
