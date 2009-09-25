Copyright (c) 2007-2009 The PureDarwin Project.
All rights reserved.

@LICENSE_HEADER_START@

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

@LICENSE_HEADER_END@

Updated on 20090925.

Preliminaries
=============

Before you start, make backups of all your data.
Do not use any production machines.
PLEASE DO NOT IGNORE THIS STEP!

These instructions are only for technical persons anyway, so they are short.
Developers and testers, please provide feedback to "contact@puredarwin.org". 

Status
======

Installation process still needs a Mac in order to build or deploy PureDarwin.
However most of these steps can be done on PureDarwin or even BSD and GNU/Linux,
only few alternatives are missing in order to replace few binaries.

Installation
============

1. Get binary roots and binary drivers with `pd_fetch'
   They will be stored respectively in:
      - pd_tmp/pd_binaryroots/ 
      - pd_tmp/pd_binarydrivers/

2. Thin them with `pd_i386thinner'
   Thinned result will be placed in pd_tmp/Packages_i386/
   
3. Then see the usage of `pd_setup':

   Set up a bootable PureDarwin system.

   Usage: pd_setup any_output_filename VolumeName

       * Install to physical disk
       pd_setup /Volumes/PureDarwin PureDarwin

       * Create an ISO 9660 image (.iso)
       pd_setup /tmp/puredarwin.iso PureDarwin

       * Create a ready-to-run VMware virtual machine (.vmwarevm)
       pd_setup puredarwin.vmwarevm PureDarwin

       * Create a ready-to-run VMware virtual disk (.vmdk)
       pd_setup puredarwin.vmdk PureDarwin

Resources
=========

http://opensource.apple.com
http://www.puredarwin.org
http://puredarwin.googlecode.com

#puredarwin on irc.freenode.net
