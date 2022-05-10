#!/bin/sh

# work around the problem documented in:
#
#   <rdar://problem/46784878> can't build multiple command-line tools with the same short name via Xcode
#
# Rename all of the "as-<something>" drivers to "as" after they're installed.

( cd ${DSTROOT}/${CCTOOLS_USRLIBEXEC}/as;
  ( cd arm; rm -f as; mv as-arm as );
  ( cd i386; rm -f as; mv as-i386 as );
  ( cd x86_64; rm -f as; mv as-x86_64 as );
);
