# FatObject.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package FatObject;

use FileHandle;

use MachO::Base;
use MachO::CPU;
use MachO::MachObject;
use MachO::ArchiveObject;

sub newWithPath {
  my ($class, $path) = @_;

  my ($self) = bless {}, $class;

  # read the file into memory
  my ($size) = (stat $path)[7];
  my ($file) = FileHandle->new($path);
  my ($data) = &MachO::readBytes($file, $size);
  return undef unless $data;

  $self->{"path"} = $path;
  $self->{"size"} = $size;
  $self->{"data"} = $data;

  # check the magic
  return &MachO::error($path, "file too small for magic") if ($size < 4);

  my ($raw) = substr $data, 0, 4;
  my ($magic) = unpack ("N", $raw);

  if ($magic != MachO::FAT_MAGIC_32) {
    return &MachO::error($path, "not fat, magic: 0x%x", $magic);
  }

  $self->{"bits"} = 32;

  # read the fat header
  my $fhsize = 8;
  my $fasize = 20;
  return &MachO::error($path, "file too small for header") if ($size < $fhsize);

  $raw = substr $data, 0, $fhsize;

  my $fh = FatHeader->new($self, $raw);
  return undef unless $fh;
  $self->{"header"} = $fh;

  # read the fat archs
  my (@fat_archs);
  my ($j) = $fhsize;
  for ($i = 0; $i < $fh->{"nfat_arch"}; ++$i) {

    if ($size < $j + $fasize) {
      return &MachO::error($path, "fat arch $i extends beyond end of file");
    }

    $raw = substr $data, $j, $fasize;

    my $fa = FatArch->new($self, $raw, $j);
    push @fat_archs, $fa;

    $j += $fasize;
  }
  $self->{"fat_archs"} = \@fat_archs;

  return $self;
}

sub description {
  my ($self, $verbose) = @_;
  my $fh = $self->{"header"};
  my $fas = $self->{"fat_archs"};

  my $desc = "Fat Header\n";
  $desc .= $fh->description($verbose);
  my $i = 0;
  foreach my $fa (@$fas) {
    $desc .=  "Fat Arch $i\n";
    foreach my $line (split /\n/, $fa->description($verbose)) {
      $desc .= "    $line\n";
    }
    #     $desc .= $lc->description($verbose);
    ++$i;
  }
  return $desc;
}

sub machObjectCount {
  my ($self) = @_;
  return $self->{"header"}->{"nfat_arch"};
}

sub machObjectAtIndex {
  my ($self, $index) = @_;
  return undef unless ($index < $self->{"header"}->{"nfat_arch"});

  my $fa = ${$self->{"fat_archs"}}[$index];

  my $path = $self->{"path"};
  my $fatsize = $self->{"size"};
  my $offset = $fa->{"offset"};
  my $size = $fa->{"size"};
  my $end = $offset + $size;

  return &MachO::error($path,
                       "fat arch $index starts beyond end of file" .
                       "($offset >= $fatsize)") unless ($offset < $fatsize);
  return &MachO::error($path,
                       "fat arch $index extends beyond end of file " .
                       "($end > $fatsize)") unless ($end <= $fatsize);

  my $raw = substr($self->{"data"}, $offset, $size);
  return MachObject->newWithData($offset, $size, $raw, "$path arch #$index");
}

sub _dataForArchFlag {
  my ($self, $archFlag) = @_;
  my $arch = MachO::archForFlag($archFlag);
  my $fa;
  foreach my $fatArch (@{$self->{"fat_archs"}}) {
    if (&MachO::hashKeysMatch($fatArch, $arch, 'cputype', 'cpusubtype')) {
      $fa = $fatArch;
    }
  }
  return &MachO::error($path, "arch not found for $archFlag")
  unless (defined($fa));

  my $path = $self->{"path"};
  my $fatsize = $self->{"size"};
  my $offset = $fa->{"offset"};
  my $size = $fa->{"size"};
  my $end = $offset + $size;

  return &MachO::error($path,
                       "fat arch $index starts beyond end of file" .
                       "($offset >= $fatsize)") unless ($offset < $fatsize);
  return &MachO::error($path,
                       "fat arch $index extends beyond end of file " .
                       "($end > $fatsize)") unless ($end <= $fatsize);

  my $raw = substr($self->{"data"}, $offset, $size);
  return ($raw, $offset, $size);
}

sub objectForArchFlag {
  my ($self, $archFlag) = @_;
  my $path = $self->{"path"};
  my ($raw, $offset, $size) = $self->_dataForArchFlag($archFlag);
  if (ArchiveObject->dataIsArchive($raw)) {
    return ArchiveObject->newWithData($raw, $path);
  }
  else {
    return MachObject->newWithData($offset, $size, $raw,
                                   "$path arch $archFlag");
  }
  return &MachO::error($path, "bad file type for $archFlag");
}

sub machObjectForArchFlag {
  my ($self, $archFlag) = @_;
  my $path = $self->{"path"};
  my ($raw, $offset, $size) = $self->dataForArchFlag($archFlag);
  return MachObject->newWithData($offset, $size, $raw,
                                 "$path arch $archFlag");
}

1;
