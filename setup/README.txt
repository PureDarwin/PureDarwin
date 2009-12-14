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

Updated on 20091209.

Preliminaries
=============

Before you start, make backups of all your data.
Do not use any production machines.
PLEASE DO NOT IGNORE THIS STEP!

These instructions are only for technical persons anyway, so they are short.
Developers and testers, please provide feedback via the way you prefer. 

Installation
============

0. Check and edit the `pd_config' configuration file to reflect your needs.

1. Get binary roots and binary drivers with `pd_fetch'.

2. Thin them with `pd_thin'.
   
3. Then see the usage of `pd_setup':

   Set up a bootable PureDarwin system.

   Usage: pd_setup any_output_filename VolumeName

       * Install to physical disk
       pd_setup /Volumes/PureDarwin PureDarwin
       pd_setup /dev/diskX PureDarwin

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
contact at puredarwin.org

Additional Licensing Information
================================

IMPORTANT LICENSING INFORMATION:  The Apple-developed portions of the
Source Code and corresponding binary package folders are covered by the
Apple Public Source License that can be found in the file /APPLE_LICENSE.txt.

The Apple binary drivers and kernel extension files are covered by a separate
Apple Binary Driver Software License Agreement that can be found in the file
/APPLE_DRIVER_LICENSE.txt.

The PureDarwin "work" is covered by the BSD License that can be found in the
file /PUREDARWIN_LICENSE.txt.

Other portions of Darwin may be covered by third party licenses.  Please
read these licenses carefully before using any of this software, as your use
of this software signifies that you have read the licenses and that you
accept and agree to their respective terms. 
Please see the respective projects for more information.

Please read all these licenses carefully before using any of this software, 
as your use of this software signifies that you have read the licenses and 
that you accept and agree to their respective terms.
