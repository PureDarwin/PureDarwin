# Trie.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

use MachO::LEB128;
use MachO::LoadCommands;

# package Trie
#
# The Trie package encodes and decodes compact exports referred to by Mach-O's
# LC_DYLD_INFO and LC_DYLD_INFO_ONLY load commands. An "export" is a hash with
# the following format:
#
#   'name'  -> String, the symbol name
#   'addr'  -> Number, offset from the text segment's vmaddr (not an address)
#   'flags' -> Number, see LoadCommands.pm

package Trie;

# PACKAGE::encode($value [, ...])
# Encode an array of export hashes (see above) into a compact binary buffer
# that can be written to a binary file. Values will be encoded in a stable,
# sorted order.
sub encode {
  my (@values) = @_;

  # build the trie
  my $trie = TrieNode->new();
  foreach my $value (@values) {
    my $name = $value->{'name'};
    $trie->add($name, $value);
  }

  # flatten the nodes
  my @nodes;
  $trie->visitNodes(sub {
                      my ($node, $context) = @_;
                      push @$context, $node;
                    }, \@nodes);

  # stabilize the uleb offsets
  my $more;
  do {
    my $offset = 0;
    $more = 0;

    foreach my $node (@nodes) {
      $more |= &_layout($node, \$offset);
    }
  } while ($more);

  # serialize
  my $data;
  foreach my $node (@nodes) {
    &_encode($node, \$data);
  }
  return $data;
}

# PACKAGE::decode($data [, $indexRef, $stringPrefix])
# When called with a single argument pointing to encoded binary Trie data, the
# data is decoded into a list of export hashes (see above). This method will
# call itself recursively, passing an index into the data array by reference and
# a string representing the current string prefix. The returned export hash list
# will be in a stable, sorted order.
sub decode ($\$$) {
  my ($data, $i, $s) = @_;
  my @values;
  my $istore = 0;
  $i = \$istore unless defined $i;
  $s = "" unless defined $s;

  my $len = &ULEB::decode($data, $i);
  if ($len != 0) {
    my $flags = &ULEB::decode($data, $i);
    my $offset = &ULEB::decode($data, $i);
    push @values, { 'name' => $s, 'flags' => $flags, 'addr' => $offset};
  }
  my $out = &ULEB::decode($data, $i);
  for (my $j = 0; $j < $out; ++$j) {
    my $str = $s . &CStr::decode($data, $i);
    my $new = &ULEB::decode($data, $i);
    &decode($data, \$new, $str);
  }

  return @values;
}

# _layout($node, $offsetRef)
# The _layout sub does two related things: it estimates the size of a Trie
# node, and it records the node's offset into a buffer. Each node is serialized
# with the following information:
#
#   * length of export data, encoded as a ULEB. If non-zero:
#     - flags, encoded as a ULEB
#     - the offset from __TEXT's vmaddr, encoded as a ULEB
#   * the number of edges, as a ULEB. For each edge:
#     - the edge's suffix, as a null-terminated C string
#     - offset to the edge's node, as a ULEB.
#
# Estimating the node's size is straight-forward except for one thing: each
# edge encodes the offset of its child node, and the encoded size of that
# offset depends on its value. A ULEB < 128 can be encoded in a single byte,
# whereas larger values are encoded in 2 or more bytes.
#
# _layout solves this problem by computing the offset of each child node, and
# then storing that offset within the child node. If a node's stored offset
# doesn't match its expected location, the new location is recorded and _layout
# will return 1, indicating the layout is not yet stable; if the offset for
# this node is currently stable, _layout will return 0.
#
# In practice, the encode() sub will call _layout on every node in the try
# until all nodes are stable. If _layout returns 1 for any node, encode will
# repeat the layout process.
#
# Note: This is the same stabilizing algorithm used by ld64.
sub _layout {
  my ($node, $offsetRef) = @_;

  # write the expected offset into this node, and remember if the value changed
  my $unstable = ($$offsetRef != ($node->{'offset'} || 0));
  $node->{'offset'} = $$offsetRef;

  # estimate the size of the node's payload
  my $size = 1;
  my $value = $node->{'value'};
  if (defined($value)) {
    my $flags = $value->{'flags'};
    if ($flags & MachO::EXPORT_SYMBOL_FLAGS_REEXPORT) {
      # TODO
    }
    else {
      $size  = length(&ULEB::encode($value->{'flags'}));
      $size += length(&ULEB::encode($value->{'addr'}));
      if ($flags & MachO::EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER) {
        # TODO
      }
    }
    $size += length(&ULEB::encode($size));
  }

  # estimate the size of the node's edges
  my @edges = sort keys %{$trie->{'edges'}};
  $size += length(&ULEB::encode(scalar(@edges)));
  foreach my $edge (@edges) {
    my $child = $self->{'edges'}->{$edge};
    my $choff = ($child->{'offset'} || 0);
    $size += length($edge) + 1;
    $size += length(&ULEB::encode($choff))
  }

  # adjust the offset counter by the estimated size of this node, where it
  # will represent the expected offset of the next node in the Trie.
  $$offsetRef += $size;

  return $unstable;
}

# _encode($trie, $dataRef)
# The _encode sub serializes a trie node into a binary buffer, and appends it
# to a data buffer passed in by reference. Each node is serialized with the
# following information:
#
#   * length of export data, encoded as a ULEB. If non-zero:
#     - flags, encoded as a ULEB
#     - the offset from __TEXT's vmaddr, encoded as a ULEB
#   * the number of edges, as a ULEB. For each edge:
#     - the edge's suffix, as a null-terminated C string
#     - offset to the edge's node, as a ULEB.
#
# _encode does not return a meaningful value.
sub _encode {
  my ($trie, $dataRef) = @_;

  my $value = $trie->{'value'};
  my $subdata = "";
  if (defined($value)) {
    $subdata .= &ULEB::encode($value->{'flags'});   # flags
    $subdata .= &ULEB::encode($value->{'addr'});    # value offset
  }
  $$dataRef .= &ULEB::encode(length($subdata));     # len
  if (length($subdata)) {
    $$dataRef .= $subdata;
  }

  my @edges = sort keys %{$trie->{'edges'}};
  $$dataRef .= &ULEB::encode(scalar(@edges));       #count
  foreach my $edge (@edges) {
    my $child = $trie->{'edges'}->{$edge};
    $$dataRef .= &CStr::encode($edge);              # string fragment
    $$dataRef .= &ULEB::encode($child->{'offset'}); # child offset
  }
}

###############################################################################
#
# The TrieNode class implements a reusable trie data structure of strings
# mapping to scalar values. Due to the minimal nature of the structure, values
# must be defined.
#
# Create a new trie
#
#   my $trie = TrieNode->new();
#
# Add a string / value pair
#
#   $trie->add($string, $value);
#
# Visit all the elements of the trie
#
#   $trie->visit(sub { print "$_[0]: $_[1]\n" });

package TrieNode;

# PACKAGE->new()
# new creates a new root trie node, used by clients to make a trie.
sub new {
  my ($class) = @_;
  return $class->_new();
}

# PACKAGE->_new()
# new creates a new trie node with a value, for TrieNode's private use.
sub _new {
  my ($class, $value) = @_;
  my $self = bless {}, $class;
  $self->{'edges'} = {};
  $self->{'value'} = $value;
  return $self;

}

# OBJ->add($string, $value)
# Add a string to the trie, and associate a scalar value with that string.
# Both the string and the value must be defined.
sub add {
  my ($self, $str, $value) = @_;

  die "bad string" unless defined $str;
  die "bad value" unless defined $value;

  # search through children looking for a node that shares data with our
  my $l = -1;
  my $edge;
  my $node;
  foreach my $e (keys %{$self->{'edges'}}) {
    my ($a, $b);
    do {
      $l++;
      $a = substr ($e, $l, 1);
      $b = substr ($str, $l, 1);
    }
    while (defined($a) and defined ($b) and $a eq $b);
    if ($l) {
      $edge = $e;
      $node = $self->{'edges'}->{$edge};
      last;
    }
  }

  # no edges match, insert data.
  if (!defined($edge)) {
    $self->{'edges'}->{$str} = $node = TrieNode->_new($value);
  }
  # str is an exact match of edge
  elsif ($str eq $edge) {
    $node->add("", $value);
  }
  # replace edge with a common prefix.
  else {
    my $cedge = substr $str, 0, $l;
    my $cnode = $self->{'edges'}->{$cedge};

    # continue down existing edge (cnode is node)
    if (defined($cnode)) {
      $str = substr $str, $l;
      if ($str eq "") {
        $cnode->{'value'} = $value;
      } else {
        $cnode->add($str, $value);
      }
    }
    else {
      # split existing edge
      delete($self->{'edges'}->{$edge});
      $cnode = $self->{'edges'}->{$cedge} = TrieNode->_new();

      $edge = substr $edge, $l;
      $cnode->{'edges'}->{$edge} = $node;

      $str = substr $str, $l;
      if ($str eq "") {
        $cnode->{'value'} = $value;
      } else {
        $cnode->{'edges'}->{$str} = TrieNode->_new($value);
      }
    }
  }
}

# OBJ->visit($function [, $context [, $[prefix]])
# visit walks the trie and calls the supplied function once for each string.
# The function is either a reference to a perl subroutine or an anonymous
# subroutine, and it receives the string and the associated value as arguments.
# Callers can include a context scalar that is also passed to the function. The
# prefix argument is used by the internal implementation and should not be
# supplied by clients.
#
# Example 1:
#
#   sub visitor {
#     my ($string, $value, $context) = @_;
#     $$context .= sprintf "%s => %s\n", $string, $value;
#   }
#
#   my $result;
#   $trie->visit(\&visitor, \$result);
#
# Example 2:
#
#   $trie->visit(sub { print "$_[0] => $_[1]\n" });
sub visit {
  my ($self, $func, $context, $prefix) = @_;

  if (defined($self->{'value'})) {
    $func->($prefix, $self->{'value'}, $context);
  }

  foreach my $edge (sort keys %{$self->{'edges'}}) {
    my $node = $self->{'edges'}->{$edge};
    $node->visit($func, $context, $prefix . $edge);
  }
}

# OBJ->visitNodes($function [, $context])
# visitNodes walks the trie and calls the supplied function once for each node.
# The arguments match those in the visit method. This method may aid clients
# who wish to serialize trie structures.
sub visitNodes {
  my ($self, $func, $context) = @_;

  $func->($self, $context);

  foreach my $edge (sort keys %{$self->{'edges'}}) {
    my $node = $self->{'edges'}->{$edge};
    $node->visitNodes($func, $context);
  }
}

1;
