# StringTable

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use MachO::Base;

# StringTable implements a Mach-O string table, pointed at by a LC_SYMTAB
# load command.
#
package StringTable;

use parent -norequire, MachO::Base;

sub new {
  my ($class, $mo, $data, $offset) = @_;
  my $self = bless {}, $class;
  $self->depack($mo, $data, $offset);
  return $self;
}

sub depack {
  my ($self, $mo, $data, $offset) = @_;

  $self->{'_data'} = $data if (defined($data));
  $self->{"_location"} = $offset if defined($offset);

  # unpack the null terminated strings into an offset => string map
  my $x = pack "x";
  my %offsetToStringMap;
  $offset = 0;
  foreach my $raw (split $x, $data) {
    my $s = unpack "a*", $raw;
    $offsetToStringMap{$offset} = $s;
    $offset += length($s) + 1;
  }
  $self->{'_offsetToStringMap'} = \%offsetToStringMap;
}

sub repack {
  my ($self, $mo) = @_;

  # because symbols index into the string table at potentially arbitrary
  # locations, we can't simply repack the data; instead client code will
  # need to build a new string table from some other source, such as the
  # symbol table.
  #
  # I suppose we could do that from the $mo; get the symbols, build the
  # string table, and rewrite the strx values. Clients would need to
  # repack the string table before repacking the symbol table. But we'd
  # want a tighter API guarantee...

#   my $data;
#
#   # concatenate null terminated strings in offset order
#   my $map = $self->{'_offsetToStringMap'};
#   foreach my $key (sort { $a <=> $b } keys %$map) {
#     my $s = $map->{$key};
#     $data .= pack "a* x", $s;
#   }
#
#   $self->{'_data'} = $data;
}

sub description {
  my ($self, $verbose) = @_;
  my ($desc) = "";

#   $desc .= sprintf("%6s  %s\n", "strings", "");

  my $map = $self->{'_offsetToStringMap'};
  foreach my $key (sort { $a <=> $b } keys %{$map}) {
    my $s = $map->{$key};
    $desc .= sprintf("%6s: %s\n", $key, $s);
  }
  return $desc;
}

sub stringAtOffset {
  my ($self, $offset) = @_;

  # get the string from the offset cache.
  my $map = $self->{'_offsetToStringMap'};
  my $string = $map->{$offset};

  # on a cache miss, we may be trying to index into the middle of a string
  # (e.g., offset 2 within "s_status" == "status"). So pull it from the
  # whole string table.
  unless (defined($string)) {
    my $data = substr $self->{'_data'}, $offset;
    my $length = index $data, pack "x";
    $string = unpack "a*", substr $data, 0, $length;
  }
  return $string;
}

1;
