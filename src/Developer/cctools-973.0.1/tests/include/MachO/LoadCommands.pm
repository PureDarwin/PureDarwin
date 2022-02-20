# LoadCommands.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use MachO::Base;

# load command constants
use constant {
  LC_REQ_DYLD => 0x80000000,
};

# load commands
use constant {
  LC_SEGMENT            => 0x1,
  LC_SYMTAB             => 0x2,
  LC_SYMSEG             => 0x3,
  LC_THREAD             => 0x4,
  LC_UNIXTHREAD         => 0x5,
  LC_LOADFVMLIB         => 0x6,
  LC_IDFVMLIB           => 0x7,
  LC_IDENT              => 0x8,
  LC_FVMFILE            => 0x9,
  LC_PREPAGE            => 0xa,
  LC_DYSYMTAB           => 0xb,
  LC_LOAD_DYLIB         => 0xc,
  LC_ID_DYLIB           => 0xd,
  LC_LOAD_DYLINKER      => 0xe,
  LC_ID_DYLINKER        => 0xf,
  LC_PREBOUND_DYLIB     => 0x10,
  LC_ROUTINES           => 0x11,
  LC_SUB_FRAMEWORK      => 0x12,
  LC_SUB_UMBRELLA       => 0x13,
  LC_SUB_CLIENT         => 0x14,
  LC_SUB_LIBRARY        => 0x15,
  LC_TWOLEVEL_HINTS     => 0x16,
  LC_PREBIND_CKSUM      => 0x17,
  LC_LOAD_WEAK_DYLIB    => 0x18 | LC_REQ_DYLD,
  LC_SEGMENT_64	        => 0x19,
  LC_ROUTINES_64        => 0x1a,
  LC_UUID               => 0x1b,
  LC_RPATH              => 0x1c | LC_REQ_DYLD,
  LC_CODE_SIGNATURE     => 0x1d,
  LC_SEGMENT_SPLIT_INFO => 0x1e,
  LC_REEXPORT_DYLIB     => 0x1F | LC_REQ_DYLD,
  LC_LAZY_LOAD_DYLIB    => 0x20,
  LC_ENCRYPTION_INFO    => 0x21,
  LC_DYLD_INFO          => 0x22,
  LC_DYLD_INFO_ONLY     => 0x22 | LC_REQ_DYLD,
  LC_LOAD_UPWARD_DYLIB  => 0x23 | LC_REQ_DYLD,
  LC_VERSION_MIN_MACOSX => 0x24,
  LC_VERSION_MIN_IPHONEOS => 0x25,
  LC_FUNCTION_STARTS    => 0x26,
  LC_DYLD_ENVIRONMENT   => 0x27,
  LC_MAIN               => 0x28 | LC_REQ_DYLD,
  LC_DATA_IN_CODE       => 0x29,
  LC_SOURCE_VERSION     => 0x2A,
  LC_DYLIB_CODE_SIGN_DRS => 0x2B,
  LC_ENCRYPTION_INFO_64 => 0x2C,
  LC_LINKER_OPTION      => 0x2D,
  LC_LINKER_OPTIMIZATION_HINT => 0x2E,
  LC_VERSION_MIN_TVOS   => 0x2F,
  LC_VERSION_MIN_WATCHOS => 0x30,
  LC_NOTE               => 0x31,
  LC_BUILD_VERSION      => 0x32,
  LC_DYLD_EXPORTS_TRIE  => 0x33 | LC_REQ_DYLD,
  LC_DYLD_CHAINED_FIXUPS => 0x34 | LC_REQ_DYLD,
};

use constant LOADCOMMANDS =>
qw(LC_SEGMENT LC_SYMTAB LC_SYMSEG LC_THREAD LC_UNIXTHREAD LC_LOADFVMLIB
   LC_IDFVMLIB LC_IDENT LC_FVMFILE LC_PREPAGE LC_DYSYMTAB LC_LOAD_DYLIB
   LC_ID_DYLIB LC_LOAD_DYLINKER LC_ID_DYLINKER LC_PREBOUND_DYLIB LC_ROUTINES
   LC_SUB_FRAMEWORK LC_SUB_UMBRELLA LC_SUB_CLIENT LC_SUB_LIBRARY
   LC_TWOLEVEL_HINTS LC_PREBIND_CKSUM LC_LOAD_WEAK_DYLIB LC_SEGMENT_64
   LC_ROUTINES_64 LC_UUID LC_RPATH LC_CODE_SIGNATURE LC_SEGMENT_SPLIT_INFO
   LC_REEXPORT_DYLIB LC_LAZY_LOAD_DYLIB LC_ENCRYPTION_INFO LC_DYLD_INFO
   LC_DYLD_INFO_ONLY LC_LOAD_UPWARD_DYLIB LC_VERSION_MIN_MACOSX
   LC_VERSION_MIN_IPHONEOS LC_FUNCTION_STARTS LC_DYLD_ENVIRONMENT LC_MAIN
   LC_DATA_IN_CODE LC_SOURCE_VERSION LC_DYLIB_CODE_SIGN_DRS
   LC_ENCRYPTION_INFO_64 LC_LINKER_OPTION LC_LINKER_OPTIMIZATION_HINT
   LC_VERSION_MIN_TVOS LC_VERSION_MIN_WATCHOS LC_NOTE LC_BUILD_VERSION
   LC_DYLD_EXPORTS_TRIE LC_DYLD_CHAINED_FIXUPS);

# segment flags
use constant {
  SG_HIGHVM             => 0x1,
  SG_FVMLIB             => 0x2,
  SG_NORELOC            => 0x4,
  SG_PROTECTED_VERSION_1=> 0x8,
  SG_READ_ONLY          => 0x10,
};

use constant SEGMENTFLAGS =>
qw(SG_HIGHVM SG_FVMLIB SG_NORELOC SG_PROTECTED_VERSION_1 SG_READ_ONLY);

# segment protect
use constant {
  SG_PROTNONE    => 0,
  SG_PROTREAD    => 0x1,
  SG_PROTWRITE   => 0x2,
  SG_PROTEXECUTE => 0x4,
};

# section constants
use constant {
  SECTION_TYPE          => 0x000000FF,
  SECTION_ATTRIBUTES    => 0xFFFFFF00,
};

# section types
use constant {
  S_REGULAR                     => 0x0,
  S_ZEROFILL                    => 0x1,
  S_CSTRING_LITERALS            => 0x2,
  S_4BYTE_LITERALS              => 0x3,
  S_8BYTE_LITERALS              => 0x4,
  S_LITERAL_POINTERS            => 0x5,
  S_NON_LAZY_SYMBOL_POINTERS    => 0x6,
  S_LAZY_SYMBOL_POINTERS        => 0x7,
  S_SYMBOL_STUBS                => 0x8,
  S_MOD_INIT_FUNC_POINTERS      => 0x9,
  S_MOD_TERM_FUNC_POINTERS      => 0xa,
  S_COALESCED                   => 0xb,
  S_GB_ZEROFILL                 => 0xc,
  S_INTERPOSING                 => 0xd,
  S_16BYTE_LITERALS             => 0xe,
  S_DTRACE_DOF                  => 0xf,
  S_LAZY_DYLIB_SYMBOL_POINTERS  => 0x10,
  S_THREAD_LOCAL_REGULAR        => 0x11,
  S_THREAD_LOCAL_ZEROFILL       => 0x12,
  S_THREAD_LOCAL_VARIABLES      => 0x13,
  S_THREAD_LOCAL_VARIABLE_POINTERS => 0x14,
  S_THREAD_LOCAL_INIT_FUNCTION_POINTERS=> 0x15,
  S_INIT_FUNC_OFFSETS           => 0x16,
};

use constant SECTIONTYPES =>
qw(S_REGULAR S_ZEROFILL S_CSTRING_LITERALS S_4BYTE_LITERALS S_8BYTE_LITERALS
   S_LITERAL_POINTERS S_NON_LAZY_SYMBOL_POINTERS S_LAZY_SYMBOL_POINTERS
   S_SYMBOL_STUBS S_MOD_INIT_FUNC_POINTERS S_MOD_TERM_FUNC_POINTERS S_COALESCED
   S_GB_ZEROFILL S_INTERPOSING S_16BYTE_LITERALS S_DTRACE_DOF
   S_LAZY_DYLIB_SYMBOL_POINTERS S_THREAD_LOCAL_REGULAR S_THREAD_LOCAL_ZEROFILL
   S_THREAD_LOCAL_VARIABLES S_THREAD_LOCAL_VARIABLE_POINTERS
   S_THREAD_LOCAL_INIT_FUNCTION_POINTERS S_INIT_FUNC_OFFSETS);

# section attributes
use constant {
  S_ATTR_PURE_INSTRUCTIONS      => 0x80000000,
  S_ATTR_NO_TOC                 => 0x40000000,
  S_ATTR_STRIP_STATIC_SYMS      => 0x20000000,
  S_ATTR_NO_DEAD_STRIP          => 0x10000000,
  S_ATTR_LIVE_SUPPORT           => 0x08000000,
  S_ATTR_SELF_MODIFYING_CODE    => 0x04000000,
  S_ATTR_DEBUG                  => 0x02000000,
  S_ATTR_SOME_INSTRUCTIONS      => 0x00000400,
  S_ATTR_EXT_RELOC              => 0x00000200,
  S_ATTR_LOC_RELOC              => 0x00000100,
};

use constant SECTIONATTRIBUTES =>
qw(S_ATTR_PURE_INSTRUCTIONS S_ATTR_NO_TOC S_ATTR_STRIP_STATIC_SYMS
   S_ATTR_NO_DEAD_STRIP S_ATTR_LIVE_SUPPORT S_ATTR_SELF_MODIFYING_CODE
   S_ATTR_DEBUG S_ATTR_SOME_INSTRUCTIONS S_ATTR_EXT_RELOC S_ATTR_LOC_RELOC);

# platform names
use constant {
  PLATFORM_MACOS                => 1,
  PLATFORM_IOS                  => 2,
  PLATFORM_TVOS                 => 3,
  PLATFORM_WATCHOS              => 4,
  PLATFORM_BRIDGEOS             => 5,
  PLATFORM_MACCATALYST          => 6,
  PLATFORM_IOSSIMULATOR         => 7,
  PLATFORM_TVOSSIMULATOR        => 8,
  PLATFORM_WATCHOSSIMULATOR     => 9,
  PLATFORM_DRIVERKIT            => 10,
};

use constant PLATFORMS =>
qw(PLATFORM_MACOS PLATFORM_IOS PLATFORM_TVOS PLATFORM_WATCHOS PLATFORM_BRIDGEOS
   PLATFORM_MACCATALYST PLATFORM_IOSSIMULATOR PLATFORM_TVOSSIMULATOR
   PLATFORM_WATCHOSSIMULATOR PLATFORM_DRIVERKIT);

# build tool names
use constant {
  TOOL_CLANG                    => 1,
  TOOL_SWIFT                    => 2,
  TOOL_LD	                => 3,
};

use constant TOOLS =>
qw(TOOL_CLANG TOOL_SWIFT TOOL_LD);

# indirect symbol specials
use constant {
  INDIRECT_SYMBOL_LOCAL         => 0x80000000,
  INDIRECT_SYMBOL_ABS           => 0x40000000,
};

# dyld rebase opcodes
use constant {
  REBASE_TYPE_POINTER => 1,
  REBASE_TYPE_TEXT_ABSOLUTE32 => 2,
  REBASE_TYPE_TEXT_PCREL32 => 3,
  REBASE_OPCODE_MASK => 0xF0,
  REBASE_IMMEDIATE_MASK => 0x0F,
  REBASE_OPCODE_DONE => 0x00,
  REBASE_OPCODE_SET_TYPE_IMM => 0x10,
  REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB => 0x20,
  REBASE_OPCODE_ADD_ADDR_ULEB => 0x30,
  REBASE_OPCODE_ADD_ADDR_IMM_SCALED => 0x40,
  REBASE_OPCODE_DO_REBASE_IMM_TIMES => 0x50,
  REBASE_OPCODE_DO_REBASE_ULEB_TIMES => 0x60,
  REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB => 0x70,
  REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB => 0x80,
};

# dyld bind opcodees
use constant {
  BIND_TYPE_POINTER => 1,
  BIND_TYPE_TEXT_ABSOLUTE32 => 2,
  BIND_TYPE_TEXT_PCREL32 => 3,
  BIND_SPECIAL_DYLIB_SELF =>  0,
  BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE => -1,
  BIND_SPECIAL_DYLIB_FLAT_LOOKUP => -2,
  BIND_SPECIAL_DYLIB_WEAK_LOOKUP => -3,
  BIND_SYMBOL_FLAGS_WEAK_IMPORT => 0x1,
  BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION => 0x8,
  BIND_OPCODE_MASK => 0xF0,
  BIND_IMMEDIATE_MASK => 0x0F,
  BIND_OPCODE_DONE => 0x00,
  BIND_OPCODE_SET_DYLIB_ORDINAL_IMM => 0x10,
  BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB => 0x20,
  BIND_OPCODE_SET_DYLIB_SPECIAL_IMM => 0x30,
  BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM => 0x40,
  BIND_OPCODE_SET_TYPE_IMM => 0x50,
  BIND_OPCODE_SET_ADDEND_SLEB => 0x60,
  BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB => 0x70,
  BIND_OPCODE_ADD_ADDR_ULEB => 0x80,
  BIND_OPCODE_DO_BIND => 0x90,
  BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB => 0xA0,
  BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED => 0xB0,
  BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB => 0xC0,
  BIND_OPCODE_THREADED => 0xD0,
  BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB => 0x00,
  BIND_SUBOPCODE_THREADED_APPLY =>  0x01,
};

use constant {
  EXPORT_SYMBOL_FLAGS_KIND_MASK => 0x03,
  EXPORT_SYMBOL_FLAGS_KIND_REGULAR => 0x00,
  EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL => 0x01,
  EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE => 0x02,
  EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION => 0x04,
  EXPORT_SYMBOL_FLAGS_REEXPORT => 0x08,
  EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER => 0x10,
};

sub loadCommandName {
  my ($value) = @_;
  return &nameForValue($value, LOADCOMMANDS);
}

sub segmentFlagName {
  my ($value) = @_;
  return &nameForValue($value, SEGMENTFLAGS);
}

sub sectionTypeName {
  my ($value) = @_;
  return &nameForValue($value, SECTIONTYPES);
}

sub sectionAttributeName {
  my ($value) = @_;
  return &nameForValue($value, SECTIONATTRIBUTES);
}

sub platformName {
  my ($value) = @_;
  return &nameForValue($value, PLATFORMS);
}

sub toolName {
  my ($value) = @_;
  return &nameForValue($value, TOOLS);
}

# LoadCommand is an object describing a load command.
#
# The load command fields are:
#
#   cmd         - type of load command
#   cmdsize     - size of load command in bytes
#
# LoadCommand is normally an abstract superclass, although it is also a
# fallback for unknown load commands. Subclasses extend this information
# for each type of load command. LoadCommand->new() will select the proper
# LoadCommand subclass based on the load command information.

package LoadCommand;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $cmd, $data, $offset) = @_;

  # Because perl's OO support is based on blessed hashes, we don't actually
  # need to call a 'new' constructor on each subclass; we can just bless
  # the hashes directly, programmatically.
  #
  # Towards that end, here is a map from load command to class name.
  my $map = {
    MachO::LC_SEGMENT			=> LoadCommandSegment,
    MachO::LC_SEGMENT_64		=> LoadCommandSegment,

    MachO::LC_LOAD_DYLIB		=> LoadCommandDylib,
    MachO::LC_ID_DYLIB			=> LoadCommandDylib,
    MachO::LC_LOAD_WEAK_DYLIB		=> LoadCommandDylib,
    MachO::LC_REEXPORT_DYLIB		=> LoadCommandDylib,
    MachO::LC_LAZY_LOAD_DYLIB		=> LoadCommandDylib,
    MachO::LC_LOAD_UPWARD_DYLIB		=> LoadCommandDylib,

    MachO::LC_SUB_FRAMEWORK		=> LoadCommandLCStr,
    MachO::LC_SUB_CLIENT		=> LoadCommandLCStr,
    MachO::LC_SUB_UMBRELLA		=> LoadCommandLCStr,
    MachO::LC_SUB_LIBRARY		=> LoadCommandLCStr,
    MachO::LC_ID_DYLINKER		=> LoadCommandLCStr,
    MachO::LC_LOAD_DYLINKER		=> LoadCommandLCStr,
    MachO::LC_DYLD_ENVIRONMENT		=> LoadCommandLCStr,
    MachO::LC_RPATH			=> LoadCommandLCStr,

    MachO::LC_DYLD_INFO			=> LoadCommandDyldInfo,
    MachO::LC_DYLD_INFO_ONLY		=> LoadCommandDyldInfo,

    MachO::LC_SYMTAB			=> LoadCommandSymtab,
    MachO::LC_DYSYMTAB			=> LoadCommandDySymtab,

    MachO::LC_UUID			=> LoadCommandUUID,

    MachO::LC_SOURCE_VERSION		=> LoadCommandSourceVersion,
    MachO::LC_BUILD_VERSION		=> LoadCommandBuildVersion,

    MachO::LC_VERSION_MIN_MACOSX	=> LoadCommandVersionMin,
    MachO::LC_VERSION_MIN_IPHONEOS	=> LoadCommandVersionMin,
    MachO::LC_VERSION_MIN_WATCHOS	=> LoadCommandVersionMin,
    MachO::LC_VERSION_MIN_TVOS		=> LoadCommandVersionMin,

    MachO::LC_MAIN			=> LoadCommandMain,

    MachO::LC_CODE_SIGNATURE		=> LoadCommandLinkEditData,
    MachO::LC_SEGMENT_SPLIT_INFO	=> LoadCommandLinkEditData,
    MachO::LC_FUNCTION_STARTS		=> LoadCommandLinkEditData,
    MachO::LC_DATA_IN_CODE		=> LoadCommandLinkEditData,
    MachO::LC_DYLIB_CODE_SIGN_DRS	=> LoadCommandLinkEditData,
    MachO::LC_LINKER_OPTIMIZATION_HINT	=> LoadCommandLinkEditData,
    MachO::LC_DYLD_EXPORTS_TRIE		=> LoadCommandLinkEditData,
    MachO::LC_DYLD_CHAINED_FIXUPS	=> LoadCommandLinkEditData,

    MachO::LC_TWOLEVEL_HINTS		=> LoadCommandTwoLevelHints,

    MachO::LC_ROUTINES			=> LoadCommandRoutines,
    MachO::LC_ROUTINES_64		=> LoadCommandRoutines,

    MachO::LC_ENCRYPTION_INFO		=> LoadCommandEncryptionInfo,
    MachO::LC_ENCRYPTION_INFO_64	=> LoadCommandEncryptionInfo,

    MachO::LC_LINKER_OPTION		=> LoadCommandLinkerOption,

    MachO::LC_NOTE			=> LoadCommandNote,

    MachO::LC_THREAD			=> LoadCommandThread,
    MachO::LC_UNIXTHREAD		=> LoadCommandThread,
  };

  $class = $map->{$cmd};
  $class = 'LoadCommand' unless $class;
  my ($self) = bless {}, $class;

  if (defined($data)) {
    $self->depack($mo, $data, $offset);
  }
  else {
    $self->{'cmd'} = $cmd;
    $self->{'cmdsize'} = 0;
  }

  return $self;
}

sub fields {
  return qw(cmd cmdsize);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2";
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq "cmd") {
    return &MachO::loadCommandName($value);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

sub location {
  my ($self) = @_;
  return $self->{"_location"};
}

sub length {
  my ($self) = @_;
  return length($self->{"_data"});
}

# LoadCommandDylib represents a struct dylib_command used by a number of
# load command types. This command differs from Mach-O slightly: the
# values in the struct dylib structure are presented at the top level.
#
# The fields are:
#
#   cmd         - LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB
#               - LC_LAZY_LOAD_DYLIB, LC_ID_DYLIB, LC_LOAD_UPWARD_DYLIB
#   cmdsize     - size of load command in bytes, including the dylib path
#   offset      - offset into the load command where the dylib path begins
#   timestamp   - dylib's build timestamp
#   current_version       - dylib's current version number
#   compatibility_version - dylib's current compatibility version number
#   name        - dylib path
#
# Recall that the load command needs to be natively word aligned. That means
# the name data beginning at offset - typically right after the compatibility
# version - will usually include a few extra null bytes. This detail is
# important if creating a new such command from scratch.

package LoadCommandDylib;

use parent -norequire, "LoadCommand";

sub fields {
  return ("cmd", "cmdsize", "offset", "timestamp", "current_version",
          "compatibility_version", "name");
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}6 A*";
}

# packsize needs to account for the variable length name, so we'll fully pack
# the data to get its size.
sub packsize {
  my ($self, $mo) = @_;

  my (@values);
  foreach my $field ($self->fieldsForPacking($mo)) {
    push @values, $self->{$field};
  }

  my $template = $self->fieldsTemplate($mo);

  no warnings 'uninitialized';
  my $raw = pack $template, @values;
  use warnings 'uninitialized';

  # pad to pointer alignment
  my $size = length($raw);
  my $bits = $mo->{'bits'};
  my $align = $bits eq 64 ? 8 : 4;
  my $pad = $size % $align > 0 ? $align - ($size % $align) : 0;
  $size += $pad;

  return $size;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  # pad to pointer alignment
  my $data = $self->{'_data'};
  my $size = length($data);
  my $bits = $mo->{'bits'};
  my $align = $bits eq 64 ? 8 : 4;
  my $pad = $size % $align > 0 ? $align - ($size % $align) : 0;
  $data .= pack "x${pad}";
  $self->{'_data'} = $data;
}

sub fieldType {
  my ($self, $field) = @_;
  return "s" if ($field eq "name");
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  if ($field eq "current_version" or
      $field eq "compatibility_version") {
    my $version = $self->{$field};
    return &MachO::versionStringXYZ($version);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

# LoadCommandLCStr represents a number of different load commands all with
# the same logical structure: a single union lc_str. Each load command type
# uses a different field name for this lc_str structure.
#
# The common fields are:
#
#   cmd         - LC_SUB_FRAMEWORK, LC_SUB_CLIENT, LC_SUB_UMBRELLA,
#               - LC_SUB_LIBRARY, LC_ID_DYLINKER, LC_LOAD_DYLINKER,
#               - LC_DYLD_ENVIRONMENT, LC_RPATH
#   cmdsize     - size of load command in bytes, including the lc_str path
#   offset      - offset into the load command where the lc_str begins
#
# The following fields are cmd specific:
#
#   umbrella    - LC_SUB_FRAMEWORK
#   client      - LC_SUB_CLIENT
#   sub_umbrella - LC_SUB_UMBRELLA
#   sub_library - LC_SUB_LIBRARY
#   name        - LC_ID_DYLINKER, LC_LOAD_DYLINKER, LC_DYLD_ENVIRONMENT
#   path        - LC_RPATH
#
# All of these fields are stored internally as "lc_str" for convenience.

package LoadCommandLCStr;

use parent -norequire, "LoadCommand";

sub fields {
  my ($self) = @_;
  my @fields = qw(cmd cmdsize offset);
  if ($self->{"cmd"} eq MachO::LC_SUB_FRAMEWORK) {
    push @fields, "umbrella";
  }
  elsif ($self->{"cmd"} eq MachO::LC_SUB_CLIENT) {
    push @fields, "client";
  }
  elsif ($self->{"cmd"} eq MachO::LC_SUB_UMBRELLA) {
    push @fields, "sub_umbrella";
  }
  elsif ($self->{"cmd"} eq MachO::LC_SUB_LIBRARY) {
    push @fields, "sub_library";
  }
  elsif ($self->{"cmd"} eq MachO::LC_ID_DYLINKER or
         $self->{"cmd"} eq MachO::LC_LOAD_DYLINKER or
         $self->{"cmd"} eq MachO::LC_DYLD_ENVIRONMENT) {
    push @fields, "name";
  }
  elsif ($self->{"cmd"} eq MachO::LC_RPATH) {
    push @fields, "path";
  }
  return @fields;
}

sub fieldsForPacking {
  return qw(cmd cmdsize offset lc_str);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  &MachO::abort("missing object") unless defined ($mo);
  my $swap = $mo->{'swap'};
  &MachO::abort("missing swap") unless defined ($swap);
  return "L${swap}3 A*";
}

# packsize needs to account for the variable length name, so we'll fully pack
# the data to get its size.
sub packsize {
  my ($self, $mo) = @_;

  my (@values);
  foreach my $field ($self->fieldsForPacking($mo)) {
    push @values, $self->{$field};
  }

  my $template = $self->fieldsTemplate($mo);

  no warnings 'uninitialized';
  my $raw = pack $template, @values;
  use warnings 'uninitialized';

  # pad to pointer alignment
  my $size = length($raw);
  my $bits = $mo->{'bits'};
  my $align = $bits eq 64 ? 8 : 4;
  my $pad = $size % $align > 0 ? $align - ($size % $align) : 0;
  $size += $pad;

  return $size;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  # pad to pointer alignment
  my $data = $self->{'_data'};
  my $size = length($data);
  my $bits = $mo->{'bits'};
  my $align = $bits eq 64 ? 8 : 4;
  my $pad = $size % $align > 0 ? $align - ($size % $align) : 0;
  $data .= pack "x${pad}";
  $self->{'_data'} = $data;
}

sub fieldType {
  my ($self, $field) = @_;
  return "s" if ($self->{"cmd"} eq MachO::LC_SUB_FRAMEWORK or
                 $self->{"cmd"} eq MachO::LC_SUB_CLIENT or
                 $self->{"cmd"} eq MachO::LC_SUB_UMBRELLA or
                 $self->{"cmd"} eq MachO::LC_SUB_LIBRARY or
                 $self->{"cmd"} eq MachO::LC_ID_DYLINKER or
                 $self->{"cmd"} eq MachO::LC_LOAD_DYLINKER or
                 $self->{"cmd"} eq MachO::LC_DYLD_ENVIRONMENT or
                 $self->{"cmd"} eq MachO::LC_RPATH);
  return $self->SUPER::fieldType($field);
}

sub fieldValue {
  my ($self, $field) = @_;
  return $self->{"lc_str"} if ($field eq "umbrella" or
                               $field eq "client" or
                               $field eq "sub_umbrella" or
                               $field eq "sub_library" or
                               $field eq "name" or
                               $field eq "path");
  return $self->SUPER::fieldValue($field);
}

# LoadCommandSegment represents LC_SEGMENT and LC_SEGMENT_64
#
# The fields are:
#
#   cmd         - LC_SEGMENT and LC_SEGMENT_64
#   cmdsize     - size of load command in bytes, including all sections
#   segname
#   vmaddr
#   vmsize
#   fileoff
#   filesize
#   maxprot
#   initprot
#   nsects
#   flags
#
# In addition, segments contain the following additional information:
#
#   bits        - bits per pointer, either 64 or 32.
#   sections    - a ref array of Section objects

package LoadCommandSegment;

use parent -norequire, "LoadCommand";

sub fields {
  return qw(cmd cmdsize segname vmaddr vmsize fileoff filesize
            maxprot initprot nsects flags);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  my $bits = $mo->{'bits'};
  if ($bits == 64) {
    return "L${swap}2 a16 Q${swap}4 L${swap}4";
  }
  elsif ($bits == 32) {
    return "L${swap}2 a16 L${swap}4 L${swap}4";
  }
  return undef;
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  # ideally we'd always work with "a" string templates, so that our fixed
  # width buffers are filled with ASCII zeroes. But perl will not treat
  # "__TEXT" and "__TEXT\0\0\0\0\0\0\0\0\0\0" as equal strings, which is a
  # real pain in the neck. So we'll just convert "a" strings to "A" strings
  # when our segment is depacked.
  $self->{'segname'} = unpack "A*", $self->{'segname'};

  my $bits = $mo->{'bits'};
  my $segsize = $bits == 64 ? 72 : 56;
  my $sectsize = $bits == 64 ? 80 : 68;
  my @sections;
  for (my $i = 0; $i < $self->{'nsects'}; ++$i) {
    my $raw = substr($data, $segsize + ($sectsize * $i), $sectsize);
    my $sect = Section->new($mo, $raw, $offset + $segsize + ($sectsize * $i));
    push @sections, $sect;
  }
  $self->{'sections'} = \@sections;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  my $bits = $mo->{'bits'};
  my $data = $self->{'_data'};
  my $location = $bits == 64 ? 72 : 56;
  my $sectsize = $bits == 64 ? 80 : 68;
  foreach my $section (@{$self->{"sections"}}) {
    $section->repack($mo);
    substr($data, $location, $sectsize) = $section->{'_data'};
    $location += $sectsize;
  }
  $self->{'_data'} = $data;
}

sub packsize {
  my ($self, $mo) = @_;
  my $size = $self->SUPER::packsize($mo);
  foreach my $section (@{$self->{"sections"}}) {
    $size += $section->packsize($mo);
  }
  return $size;
}

sub fieldType {
  my ($self, $field) = @_;
  return "s" if ($field eq "segname");
  return "x" if ($field eq "vmaddr" or
                 $field eq "vmsize" or
                 $field eq "maxprot" or
                 $field eq "initprot" or
                 $field eq "flags");
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq "maxprot" or
      $field eq "initprot")
  {
    my $perms = "";
    $perms .= $value & 1 ? "r" : "-";
    $perms .= $value & 2 ? "w" : "-";
    $perms .= $value & 4 ? "x" : "-";
    return $perms;
  }
  elsif ($field eq "flags") {
    my $string = "";
    for (my $bit = 0; $bit < 32; ++$bit) {
      my $flag = (1 << $bit);
      if ($value & $flag) {
        my $name = &MachO::segmentFlagName($flag);
        if (defined($name) and defined($string)) {
          $string .= " $name";
        } else {
          $string = $name;
        }
      }
    }
    return $string;
  }
  return $self->SUPER::fieldVerboseValue($field, $value);
}

sub description {
  my ($self, $verbose) = @_;
  my $desc = $self->SUPER::description($verbose);
  foreach my $section (@{$self->{"sections"}}) {
    $desc .= "Section\n";
    #$desc .= $section->description($verbose);
    foreach my $line (split /\n/, $section->description($verbose)) {
      $desc .= "    $line\n";
    }
  }
  return $desc;
}

sub locate {
  my ($self, $mo, $location) = @_;
  $self->SUPER::locate($mo, $location);
  my ($sectloc) = 0;
  foreach my $section (@{$self->{"sections"}}) {
    $section->locate($location + $sectloc);
    $sectloc += $section->packsize($mo);
  }
}

# Section represents struct section and struct section_64
#
# The fields are:
#
#   sectname
#   segname
#   addr
#   size
#   offset
#   align
#   reloff
#   nreloc
#   flags
#   reserved1   - (for indirect sections: index into indirect symbol table)
#   reserved2   - (for __stubs: size of stubs)
#   reserved3   - 64-bit only
#
# In addition, sections may contain the following additional information:
#
#   relocations - a ref array of Relocation objects

package Section;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;

  my $bits = $mo->{'bits'};
  $self->{'bits'} = $bits;
  $self->depack($mo, $data, $offset) if ($data);

  return $self;
}

sub fields {
  my ($self) = @_;
  my @fields = qw(sectname segname addr size offset align
                  reloff nreloc flags reserved1 reserved2);
  if ($self->{"bits"} == 64) {
    push @fields, qw(reserved3);
  }
  return @fields;
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  my $bits = $mo->{'bits'};
  if ($bits == 64) {
    return "a16 a16 Q${swap}2 L${swap}8";
  }
  elsif ($bits == 32) {
    return "a16 a16 L${swap}10";
  }
  return undef;
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "segname" or
      $field eq "sectname" or
      $field eq "align")
  {
    return "s";
  }
  elsif ($field eq "addr" or
         $field eq "size" or
         $field eq "flags")
  {
    return "x";
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
  my $value = $self->{$field};
  if ($field eq "flags") {
    my $type = $value & MachO::SECTION_TYPE;
    my $string = &MachO::sectionTypeName($type);
    my $attr = $value >> 8;
    for (my $bit = 0; $bit < 24; ++$bit) {
      my $flag = (1 << $bit);
      if ($value & $flag) {
        my $name = &MachO::sectionAttributeName($flag);
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

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  # See LoadCommandSegment::depack
  $self->{'segname'} = unpack "A*", $self->{'segname'};
  $self->{'sectname'} = unpack "A*", $self->{'sectname'};

  # relocations are relative to the start of the mach object
  $data = $mo->{'data'};

  my $relsize = 8;
  my $reloff = $self->{'reloff'};
  my @relocations;
  for (my $i = 0; $i < $self->{'nreloc'}; ++$i) {
    $offset = $reloff + ($relsize * $i);
    my $raw = substr($data, $offset, $relsize);
    my $reloc = Relocation->new($mo, $raw, $offset);
    push @relocations, $reloc;
  }
  $self->{'relocations'} = \@relocations;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  my $nreloc = $self->{'nreloc'};
  if ($nreloc) {
    my $raw;
    my $size = 8 * $nreloc;
    foreach my $reloc (@{$self->{'relocations'}}) {
      $raw .= $reloc->{'_data'};
    }
    my $rsize = defined $raw ? length($raw) : 0;
    die "bad relocation data (size = $size, rsize = $rsize)\n"
      if ($size ne $rsize);
    substr($mo->{'data'}, $self->{'reloff'}, $size) = $raw;
  }
}

package Relocation;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;
  if (defined($data)) {
    $self->depack($mo, $data, $offset);
  }
  return $self;
}

sub fields {
  return qw(r_address r_symbolnum r_pcrel r_length r_extern r_type);
}

sub fieldsForPacking {
  return qw(r_address r_value);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "l${swap} L${swap}";
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  my $value = $self->{'r_value'};
  $self->{'r_symbolnum'} = $value & 0xFFFFFF; $value >>= 24;
  $self->{'r_pcrel'} = $value & 0x1; $value >>= 1;
  $self->{'r_length'} = $value & 0x3; $value >>= 2;
  $self->{'r_extern'} = $value & 0x1; $value >>= 1;
  $self->{'r_type'} = $value & 0xF;
}

sub repack {
  my ($self, $mo) = @_;

  my $value = 0;
  $value |= $self->{'r_type'} & 0xF; $value <<= 4;
  $value |= $self->{'r_extern'} & 0x1; $value <<= 1;
  $value |= $self->{'r_length'} & 0x3; $value <<= 2;
  $value |= $self->{'r_pcrel'} & 0x1; $value <<= 1;
  $value |= $self->{'r_symbolnum'} & 0xFFFFFF;
  $self->{'r_value'} = $value;
  $self->SUPER::repack($mo);
}

sub fieldType {
  my ($self, $field) = @_;
  return "x" if ($field eq 'r_address');
  return "d";
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq 'r_address') {
    return sprintf "%08x", $value;
  }
  if ($field eq 'r_pcrel' or
      $field eq 'r_extern') {
    return $value ? 'true' : 'false';
  }
  if ($field eq 'r_length') {
    my @lengths = qw(byte word long quad);
    return $lengths[$value];
  }
  return $self->SUPER::fieldVerboseValue($field);
}

# LoadCommandDyldInfo
#
# Fields are:
#
#   cmd         - LC_DYLD_INFO and LC_DYLD_INFO_ONLY
#   cmdsize     - size of load command in bytes
#   rebase_off
#   rebase_size
#   bind_off
#   bind_size
#   weak_bind_off
#   weak_bind_size
#   lazy_bind_off
#   lazy_bind_size
#   export_off
#   export_size

package LoadCommandDyldInfo;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize rebase_off rebase_size bind_off bind_size
            weak_bind_off weak_bind_size lazy_bind_off lazy_bind_size
            export_off export_size);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}12";
}

# LoadCommandSymtab
#
# Fields are:
#
#   cmd         - LC_SYMTAB
#   cmdsize     - size of load command in bytes
#   symoff
#   nsyms
#   stroff
#   strsize

package LoadCommandSymtab;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize symoff nsyms stroff strsize);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}6";
}

# LoadCommandDySymtab
#
# Fields are
#   cmd         - LC_DYSYMTAB
#   cmdsize     - size of load command in bytes
#   ilocalsym
#   nlocalsym
#   iextdefsym
#   nextdefsym
#   iundefsym
#   nundefsym
#   tocoff
#   ntoc
#   modtaboff
#   nmodtab
#   extrefsymoff
#   nextrefsyms
#   indirectsymoff
#   nindirectsyms
#   extreloff
#   nextrel
#   locreloff
#   nlocrel

package LoadCommandDySymtab;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize ilocalsym nlocalsym iextdefsym nextdefsym
            iundefsym nundefsym tocoff ntoc modtaboff nmodtab
            extrefsymoff nextrefsyms indirectsymoff nindirectsyms
            extreloff nextrel locreloff nlocrel);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}20";
}

# LoadCommandUUID
#
#   cmd         - LC_UUID
#   cmdsize     - size of load command in bytes
#   uuid        - a raw buffer holding 16 unsigned chars
#
# When accessed through fieldValue, 'uuid' will return the uuid as a nicely
# formatted string, generated from the raw bytes.

package LoadCommandUUID;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize uuid);
}

sub fieldsForPacking {
  return qw(cmd cmdsize);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2";
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);
  $self->{'uuid'} = substr($data, 8, 16);
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);
  my $data = $self->{'_data'};
  substr($data, 8, 16) = $self->{'uuid'};
  $self->{'_data'} = $data;
}

sub packsize {
  my ($self, $mo) = @_;
  my $size = $self->SUPER::packsize($mo);
  return $size + 16;
}

sub fieldType {
  my ($self, $field) = @_;
  return "s" if ($field eq 'uuid');
  return $self->SUPER::fieldType($field);
}

sub fieldValue {
  my ($self, $field) = @_;
  if ($field eq 'uuid') {
    my $uuid = "";
    my @chars = unpack("C16", $self->{$field});
    foreach my $char (@chars) {
      $uuid .= sprintf("%X", $char);
    }
    return (substr($uuid, 0, 8) . '-' .
            substr($uuid, 8, 4) . '-' .
            substr($uuid, 12, 4) . '-' .
            substr($uuid, 16, 4) . '-' .
            substr($uuid, 20, 12));
  }
  return $self->SUPER::fieldValue($field);
}

# LoadCommandSourceVersion
#
#   cmd         - LC_SOURCE_VERSION
#   cmdsize     - size of load command in bytes
#   version     - 64-bit number: A.B.C.D.E packed as a24.b10.c10.d10.e10

package LoadCommandSourceVersion;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize version);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2 Q${swap}";
}

sub fieldType {
  my ($self, $field) = @_;
  return "x" if ($field eq "version");
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  if ($field eq "version") {
    my $version = $self->{$field};
    return &MachO::versionStringABCDE($version);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

# LoadCommandVersionMin
#
#   cmd         - LC_VERSION_MIN_MACOSX, LC_VERSION_MIN_IPHONEOS,
#                 LC_VERSION_MIN_WATCHOS, LC_VERSION_MIN_TVOS
#   cmdsize     - size of load command in bytes
#   version     - 32-bit number: X.Y.Z is encoded in nibbles xxxx.yy.zz
#   sdk         - 32-bit number: X.Y.Z is encoded in nibbles xxxx.yy.zz

package LoadCommandVersionMin;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize version sdk);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}4";
}

sub fieldType {
  my ($self, $field) = @_;
  return "x" if ($field eq "version" or
                 $field eq "sdk");
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  if ($field eq "version" or
      $field eq "sdk") {
    my $version = $self->{$field};
    return &MachO::versionStringXYZ($version);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

# LoadCommandBuildVersion
#
#   cmd         - LC_BUILD_VERSION
#   cmdsize     - size of load command in bytes
#   platform    - platform enum
#   minos       - 32-bit number: X.Y.Z is encoded in nibbles xxxx.yy.zz
#   sdk         - 32-bit number: X.Y.Z is encoded in nibbles xxxx.yy.zz
#   ntools      - number of optional tools
#
# In addition, build versions contain the following additional information:
#
#   tools    - a ref array of BuildToolVersion objects

package LoadCommandBuildVersion;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize platform minos sdk ntools);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}6";
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  my $lcsize = 24;
  my $toolsize = 8;
  my @tools;
  for (my $i = 0; $i < $self->{"ntools"}; ++$i) {
    my $raw = substr($data, $lcsize + ($toolsize * $i), $toolsize);
    my $tool = BuildToolVersion->new($mo, $raw,
                                     $offset + $lcsize + ($toolsize * $i));
    push @tools, $tool;
  }
  $self->{"tools"} = \@tools;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  my $data = $self->{'_data'};
  my $location = 24;
  my $toolsize = 8;
  foreach my $tool (@{$self->{'tools'}}) {
    $tool->repack($mo);
    substr($data, $location, $toolsize) = $tool->{'_data'};
    $location += $toolsize;
  }
  $self->{'_data'} = $data;
}

sub locate {
  my ($self, $mo, $location) = @_;
  $self->SUPER::locate($mo, $location);
  my ($toolloc) = 0;
  foreach my $tool (@{$self->{'tools'}}) {
    $tool->locate($location + $toolloc);
    $toolloc += $tool->packsize($mo);
  }
}

sub packsize {
  my ($self, $mo) = @_;
  my $size = $self->SUPER::packsize($mo);
  foreach my $tool (@{$self->{'tools'}}) {
    $size += $tool->packsize($mo);
  }
  return $size;
}

sub fieldType {
  my ($self, $field) = @_;
  return "x" if ($field eq "minos" or
                 $field eq "sdk");
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  if ($field eq "minos" or
      $field eq "sdk") {
    my $version = $self->{$field};
    return &MachO::versionStringXYZ($version);
  }
  elsif ($field eq "platform") {
    return &MachO::platformName($self->{$field});
  }
  return $self->SUPER::fieldVerboseValue($field);
}

sub description {
  my ($self, $verbose) = @_;
  my $desc = $self->SUPER::description($verbose);
  foreach my $tool (@{$self->{"tools"}}) {
    $desc .= "Tool\n";
    $desc .= $tool->description($verbose);
    #foreach my $line (split /\n/, $tool->description($verbose)) {
    #  $desc .= "    $line\n";
    #}
  }
  return $desc;
}

# BuildToolVersion represents struct build_tool_version
#
# The fields are:
#
#   tool        - tool enum
#   version     - 32-bit number: X.Y.Z is encoded in nibbles xxxx.yy.zz

package BuildToolVersion;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;

  $self->depack($mo, $data, $offset);

  return $self;
}

sub fields {
  return qw(tool version);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2";
}

# sub repack {
#   my ($self, $mo) = @_;
#   my $t = $self->fieldsTemplate($mo);
#   print "in repack! $t\n";
#   $self->SUPER::repack($mo);
# }

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "version")
  {
    return "x";
  }
  return $self->SUPER::fieldType($field);
}

sub fieldVerboseValue {
  my ($self, $field) = @_;
  my $value = $self->{$field};
  if ($field eq "tool") {
    return &MachO::toolName($value);
  }
  elsif ($field eq "version") {
    return &MachO::versionStringXYZ($value);
  }
  return $self->SUPER::fieldVerboseValue($field);
}

# LoadCommandMain
#
#   cmd         - LC_MAIN
#   cmdsize     - size of load command in bytes
#   entryoff
#   stacksize

package LoadCommandMain;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize entryoff stacksize);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2 Q${swap}2";
}

# LoadCommandLinkEditData
#
#   cmd         - LC_CODE_SIGNATURE, LC_SEGMENT_SPLIT_INFO, LC_FUNCTION_STARTS,
#                 LC_DATA_IN_CODE, LC_DYLIB_CODE_SIGN_DRS,
#                 LC_LINKER_OPTIMIZATION_HINT, LC_DYLD_EXPORTS_TRIE,
#                 LC_DYLD_CHAINED_FIXUPS
#   cmdsize     - size of load command in bytes
#   dataoff
#   datasize

package LoadCommandLinkEditData;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize dataoff datasize);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}4";
}

# LoadCommandTwoLevelHints
#
#   cmd         - LC_TWOLEVEL_HINTS
#   cmdsize     - size of load command in bytes
#   offset
#   nhints

package LoadCommandTwoLevelHints;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize offset nhints);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}4";
}

# LoadCommandRoutines
#
#   cmd         - LC_ROUTINES, LC_ROUTINES_64
#   cmdsize     - size of load command in bytes
#   init_address
#   init_module
#   reserved1
#   reserved2
#   reserved3
#   reserved4
#   reserved5
#   reserved6

package LoadCommandRoutines;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize init_address init_module reserved1
            reserved2 reserved3 reserved4 reserved5 reserved6);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  my $bits = $mo->{'bits'};
  if ($bits == 64) {
    return "L${swap}2 Q${swap}8";
  }
  elsif ($bits == 32) {
    return "L${swap}10";
  }
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "init_address")
  {
    return "x";
  }
  return $self->SUPER::fieldType($field);
}

# LoadCommandEncryptionInfo
#
#   cmd         - LC_ENCRYPTION_INFO, LC_ENCRYPTION_INFO_64
#   cmdsize     - size of load command in bytes
#   cryptoff
#   cryptsize
#   cryptid
#   pad         - 64-bit only
#
# LoadCommandEncryptionInfo contain the following additional information:
#
#   bits        - bits per pointer, either 64 or 32.

package LoadCommandEncryptionInfo;

use parent -norequire, LoadCommand;

sub fields {
  my ($self) = @_;
  my (@fields) = qw(cmd cmdsize cryptoff cryptsize cryptid);
  if ($self->{'bits'} == 64) {
    push @fields, "pad";
  }
  return @fields;
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->{'bits'} = $mo->{'bits'}; # hack?
  $self->SUPER::depack($mo, $data, $offset);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  my $bits = $mo->{'bits'};
  if ($bits == 64) {
    return "L${swap}6";
  }
  elsif ($bits == 32) {
    return "L${swap}5";
  }
}

# LoadCommandLinkerOption
#
#   cmd         - LC_LINKER_OPTION
#   cmdsize     - size of load command in bytes
#   count
#
# The strings associated with the LC_LINKER_OPTION are currently unavailable.

package LoadCommandLinkerOption;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize count);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}3";
}

# LoadCommandNote
#
#   cmd         - LC_NOTE
#   cmdsize     - size of load command in bytes
#   data_owner
#   offset
#   size

package LoadCommandNote;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize data_owner offset size);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2 a16 Q${swap}2";
}

sub fieldType {
  my ($self, $field) = @_;
  if ($field eq "data_owner")
  {
    return "s";
  }
  return $self->SUPER::fieldType($field);
}

# LoadCommandThread
#
#   cmd         - LC_THREAD or LC_UNIXTHREAD
#   cmdsize     - size of load command in bytes
#
#   states

package LoadCommandThread;

use parent -norequire, LoadCommand;

sub fields {
  return qw(cmd cmdsize);
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  my $swap = $mo->{'swap'};
  my $read = 8;
  my $total = $self->{'cmdsize'};
  my @states;
  while ($read < $total) {
    my $raw = substr($data, $read, 8);
    my ($flavor, $words) = unpack "L${swap}2", $raw;
    $raw = substr($data, $read, 8 + ($words * 4));
    my $state = ThreadState->new($mo, $raw, $offset + $read);
    push @states, $state;
    $read += 8 + ($words * 4);
  }
  $self->{'states'} = \@states;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);

  my $data = $self->{'_data'};
  my $location = 8;
  foreach my $state (@{$self->{'states'}}) {
    $state->repack($mo);
    my $statesize = $state->packsize($mo);
    substr($data, $location, $statesize) = $state->{'_data'};

    my $a = $self->{'_location'};
    my $b = $self->{'_location'} + $statesize;

    $location += $statesize;
  }
  $self->{'_data'} = $data;
}

sub locate {
  my ($self, $mo, $location) = @_;
  $self->SUPER::locate($mo, $location);
  my ($stateloc) = 0;
  foreach my $state (@{$self->{'states'}}) {
    $state->locate($location + $stateloc);
    $stateloc += $state->packsize($mo);
  }
}

sub packsize {
  my ($self, $mo) = @_;
  my $size = $self->SUPER::packsize($mo);
  foreach my $state (@{$self->{'states'}}) {
    $size += $state->packsize($mo);
  }
  return $size;
}

sub description {
  my ($self, $verbose) = @_;
  my $desc = $self->SUPER::description($verbose);
  foreach my $state (@{$self->{"states"}}) {
    $desc .= "State\n";
    $desc .= $state->description($verbose);
  }
  return $desc;
}

# ThreadState represents struct XXX_thread_state
#
# The fields are:
#
#   flavor      - tool enum
#   count       - number of 32-bit values
#
#   values

package ThreadState;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;

  $self->depack($mo, $data, $offset);

  return $self;
}

sub fields {
  return qw(flavor count);
}

sub fieldsTemplate {
  my ($self, $mo) = @_;
  my $swap = $mo->{'swap'};
  return "L${swap}2";
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;
  $self->SUPER::depack($mo, $data, $offset);

  my $swap = $mo->{'swap'};
  my $count = $self->{'count'};
  my $raw = substr($data, 8, length($data)-8);
  my @values = unpack "L${swap}${count}", $raw;
  $self->{'values'} = \@values;
}

sub repack {
  my ($self, $mo) = @_;
  $self->SUPER::repack($mo);
  my $swap = $mo->{'swap'};
  my $data = $self->{'_data'};
  my $count = $self->{'count'};
  substr($data, 8, $count * 4) = pack "L${swap}${count}", @{$self->{'values'}};
}

sub packsize {
  my ($self, $mo) = @_;
  my $size = $self->SUPER::packsize($mo);
  $size += $self->{'count'} * 4;
  return $size;
}

sub description {
  my ($self, $verbose) = @_;
  my $desc = $self->SUPER::description($verbose);
  my $i = 0;
  foreach my $value (@{$self->{'values'}}) {
    $desc .= "\n" if ($i and ($i % 4) == 0);
    $desc .= "  " . sprintf "%08x", $value;
    $i += 1;
  }
  $desc .= "\n";
  return $desc;

}

1;
