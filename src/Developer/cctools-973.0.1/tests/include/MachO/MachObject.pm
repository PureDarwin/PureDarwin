# MachObject.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachObject;

use FileHandle;

use MachO::Base;
use MachO::CPU;
use MachO::MachHeader;
use MachO::LoadCommands;
use MachO::SymbolTable;
use MachO::StringTable;

sub new {
  my ($class) = @_;
  my ($self) = bless {}, $class;
  $self->{'loadcmds'} = [];
  return $self;
}

sub newForArch {
  my ($class, $arch) = @_;
  my ($self) = $class->new();

  $self->{'arch'} = $arch;
  $self->{'bits'} = $arch->{'bits'};
  $self->{'swap'} = $arch->{'swap'};

  return $self;
}

sub newWithPath {
  my ($class, $path) = @_;
  my ($self) = $class->new();

  # read the file into memory
  my ($size) = (stat $path)[7];
  my ($fh) = FileHandle->new($path);
  my ($data) = &MachO::readBytes($fh, $size);
  return undef unless $data;

  # finish initialization
  return $self->initialize($path, $data, 0);
}

sub newWithData {
  my ($class, $offset, $size, $data, $name) = @_;
  my ($self) = $class->new();
  return $self->initialize($name, $data, $offset);
}

sub initialize {
  my ($self, $path, $data, $offset) = @_;

  my $size = length($data);
  $self->{'path'} = $path;
  $self->{'size'} = $size;
  $self->{'offset'} = $offset;
  $self->{'data'} = $data;

  # check the magic
  return &MachO::error($path, "file too small for magic") if ($size < 4);

  my ($raw) = substr $data, 0, 4;
  my ($magic) = unpack ("L", $raw);

  my ($is64, $isSwap);
  if ($magic == MachO::MH_MAGIC_64) {
    $is64 = 1;
  }
  elsif ($magic == MachO::MH_CIGAM_64) {
    $isSwap = $is64 = 1;
  }
  elsif ($magic == MachO::MH_MAGIC_32) {
  }
  elsif ($magic == MachO::MH_CIGAM_32) {
    $isSwap = 1;
  }
  else {
    return &MachO::error($path, "not mach-o, magic: 0x%x", $magic);
  }

  my $swap = "";
  if ($isSwap) {
    my $big = pack("L", 1) eq pack("N", 1);
    $swap = $big ? "<" : ">";
  }
  $self->{"swap"} = $swap;
  $self->{"bits"} = $is64 ? 64 : 32;

  # read the mach_header
  my ($count) = $is64 ? 8 : 7;
  my ($mhsize) = 4 * $count;

  return &MachO::error($path, "file too small for header") if ($size < $mhsize);

  $raw = substr $data, 0, $mhsize;

  my $mh = MachHeader->new($self, $raw);
  return undef unless $mh;
  $self->{"header"} = $mh;

  # read the load commands
  my (@loadcmds);
  my ($j) = $mhsize;
  for ($i = 0; $i < $mh->{"ncmds"}; ++$i) {

    if ($size < $j + 8) {
      return &MachO::error($path, "load command $i extends beyond end of file");
    }

    if ($mhsize + $mh->{"sizeofcmds"} < $j + 8) {
      return &MachO::error($path, "load command $i extends beyond sizeofcmds");
    }

    $raw = substr $data, $j, 8;
    my ($cmd, $cmdsize) = unpack ("L${swap}${count}", $raw);

    if ($size < $j + $cmdsize) {
      return &MachO::error($path, "load command $i extends beyond end of file");
    }

    if ($mhsize + $mh->{"sizeofcmds"} < $j + $cmdsize) {
      return &MachO::error($path, "load command $i extends beyond sizeofcmds");
    }

    $raw = substr $data, $j, $cmdsize;

    my $lc = LoadCommand->new($self, $cmd, $raw, $j);
    push @loadcmds, $lc;

    $j += $cmdsize;
  }
  $self->{"loadcmds"} = \@loadcmds;

  return $self;
}

sub description {
  my ($self, $verbose) = @_;
  my $mh = $self->{"header"};
  my $lcs = $self->{"loadcmds"};

  my $desc = "Mach Header\n";
  $desc .= $mh->description($verbose);
  my $i = 0;
  foreach my $lc (@$lcs) {
    $desc .=  "Load Command $i\n";
    foreach my $line (split /\n/, $lc->description($verbose)) {
      $desc .= "    $line\n";
    }
    #     $desc .= $lc->description($verbose);
    ++$i;
  }
  return $desc;
}

sub repack {
  my ($self) = @_;
  my $data = $self->{'data'};

  $data = "" unless defined $data;

  my $mh = $self->{'header'};
  $mh->repack($self);
  my ($raw, $loc, $len) = $mh->data();
  MachO::abort "$mh undefined pack data" unless defined $raw;
  MachO::abort "$mh undefined pack location" unless defined $loc;
  MachO::abort "$mh undefined pack length" unless defined $len;
  substr($data, $loc, $len) = $raw;

  my $lcs = $self->{"loadcmds"};
  foreach my $lc (@$lcs) {
    my $name = MachO::loadCommandName($lc->{'cmd'});
    $lc->repack($self);
    my ($raw, $loc, $len) = $lc->data();
    MachO::abort "$lc ($name) undefined pack data" unless defined $raw;
    MachO::abort "$lc ($name) undefined pack location" unless defined $loc;
    MachO::abort "$lc ($name) undefined pack length" unless defined $len;
    substr($data, $loc, $len) = $raw;
  }

  $self->{'data'} = $data;
}

sub findLoadCommands {
  my ($self, @cmds) = @_;
  my (@lcs);
  my %cmdMap = map { $_ => 1 } @cmds;

  foreach my $lc (@{$self->{'loadcmds'}}) {
    if ($cmdMap{$lc->{'cmd'}}) {
      push @lcs, $lc;
    }
  }
  return wantarray ? @lcs : $lcs[0];
}

sub stringTable {
  my ($self) = @_;

  my $strtab = $self->{'strtab'};
  unless (defined($strtab)) {
    my $lc = $self->findLoadCommands(MachO::LC_SYMTAB);
    my $raw = substr $self->{'data'}, $lc->{'stroff'}, $lc->{'strsize'};
    $strtab = StringTable->new($self, $raw, $lc->{'stroff'});
    $self->{'strtab'} = $strtab;
  }

  return $strtab;
}

sub symbolTable {
  my ($self) = @_;

  my $symtab = $self->{'symtab'};
  unless (defined($symtab)) {
    my $lc = $self->findLoadCommands(MachO::LC_SYMTAB);
    my $nsym = $lc->{'nsyms'};
    my $symsize = SymbolTable::sizeOfSymbol($self);
    my $raw = substr $self->{'data'}, $lc->{'symoff'}, $symsize * $nsym;
    $symtab = SymbolTable->new($self, $raw, $lc->{'symoff'});
    $self->{'symtab'} = $symtab;
  }

  return $symtab;
}

sub sections {
  my ($self) = @_;

  my $sections = $self->{'sections'};
  unless (defined($sections)) {
    my @sections;
    foreach my $lc ($self->findLoadCommands(MachO::LC_SEGMENT,
                                            MachO::LC_SEGMENT_64))
    {
      push @sections, @{$lc->{'sections'}};
    }
    $sections = \@sections;
    $self->{'sections'} = $sections;
  }
  return $sections;
}

sub sectionAtIndex {
  my ($self, $index) = @_;
  my $sections = $self->{'sections'};
  return $sections->[$index];
}

sub libraryAtIndex {
  my ($self, $index) = @_;

  my $libraries = $self->{'libraries'};
  unless (defined($libraries)) {
    my @libraries;
    foreach my $lc ($self->findLoadCommands(MachO::LC_LOAD_DYLIB,
                                            MachO::LC_LOAD_WEAK_DYLIB,
                                            MachO::LC_REEXPORT_DYLIB,
                                            MachO::LC_LOAD_UPWARD_DYLIB,
                                            MachO::LC_LAZY_LOAD_DYLIB))
    {
      $lc->{'name'} =~ m|/?([^/]*)$|;
      my $name = $1;
      push @libraries, $1;
    }
    $libraries = \@libraries;
    $self->{'libraries'} = $libraries;
  }
  return $libraries->[$index];
}

sub store {
  my ($self, $subdata, $loc, $len) = @_;

  MachO::abort "missing subdata" unless defined $subdata;
  MachO::abort "missing loc" unless defined $loc;
  MachO::abort "missing len" unless defined $len;

  my $data = $self->{'data'};
  $data = "" unless defined $data;

  my $size = length($data);
  if ($loc > $size) {
    my $pad = $loc - $size;
    $data .= pack "x${pad}";
  }
  substr ($data, $loc, $len) = $subdata;
  $self->{'data'} = $data;
}

sub pointerAlignment {
  my ($self) = @_;
  return $self->{'bits'} eq 64 ? 3 : 2;
}

sub segmentAlignment {
  my ($self) = @_;
  my $segalign = 15; # 2**15 or 0x8000
  my $filetype = $self->{'header'}->{'filetype'};
  my $ptralign = $self->pointerAlignment();

  foreach my $lc ($self->findLoadCommands(MachO::LC_SEGMENT,
                                          MachO::LC_SEGMENT_64)) {
    my $al;
    if ($filetype eq MachO::MH_OBJECT) {
      # choose the largest section alignment.
      $al = $ptralign;
      foreach my $section (@{$lc->{'sections'}}) {
        $al = $ection->{'align'} if ($al < $section->{'align'});
      }
    }
    else {
      # choose the smallest segment alignment, being wary of 0
      $al = $segalign;
      my $addr = $lc->{'vmaddr'};
      if ($addr) {
        $al = 0;
        my $bit = 1;
        while (0 == ($addr & $bit)) {
          $bit <<= 1;
          $al += 1;
        }
        $al = $al > 15 ? 15 : $al;
        $al = $al < 2 ? 2 : $al
      }
    }
    $segalign = $al if ($al < $segalign);
  }

  return $segalign;
}

1;
