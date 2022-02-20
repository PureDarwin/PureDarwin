# SymbolTable

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use constant {
  N_STAB => 0xe0,
  N_PEXT => 0x10,
  N_TYPE => 0x0e,
  N_EXT  => 0x01,

  N_UNDF => 0x0,
  N_ABS  => 0x2,
  N_SECT => 0xe,
  N_PBUD => 0xc,
  N_INDR => 0xa,

  NO_SECT => 0,
  MAX_SECT => 255,
};

use constant NTYPES =>
qw(N_UNDF N_ABS N_SECT N_PBUD N_INDR);

use constant {
  REFERENCE_TYPE => 0x7,

  REFERENCE_FLAG_UNDEFINED_NON_LAZY => 0,
  REFERENCE_FLAG_UNDEFINED_LAZY => 1,
  REFERENCE_FLAG_DEFINED => 2,
  REFERENCE_FLAG_PRIVATE_DEFINED => 3,
  REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY => 4,
  REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY => 5,
};

use constant REFERENCETYPES =>
qw(REFERENCE_FLAG_UNDEFINED_NON_LAZY REFERENCE_FLAG_UNDEFINED_LAZY
   REFERENCE_FLAG_DEFINED REFERENCE_FLAG_PRIVATE_DEFINED
   REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY
   REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY);

use constant {
  REFERENCED_DYNAMICALLY => 0x0010,
  N_NO_DEAD_STRIP => 0x0020,
  N_DESC_DISCARDED => 0x0020,
  N_WEAK_REF => 0x0040,
  N_WEAK_DEF => 0x0080,
  N_REF_TO_WEAK => 0x0080,
  N_ARM_THUMB_DEF => 0x0008,
  N_SYMBOL_RESOLVER => 0x0100,
  N_ALT_ENTRY => 0x0200,
  N_COLD_FUNC => 0x0400,
};

use constant NDESCFLAGS =>
qw(REFERENCED_DYNAMICALLY N_NO_DEAD_STRIP N_DESC_DISCARDED N_WEAK_REF N_WEAK_DEF N_REF_TO_WEAK
   N_ARM_THUMB_DEF N_SYMBOL_RESOLVER N_ALT_ENTRY N_COLD_FUNC);

use constant {
  N_GSYM	=> 0x20, # global symbol: name,,NO_SECT,type,0
  N_FNAME	=> 0x22, # procedure name (f77 kludge): name,,NO_SECT,0,0
  N_FUN		=> 0x24, # procedure: name,,n_sect,linenumber,address
  N_STSYM	=> 0x26, # static symbol: name,,n_sect,type,address
  N_LCSYM	=> 0x28, # .lcomm symbol: name,,n_sect,type,address
  N_BNSYM	=> 0x2e, # begin nsect sym: 0,,n_sect,0,address
  N_AST		=> 0x32, # AST file path: name,,NO_SECT,0,0
  N_OPT		=> 0x3c, # emitted with gcc2_compiled and in gcc source
  N_RSYM	=> 0x40, # register sym: name,,NO_SECT,type,register
  N_SLINE	=> 0x44, # src line: 0,,n_sect,linenumber,address
  N_ENSYM	=> 0x4e, # end nsect sym: 0,,n_sect,0,address
  N_SSYM	=> 0x60, # structure elt: name,,NO_SECT,type,struct_offset
  N_SO		=> 0x64, # source file name: name,,n_sect,0,address
  N_OSO		=> 0x66, # object file name: name,,0*,0,st_mtime
  N_LSYM	=> 0x80, # local sym: name,,NO_SECT,type,offset
  N_BINCL	=> 0x82, # include file beginning: name,,NO_SECT,0,sum
  N_SOL		=> 0x84, # #included file name: name,,n_sect,0,address
  N_PARAMS	=> 0x86, # compiler parameters: name,,NO_SECT,0,0
  N_VERSION	=> 0x88, # compiler version: name,,NO_SECT,0,0
  N_OLEVEL	=> 0x8A, # compiler -O level: name,,NO_SECT,0,0
  N_PSYM	=> 0xa0, # parameter: name,,NO_SECT,type,offset
  N_EINCL	=> 0xa2, # include file end: name,,NO_SECT,0,0
  N_ENTRY	=> 0xa4, # alternate entry: name,,n_sect,linenumber,address
  N_LBRAC	=> 0xc0, # left bracket: 0,,NO_SECT,nesting level,address
  N_EXCL	=> 0xc2, # deleted include file: name,,NO_SECT,0,sum
  N_RBRAC	=> 0xe0, # right bracket: 0,,NO_SECT,nesting level,address
  N_BCOMM	=> 0xe2, # begin common: name,,NO_SECT,0,0
  N_ECOMM	=> 0xe4, # end common: name,,n_sect,0,0
  N_ECOML	=> 0xe8, # end common (local name): 0,,n_sect,0,address
  N_LENG	=> 0xfe, # second stab entry with length information
  N_PC		=> 0x30, # global pascal symbol**: name,,NO_SECT,subtype,line
};

# stabs are debugger symbols. Comments above give the conventional use of each
# symbol in terms of the .stabs assembly directive:
#
#   .stabs "n_name", n_type, n_sect, n_desc, n_value
#
# where n_type is the defined constant and not listed in the comment. Other
# fields where not listed are 0.
#
# *:  The OSO stab was historically set n_sect to 0. Modern Mach-O linkers
#     now store the low byte of the Mach-O header's cpusubtype value.
# **: The PC stab is specific to the berkeley pascal compiler, pc(1).

use constant STABS =>
qw(N_GSYM N_FNAME N_FUN N_STSYM N_LCSYM N_BNSYM N_AST N_OPT N_RSYM N_SLINE
   N_ENSYM N_SSYM N_SO N_OSO N_LSYM N_BINCL N_SOL N_PARAMS N_VERSION N_OLEVEL
   N_PSYM N_EINCL N_ENTRY N_LBRAC N_EXCL N_RBRAC N_BCOMM N_ECOMM N_ECOML
   N_LENG N_PC);

sub referenceeTypeName {
  my ($value) = @_;
  return &nameForValue($value, REFERENCETYPES);
}

sub ntypeName {
  my ($value) = @_;
  return &nameForValue($value, NTYPES);
}

sub ndescFlagName {
  my ($value) = @_;
  return &nameForValue($value, NDESCFLAGS);
}

sub stabName {
  my ($value) = @_;
  return &nameForValue($value, STABS);
}

use MachO::Base;
use MachO::MachHeader;

# SymbolTable implements a Mach-O symbol table, pointed at by a LC_SYMTAB
# load command.
#
package SymbolTable;

use parent -norequire, MachO::Base;

sub sizeOfSymbol {
  my ($mo) = @_;
  my $bits = $mo->{'bits'};
  return $bits == 64 ? 16 : 12;
}

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;
  $self->{'object'} = $mo;
  $self->depack($mo, $data, $offset);
  return $self;
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  my $symsize = &sizeOfSymbol($mo);
  my $nsym = length($data) / $symsize;
  my @symbols;
  for (my $i = 0; $i < $nsym; ++$i) {
    my $raw = substr $data, $symsize * $i, $symsize;
    my $symbol = Symbol->newWithData($mo, $raw, $symsize * $i);
    push @symbols, $symbol;
  }
  $self->{'symbols'} = \@symbols;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  my $data = $self->{'_data'};
  foreach my $symbol (@{$self->{'symbols'}}) {
    $symbol->repack($mo);
    my ($raw, $offset, $size) = $symbol->data();
    substr ($data, $offset, $size) = $raw;
  }

  $self->{'_data'} = $data;
}

sub description {
  my ($self, $verbose) = @_;
  my ($desc) = "";
  my ($i) = 0;

  foreach my $symbol (@{$self->{'symbols'}}) {
    $desc .= "Symbol $i\n";
    foreach my $line (split /\n/, $symbol->description($verbose)) {
      $desc .= "    $line\n";
    }
    $i++;
  }
  return $desc;
}

package Symbol;

use parent -norequire, MachO::Base;

sub new {
  my ($class, @values) = @_;
  my $self = bless {}, $class;
  foreach my $field ($self->fields()) {
    $self->{$field} = shift @values;
  }
  return $self;
}

sub newWithData {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;
  $self->{'object'} = $mo;
  $self->depack($mo, $data, $offset);
  return $self;
}

sub fields {
  return qw(n_strx n_type n_sect n_desc n_value);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  my $bits = $mo->{'bits'};
  return ($bits == 64 ?
          "L${swap} C C S${swap} Q${swap}":
          "L${swap} C C S${swap} L${swap}");
}

sub fieldType {
  return "x";
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq "n_strx" && $value) {
    my $mo = $self->{'object'};
    my $strtab = defined($mo) ? $mo->stringTable() : undef;
    return defined($strtab) ? $strtab->stringAtOffset($value) : undef;
  }
  elsif ($field eq "n_type") {
    my $strtab = $self->{'strtab'};
    if ($value & MachO::N_STAB) {
      return &MachO::stabName($value);
    }
    else {
      my $type = $value & MachO::N_TYPE;
      my $name = &MachO::ntypeName($type);

      if ($type & MachO::N_EXT) {
        $name .= " external";
      }
      return $name;
    }
  }
  elsif ($field eq "n_sect" && $value) {
    my $mo = $self->{'object'};
    my $section = defined($mo) ? $mo->sectionAtIndex($value - 1) : undef;
    if (defined($section)) {
      my $scname = $section->{'sectname'};
      my $sgname = $section->{'segname'};
      if (defined($scname) and defined($sgname)) {
        return "($sgname,$scname)";
      }
    }
    return undef;
  }
  elsif ($field eq "n_desc") {
    my $mo = $self->{'object'};
    my $mh = defined($mo) ? $mo->{'header'} : undef;
    my $mh_flags = defined($mh) ? $mh->{'flags'} : 0;
    my $libord = 0;
    my $desc;

    if ($mh_flags & &MachO::MH_TWOLEVEL()) {
      $libord = ($value >> 8) & 0xFF;
      $value = $value & 0xFF;
    }

    my @flags;
    foreach my $flag (MachO::NDESCFLAGS) {
      my $const = "MachO::$flag";
      if ($value & &$const()) {
        push @flags, $flag;
      }
    }
    $desc = join ' ', @flags;

    if ($libord) {
      my $libname = $mo->libraryAtIndex($libord - 1);
      $desc .= "in $libname";
    }
    return $desc;
  }

  return $self->SUPER::fieldVerboseValue($field);
}

1;
