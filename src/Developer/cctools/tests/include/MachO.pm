# MachO proivdes two classes for working with object files
#
# MachObject
#
# An object representing a Mach-O file. MachObject provides the following:
#
#   Values and Methods:
#     path      - a path or string identifying the MachObject
#     size      - the size of the MachObject in bytes
#     data      - raw bytes of Mach-O data
#     bits      - bits-per-pointer. 64 or 32.
#     swap      - pack/unpack swap character. "<" or ">".
#     offset    - an offset describing where the data was foudn in the context
#                 of some larger buffer or file. 0 for MachObjects instantiated
#                 from path.
#     header    - MachHeader object reference.
#     loadcmds  - array reference of LoadCommand object references.
#
#   newWithPath <path>
#     path      - path to read
#   Instantiate a MachObject from a file on disk at <path>
#
#   newWithData <offset> <size> <data> <name>
#     offset    - an offset describing where the data was foudn in the context
#                 of some larger buffer or file
#     size      - size of the data buffer in bytes
#     data      - the raw data buffer
#     name      - a path or name used to identify the buffer, largely for
#                 reporting errors.
#   Instantiate a MachObject from a raw buffer. The MachObject will keep track
#   of a scalar offset describing where the data was foudn in the context of
#   some larger buffer or file.
#
#   description <verbose>
#     verbose   - if true, replace numeric values with symbolic constants
#   Return a string describing the MachObject constants.
#
# FatObject
#
# An object representing a fat / universal file. FatObject provides the
# following:
#
#   Values and Methods:
#     path      - a path or string identifying the MachObject
#     size      - the size of the MachObject in bytes
#     data      - raw bytes of Mach-O data
#     bits      - bits-per-pointer. 64 or 32.
#     heeader   - FatHeader object reference.
#     fat_archs - array reference of FatArch object references.
#
#   newWithPath <path>
#     path      - path to read
#   Instantiate a FatObject from a file on disk at <path>
#
#   newWithData <offset> <size> <data> <name>
#     offset    - an offset describing where the data was foudn in the context
#                 of some larger buffer or file
#     size      - size of the data buffer in bytes
#     data      - the raw data buffer
#     name      - a path or name used to identify the buffer, largely for
#
#   description <verbose>
#     verbose   - if true, replace numeric values with symbolic constants
#   Return a string describing the FatObject constants.
#
#   machObjectCount
#   Returns the number of Mach-Os contained in the fat file.
#
#   machObjectAtIndex <index>
#     index     - index indicating a MachO contained in the fat file.
#   Returns a MachObject for the requested index.

BEGIN {
  # silence editor error messages.
  unshift @INC, "..";
}

package MachO;

use FileHandle;

use MachO::Base;
use MachO::CPU;
use MachO::FatHeader;
use MachO::MachHeader;
use MachO::LoadCommands;
use MachO::StringTable;
use MachO::SymbolTable;
use MachO::LEB128;
use MachO::Trie;
use MachO::FatObject;
use MachO::MachObject;
use MachO::ArchiveObject;

sub newWithPath {
  my ($class, $path, $archFlag) = @_;

  if (defined($archFlag)) {
    my $size = (stat $path)[7];
    my $file = FileHandle->new($path);
    return &MachO::error($path, "can't open file") unless $file;
    my $data = &MachO::readBytes($file, 32);
    return &MachO::error($path, "file too small") unless $data;

    my $magic = unpack "L", substr $data, 0, 4;
    if ($magic eq MachO::FAT_MAGIC_32 or $magic eq MachO::FAT_CIGAM_32) {
      my $fo = FatObject->newWithPath($path);
      return &MachO::error($path, "can't load fat file") unless $data;
      return $fo->objectForArchFlag($archFlag);
    }
    elsif ($magic eq MachO::MH_MAGIC_32 or $magic eq MachO::MH_CIGAM_32 or
           $magic eq MachO::MH_MAGIC_64 or $magic eq MachO::MH_CIGAM_64) {
      my $mo = MachObject->newWithPath($path);
      my $mh = $mo->{'header'};
      my $arch = &MachO::archForFlag($archFlag);
      return &MachO::error($path, "unknown arch flag: $archFlag") unless $arch;
      if (&MachO::hashKeysMatch($mh, $arch, 'cputype', 'cpusubtype')) {
        return $mo;
      }
      return undef;
    }
    elsif (ArchiveObject->pathIsArchive($path)) {
      return ArchiveObject->newWithPath($path);
    }
    return MachO::error($path, "not an object file");
  }
  return MachObject->newWithPath($path);
}

1;
