# CPU.pm

# silence perl development errors by patching @INC
BEGIN { push @INC, ".." unless $INC[-1] eq ".."; }

package MachO;

use MachO::Base;

use constant {
  CPU_ARCH_ABI64    => 0x1000000,
  CPU_ARCH_ABI64_32 => 0x2000000,
};

use constant {
  CPU_TYPE_I386 => 7,
  CPU_TYPE_X86_64 => 7 | CPU_ARCH_ABI64,
  CPU_TYPE_ARM => 12,
  CPU_TYPE_ARM64 => 12 | CPU_ARCH_ABI64,
  CPU_TYPE_ARM64_32 => 12 | CPU_ARCH_ABI64_32,
  CPU_TYPE_POWERPC => 18,
};

use constant CPUTYPES =>
qw(CPU_TYPE_I386 CPU_TYPE_X86_64 CPU_TYPE_ARM CPU_TYPE_ARM64 CPU_TYPE_ARM64_32
   CPU_TYPE_POWERPC);

use constant {
  CPU_SUBTYPE_I386_ALL => 3,
};

use constant CPUSUBTYPES_I386 =>
qw(CPU_SUBTYPE_I386_ALL);

use constant {
  CPU_SUBTYPE_LIB64 => 0x80000000,
};

use constant {
  CPU_SUBTYPE_X86_64_ALL => CPU_SUBTYPE_I386_ALL,
  CPU_SUBTYPE_X86_64_H => 8,
};

use constant CPUSUBTYPES_X86_64 =>
qw(CPU_SUBTYPE_X86_64_ALL CPU_SUBTYPE_X86_64_H);

use constant {
  CPU_SUBTYPE_ARM_V7K => 12,
  CPU_SUBTYPE_ARM_V8 => 13,
};

use constant CPUSUBTYPES_ARM =>
qw(CPU_SUBTYPE_ARM_V7K CPU_SUBTYPE_ARM_V8);


use constant {
  CPU_SUBTYPE_ARM64_ALL => 0,
  CPU_SUBTYPE_ARM64_V8 => 1,
  CPU_SUBTYPE_ARM64E => 2,
};

use constant CPUSUBTYPES_ARM64 =>
qw(CPU_SUBTYPE_ARM64_ALL CPU_SUBTYPE_ARM64_V8 CPU_SUBTYPE_ARM64E);

use constant {
  CPU_SUBTYPE_ARM64_32_V8 => 1,
};

use constant CPUSUBTYPES_ARM64E =>
qw(CPU_SUBTYPE_ARM64_ALL CPU_SUBTYPE_ARM64_V8 CPU_SUBTYPE_ARM64E);

use constant {
  CPU_SUBTYPE_POWERPC_ALL	=> 0,
  CPU_SUBTYPE_POWERPC_601	=> 1,
  CPU_SUBTYPE_POWERPC_602	=> 2,
  CPU_SUBTYPE_POWERPC_603	=> 3,
  CPU_SUBTYPE_POWERPC_603e	=> 4,
  CPU_SUBTYPE_POWERPC_603ev	=> 5,
  CPU_SUBTYPE_POWERPC_604	=> 6,
  CPU_SUBTYPE_POWERPC_604e	=> 7,
  CPU_SUBTYPE_POWERPC_620	=> 8,
  CPU_SUBTYPE_POWERPC_750	=> 9,
  CPU_SUBTYPE_POWERPC_7400	=> 10,
  CPU_SUBTYPE_POWERPC_7450	=> 11,
  CPU_SUBTYPE_POWERPC_970	=> 100,
};

use constant CPUSUBTYPES_POWERPC =>
qw(CPU_SUBTYPE_POWERPC_ALL CPU_SUBTYPE_POWERPC_601 CPU_SUBTYPE_POWERPC_602
   CPU_SUBTYPE_POWERPC_603 CPU_SUBTYPE_POWERPC_603e CPU_SUBTYPE_POWERPC_603ev
   CPU_SUBTYPE_POWERPC_604 CPU_SUBTYPE_POWERPC_604e CPU_SUBTYPE_POWERPC_620
   CPU_SUBTYPE_POWERPC_750 CPU_SUBTYPE_POWERPC_7400 CPU_SUBTYPE_POWERPC_7450
   CPU_SUBTYPE_POWERPC_970);

# common arch flags
use constant {
  i386     => { 'cputype' => CPU_TYPE_I386,     'cpusubtype' => CPU_SUBTYPE_I386_ALL,    'swap' => '<', 'bits' => 32, 'align' => 0x1000 },
  x86_64   => { 'cputype' => CPU_TYPE_X86_64,   'cpusubtype' => CPU_SUBTYPE_X86_64_ALL,  'swap' => '<', 'bits' => 64, 'align' => 0x1000 },
  x86_64h  => { 'cputype' => CPU_TYPE_X86_64,   'cpusubtype' => CPU_SUBTYPE_X86_64_H,    'swap' => '<', 'bits' => 64, 'align' => 0x1000 },
  armv7k   => { 'cputype' => CPU_TYPE_ARM,      'cpusubtype' => CPU_SUBTYPE_ARM_V7K,     'swap' => '<', 'bits' => 32, 'align' => 0x4000 },
  arm64    => { 'cputype' => CPU_TYPE_ARM64,    'cpusubtype' => CPU_SUBTYPE_X86_64_H,    'swap' => '<', 'bits' => 64, 'align' => 0x4000 },
  arm64e   => { 'cputype' => CPU_TYPE_ARM64,    'cpusubtype' => CPU_SUBTYPE_X86_64_H,    'swap' => '<', 'bits' => 64, 'align' => 0x4000 },
  arm64_32 => { 'cputype' => CPU_TYPE_ARM64_32, 'cpusubtype' => CPU_SUBTYPE_ARM64_32_V8, 'swap' => '<', 'bits' => 32, 'align' => 0x4000 },
  ppc      => { 'cputype' => CPU_TYPE_POWERPC,  'cpusubtype' => CPU_SUBTYPE_POWERPC_ALL, 'swap' => '>', 'bits' => 64, 'align' => 0x1000 },
};

use constant ARCHFLAGS =>
qw(i386 x86_64 x86_64h armv7k arm64 arm64e arm64_32 ppc);

sub archForFlag {
  my ($flag) = @_;
  return &valueForName($flag, ARCHFLAGS);
}

sub archForCPU {
  my ($cputype, $cpusubtype) = @_;
  foreach my $arch (ARCHFLAGS) {
    if (&$arch()->{'cputype'} eq $cputype and
        &$arch()->{'cpusubtype'} eq $cpusubtype) {
      return &$arch();
    }
  }
  return undef;
}

sub flagForArch {
  my ($value) = @_;
  foreach my $arch (ARCHFLAGS) {
    my $equal = 1;
    foreach my $key (qw(cputype cpusubtype swap)) {
      if (&$arch()->{$key} ne $value->{$key}) {
        $equal = 0;
        last;
      }
    }
    return $arch if $equal;
  }
  return undef;
}

sub cputypeName {
  my ($value) = @_;
  return &nameForValue($value, CPUTYPES);
}

sub cpusubtypeName {
  my ($cputype, $value) = @_;

  # we currently require clients to strip the cap flags themselves, as the
  # rewrite-macho.pl script uses this method to print names for canonical
  # values only. perhaps this is not correct ...
  #$value &= 0xFFFFFF;

  if ($cputype eq CPU_TYPE_I386) {
    return &nameForValue($value, CPUSUBTYPES_I386);
  }
  if ($cputype eq CPU_TYPE_X86_64) {
    return &nameForValue($value, CPUSUBTYPES_X86_64);
  }
  if ($cputype eq CPU_TYPE_ARM) {
    return &nameForValue($value, CPUSUBTYPES_ARM);
  }
  if ($cputype eq CPU_TYPE_ARM64) {
    return &nameForValue($value, CPUSUBTYPES_ARM64);
  }
  if ($cputype eq CPU_TYPE_ARM64_32) {
    return &nameForValue($value, CPUSUBTYPES_ARM64E);
  }
  if ($cputype eq CPU_TYPE_POWERPC) {
    return &nameForValue($value, CPUSUBTYPES_POWERPC);
  }
}

sub cputypeFromName {
  my ($name) = @_;
  return &valueForName($name, CPUTYPES);
}

sub cpusubtypeFromName {
  my ($cputype, $name) = @_;
  if ($cputype eq CPU_TYPE_I386) {
    return &valueForName($name, CPUSUBTYPES_I386);
  }
  if ($cputype eq CPU_TYPE_X86_64) {
    return &valueForName($name, CPUSUBTYPES_X86_64);
  }
  if ($cputype eq CPU_TYPE_ARM) {
    return &valueForName($name, CPUSUBTYPES_ARM);
  }
  if ($cputype eq CPU_TYPE_ARM64) {
    return &valueForName($name, CPUSUBTYPES_ARM64);
  }
  if ($cputype eq CPU_TYPE_ARM64_32) {
    return &valueForName($name, CPUSUBTYPES_ARM64E);
  }
  if ($cputype eq CPU_TYPE_POWERPC) {
    return &valueForName($name, CPUSUBTYPES_POWERPC);
  }
}

1;
