# LEB128.pm
#
# Routines for encoding and decoding ULEB and SLEB values.
#
# Encoding is straight forward: pass in a value, get back a packed array with
# the requested LEB encoing. E.g.,
#
#   my $data = &ULEB::encode($value);
#   my $data = &SLEB::encode($value);
#
# Decoding can be done in the same way:
#
#   my $value = &ULEB::decode($data);
#
# But usually one decodes LEB values from a byte stream, which is not something
# native perl is especially good at. As an expensive but convenient solution,
# the decoders will return both the value and a new array with the remaining
# data when called in a scalar context:
#
#   my ($value1, $value2);
#   ($value1, $data) = &ULEB::decode($data);
#   ($value2, $data) = &ULEB::decode($data);
#
# If returning a new copy of $data is too expensive, one can pass a ref to an
# index into the decoder, where on exit the decoder will advance the index past
# the end of the ULEB content.
#
#   my $i = 0;
#   my $value1 = &ULEB::decode($data, \$i);
#   my $value2 = &ULEB::decode($data, \$i);
#
# This avoids unnecessary copying if one is decoding a stream of numbers, at
# the expense of being less convenient to use with pack/unpack.
#
# Note: Pack does support unsigned BER. But we need LEB, signed and unsigned.
# Another approach might involve requiring callers to unpack buffers into
# arrays of unsigned bytes, but that just passes the burden of managing the
# data onto the callers, and it still doesn't solve the streaming problem.

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

use MachO::Base;

package ULEB;

sub encode {
  my ($value) = @_;
  my @bytes;

  MachO::abort "value is undefined" unless defined $value;

  do {
    my $byte = $value & 0x7F;
    $value >>= 7;
    $byte |= 0x80 if ($value);
    push @bytes, $byte;
  } while ($value);

  return pack "C*", @bytes;
}

sub decode {
  my ($data, $iRef) = @_;
  my $value = 0;
  my $shift = 0;
  my $byte = 0x80;
  my $i = defined ($iRef) ? $$iRef : 0;

  while ($byte & 0x80) {
    $byte = unpack "C", substr $data, $i++, 1;
    $value |= ($byte & 0x7F) << $shift;
    $shift += 7;
  }

  $$iRef = $i if defined ($iRef);
  return wantarray ? ($value, substr $data, $i) : $value;
}

package SLEB;

sub encode {
  my ($value) = @_;
  my @bytes;
  my $neg = $value < 0 ? 1 : 0;
  my $byte = $neg ? 0x40 : 0;
  my $size = 64;
  my $done = 0;

  while (!$done)
  {
    my $byte = $value & 0x7F;
    $value >>= 7;
    $value |= (~0 << ($size - 7)) if ($neg);
    if ( ($byte & 0x40) and $value == ~0 or
        !($byte & 0x40) and $value == 0) {
      $done = 1;
    } else {
      $byte |= 0x80;
    }
    push @bytes, $byte;
  }

  return pack "C*", @bytes;
}

sub decode {
  my ($data, $iRef) = @_;
  my $value = 0;
  my $shift = 0;
  my $byte = 0x80;
  my $i = defined ($iRef) ? $$iRef : 0;

  while ($byte & 0x80) {
    $byte = unpack "C", substr $data, $i++, 1;
    $value |= ($byte & 0x7F) << $shift;
    $shift += 7;
  }

  if ($byte & 0x40) {
    $value |= (~0 << $shift);

    # coerce perl into using signed numbers instead of unsigned ...
    $value = (~$value + 1) * -1;
  }

  $$iRef = $i if defined ($iRef);
  return wantarray ? ($value, substr $data, $i) : $value;
}

package CStr;

sub encode ($) {
  my ($value) = @_;
  return pack "a* x", $value;
}

sub decode ($\$) {
  my ($data, $i) = @_;
  my (@bytes, $byte);
  do {
    $byte = unpack "C", substr $data, ${$i}++, 1;
    push @bytes, $byte;
  } while ($byte ne 0);
  return unpack "A*", pack "C*", @bytes;
}

1;
