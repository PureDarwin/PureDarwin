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

use FileHandle;
use MachO;

exit &main();

##############################################################################
#
# package NoteDesc
#

package NoteDesc;

sub new {
  my ($class, $data_owner, $content) = @_;
  my $self = bless {}, $class;
  $self->{'data_owner'} = $data_owner;
  $self->{'content'} = $content;
  return $self;
}

sub data_owner { return $_[0]->{'data_owner'}; }
sub content { return $_[0]->{'content'}; }

##############################################################################
#
# main
#

package main;

sub main {
  my $input;
  my $output;
  my @notes;
  my $verbose;

  # parse the arguments
  return &usage() unless (@ARGV);
  while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg eq "-h") {
      return &usage();
    }
    elsif ($arg eq "-v") {
      $verbose = 1;
    }
    elsif ($arg eq "-o") {
      return &usage("only one output file can be specified")
        if defined ($output);
      $output = shift @ARGV;
    }
    elsif ($arg eq "-add-note") {
      my $owner = shift @ARGV;
      my $content = shift @ARGV;
      return &usage("missing data-owner") unless $owner;
      return &usage("missing content") unless $content;
      push @notes, NoteDesc->new($owner, $content);
    }
    elsif ($arg eq "-add-note-file") {
      my $owner = shift @ARGV;
      my $file = shift @ARGV;
      return &usage("missing data-owner") unless $owner;
      return &usage("missing file") unless $file;
      die "file not found: $file\n" unless ( -e $file );
      my $fh = FileHandle->new("$file");
      my $content = join '', <$fh>;
      push @notes, NoteDesc->new($owner, $content);
    }
    elsif ($arg =~ /^-/) {
      return &usage("unknown option: $arg");
    }
    else {
      return &usage("only one input file can be specified") if defined ($input);
      $input = $arg;
    }
  }

  return &usage("one input file must be specified")
    unless defined ($input);
  return &usage("one output file must be specified")
    unless defined ($output);
#   return &usage("at least one note must be specified")
#     unless scalar(@notes);

  # open the mach-o
  die "file not found: $input\n" unless ( -e $input );
  my ($perm) = (stat $input)[2] & 07777;
  my ($inputSize) = (stat $input)[7];
  my ($obj) = MachO->newWithPath($input);
  die "unable to read $input\n" unless ($obj);

  my $mh = $obj->{'header'};
  my $mhsize = $mh->packsize($obj);
  my $sizeofcmds = $mh->{'sizeofcmds'};
  my $ncmds = $mh->{'ncmds'};
  my $ptralign = $obj->pointerAlignment();
  my $pagealign = $obj->segmentAlignment();

  # check for, and remove, the code signature load command
  my $cs_size = 0;
  my ($cs) = $obj->findLoadCommands(MachO::LC_CODE_SIGNATURE);
  if ($cs) {
    print "warning: stripping code signature\n";
    $sizeofcmds -= $cs->packsize($obj);
    $cs_size = $cs->{'datasize'};

    # ask perl to compute the index for this load command, as pennance for
    # using findLoadCommands instead of looping over all the items manually.
    my ($lcmds) = $obj->{'loadcmds'};
    my ($cs_idx) = grep { $$lcmds[$_] eq $cs } 0..$#$lcmds;
    splice @{$obj->{'loadcmds'}}, $cs_idx, 1;
    $ncmds -= 1;
  }

  # measure the notes load command sizes;
  my $notesCmdSize = 0;
  foreach my $noteRef (@notes) {
    $notesCmdSize += LoadCommandNote->packsize($obj);
    $ncmds += 1;
  }

  # verify we can fit the new load commands before start of __TEXT
  my $startOfText;
  foreach my $sectionRef (@{$obj->sections()}) {
    if ($sectionRef->{'segname'} eq "__TEXT") {
      my $offset = $sectionRef->{'offset'};
      if (!defined($startOfText) or
          $offset < $startOfText) {
        $startOfText = $offset;
      }
    }
  }

  die "not enough space to add new load commands\n"
    if ($startOfText < $mhsize + $sizeofcmds + $notesCmdSize);
  $sizeofcmds += $notesCmdSize;

  # measure the size of the symbol table(s)
  my $st_offset = $inputSize;
  my $st_size = 0;
  my ($st) = $obj->findLoadCommands(MachO::LC_SYMTAB);
  if ($st) {
    my $symsize = SymbolTable::sizeOfSymbol($obj);
    $st_offset = $st->{'symoff'};
    $st_size = $st->{'nsyms'} * $symsize + $st->{'strsize'};
  }
  my $output_st_offset = $st_offset;

  my ($dyst) = $obj->findLoadCommands(MachO::LC_DYSYMTAB);
  if ($dyst) {
    $st_size += &align(8 * $dyst->{'ntoc'}, $ptralign);
    $st_size += &align(52 * $dyst->{'nmodtab'}, $ptralign);
    $st_size += &align(4 * $dyst->{'nextrefsyms'}, $ptralign);
    $st_size += &align(4 * $dyst->{'nindirectsyms'}, $ptralign);
    $st_size += &align(8 * $dyst->{'nextrel'}, $ptralign);
    $st_size += &align(8 * $dyst->{'nlocrel'}, $ptralign);
  }

  # add the new note load commands at the end of the load command array,
  # reserve space for the note payload right before the symbol table, and
  # move the symbol table data out of the way.
  my $notesDataSize = 0;
  foreach my $noteRef (@notes) {
    my $length = length($noteRef->content());
    $length = &align($length, $ptralign);
    $notesDataSize += $length;

    my $lc = LoadCommand->new($obj, MachO::LC_NOTE);
    $lc->{'cmdsize'} = $lc->packsize($obj); # TODO: should this be automatic?
    $lc->{'data_owner'} = $noteRef->{'data_owner'};
    $lc->{'offset'} = $output_st_offset;
    $lc->{'size'} = $length;
    $output_st_offset += $length;
    if ($st) {
      $st->{'symoff'} += $length;
      $st->{'stroff'} += $length;
    }
    if ($dyst) {
      $dyst->{'tocoff'} += $length if ($dyst->{'tocoff'});
      $dyst->{'modtaboff'} += $length if ($dyst->{'modtaboff'});
      $dyst->{'extrefsymoff'} += $length if ($dyst->{'extrefsymoff'});
      $dyst->{'indirectsymoff'} += $length if ($dyst->{'indirectsymoff'});
      $dyst->{'extreloff'} += $length if ($dyst->{'extreloff'});
      $dyst->{'locreloff'} += $length if ($dyst->{'locreloff'});
    }

    push @{$obj->{'loadcmds'}}, $lc;
  }

  # adjust linkedit and mach_header to account for the file changes (removing
  # the code signature and adding the notes)
  my $le;
  foreach my $lc ($obj->findLoadCommands(MachO::LC_SEGMENT,
                                         MachO::LC_SEGMENT_64)) {
    if ($lc->{'segname'} eq "__LINKEDIT") {
      die "multiple __LINKEDIT load commands\n" if defined($le);
      $le = $lc;
    }
  }
  if ($le) {
    $le->{'filesize'} = $le->{'filesize'} + $notesDataSize - $cs_size;
    $le->{'vmsize'} = &align($le->{'filesize'}, $pagealign);
  }

  $mh->{'sizeofcmds'} = $sizeofcmds;
  $mh->{'ncmds'} = $ncmds;

  # now that the header and load commands are correct, repack the file data.
  # this updates the load command array, but does not modify linkedit.
  my $offset = $mhsize;
  foreach my $lc (@{$obj->{'loadcmds'}}) {
    $lc->locate($obj, $offset);
    $offset += $lc->packsize($obj);
  }
  $obj->repack();

  # create the output file. Write out all the data up to the beginning of the
  # old symbol table. Then write out the new notes. Finally, write out the
  # symbol table(s).
  my $temp = "$output.$$";
  my $fh = FileHandle->new(">$temp");
  my $data = $obj->{"data"};
  my $wrote = syswrite($fh, $data, $st_offset);
  if ($wrote != $st_offset) {
    print STDERR "can't write to $temp: $!\n";
    unlink $temp;
    return 1;
  }
  foreach my $noteRef (@notes) {
    my $content = &alignstr($noteRef->content(), $ptralign);
    $wrote = syswrite($fh, $content, length($content));
    if ($wrote != length($content)) {
      print STDERR "can't write to $temp: $!\n";
      unlink $temp;
      return 1;
    }
  }
  if ($st) {
    my ($st_data) = substr($data, $st_offset, $st_size);
    $wrote = syswrite($fh, $st_data, length($st_data));
    if ($wrote != length($st_data)) {
      print STDERR "can't write to $temp: $!\n";
      unlink $temp;
      return 1;
    }
  }

  chmod $perm, $temp;
  rename $temp, $output;

  return 0;
}

##############################################################################
#
# alpow
#
# given a value, return the index of the lowest set bit. For values that are
# a power of two, this returns the power exponent; e.g., 4 => 2, 8 => 3, etc.
#
# This is commonly used to take a pointer size or page size and convert it
# to an exponenent for the align(), alignstr(), and alignment() functions.
#
# BUG: This has a really bad name.

sub alpow {
  my ($value) = @_;
  my $result;
  for (my $i = 0; $i < 32; ++$i) {
    if ($value & (1 << $i)) {
      if (defined($result)) {
        return undef;
      } else {
        $result = $i;
      }
    }
  }

  return $result;
}

##############################################################################
#
# alignment
#
# given a value and an exponent, return a value necessary to round the input
# value to be a multiple of 2 raised to that exponenet. See "align" for the
# more general case value.
#
# BUG: Working with exponent values is annoying.
sub alignment {
  my ($value, $alpow, $backwards) = @_;
  MachO::abort "undefined value" unless defined($value);
  MachO::abort "undefined align" unless defined($alpow);
  my $align = 1 << $alpow;
  if ($backwards) {
    return -1 * ($value % $align);
  } elsif ($value % $align) {
    return $align - ($value % $align);
  }
  return 0;
}

##############################################################################
#
# alignment
#
# round a given value to be a multiple of 2 raised to an exponenet. Can be used
# to align values to pointer boundaries, or sizes to page boundaries.
#
# BUG: Working with exponent values is annoying.

sub align {
  my ($value, $alpow, $backwards) = @_;
  return $value + &alignment($value, $alpow, $backwards);
}

##############################################################################
#
# alignstr
#
# pad a string value so its length is a multiple of 2 raised to an exponenet.
#
# BUG: Working with exponent values is annoying.

sub alignstr {
  my ($str, $alpow) = @_;
  my $pad = &main::alignment(length($str), $alpow);
  $str .= pack "x${pad}";
  return $str;
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
  -h          - Help, print usage and exit.
  -add-note <data-owner> <content>
                Add a note to a Mach-O file. The note load command will have
                its data_owner character array to <data-owner>. The note
                payload, <content>, will be written right before the Mach-O's
                symbol table.
  -add-note-file <data-owner> <content file>
                As -add-note, but the note content is read from <content file>.
  -o <output> - Output file. Required.
  -v          - Print verbose output.
USAGE
  return 1;
}
